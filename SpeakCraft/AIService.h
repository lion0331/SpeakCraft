#pragma once
#include "framework.h"
#include "LessonData.h"

/// Asynchronous AI service for interacting with OpenAI-compatible APIs
/// Runs HTTP requests on background threads and posts results via window messages
class AIService {
public:
    AIService();
    ~AIService();

    /// Initialize with target window for async notifications
    void Initialize(HWND hwndNotify);

    /// Send a chat message asynchronously. Response arrives via WM_AI_RESPONSE.
    /// wParam: pointer to heap-allocated std::wstring (caller must delete)
    /// lParam: (int) 1 = stream chunk, 0 = complete
    bool SendMessage(const std::wstring& userMessage,
                     const std::wstring& systemContext = L"");

    /// Send pronunciation evaluation request
    bool EvaluatePronunciation(const std::wstring& userSpeech,
                               const std::wstring& referenceText);

    /// Check if currently processing
    bool IsProcessing() const { return m_processing.load(); }

    /// Cancel current request
    void Cancel();

    /// Get the conversation history
    const std::vector<ChatMessage>& GetHistory() const { return m_history; }

    /// Clear conversation history
    void ClearHistory();

    /// Test API connection (synchronous)
    bool TestConnection(std::wstring& outError);

private:
    /// Background thread function for HTTP requests
    void ProcessRequest(std::wstring requestBody);

    /// Build OpenAI-compatible JSON request body
    std::wstring BuildChatRequestBody(const std::wstring& userMessage,
                                       const std::wstring& systemContext) const;
    std::wstring BuildEvalRequestBody(const std::wstring& userSpeech,
                                       const std::wstring& referenceText) const;

    /// WinHTTP helper
    std::wstring SendHttpRequest(const std::wstring& body, bool& success);

    /// Parse AI response
    std::wstring ParseResponseContent(const std::wstring& jsonResponse) const;

    /// Escape JSON string
    static std::wstring JsonEscape(const std::wstring& s);

    HWND m_hwndNotify = nullptr;
    HINTERNET m_hSession = nullptr;
    std::atomic<bool> m_processing{false};
    std::atomic<bool> m_cancelled{false};
    std::vector<ChatMessage> m_history;
    mutable std::mutex m_historyMutex;
    std::unique_ptr<std::thread> m_workerThread;
};
