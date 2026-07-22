#pragma once
#include "framework.h"
#include "LessonData.h"

/// Manages lesson content — built-in New Concept English + extensible external books
class LessonManager {
public:
    static LessonManager& Instance();

    /// Load built-in lessons and external book files
    bool Initialize();

    /// Get all books
    const std::vector<Book>& GetBooks() const { return m_books; }

    /// Get a specific book by ID
    const Book* GetBook(const std::wstring& bookId) const;

    /// Get a lesson by book ID and lesson number
    const Lesson* GetLesson(const std::wstring& bookId, int lessonNumber) const;

    /// Get current active lesson
    const Lesson* GetCurrentLesson() const { return m_pCurrentLesson; }

    /// Set current lesson
    void SetCurrentLesson(const std::wstring& bookId, int lessonNumber);

    /// Load an external book from a JSON file
    bool LoadExternalBook(const std::filesystem::path& jsonPath);

    /// Get practice context string for AI (combines lesson info)
    std::wstring BuildPracticeContext(const Lesson& lesson) const;

    /// Get a sample practice prompt
    std::wstring GeneratePracticePrompt(const Lesson& lesson) const;

    /// Get book count
    size_t GetBookCount() const { return m_books.size(); }

private:
    LessonManager() = default;
    ~LessonManager() = default;
    LessonManager(const LessonManager&) = delete;
    LessonManager& operator=(const LessonManager&) = delete;

    /// Create built-in New Concept English books (1-4)
    void CreateBuiltInBooks();

    /// Create NCE Book 1 lessons (First Things First)
    void CreateNCE1Lessons(Book& book);
    /// Create NCE Book 2 lessons (Practice and Progress)
    void CreateNCE2Lessons(Book& book);
    /// Create NCE Book 3 lessons (Developing Skills)
    void CreateNCE3Lessons(Book& book);
    /// Create NCE Book 4 lessons (Fluency in English)
    void CreateNCE4Lessons(Book& book);

    std::vector<Book> m_books;
    const Lesson* m_pCurrentLesson = nullptr;
};
