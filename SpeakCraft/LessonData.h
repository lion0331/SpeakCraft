#pragma once
#include "framework.h"

// ─── Lesson Data Structures ──────────────────────

/// Represents vocabulary item with pronunciation and example
struct VocabularyItem {
    std::wstring word;
    std::wstring phonetic;      // IPA phonetic notation
    std::wstring translation;   // Chinese translation
    std::wstring exampleSentence;
};

/// Represents a single lesson
struct Lesson {
    int id;
    std::wstring title;
    std::wstring bookName;          // e.g., "新概念英语第一册"
    std::wstring dialogueText;      // Main dialogue / text of the lesson
    std::wstring keyGrammar;        // Key grammar points (Chinese)
    std::wstring practicePrompt;    // AI practice prompt for this lesson
    std::vector<VocabularyItem> vocabulary;
    int difficulty;                 // 1-10
    int lessonNumber;
};

/// Represents a book (collection of lessons)
struct Book {
    std::wstring id;                // Unique book identifier
    std::wstring name;              // Display name
    std::wstring description;
    std::wstring sourceFile;        // File path for external JSON books
    std::vector<Lesson> lessons;
    bool isBuiltIn;                 // Built-in vs user-loaded
};

/// AI conversation message
struct ChatMessage {
    std::wstring role;       // "system", "user", "assistant"
    std::wstring content;
    std::chrono::system_clock::time_point timestamp;
};

/// Practice session state
enum class PracticeState {
    Idle,
    Listening,      // Waiting for user speech
    Processing,     // Sending to AI
    Responding,     // AI speaking response
    Evaluating      // AI giving feedback
};

/// Pronunciation evaluation result (from AI)
struct PronunciationResult {
    double overallScore;        // 0-100
    double fluencyScore;
    double accuracyScore;
    std::wstring feedback;      // AI feedback text
    std::vector<std::wstring> problemWords;
    std::wstring correctedText;
};
