#pragma once
#include "framework.h"
#include <vector>
#include <chrono>

// ─── Practice Mode Types ─────────────────────────

/// Six distinct practice modes
enum class PracticeModeType {
    TextShadowing = 0,      // 课文跟读 — sentence-by-sentence shadowing
    RolePlay = 1,           // 角色扮演 — scripted dialogue role-play
    SentencePattern = 2,    // 句型替换 — key pattern + keyword substitution
    FreeConversation = 3,   // 自由对话 — topic-based free chat
    PronunciationCorrection = 4,  // 发音纠错 — speak freely, AI evaluates pronunciation
    LearningReport = 5      // 学习追踪 — view progress & reports
};

/// Pronunciation word-level score
enum class WordScore {
    Accurate = 0,       // 🟢 accurate
    Fair = 1,           // 🟡 needs minor improvement
    NeedsWork = 2       // 🔴 needs significant improvement
};

/// A single word's pronunciation result
struct WordResult {
    std::wstring word;
    WordScore score;
    std::wstring phoneticExpected;      // expected IPA
    std::wstring suggestion;            // tip for improvement
};

/// Pronunciation evaluation result for a sentence
struct PronunciationEval {
    std::wstring referenceText;         // original text
    std::wstring userText;              // what user actually said (ASR result)
    double overallScore;                // 0-100
    std::vector<WordResult> wordResults;
    std::wstring generalFeedback;
};

/// A single grammar correction item
struct GrammarCorrection {
    std::wstring originalText;          // what user said
    std::wstring correctedText;         // corrected version
    std::wstring explanation;           // grammar point explanation
    std::wstring errorType;             // e.g., "tense", "article", "preposition"
};

/// Grammar correction session result
struct GrammarCorrectionResult {
    std::wstring topicDescription;      // what user talked about
    std::vector<GrammarCorrection> corrections;
    double overallGrammarScore;         // 0-100
    std::wstring summary;               // overall assessment
    std::vector<std::wstring> strengths;
    std::vector<std::wstring> weakAreas;
};

/// Sentence pattern exercise item
struct PatternExercise {
    std::wstring pattern;               // e.g., "There is a ___ on the ___."
    std::wstring keyword;               // substitution keyword, e.g., "book / table"
    std::wstring expectedAnswer;        // model answer
    std::wstring hint;                  // hint if user gets it wrong
};

/// Sentence pattern session
struct PatternSession {
    std::wstring corePattern;           // the core sentence pattern
    std::wstring explanation;           // grammar explanation of the pattern
    std::vector<PatternExercise> exercises;
    int currentExerciseIndex = 0;
    int correctCount = 0;
    int totalAttempts = 0;
};

/// Role-play script segment
struct RolePlayLine {
    std::wstring character;             // "AI" or "User"
    std::wstring text;                  // dialogue line
    bool isUserTurn;                    // true if user should speak
    std::wstring hint;                  // hint for user's line
};

/// Role play session data
struct RolePlaySession {
    std::wstring scenario;              // scenario description
    std::wstring aiCharacter;           // AI character name
    std::wstring userCharacter;         // user character name
    std::vector<RolePlayLine> script;
    int currentLineIndex = 0;
    std::vector<std::wstring> corrections;  // real-time corrections
};

/// Free conversation session
struct FreeConversationSession {
    std::wstring topic;
    std::vector<std::wstring> targetVocab;       // vocabulary to use
    std::vector<std::wstring> vocabUsed;         // vocab user actually used
    std::vector<std::wstring> vocabMissed;       // vocab user didn't use
    double vocabUsageRate;                       // 0-100
    std::wstring summary;
};

// ─── Learning Tracking Structures ────────────────

/// Per-skill score (0-100)
struct SkillScores {
    double grammar = 0.0;
    double vocabulary = 0.0;
    double pronunciation = 0.0;
    double fluency = 0.0;
};

/// A single practice session record
struct SessionRecord {
    std::chrono::system_clock::time_point timestamp;
    PracticeModeType mode;
    std::wstring bookId;
    int lessonNumber = 0;
    std::wstring lessonTitle;
    SkillScores scores;
    std::vector<std::wstring> errorWords;        // words pronounced incorrectly
    std::vector<std::wstring> errorGrammar;      // grammar error patterns
    std::vector<std::wstring> vocabUsed;
    std::wstring aiFeedback;
    int durationSeconds = 0;
};

/// Error pattern with frequency
struct ErrorPattern {
    std::wstring pattern;           // e.g., "third person -s", "th-sound"
    std::wstring category;          // "pronunciation", "grammar", "vocabulary"
    int frequency = 0;
    std::chrono::system_clock::time_point lastOccurrence;
};

/// Milestone record
struct Milestone {
    std::chrono::system_clock::time_point date;
    std::wstring title;             // e.g., "Completed 10 lessons"
    std::wstring description;
    int thresholdValue = 0;
};

/// Long-term user profile
struct UserProfile {
    std::wstring userName;
    std::wstring displayName;
    int totalSessions = 0;
    int totalMinutes = 0;
    int lessonsCompleted = 0;
    int currentStreak = 0;          // consecutive days
    int longestStreak = 0;
    std::chrono::system_clock::time_point lastSessionDate;
    SkillScores averageScores;
    std::vector<ErrorPattern> errorPatterns;
    std::vector<Milestone> milestones;
    std::vector<SessionRecord> recentSessions;  // last 50 sessions
    std::chrono::system_clock::time_point createdDate;

    // Computed: progress data for charts
    std::vector<std::pair<std::wstring, SkillScores>> progressHistory; // date → scores
};
