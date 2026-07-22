#pragma once
#include "framework.h"

/// Windows SAPI-based speech service for TTS (text-to-speech) and STT (speech-to-text)
class SpeechService {
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
};
