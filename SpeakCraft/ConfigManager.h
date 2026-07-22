#pragma once
#include "framework.h"

/// Manages application configuration (API keys, endpoints, voice settings)
/// Settings are stored in %APPDATA%\SpeakCraft\config.json
class ConfigManager {
public:
    static ConfigManager& Instance();

    // Load / Save
    bool Load();
    bool Save();

    // AI API settings
    std::wstring GetApiEndpoint() const;
    void SetApiEndpoint(const std::wstring& endpoint);
    std::wstring GetApiKey() const;
    void SetApiKey(const std::wstring& key);
    std::wstring GetModelName() const;
    void SetModelName(const std::wstring& model);

    // Voice settings
    std::wstring GetVoiceToken() const;
    void SetVoiceToken(const std::wstring& token);
    int GetSpeechRate() const;
    void SetSpeechRate(int rate);
    int GetSpeechVolume() const;
    void SetSpeechVolume(int volume);

    // System prompt template
    std::wstring GetSystemPrompt() const;
    void SetSystemPrompt(const std::wstring& prompt);

    // Config path
    std::filesystem::path GetConfigPath() const;

private:
    ConfigManager() = default;
    ~ConfigManager() = default;
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    std::wstring m_apiEndpoint = L"https://api.openai.com/v1/chat/completions";
    std::wstring m_apiKey;
    std::wstring m_modelName = L"gpt-4o-mini";
    std::wstring m_voiceToken;          // SAPI voice token
    int m_speechRate = 0;               // -10 to 10
    int m_speechVolume = 100;           // 0 to 100
    std::wstring m_systemPrompt = L"You are an English speaking practice tutor. "
        L"Your role is to help the user practice spoken English based on New Concept English (新概念英语) curriculum. "
        L"Engage in natural conversation, correct pronunciation and grammar errors gently, "
        L"and provide encouraging feedback. Keep responses concise and conversational. "
        L"Always respond in English unless the user explicitly asks for Chinese explanation. "
        L"Focus on the current lesson's vocabulary and grammar patterns.";
};
