#pragma once
#include "framework.h"
#include "Resource.h"
#include "LessonData.h"
#include "AIService.h"
#include "SpeechService.h"

/// Main application window — manages all UI and orchestrates services
class MainWindow
{
public:
	MainWindow();
	~MainWindow();

	/// Register window class and create the window
	bool Create(int nCmdShow);

	/// Get the HWND
	HWND GetHwnd() const
	{
		return m_hwnd;
	}

	/// Run the message loop
	static int Run();

	/// Access SpeechService for dialogs
	SpeechService* GetSpeechService() { return m_pSpeechService.get(); }

private:
	// ─── Window Procedure ─────────────────────────
	static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
	LRESULT HandleMessage(UINT msg, WPARAM wp, LPARAM lp);

	// ─── Message Handlers ─────────────────────────
	LRESULT OnCreate();
	LRESULT OnSize(int width, int height);
	LRESULT OnCommand(WPARAM wp, LPARAM lp);
	LRESULT OnNotify(NMHDR* pnmh);
	LRESULT OnDestroy();
	LRESULT OnAiResponse(WPARAM wp, LPARAM lp);
	LRESULT OnSpeechComplete(WPARAM wp, LPARAM lp);

	// ─── Layout ───────────────────────────────────
	void LayoutControls(int width, int height);
	void CreateChildControls();

	// ─── Lesson Navigation ────────────────────────
	void PopulateLessonTree();
	void SelectLesson(const std::wstring& bookId, int lessonNumber);
	void DisplayLesson(const Lesson& lesson);
	void OnTreeSelectionChanged(HTREEITEM hItem);

	// ─── Chat ─────────────────────────────────────
	void SendChatMessage();
	void AppendChatMessage(const std::wstring& role, const std::wstring& content);
	void AppendToChatLog(const std::wstring& text);

	// ─── Speech / Practice ────────────────────────
	void StartPractice();
	void StopPractice();
	void ReadLessonAloud();

	// ─── Settings ─────────────────────────────────
	void ShowSettingsDialog();
	void ShowVoiceSettingsDialog();
	void ShowAboutDialog();

	// ─── Status Bar ───────────────────────────────
	void SetStatus(const std::wstring& text);

	// ─── Window Handle ────────────────────────────
	HWND m_hwnd = nullptr;
	HINSTANCE m_hInstance = nullptr;

	// ─── Child Controls ───────────────────────────
	HWND m_hwndLessonTree = nullptr;      // Left panel: TreeView
	HWND m_hwndLessonContent = nullptr;   // Right upper: RichEdit
	HWND m_hwndChatHistory = nullptr;     // Right lower: ListBox for chat
	HWND m_hwndChatInput = nullptr;       // Right lower: Edit for input
	HWND m_hwndSendBtn = nullptr;         // Send button
	HWND m_hwndRecordBtn = nullptr;       // Record / practice button
	HWND m_hwndPlayBtn = nullptr;         // Read aloud button
	HWND m_hwndStatusBar = nullptr;       // Status bar

	// ─── Services ─────────────────────────────────
	std::unique_ptr<AIService> m_pAiService;
	std::unique_ptr<SpeechService> m_pSpeechService;

	// ─── State ────────────────────────────────────
	PracticeState m_practiceState = PracticeState::Idle;
	const Lesson* m_pCurrentLesson = nullptr;
	std::wstring m_currentBookId;

	// ─── Tree item data — persistent, lParam points here ───
	struct TreeItemData
	{
		bool isBook = false;
		std::wstring bookId;
		std::wstring displayName;   // pre-built from book/lesson name — NOT on stack
		int lessonNumber = 0;
	};
	std::vector<std::unique_ptr<TreeItemData>> m_treeItemData;

	// ─── Layout constants ─────────────────────────
	static constexpr int LEFT_PANEL_WIDTH = 280;
	static constexpr int CHAT_AREA_HEIGHT = 250;
	static constexpr int INPUT_AREA_HEIGHT = 32;
	static constexpr int BUTTON_WIDTH = 80;
	static constexpr int STATUS_HEIGHT = 24;
	static constexpr int MARGIN = 4;
	static constexpr int TOOLBAR_HEIGHT = 36;
};


