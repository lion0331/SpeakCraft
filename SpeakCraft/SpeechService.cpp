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
	if (m_pVoice)
	{
		m_pVoice->Release(); m_pVoice = nullptr;
	}
	if (m_pCurrentToken)
	{
		m_pCurrentToken->Release(); m_pCurrentToken = nullptr;
	}
	if (m_pRecoContext)
	{
		m_pRecoContext->Release(); m_pRecoContext = nullptr;
	}
	if (m_pRecognizer)
	{
		m_pRecognizer->Release(); m_pRecognizer = nullptr;
	}
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
		ULONGLONG interest = SPFEI(SPEI_END_INPUT_STREAM);
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
	std::lock_guard<std::recursive_mutex> lock(m_mutex);

	if (m_bRecognizing && m_pRecoContext)
	{
		m_hwndRecoNotify = hwndNotify;
		return true;
	}

	StopRecognition();
	if (!m_initialized)
	{
		Initialize(); if (!m_initialized) return false;
	}

	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	m_bComInitialized = (hr == S_OK || hr == S_FALSE);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		OutputDebugStringW(L"[STT] CoInit failed\n"); return false;
	}

	auto fail = [&]() {
		StopRecognition();
		return false;
		};

	// ── Find a recognizer ──────────────────────────────────
	// 1) User-specified recognizer  2) English best-match  3) Enumerate English
	ISpObjectToken* pToken = nullptr;

	// Helper: check whether a recognizer token is English (langid & 0xFF == 0x09)
	auto isEnglishToken = [](ISpObjectToken* tok) -> bool {
		LPWSTR pszAttrib = nullptr;
		if (SUCCEEDED(tok->GetStringValue(L"Language", &pszAttrib)) && pszAttrib)
		{
			long langId = wcstol(pszAttrib, nullptr, 16);
			::CoTaskMemFree(pszAttrib);
			return ((langId & 0xFF) == 0x09);   // primary lang 0x09 = English
		}
		return false;
	};

	// If the user explicitly chose a recognizer, use it directly
	if (!m_preferredRecognizer.empty())
	{
		IEnumSpObjectTokens* pEnum = nullptr;
		if (SUCCEEDED(SpEnumTokens(SPCAT_RECOGNIZERS, nullptr, nullptr, &pEnum)) && pEnum)
		{
			ISpObjectToken* pCandidate = nullptr;
			while (pEnum->Next(1, &pCandidate, nullptr) == S_OK)
			{
				LPWSTR desc = nullptr;
				if (SUCCEEDED(SpGetDescription(pCandidate, &desc)) && desc)
				{
					if (m_preferredRecognizer == desc)
					{
						pToken = pCandidate;
						::CoTaskMemFree(desc);
						OutputDebugStringW((L"[STT] Using user-preferred recognizer: " + m_preferredRecognizer + L"\n").c_str());
						break;
					}
					::CoTaskMemFree(desc);
				}
				pCandidate->Release();
			}
			pEnum->Release();
		}
		if (!pToken)
		{
			OutputDebugStringW(L"[STT] Preferred recognizer not found; falling back to auto-detect\n");
		}
	}

	if (!pToken)
	{
		hr = SpFindBestToken(SPCAT_RECOGNIZERS, L"Language=409", nullptr, &pToken);  // en-US
		if (FAILED(hr))
		{
			hr = SpFindBestToken(SPCAT_RECOGNIZERS, L"Language=809", nullptr, &pToken);  // en-GB
		}
	}
	if (!pToken)
	{
		// Enumerate all recognizers and pick the first English one
		OutputDebugStringW(L"[STT] SpFindBestToken failed; enumerating all recognizers...\n");
		IEnumSpObjectTokens* pEnum = nullptr;
		if (SUCCEEDED(SpEnumTokens(SPCAT_RECOGNIZERS, nullptr, nullptr, &pEnum)) && pEnum)
		{
			ISpObjectToken* pCandidate = nullptr;
			while (pEnum->Next(1, &pCandidate, nullptr) == S_OK)
			{
				if (isEnglishToken(pCandidate))
				{
					pToken = pCandidate;          // found!  (already AddRef'd by Next)
					OutputDebugStringW(L"[STT] Found English recognizer via enumeration\n");
					break;
				}
				pCandidate->Release();
			}
			pEnum->Release();
		}
	}
	if (!pToken)
	{
		OutputDebugStringW(L"[STT] No English recognizer; using system default\n");
		SpGetDefaultTokenFromCategoryId(SPCAT_RECOGNIZERS, &pToken);
	}

	WCHAR szLangName[256] = L"default";
	if (pToken)
	{
		LPWSTR pszId = nullptr;
		if (SUCCEEDED(pToken->GetId(&pszId)) && pszId)
		{
			wcscpy_s(szLangName, pszId); CoTaskMemFree(pszId);
		}
		// Get human-readable description of the recognizer token
		LPWSTR pszDesc = nullptr;
		if (SUCCEEDED(SpGetDescription(pToken, &pszDesc)) && pszDesc)
		{
			m_recognizerLanguage = pszDesc;
			::CoTaskMemFree(pszDesc);
		}
		else
		{
			m_recognizerLanguage = szLangName;
		}
		OutputDebugStringW((std::wstring(L"[STT] Token: ") + szLangName + L"\n").c_str());
	}
	else
	{
		m_recognizerLanguage = L"Unknown (no recognizer found)";
	}
	if (!pToken)
	{
		OutputDebugStringW(L"[STT] *** No recognizer ***\n");
		return fail();
	}

	hr = CoCreateInstance(CLSID_SpInprocRecognizer, nullptr, CLSCTX_ALL,
		IID_ISpRecognizer, reinterpret_cast<void**>(&m_pRecognizer));
	OutputDebugStringW((L"[STT] SpInprocRecognizer → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr) || !m_pRecognizer)
	{
		pToken->Release(); return fail();
	}

	hr = m_pRecognizer->SetRecognizer(pToken);
	pToken->Release();
	OutputDebugStringW((L"[STT] SetRecognizer → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr)) return fail();

	ISpAudio* pAudio = nullptr;
	hr = SpCreateDefaultObjectFromCategoryId(SPCAT_AUDIOIN, &pAudio);
	OutputDebugStringW((L"[STT] Default audio input → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr) || !pAudio) return fail();

	hr = m_pRecognizer->SetInput(pAudio, TRUE);
	pAudio->Release();
	OutputDebugStringW((L"[STT] SetInput → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr)) return fail();

	hr = m_pRecognizer->CreateRecoContext(&m_pRecoContext);
	OutputDebugStringW((L"[STT] CreateRecoContext → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr) || !m_pRecoContext) return fail();

	m_hwndRecoNotify = hwndNotify;
	hr = m_pRecoContext->SetNotifyWindowMessage(hwndNotify, WM_USER_STT_EVENT, 0, 0);
	OutputDebugStringW((L"[STT] SetNotifyWindow → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr)) return fail();

	const ULONGLONG interest =
		SPFEI(SPEI_RECOGNITION) | SPFEI(SPEI_FALSE_RECOGNITION) |
		SPFEI(SPEI_SOUND_START) | SPFEI(SPEI_SOUND_END) |
		SPFEI(SPEI_PHRASE_START) | SPFEI(SPEI_HYPOTHESIS) |
		SPFEI(SPEI_INTERFERENCE);
	hr = m_pRecoContext->SetInterest(interest, interest);
	OutputDebugStringW((L"[STT] SetInterest → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr)) return fail();

	hr = m_pRecoContext->CreateGrammar(1, &m_pRecoGrammar);
	OutputDebugStringW((L"[STT] CreateGrammar → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr) || !m_pRecoGrammar) return fail();

	hr = m_pRecoGrammar->LoadDictation(nullptr, SPLO_STATIC);
	if (FAILED(hr)) hr = m_pRecoGrammar->LoadDictation(nullptr, SPLO_DYNAMIC);
	OutputDebugStringW((L"[STT] LoadDictation → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr)) return fail();

	hr = m_pRecoGrammar->SetDictationState(SPRS_ACTIVE);
	OutputDebugStringW((L"[STT] SetDictationState → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr)) return fail();

	hr = m_pRecoContext->SetContextState(SPCS_ENABLED);
	OutputDebugStringW((L"[STT] SetContextState(ENABLED) → " + std::to_wstring(hr) + L"\n").c_str());
	if (FAILED(hr)) return fail();

	if (m_pRecognizer)
	{
		hr = m_pRecognizer->SetRecoState(SPRST_ACTIVE_ALWAYS);
		OutputDebugStringW((L"[STT] SetRecoState(ACTIVE_ALWAYS) → " + std::to_wstring(hr) + L"\n").c_str());
		if (FAILED(hr)) return fail();
	}

	m_recognizedText.clear();
	m_hypothesisText.clear();
	m_falseRecoText.clear();
	m_bSoundDetected = false;
	m_bRecognizing = true;

	OutputDebugStringW(L"[STT] >>> READY (InProc dictation) — speak now <<<\n");
	return true;
}

void SpeechService::StopRecognition()
{
	std::lock_guard<std::recursive_mutex> lock(m_mutex);
	m_bRecognizing = false;
	m_bSoundDetected = false;

	if (m_pRecoGrammar)
	{
		m_pRecoGrammar->SetDictationState(SPRS_INACTIVE);
		m_pRecoGrammar->Release();
		m_pRecoGrammar = nullptr;
	}

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
	// Prefer final recognition results
	std::wstring text = m_recognizedText;
	m_recognizedText.clear();

	// Fallback 1: use last hypothesis (interim result) if no final result
	if (text.empty() && !m_hypothesisText.empty())
	{
		text = m_hypothesisText;
		m_hypothesisText.clear();
		OutputDebugStringW((L"[STT] ⚠ Using hypothesis fallback: \"" + text + L"\"\n").c_str());
	}

	// Fallback 2: use last false-recognition text (low confidence) if nothing else
	if (text.empty() && !m_falseRecoText.empty())
	{
		text = m_falseRecoText;
		m_falseRecoText.clear();
		OutputDebugStringW((L"[STT] ⚠ Using false-recognition fallback: \"" + text + L"\"\n").c_str());
	}

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
			m_bSoundDetected = true;
			OutputDebugStringW(L"[STT] 🎤 Sound detected\n");
			break;
		case SPEI_PHRASE_START:
			m_bSoundDetected = true;
			OutputDebugStringW(L"[STT] Phrase started\n");
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
		{
			// Collect low-confidence text as fallback — some recognizers only produce false recognitions
			ISpRecoResult* pFalseResult = spEvent.RecoResult();
			if (pFalseResult)
			{
				LPWSTR pszFalseText = nullptr;
				if (SUCCEEDED(pFalseResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &pszFalseText, nullptr)) && pszFalseText)
				{
					m_falseRecoText = pszFalseText;
					::CoTaskMemFree(pszFalseText);
					OutputDebugStringW((L"[STT] ❌ False recognition: \"" + m_falseRecoText + L"\" (low confidence, saved as fallback)\n").c_str());
				}
				else
				{
					OutputDebugStringW(L"[STT] ❌ False recognition (no text)\n");
				}
			}
			else
			{
				OutputDebugStringW(L"[STT] ❌ False recognition (no result object)\n");
			}
			break;
		}
		case SPEI_HYPOTHESIS:
		{
			// Collect interim hypothesis as fallback
			ISpRecoResult* pHypResult = spEvent.RecoResult();
			if (pHypResult)
			{
				LPWSTR pszHypText = nullptr;
				if (SUCCEEDED(pHypResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, TRUE, &pszHypText, nullptr)) && pszHypText)
				{
					m_hypothesisText = pszHypText;
					::CoTaskMemFree(pszHypText);
					OutputDebugStringW((L"[STT] 💭 Hypothesis: \"" + m_hypothesisText + L"\"\n").c_str());
				}
			}
			break;
		}
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

// ─── Recognizer selection ──────────────────────────

std::vector<std::wstring> SpeechService::GetAvailableRecognizers()
{
	std::vector<std::wstring> recognizers;

	IEnumSpObjectTokens* pEnum = nullptr;
	HRESULT hr = SpEnumTokens(SPCAT_RECOGNIZERS, nullptr, nullptr, &pEnum);
	if (FAILED(hr) || !pEnum) return recognizers;

	ISpObjectToken* pToken = nullptr;
	while (pEnum->Next(1, &pToken, nullptr) == S_OK)
	{
		LPWSTR desc = nullptr;
		if (SUCCEEDED(SpGetDescription(pToken, &desc)) && desc)
		{
			recognizers.push_back(desc);
			::CoTaskMemFree(desc);
		}
		pToken->Release();
	}
	pEnum->Release();
	return recognizers;
}

void SpeechService::SetPreferredRecognizer(const std::wstring& description)
{
	m_preferredRecognizer = description;
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
