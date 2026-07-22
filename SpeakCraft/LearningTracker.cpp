#include "LearningTracker.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <ctime>

// ─── Singleton ───────────────────────────────────

LearningTracker& LearningTracker::Instance() {
    static LearningTracker instance;
    return instance;
}

std::filesystem::path LearningTracker::GetDataPath() const {
    wchar_t appData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData))) {
        std::filesystem::path dir(appData);
        dir /= L"SpeakCraft";
        return dir / L"learning_data.json";
    }
    return L"learning_data.json";
}

// ─── JSON Escape ─────────────────────────────────

std::wstring LearningTracker::EscapeJson(const std::wstring& s) {
    std::wstring out;
    out.reserve(s.size() + 20);
    for (wchar_t ch : s) {
        switch (ch) {
        case L'\"': out += L"\\\""; break;
        case L'\\': out += L"\\\\"; break;
        case L'\n': out += L"\\n"; break;
        case L'\r': out += L"\\r"; break;
        case L'\t': out += L"\\t"; break;
        default: out += ch; break;
        }
    }
    return out;
}

// ─── Profile Initialization ──────────────────────

void LearningTracker::InitializeProfile() {
    m_profile = UserProfile{};
    m_profile.userName = L"learner";
    m_profile.displayName = L"Learner";
    m_profile.createdDate = std::chrono::system_clock::now();
    m_profile.lastSessionDate = m_profile.createdDate;
}

// ─── Load ────────────────────────────────────────

bool LearningTracker::Load() {
    auto path = GetDataPath();
    if (!std::filesystem::exists(path)) {
        InitializeProfile();
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        InitializeProfile();
        return false;
    }

    std::string rawBytes((std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>());
    file.close();

    if (rawBytes.empty()) {
        InitializeProfile();
        return false;
    }

    // UTF-8 → wstring
    int len = MultiByteToWideChar(CP_UTF8, 0, rawBytes.c_str(), (int)rawBytes.size(), nullptr, 0);
    if (len <= 0) { InitializeProfile(); return false; }
    std::wstring json(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, rawBytes.c_str(), (int)rawBytes.size(), &json[0], len);

    InitializeProfile();

    // Simple JSON parsing for learning data
    auto extractString = [&](const std::wstring& key) -> std::wstring {
        std::wstring search = L'"' + key + L'"';
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return L"";
        pos = json.find(L'"', pos + search.length());
        if (pos == std::wstring::npos) return L"";
        size_t end = json.find(L'"', pos + 1);
        if (end == std::wstring::npos) return L"";
        return json.substr(pos + 1, end - pos - 1);
    };

    auto extractInt = [&](const std::wstring& key, int def = 0) -> int {
        std::wstring search = L'"' + key + L'"';
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return def;
        pos = json.find(L':', pos);
        if (pos == std::wstring::npos) return def;
        pos++;
        while (pos < json.length() && iswspace(json[pos])) pos++;
        std::wstring num;
        while (pos < json.length() && (iswdigit(json[pos]) || json[pos] == L'-')) {
            num += json[pos++];
        }
        if (num.empty()) return def;
        return std::stoi(num);
    };

    auto extractDouble = [&](const std::wstring& key, double def = 0.0) -> double {
        std::wstring search = L'"' + key + L'"';
        size_t pos = json.find(search);
        if (pos == std::wstring::npos) return def;
        pos = json.find(L':', pos);
        if (pos == std::wstring::npos) return def;
        pos++;
        while (pos < json.length() && iswspace(json[pos])) pos++;
        std::wstring num;
        while (pos < json.length() && (iswdigit(json[pos]) || json[pos] == L'.' || json[pos] == L'-')) {
            num += json[pos++];
        }
        if (num.empty()) return def;
        return std::stod(num);
    };

    m_profile.userName = extractString(L"user_name");
    if (m_profile.userName.empty()) m_profile.userName = L"learner";
    m_profile.displayName = extractString(L"display_name");
    if (m_profile.displayName.empty()) m_profile.displayName = L"Learner";

    m_profile.totalSessions = extractInt(L"total_sessions", 0);
    m_profile.totalMinutes = extractInt(L"total_minutes", 0);
    m_profile.lessonsCompleted = extractInt(L"lessons_completed", 0);
    m_profile.currentStreak = extractInt(L"current_streak", 0);
    m_profile.longestStreak = extractInt(L"longest_streak", 0);

    m_profile.averageScores.grammar = extractDouble(L"avg_grammar", 0.0);
    m_profile.averageScores.vocabulary = extractDouble(L"avg_vocabulary", 0.0);
    m_profile.averageScores.pronunciation = extractDouble(L"avg_pronunciation", 0.0);
    m_profile.averageScores.fluency = extractDouble(L"avg_fluency", 0.0);

    return true;
}

// ─── Save ────────────────────────────────────────

bool LearningTracker::Save() {
    auto path = GetDataPath();
    std::filesystem::create_directories(path.parent_path());

    std::wstring json;
    json += L"{\n";
    json += L"  \"user_name\": \"" + EscapeJson(m_profile.userName) + L"\",\n";
    json += L"  \"display_name\": \"" + EscapeJson(m_profile.displayName) + L"\",\n";
    json += L"  \"total_sessions\": " + std::to_wstring(m_profile.totalSessions) + L",\n";
    json += L"  \"total_minutes\": " + std::to_wstring(m_profile.totalMinutes) + L",\n";
    json += L"  \"lessons_completed\": " + std::to_wstring(m_profile.lessonsCompleted) + L",\n";
    json += L"  \"current_streak\": " + std::to_wstring(m_profile.currentStreak) + L",\n";
    json += L"  \"longest_streak\": " + std::to_wstring(m_profile.longestStreak) + L",\n";
    json += L"  \"avg_grammar\": " + std::to_wstring(m_profile.averageScores.grammar) + L",\n";
    json += L"  \"avg_vocabulary\": " + std::to_wstring(m_profile.averageScores.vocabulary) + L",\n";
    json += L"  \"avg_pronunciation\": " + std::to_wstring(m_profile.averageScores.pronunciation) + L",\n";
    json += L"  \"avg_fluency\": " + std::to_wstring(m_profile.averageScores.fluency) + L",\n";

    // Progress history
    json += L"  \"progress_history\": [\n";
    for (size_t i = 0; i < m_profile.progressHistory.size(); i++) {
        auto& [date, scores] = m_profile.progressHistory[i];
        json += L"    {\"date\":\"" + EscapeJson(date) + L"\",";
        json += L"\"grammar\":" + std::to_wstring(scores.grammar) + L",";
        json += L"\"vocabulary\":" + std::to_wstring(scores.vocabulary) + L",";
        json += L"\"pronunciation\":" + std::to_wstring(scores.pronunciation) + L",";
        json += L"\"fluency\":" + std::to_wstring(scores.fluency) + L"}";
        if (i < m_profile.progressHistory.size() - 1) json += L",";
        json += L"\n";
    }
    json += L"  ],\n";

    // Error patterns
    json += L"  \"error_patterns\": [\n";
    for (size_t i = 0; i < m_profile.errorPatterns.size(); i++) {
        auto& e = m_profile.errorPatterns[i];
        json += L"    {\"pattern\":\"" + EscapeJson(e.pattern) + L"\",";
        json += L"\"category\":\"" + EscapeJson(e.category) + L"\",";
        json += L"\"frequency\":" + std::to_wstring(e.frequency) + L"}";
        if (i < m_profile.errorPatterns.size() - 1) json += L",";
        json += L"\n";
    }
    json += L"  ],\n";

    // Milestones
    json += L"  \"milestones\": [\n";
    for (size_t i = 0; i < m_profile.milestones.size(); i++) {
        auto& m = m_profile.milestones[i];
        json += L"    {\"title\":\"" + EscapeJson(m.title) + L"\",";
        json += L"\"description\":\"" + EscapeJson(m.description) + L"\",";
        json += L"\"threshold\":" + std::to_wstring(m.thresholdValue) + L"}";
        if (i < m_profile.milestones.size() - 1) json += L",";
        json += L"\n";
    }
    json += L"  ]\n";
    json += L"}\n";

    // UTF-8 conversion
    int utf8Len = WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(), nullptr, 0, nullptr, nullptr);
    std::string utf8(utf8Len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, json.c_str(), (int)json.size(), &utf8[0], utf8Len, nullptr, nullptr);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) return false;
    file.write(utf8.c_str(), utf8.size());
    file.close();
    return true;
}

// ─── User Profile ────────────────────────────────

void LearningTracker::SetUserName(const std::wstring& name) {
    m_profile.userName = name;
    Save();
}

void LearningTracker::SetDisplayName(const std::wstring& name) {
    m_profile.displayName = name;
    Save();
}

// ─── Session Recording ───────────────────────────

void LearningTracker::RecordSession(const SessionRecord& record) {
    m_profile.recentSessions.push_back(record);
    // Keep only last 50 sessions
    if (m_profile.recentSessions.size() > 50) {
        m_profile.recentSessions.erase(m_profile.recentSessions.begin());
    }

    m_profile.totalSessions++;
    m_profile.totalMinutes += record.durationSeconds / 60;
    m_profile.lastSessionDate = std::chrono::system_clock::now();

    UpdateScores(record.scores);
    UpdateStreak();

    // Record error patterns
    for (auto& w : record.errorWords) {
        RecordError(w, L"pronunciation");
    }
    for (auto& g : record.errorGrammar) {
        RecordError(g, L"grammar");
    }

    AddProgressPoint(record.scores);
    CheckMilestones();

    Save();
}

// ─── Scoring ─────────────────────────────────────

void LearningTracker::UpdateScores(const SkillScores& newScores) {
    int n = m_profile.totalSessions;
    // Rolling average
    m_profile.averageScores.grammar = (m_profile.averageScores.grammar * (n - 1) + newScores.grammar) / n;
    m_profile.averageScores.vocabulary = (m_profile.averageScores.vocabulary * (n - 1) + newScores.vocabulary) / n;
    m_profile.averageScores.pronunciation = (m_profile.averageScores.pronunciation * (n - 1) + newScores.pronunciation) / n;
    m_profile.averageScores.fluency = (m_profile.averageScores.fluency * (n - 1) + newScores.fluency) / n;
}

// ─── Error Patterns ──────────────────────────────

void LearningTracker::RecordError(const std::wstring& pattern, const std::wstring& category) {
    auto it = std::find_if(m_profile.errorPatterns.begin(), m_profile.errorPatterns.end(),
        [&](const ErrorPattern& e) { return e.pattern == pattern && e.category == category; });

    if (it != m_profile.errorPatterns.end()) {
        it->frequency++;
        it->lastOccurrence = std::chrono::system_clock::now();
    } else {
        ErrorPattern ep;
        ep.pattern = pattern;
        ep.category = category;
        ep.frequency = 1;
        ep.lastOccurrence = std::chrono::system_clock::now();
        m_profile.errorPatterns.push_back(ep);
    }

    // Sort by frequency descending
    std::sort(m_profile.errorPatterns.begin(), m_profile.errorPatterns.end(),
        [](const ErrorPattern& a, const ErrorPattern& b) { return a.frequency > b.frequency; });

    // Keep top 100
    if (m_profile.errorPatterns.size() > 100) {
        m_profile.errorPatterns.resize(100);
    }
}

// ─── Weak Areas ──────────────────────────────────

std::vector<std::wstring> LearningTracker::GetWeakAreas() const {
    std::vector<std::wstring> areas;
    // Return top 3 most frequent error patterns
    int count = std::min(3, (int)m_profile.errorPatterns.size());
    for (int i = 0; i < count; i++) {
        areas.push_back(m_profile.errorPatterns[i].category + L": " + m_profile.errorPatterns[i].pattern);
    }
    return areas;
}

// ─── Milestones ──────────────────────────────────

void LearningTracker::CheckMilestones() {
    auto now = std::chrono::system_clock::now();

    auto hasMilestone = [&](const std::wstring& title) -> bool {
        return std::any_of(m_profile.milestones.begin(), m_profile.milestones.end(),
            [&](const Milestone& m) { return m.title == title; });
    };

    // Check various milestones
    struct MilestoneCheck {
        int threshold;
        int current;
        std::wstring title;
        std::wstring desc;
    };

    std::vector<MilestoneCheck> checks = {
        {1, m_profile.totalSessions, L"First Practice!", L"Completed your first practice session."},
        {5, m_profile.totalSessions, L"5 Sessions", L"Completed 5 practice sessions."},
        {10, m_profile.totalSessions, L"10 Sessions", L"Completed 10 practice sessions — keep it up!"},
        {25, m_profile.totalSessions, L"25 Sessions", L"Completed 25 practice sessions!"},
        {50, m_profile.totalSessions, L"50 Sessions", L"Completed 50 practice sessions — you're dedicated!"},
        {100, m_profile.totalSessions, L"100 Sessions", L"Century! 100 practice sessions completed."},
        {3, m_profile.currentStreak, L"3-Day Streak", L"Practiced 3 days in a row!"},
        {7, m_profile.currentStreak, L"7-Day Streak", L"One week streak! Amazing consistency!"},
        {30, m_profile.currentStreak, L"30-Day Streak", L"30-day streak! You're a learning machine!"},
    };

    for (auto& check : checks) {
        if (check.current >= check.threshold && !hasMilestone(check.title)) {
            Milestone m;
            m.date = now;
            m.title = check.title;
            m.description = check.desc;
            m.thresholdValue = check.threshold;
            m_profile.milestones.push_back(m);
        }
    }

    // Score-based milestones
    double overallScore = (m_profile.averageScores.grammar + m_profile.averageScores.vocabulary +
        m_profile.averageScores.pronunciation + m_profile.averageScores.fluency) / 4.0;
    if (overallScore >= 50.0 && !hasMilestone(L"Score: 50+")) {
        m_profile.milestones.push_back({ now, L"Score: 50+", L"Overall average score reached 50+", 50 });
    }
    if (overallScore >= 70.0 && !hasMilestone(L"Score: 70+")) {
        m_profile.milestones.push_back({ now, L"Score: 70+", L"Overall average score reached 70+ — great progress!", 70 });
    }
    if (overallScore >= 85.0 && !hasMilestone(L"Score: 85+")) {
        m_profile.milestones.push_back({ now, L"Score: 85+", L"Overall average score reached 85+ — excellent!", 85 });
    }
}

// ─── Progress History ─────────────────────────────

void LearningTracker::AddProgressPoint(const SkillScores& scores) {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_s(&tm, &t);
    wchar_t buf[32];
    wcsftime(buf, 32, L"%Y-%m-%d", &tm);
    m_profile.progressHistory.push_back({ buf, scores });

    // Keep last 365 points
    if (m_profile.progressHistory.size() > 365) {
        m_profile.progressHistory.erase(m_profile.progressHistory.begin());
    }
}

// ─── Streak ──────────────────────────────────────

void LearningTracker::UpdateStreak() {
    auto now = std::chrono::system_clock::now();
    auto lastTime = std::chrono::system_clock::to_time_t(m_profile.lastSessionDate);
    auto nowTime = std::chrono::system_clock::to_time_t(now);

    std::tm lastTm, nowTm;
    localtime_s(&lastTm, &lastTime);
    localtime_s(&nowTm, &nowTime);

    // Calculate day difference — create named tm structs (can't take address of temporary)
    std::tm lastDayTm = {};
    lastDayTm.tm_sec = lastTm.tm_sec; lastDayTm.tm_min = lastTm.tm_min;
    lastDayTm.tm_hour = lastTm.tm_hour; lastDayTm.tm_mday = lastTm.tm_mday;
    lastDayTm.tm_mon = lastTm.tm_mon; lastDayTm.tm_year = lastTm.tm_year;
    lastDayTm.tm_isdst = 0;
    std::tm nowDayTm = {};
    nowDayTm.tm_sec = nowTm.tm_sec; nowDayTm.tm_min = nowTm.tm_min;
    nowDayTm.tm_hour = nowTm.tm_hour; nowDayTm.tm_mday = nowTm.tm_mday;
    nowDayTm.tm_mon = nowTm.tm_mon; nowDayTm.tm_year = nowTm.tm_year;
    nowDayTm.tm_isdst = 0;

    auto lastDay = std::chrono::system_clock::from_time_t(std::mktime(&lastDayTm));
    auto nowDay = std::chrono::system_clock::from_time_t(std::mktime(&nowDayTm));

    auto diff = std::chrono::duration_cast<std::chrono::hours>(nowDay - lastDay).count() / 24;

    if (diff <= 1) {
        m_profile.currentStreak++;
    } else {
        m_profile.currentStreak = 1;
    }

    if (m_profile.currentStreak > m_profile.longestStreak) {
        m_profile.longestStreak = m_profile.currentStreak;
    }
}

// ─── Computed Statistics ─────────────────────────

double LearningTracker::GetOverallProgress() const {
    return (m_profile.averageScores.grammar + m_profile.averageScores.vocabulary +
        m_profile.averageScores.pronunciation + m_profile.averageScores.fluency) / 4.0;
}

double LearningTracker::GetSkillProgress(const std::wstring& skill) const {
    if (skill == L"grammar") return m_profile.averageScores.grammar;
    if (skill == L"vocabulary") return m_profile.averageScores.vocabulary;
    if (skill == L"pronunciation") return m_profile.averageScores.pronunciation;
    if (skill == L"fluency") return m_profile.averageScores.fluency;
    return 0.0;
}
