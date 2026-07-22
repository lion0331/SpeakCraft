#pragma once
#include "framework.h"
#include "PracticeMode.h"

/// Persists learning data: user profile, session records, error patterns, milestones
/// Stores data in %APPDATA%\SpeakCraft\learning_data.json
class LearningTracker {
public:
    static LearningTracker& Instance();

    // Load / Save
    bool Load();
    bool Save();

    // ─── User Profile ────────────────────────────
    const UserProfile& GetProfile() const { return m_profile; }
    void SetUserName(const std::wstring& name);
    void SetDisplayName(const std::wstring& name);

    // ─── Session Recording ───────────────────────
    void RecordSession(const SessionRecord& record);
    const std::vector<SessionRecord>& GetRecentSessions() const { return m_profile.recentSessions; }

    // ─── Scoring ─────────────────────────────────
    /// Calculate and update rolling average scores
    void UpdateScores(const SkillScores& newScores);

    // ─── Error Patterns ──────────────────────────
    void RecordError(const std::wstring& pattern, const std::wstring& category);
    const std::vector<ErrorPattern>& GetErrorPatterns() const { return m_profile.errorPatterns; }

    // ─── Weak Areas ──────────────────────────────
    /// Auto-deduce weak areas from error patterns (top 3 frequent categories)
    std::vector<std::wstring> GetWeakAreas() const;

    // ─── Milestones ──────────────────────────────
    void CheckMilestones();

    // ─── Progress History ────────────────────────
    /// Add a progress data point (called after each session)
    void AddProgressPoint(const SkillScores& scores);

    // ─── Streak ──────────────────────────────────
    void UpdateStreak();

    // ─── Computed Statistics ─────────────────────
    double GetOverallProgress() const;           // 0-100
    double GetSkillProgress(const std::wstring& skill) const; // "grammar", "vocabulary", "pronunciation", "fluency"
    int GetTotalSessions() const { return m_profile.totalSessions; }
    int GetTotalMinutes() const { return m_profile.totalMinutes; }

private:
    LearningTracker() = default;
    ~LearningTracker() = default;
    LearningTracker(const LearningTracker&) = delete;
    LearningTracker& operator=(const LearningTracker&) = delete;

    std::filesystem::path GetDataPath() const;
    void InitializeProfile();

    // JSON helpers
    static std::wstring SerializeSession(const SessionRecord& r);
    static std::wstring SerializeScores(const SkillScores& s);
    static std::wstring SerializeErrorPattern(const ErrorPattern& e);
    static std::wstring SerializeMilestone(const Milestone& m);
    static std::wstring EscapeJson(const std::wstring& s);

    UserProfile m_profile;
};
