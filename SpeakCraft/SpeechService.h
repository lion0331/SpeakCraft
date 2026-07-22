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

    /// Get available voices
    std::vector<std::wstring> GetAvailableVoices();

    /// Set voice by token
    bool SetVoice(const std::wstring& token);

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
};
