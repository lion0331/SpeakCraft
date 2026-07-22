#include "SpeechService.h"
#include "ConfigManager.h"
#include "Resource.h"
#include <sphelper.h>

SpeechService::SpeechService()
{}

SpeechService::~SpeechService()
{
	StopSpeaking();
	if (m_pVoice)
	{
		m_pVoice->Release();
		m_pVoice = nullptr;
	}
	if (m_pCurrentToken)
	{
		m_pCurrentToken->Release();
		m_pCurrentToken = nullptr;
	}
	// Do NOT CoUninitialize — COM lifetime is managed by main.cpp
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
