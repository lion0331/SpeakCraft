#pragma once
#include "framework.h"

/// Windows SAPI-based speech service for TTS (text-to-speech) and STT (speech-to-text)
class SpeechService
{
public:
	SpeechService();
	~SpeechService();

	/// Initialize COM and SAPI
	bool Initialize();

	/// Speak text asynchronously. Posts WM_SPEECH_COMPLETE when done.
	bool SpeakAsync(const std::wstring& text, HWND hwndNotify);

	/// Speak text synchronously
	bool Speak(const std::wstring& text);

	/// Stop speaking
	void StopSpeaking();

	/// Check if currently speaking
	bool IsSpeaking() const;

	/// Start speech recognition (STT). 
	/// Results arrive as WM_USER_STT_RESULT with wParam = heap-allocated wstring*
	bool StartRecognition(HWND hwndNotify);

	/// Stop speech recognition 
	void StopRecognition();

	/// Whether recognition is currently active
	bool IsRecognizing() const
	{
		return m_bRecognizing;
	}

	/// Handle SAPI STT event (called from WndProc on WM_USER_STT_EVENT)
	/// Returns true if event was consumed
	bool HandleSttEvent();

	/// Get accumulated recognized text and clear buffer (does NOT stop recognition)
	std::wstring PopRecognizedText();

	/// Check if recognition is active and has pending recognized text
	bool HasRecognizedText() const
	{
		return !m_recognizedText.empty();
	}

	/// Whether SAPI has reported microphone sound during the current recognition session
	bool HasDetectedSound() const
	{
		return m_bSoundDetected;
	}

	/// Get the current recognizer language description (e.g. "English (United States)")
	std::wstring GetRecognizerLanguage() const
	{
		return m_recognizerLanguage;
	}

	/// Get available voices
	std::vector<std::wstring> GetAvailableVoices();

	/// Set voice by token
	bool SetVoice(const std::wstring& token);

	/// Get available speech recognizers (human-readable descriptions)
	std::vector<std::wstring> GetAvailableRecognizers();

	/// Explicitly choose a recognizer by description. Call before StartRecognition.
	/// If empty, the default English-preference logic is used.
	void SetPreferredRecognizer(const std::wstring& description);

	/// Get the preferred recognizer description (empty = auto)
	std::wstring GetPreferredRecognizer() const { return m_preferredRecognizer; }

	/// Set speech rate (-10 to 10)
	void SetRate(int rate);

	/// Set volume (0 to 100)
	void SetVolume(int volume);

	/// Get current voice token
	std::wstring GetCurrentVoiceToken() const;

private:
	ISpVoice* m_pVoice = nullptr;
	ISpObjectToken* m_pCurrentToken = nullptr;
	bool m_initialized = false;
	mutable std::recursive_mutex m_mutex;

	// Speech recognition
	ISpRecognizer* m_pRecognizer = nullptr;
	ISpRecoContext* m_pRecoContext = nullptr;
	ISpRecoGrammar* m_pRecoGrammar = nullptr;
	HWND m_hwndRecoNotify = nullptr;
	bool m_bRecognizing = false;
	bool m_bSoundDetected = false;
	bool m_bComInitialized = false;   // true if StartRecognition called CoInitializeEx
	std::wstring m_recognizedText;
	std::wstring m_hypothesisText;    // interim (SPEI_HYPOTHESIS) fallback text
	std::wstring m_falseRecoText;     // low-confidence (SPEI_FALSE_RECOGNITION) fallback text
	std::wstring m_recognizerLanguage; // human-readable language description of the current recognizer
	std::wstring m_preferredRecognizer; // user-chosen recognizer (empty = auto-detect English)
};
