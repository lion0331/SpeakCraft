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
    /// lParam: (int) 1 = success, 0 = error. 
    /// modeTag: added to wParam via high bits or use WM_AI_RESPONSE + mode offset
    bool SendMessage(const std::wstring& userMessage,
                     const std::wstring& systemContext = L"");

    // ─── Mode-Specific Methods ──────────────────

    /// 1. Text Shadowing: evaluate pronunciation of a single sentence
    /// Expects AI to return JSON with word-level scores
    bool EvaluatePronunciation(const std::wstring& userSpeech,
                               const std::wstring& referenceText);

    /// 2. Role Play: start a role-play scenario or continue the dialogue
    bool StartRolePlay(const std::wstring& scenario,
                       const std::wstring& userCharacter,
                       const std::wstring& aiCharacter,
                       const std::wstring& lessonContext);

    /// Continue role-play dialogue
    bool ContinueRolePlay(const std::wstring& userLine);

    /// 3. Sentence Pattern: generate a substitution exercise
    bool GeneratePatternExercise(const std::wstring& corePattern,
                                  const std::wstring& lessonContext);

    /// Check user's pattern substitution answer
    bool CheckPatternAnswer(const std::wstring& userAnswer,
                            const std::wstring& pattern,
                            const std::wstring& keyword,
                            const std::wstring& expectedAnswer);

    /// 4. Free Conversation: start topic-based conversation
    bool StartFreeConversation(const std::wstring& topic,
                               const std::vector<std::wstring>& targetVocab,
                               const std::wstring& lessonContext);

    /// Continue free conversation
    bool ContinueFreeConversation(const std::wstring& userMessage);

    /// End free conversation and get vocabulary usage summary
    bool EndFreeConversation();

    /// 5. Grammar Correction: submit speech for sentence-by-sentence correction
    bool CorrectGrammar(const std::wstring& userSpeech,
                        const std::wstring& topicContext);

    /// 6. Learning Report: generate learning report summary
    bool GenerateLearningReport(const std::wstring& statsJson);

    /// Check if currently processing
    bool IsProcessing() const { return m_processing.load(); }

    /// Cancel current request
    void Cancel();

    /// Get the conversation history
    const std::vector<ChatMessage>& GetHistory() const { return m_history; }

    /// Clear conversation history
    void ClearHistory();

    /// Get the current mode context (for UI to know what's happening)
    std::wstring GetCurrentMode() const { return m_currentMode; }

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
    std::wstring BuildRolePlayRequestBody(const std::wstring& content,
                                           const std::wstring& systemPrompt) const;
    std::wstring BuildPatternCheckBody(const std::wstring& content,
                                        const std::wstring& systemPrompt) const;
    std::wstring BuildGrammarCorrectionBody(const std::wstring& content,
                                             const std::wstring& systemPrompt) const;
    std::wstring BuildReportBody(const std::wstring& statsJson) const;

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
    std::wstring m_currentMode;  // tracks current practice mode for context
};
