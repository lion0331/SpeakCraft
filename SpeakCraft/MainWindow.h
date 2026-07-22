#pragma once
#include "framework.h"
#include "Resource.h"
#include "LessonData.h"
#include "PracticeMode.h"
#include "AIService.h"
#include "SpeechService.h"
#include "LearningTracker.h"

/// Main application window — manages all UI and orchestrates services
class MainWindow
{
public:
	MainWindow();
	~MainWindow();

	/// Register window class and create the window
	bool Create(int nCmdShow);

	/// Get the HWND
	HWND GetHwnd() const { return m_hwnd; }

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

	// ─── Mode Management ──────────────────────────
	void SwitchMode(PracticeModeType newMode);
	void UpdateModeButtons();
	void UpdateModePanel();

	// ─── Chat ─────────────────────────────────────
	void SendChatMessage();
	void AppendChatMessage(const std::wstring& role, const std::wstring& content);
	void AppendToChatLog(const std::wstring& text);
	void ClearChatLog();

	// ─── Mode 1: Text Shadowing ──────────────────
	void StartTextShadowing();
	void ProcessPronunciationResult(const std::wstring& aiResponse);

	// ─── Mode 2: Role Play ────────────────────────
	void StartRolePlay();
	void ProcessRolePlayResponse(const std::wstring& aiResponse);

	// ─── Mode 3: Sentence Pattern ────────────────
	void StartSentencePattern();
	void CheckPatternAnswer();
	void ProcessPatternResult(const std::wstring& aiResponse);

	// ─── Mode 4: Free Conversation ───────────────
	void StartFreeConversation();
	void EndFreeConversation();
	void ProcessFreeConvResult(const std::wstring& aiResponse);

	// ─── Mode 5: Grammar Correction ──────────────
	void StartGrammarCorrection();
	void SubmitGrammarCheck(const std::wstring& speech);
	void ProcessGrammarResult(const std::wstring& aiResponse);

	// ─── Mode 6: Learning Report ─────────────────
	void ShowLearningReport();

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

	// ─── Helpers ──────────────────────────────────
	std::wstring GetCurrentModeLabel() const;

	// ─── Window Handle ────────────────────────────
	HWND m_hwnd = nullptr;
	HINSTANCE m_hInstance = nullptr;

	// ─── Child Controls ───────────────────────────
	HWND m_hwndLessonTree = nullptr;      // Left panel: TreeView
	HWND m_hwndLessonContent = nullptr;   // Right upper: RichEdit for lesson content
	HWND m_hwndChatHistory = nullptr;     // Right middle: ListBox for chat/output
	HWND m_hwndChatInput = nullptr;       // Chat input
	HWND m_hwndSendBtn = nullptr;         // Send button
	HWND m_hwndRecordBtn = nullptr;       // Record / Action button
	HWND m_hwndPlayBtn = nullptr;         // Read aloud button
	HWND m_hwndStatusBar = nullptr;       // Status bar

	// ─── Mode Buttons (toolbar) ───────────────────
	HWND m_hwndModeBtns[6] = {};         // 6 mode buttons

	// ─── Services ─────────────────────────────────
	std::unique_ptr<AIService> m_pAiService;
	std::unique_ptr<SpeechService> m_pSpeechService;

	// ─── Mode State ────────────────────────────────
	PracticeModeType m_currentMode = PracticeModeType::TextShadowing;
	PracticeState m_practiceState = PracticeState::Idle;

	// ─── Mode-specific session data ────────────────
	std::unique_ptr<RolePlaySession> m_pRolePlaySession;
	std::unique_ptr<PatternSession> m_pPatternSession;
	std::unique_ptr<FreeConversationSession> m_pFreeConvSession;

	// Mode 1: shadowing state — sentence list from dialogue
	std::vector<std::wstring> m_shadowSentences;
	size_t m_shadowIndex = 0;

	// Mode 5: grammar correction temp storage
	std::wstring m_grammarSpeechBuffer;

	// ─── Lesson State ──────────────────────────────
	const Lesson* m_pCurrentLesson = nullptr;
	std::wstring m_currentBookId;

	// ─── Session tracking ──────────────────────────
	std::chrono::steady_clock::time_point m_sessionStartTime;
	SkillScores m_sessionScores;

	// ─── Tree item data — persistent, lParam points here ───
	struct TreeItemData
	{
		bool isBook = false;
		std::wstring bookId;
		std::wstring displayName;
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
	static constexpr int MODE_BTN_WIDTH = 100;
};
