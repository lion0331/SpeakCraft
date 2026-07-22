#include "SpeechService.h"
#include "ConfigManager.h"
#include "Resource.h"
#include <sphelper.h>

SpeechService::SpeechService()
{}

SpeechService::~SpeechService()
{
	StopRecognition();
	StopSpeaking();
	if (m_pVoice)      { m_pVoice->Release(); m_pVoice = nullptr; }
	if (m_pCurrentToken) { m_pCurrentToken->Release(); m_pCurrentToken = nullptr; }
	if (m_pRecoContext) { m_pRecoContext->Release(); m_pRecoContext = nullptr; }
	if (m_pRecognizer)  { m_pRecognizer->Release(); m_pRecognizer = nullptr; }
}

bool SpeechService::Initialize()
{
	if (m_initialized) return true;

	// COM must already be initialized on this thread (main.cpp does it)
	HRESULT hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL,
		IID_ISpVoice, reinterpret_cast<void**>(&m_pVoice));
	if (FAILED(hr) || !m_pVoice)
	{
		return false;
	}

	// Apply saved settings
	auto& cfg = ConfigManager::Instance();
	if (!cfg.GetVoiceToken().empty())
	{
		SetVoice(cfg.GetVoiceToken());
	}
	SetRate(cfg.GetSpeechRate());
	SetVolume(cfg.GetSpeechVolume());

	m_initialized = true;
	return true;
}

bool SpeechService::SpeakAsync(const std::wstring& text, HWND hwndNotify)
{
	if (!m_initialized || !m_pVoice) return false;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	StopSpeaking();

	// Set notification
	if (hwndNotify)
	{
		m_pVoice->SetNotifyWindowMessage(hwndNotify, WM_SPEECH_COMPLETE, 0, 0);
		ULONGLONG interest = SPFEI(SPEI_END_INPUT_STREAM) | SPFEI(SPEI_WORD_BOUNDARY);
		m_pVoice->SetInterest(interest, interest);
	}

	HRESULT hr = m_pVoice->Speak(text.c_str(), SPF_ASYNC, nullptr);
	return SUCCEEDED(hr);
}

bool SpeechService::Speak(const std::wstring& text)
{
	if (!m_initialized || !m_pVoice) return false;

	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	StopSpeaking();

	HRESULT hr = m_pVoice->Speak(text.c_str(), SPF_DEFAULT, nullptr);
	return SUCCEEDED(hr);
}

void SpeechService::StopSpeaking()
{
	if (m_pVoice)
	{
		m_pVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
		// Do NOT call Pause() — it can interfere with the next Speak()
	}
}

bool SpeechService::IsSpeaking() const
{
	if (!m_pVoice) return false;
	SPVOICESTATUS status;
	m_pVoice->GetStatus(&status, nullptr);
	return (status.dwRunningState == SPRS_IS_SPEAKING);
}


 // ─── Speech Recognition (STT) ─────────────────────

bool SpeechService::StartRecognition(HWND hwndNotify)
{
	// ── Cooldown guard: prevent rapid-fire re-initialization ──
	auto now = std::chrono::steady_clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastStartAttempt).count();
	if (elapsed < 1000)
	{
		OutputDebugStringW((L"[STT] Skipped — cooldown (" + std::to_wstring(elapsed) + L"ms)\n").c_str());
		return m_bRecognizing;  // if already running, report success; else just skip
	}
	m_lastStartAttempt = now;

	StopRecognition();
	if (!m_initialized) { Initialize(); if (!m_initialized) return false; }

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	bool bComInitHere = (hr == S_OK);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) { OutputDebugStringW(L"[STT] CoInit failed\n"); return false; }

	// ── 1. Find recognizer token & detect language ──
	ISpObjectToken* pToken = nullptr;
	bool bIsEnglish = true;
	hr = SpFindBestToken(SPCAT_RECOGNIZERS, L"Language=409", nullptr, &pToken);  // en-US
	if (FAILED(hr))
	{
		hr = SpFindBestToken(SPCAT_RECOGNIZERS, L"Language=809", nullptr, &pToken);  // en-GB
	}
	if (FAILED(hr) || !pToken)
	{
		bIsEnglish = false;
		OutputDebugStringW(L"[STT] Non-English system — forcing Shared path\n");
		hr = SpGetDefaultTokenFromCategoryId(SPCAT_RECOGNIZERS, &pToken);
	}

	WCHAR szLangName[256] = L"default";
	if (SUCCEEDED(hr) && pToken)
	{
		LPWSTR pszId = nullptr;
		if (SUCCEEDED(pToken->GetId(&pszId)) && pszId) { wcscpy_s(szLangName, pszId); CoTaskMemFree(pszId); }
		OutputDebugStringW((std::wstring(L"[STT] Token: ") + szLangName + L"\n").c_str());
	}
	if (FAILED(hr) || !pToken) { OutputDebugStringW(L"[STT] *** No recognizer ***\n"); if (bComInitHere) CoUninitialize(); return false; }

	// ── 2. Choose path ──
	//   English → InProc (private dictation engine)
	//   Non-English → SpSharedRecoContext (taps into system SR session)
	// InProc dictation with non-English engines (e.g. MS-2052-80-DESK) produces zero events.
	m_bSharedRecognizer = !bIsEnglish;
	m_pRecognizer = nullptr;
	m_pRecoContext = nullptr;

	if (!m_bSharedRecognizer)
	{
		// ═══════════ InProc path (English only) ═══════════
		hr = CoCreateInstance(CLSID_SpInprocRecognizer, nullptr, CLSCTX_ALL,
			IID_ISpRecognizer, reinterpret_cast<void**>(&m_pRecognizer));
		if (FAILED(hr) || !m_pRecognizer)
		{
			OutputDebugStringW(L"[STT] InProc creation failed → fallback Shared\n");
			m_bSharedRecognizer = true;
			pToken->Release();
			hr = SpGetDefaultTokenFromCategoryId(SPCAT_RECOGNIZERS, &pToken);
			if (FAILED(hr) || !pToken) { if (bComInitHere) CoUninitialize(); return false; }
			goto shared_path;
		}

		OutputDebugStringW(L"[STT] InProc path\n");
		hr = m_pRecognizer->SetRecognizer(pToken);
		pToken->Release();
		OutputDebugStringW((L"[STT] SetRecognizer → " + std::to_wstring(hr) + L"\n").c_str());
		if (FAILED(hr)) { if (bComInitHere) CoUninitialize(); StopRecognition(); return false; }

		ISpObjectToken* pMicToken = nullptr;
		if (SUCCEEDED(SpGetDefaultTokenFromCategoryId(SPCAT_AUDIOIN, &pMicToken)) && pMicToken)
		{
			hr = m_pRecognizer->SetInput(pMicToken, TRUE);
			pMicToken->Release();
		}
		OutputDebugStringW((L"[STT] SetInput → " + std::to_wstring(hr) + L"\n").c_str());
		if (FAILED(hr)) { if (bComInitHere) CoUninitialize(); StopRecognition(); return false; }

		hr = m_pRecognizer->CreateRecoContext(&m_pRecoContext);
		OutputDebugStringW((L"[STT] CreateRecoContext → " + std::to_wstring(hr) + L"\n").c_str());
		if (FAILED(hr)) { if (bComInitHere) CoUninitialize(); StopRecognition(); return false; }

		ISpRecoGrammar* pGrammar = nullptr;
		if (SUCCEEDED(m_pRecoContext->CreateGrammar(0, &pGrammar)) && pGrammar)
		{
			HRESULT hrDict = pGrammar->LoadDictation(nullptr, SPLO_STATIC);
			if (FAILED(hrDict)) hrDict = pGrammar->LoadDictation(nullptr, SPLO_DYNAMIC);
			if (SUCCEEDED(hrDict)) pGrammar->SetDictationState(SPRS_ACTIVE);
			pGrammar->Release();
		}
	}
	else
	{
shared_path:
		// ═══════════ Shared path (non-English or InProc fallback) ═══════════
		OutputDebugStringW(L"[STT] Shared path (SpSharedRecoContext)\n");

		// SpSharedRecoContext wraps system SR — no separate ISpRecognizer needed
		hr = CoCreateInstance(CLSID_SpSharedRecoContext, nullptr, CLSCTX_ALL,
			IID_ISpRecoContext, reinterpret_cast<void**>(&m_pRecoContext));
		OutputDebugStringW((L"[STT] SpSharedRecoContext → " + std::to_wstring(hr) + L"\n").c_str());
		if (FAILED(hr) || !m_pRecoContext) {
			if (pToken) pToken->Release();
			if (bComInitHere) CoUninitialize();
			return false;
		}

		// Get internal recognizer for state control
		hr = m_pRecoContext->GetRecognizer(&m_pRecognizer);
		OutputDebugStringW((L"[STT] GetRecognizer → " + std::to_wstring(hr) + L"\n").c_str());

		// Set the language on the internal recognizer
		if (m_pRecognizer && pToken)
		{
			hr = m_pRecognizer->SetRecognizer(pToken);
			OutputDebugStringW((L"[STT] Shared SetRecognizer → " + std::to_wstring(hr) + L"\n").c_str());
		}
		if (pToken) pToken->Release();

		// Dictation grammar — this is what connects us to system SR session
		ISpRecoGrammar* pGrammar = nullptr;
		hr = m_pRecoContext->CreateGrammar(0, &pGrammar);
		OutputDebugStringW((L"[STT] CreateGrammar → " + std::to_wstring(hr) + L"\n").c_str());
		if (SUCCEEDED(hr) && pGrammar)
		{
			HRESULT hrDict = pGrammar->LoadDictation(nullptr, SPLO_STATIC);
			if (FAILED(hrDict)) hrDict = pGrammar->LoadDictation(nullptr, SPLO_DYNAMIC);
			OutputDebugStringW((L"[STT] LoadDictation → " + std::to_wstring(hrDict) + L"\n").c_str());
			if (SUCCEEDED(hrDict))
			{
				HRESULT hrState = pGrammar->SetDictationState(SPRS_ACTIVE);
				OutputDebugStringW((L"[STT] SetDictationState → " + std::to_wstring(hrState) + L"\n").c_str());
			}
			pGrammar->Release();
		}

		// Hide SR floating bar
		Sleep(300);
		HWND hSrBar = FindWindowW(L"Speech Recognition", nullptr);
		if (!hSrBar) hSrBar = FindWindowW(L"语音识别", nullptr);
		if (hSrBar) { ShowWindow(hSrBar, SW_HIDE); OutputDebugStringW(L"[STT] SR bar hidden\n"); }
	}

	// ── 3. Common setup ──
	m_hwndRecoNotify = hwndNotify;
	hr = m_pRecoContext->SetNotifyWindowMessage(hwndNotify, WM_USER_STT_EVENT, 0, 0);
	OutputDebugStringW((L"[STT] SetNotifyWindow → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr)) { if (bComInitHere) CoUninitialize(); StopRecognition(); return false; }

	const ULONGLONG interest =
		SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION) |
		SPFEI(SPEI_SOUND_START) | SPFEI(SPEI_SOUND_END) | SPFEI(SPEI_INTERFERENCE);
	m_pRecoContext->SetInterest(interest, interest);
	m_pRecoContext->SetContextState(SPCS_ENABLED);

	if (m_pRecognizer)
	{
		hr = m_pRecognizer->SetRecoState(SPRST_ACTIVE);
		OutputDebugStringW((L"[STT] SetRecoState(ACTIVE) → " + std::to_wstring(hr) + L"\n").c_str());
	}

	m_recognizedText.clear();
	m_bRecognizing = true;
	m_bComInitialized = bComInitHere;

	OutputDebugStringW((std::wstring(L"[STT] >>> READY (") +
		(m_bSharedRecognizer ? L"Shared" : L"InProc") +
		L") — speak now <<<\n").c_str());
	return true;
}

void SpeechService::StopRecognition()
{
	if (!m_bRecognizing) return;
	m_bRecognizing = false;

	if (m_pRecoContext)
	{
		m_pRecoContext->SetContextState(SPCS_DISABLED);
		m_pRecoContext->SetNotifyWindowMessage(nullptr, 0, 0, 0);
		m_pRecoContext->Release();
		m_pRecoContext = nullptr;
	}
	if (m_pRecognizer)
	{
		m_pRecognizer->SetRecoState(SPRST_INACTIVE);
		m_pRecognizer->Release();
		m_pRecognizer = nullptr;
	}

	// If we called CoInitializeEx in StartRecognition, balance it here
	if (m_bComInitialized)
	{
		CoUninitialize();
		m_bComInitialized = false;
	}
}

std::wstring SpeechService::PopRecognizedText()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	std::wstring text = m_recognizedText;
	m_recognizedText.clear();
	return text;
}

/// Handle SAPI recognition event — called from MainWindow's WndProc
/// Returns true if event was consumed
bool SpeechService::HandleSttEvent()
{
	if (!m_bRecognizing || !m_pRecoContext) return false;

	CSpEvent spEvent;
	while (spEvent.GetFrom(m_pRecoContext) == S_OK)
	{
		switch (spEvent.eEventId)
		{
		case SPEI_SOUND_START:
			OutputDebugStringW(L"[STT] 🎤 Sound detected\n");
			break;
		case SPEI_SOUND_END:
			OutputDebugStringW(L"[STT] 🔇 Sound ended\n");
			break;
		case SPEI_RECOGNITION:
		{
			ISpRecoResult* pResult = spEvent.RecoResult();
			if (pResult)
			{
				LPWSTR pszText = nullptr;
				if (SUCCEEDED(pResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &pszText, nullptr)) && pszText)
				{
					std::wstring phrase(pszText);
					::CoTaskMemFree(pszText);

					OutputDebugStringW((L"[STT] ✅ Recognized: \"" + phrase + L"\"\n").c_str());

					if (!phrase.empty() && !m_recognizedText.empty())
						m_recognizedText += L" ";
					m_recognizedText += phrase;

					if (m_hwndRecoNotify)
					{
						std::wstring* pCopy = new std::wstring(phrase);
						PostMessage(m_hwndRecoNotify, WM_USER_STT_RESULT,
							reinterpret_cast<WPARAM>(pCopy), 0);
					}
				}
			}
			break;
		}
		case SPEI_FALSE_RECOGNITION:
			OutputDebugStringW(L"[STT] ❌ False recognition\n");
			break;
		case SPEI_INTERFERENCE:
			OutputDebugStringW((L"[STT] ⚠ Interference: " +
				std::to_wstring(spEvent.Interference()) + L"\n").c_str());
			break;
		}
		spEvent.Clear();
	}
	return true;
}
std::vector<std::wstring> SpeechService::GetAvailableVoices()
{
	std::vector<std::wstring> voices;
	if (!m_initialized || !m_pVoice) return voices;

	ISpObjectTokenCategory* pCategory = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
		IID_ISpObjectTokenCategory,
		reinterpret_cast<void**>(&pCategory));
	if (FAILED(hr)) return voices;

	hr = pCategory->SetId(SPCAT_VOICES, FALSE);
	if (FAILED(hr))
	{
		pCategory->Release();
		return voices;
	}

	IEnumSpObjectTokens* pEnum = nullptr;
	hr = pCategory->EnumTokens(nullptr, nullptr, &pEnum);
	if (FAILED(hr))
	{
		pCategory->Release();
		return voices;
	}

	ISpObjectToken* pToken = nullptr;
	while (pEnum->Next(1, &pToken, nullptr) == S_OK)
	{
		LPWSTR desc = nullptr;
		if (SUCCEEDED(SpGetDescription(pToken, &desc)) && desc)
		{
			voices.push_back(desc);
			::CoTaskMemFree(desc);
		}
		pToken->Release();
	}

	pEnum->Release();
	pCategory->Release();
	return voices;
}

bool SpeechService::SetVoice(const std::wstring& token)
{
	if (!m_pVoice) return false;

	// Find voice by description
	ISpObjectTokenCategory* pCategory = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL,
		IID_ISpObjectTokenCategory,
		reinterpret_cast<void**>(&pCategory));
	if (FAILED(hr)) return false;

	hr = pCategory->SetId(SPCAT_VOICES, FALSE);
	if (FAILED(hr))
	{
		pCategory->Release(); return false;
	}

	IEnumSpObjectTokens* pEnum = nullptr;
	hr = pCategory->EnumTokens(nullptr, nullptr, &pEnum);
	if (FAILED(hr))
	{
		pCategory->Release(); return false;
	}

	ISpObjectToken* pToken = nullptr;
	bool found = false;
	while (pEnum->Next(1, &pToken, nullptr) == S_OK)
	{
		LPWSTR desc = nullptr;
		if (SUCCEEDED(SpGetDescription(pToken, &desc)) && desc)
		{
			if (token == desc)
			{
				found = true;
				if (m_pCurrentToken) m_pCurrentToken->Release();
				m_pCurrentToken = pToken;
				m_pCurrentToken->AddRef();
				m_pVoice->SetVoice(pToken);
			}
			::CoTaskMemFree(desc);
		}
		pToken->Release();
		if (found) break;
	}

	pEnum->Release();
	pCategory->Release();
	return found;
}

void SpeechService::SetRate(int rate)
{
	if (m_pVoice)
	{
		// SAPI rate: -10 to 10
		int clamped = std::clamp(rate, -10, 10);
		m_pVoice->SetRate(clamped);
	}
}

void SpeechService::SetVolume(int volume)
{
	if (m_pVoice)
	{
		int clamped = std::clamp(volume, 0, 100);
		m_pVoice->SetVolume(static_cast<USHORT>(clamped));
	}
}

std::wstring SpeechService::GetCurrentVoiceToken() const
{
	if (!m_pCurrentToken) return L"";
	LPWSTR desc = nullptr;
	if (SUCCEEDED(SpGetDescription(m_pCurrentToken, &desc)) && desc)
	{
		std::wstring result(desc);
		::CoTaskMemFree(desc);
		return result;
	}
	return L"";
}
