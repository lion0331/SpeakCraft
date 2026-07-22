#include "MainWindow.h"
#include "ConfigManager.h"
#include "LessonManager.h"

// Global instance for window proc forwarding
static MainWindow* g_pMainWindow = nullptr;

// ─── Window Class Name ───────────────────────────
static constexpr const wchar_t* WINDOW_CLASS = L"SpeakCraftMainWindow";

// ─── Constructor / Destructor ────────────────────

MainWindow::MainWindow()
{
	m_pAiService = std::make_unique<AIService>();
	m_pSpeechService = std::make_unique<SpeechService>();
}

MainWindow::~MainWindow()
{
	g_pMainWindow = nullptr;
}

// ─── Create Window ───────────────────────────────

bool MainWindow::Create(int nCmdShow)
{
	m_hInstance = GetModuleHandle(nullptr);
	g_pMainWindow = this;

	// Register window class
	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = m_hInstance;
	wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION); // Use system icon if custom not available
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wc.lpszClassName = WINDOW_CLASS;
	wc.hIconSm = wc.hIcon;

	if (!RegisterClassExW(&wc)) return false;

	// Create window
	m_hwnd = CreateWindowExW(
		0, WINDOW_CLASS,
		L"SpeakCraft — AI英语口语练习助手",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		1200, 800,
		nullptr,
		LoadMenu(m_hInstance, MAKEINTRESOURCE(IDR_MAIN_MENU)),
		m_hInstance, nullptr);

	if (!m_hwnd) return false;

	ShowWindow(m_hwnd, nCmdShow);
	UpdateWindow(m_hwnd);
	return true;
}

// ─── Static Window Procedure ─────────────────────

LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	if (g_pMainWindow && (!g_pMainWindow->m_hwnd || g_pMainWindow->m_hwnd == hwnd))
	{
		g_pMainWindow->m_hwnd = hwnd;
		return g_pMainWindow->HandleMessage(msg, wp, lp);
	}
	return DefWindowProc(hwnd, msg, wp, lp);
}

// ─── Message Router ──────────────────────────────

LRESULT MainWindow::HandleMessage(UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_CREATE:       return OnCreate();
	case WM_SIZE:         return OnSize(LOWORD(lp), HIWORD(lp));
	case WM_COMMAND:      return OnCommand(wp, lp);
	case WM_NOTIFY:       return OnNotify(reinterpret_cast<NMHDR*>(lp));
	case WM_DESTROY:      return OnDestroy();
	case WM_AI_RESPONSE:  return OnAiResponse(wp, lp);
	case WM_SPEECH_COMPLETE: return OnSpeechComplete(wp, lp);
	}
	return DefWindowProc(m_hwnd, msg, wp, lp);
}

// ─── WM_CREATE ───────────────────────────────────

LRESULT MainWindow::OnCreate()
{
	// MUST be first: register common control classes before creating any controls
	INITCOMMONCONTROLSEX icc = {};
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES |
		ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
	InitCommonControlsEx(&icc);

	// COM is already initialized by main.cpp

	// Initialize services
	ConfigManager::Instance().Load();
	m_pSpeechService->Initialize();
	m_pAiService->Initialize(m_hwnd);
	LessonManager::Instance().Initialize();

	CreateChildControls();
	PopulateLessonTree();  // also selects first lesson + expands + redraws

	// Select first lesson
	auto& lm = LessonManager::Instance();
	if (lm.GetBookCount() > 0)
	{
		auto& books = lm.GetBooks();
		if (!books[0].lessons.empty())
		{
			SelectLesson(books[0].id, books[0].lessons[0].lessonNumber);
		}
	}

	SetStatus(L"Ready — Select a lesson to begin");
	return 0;
}

// ─── Create Child Controls ───────────────────────

void MainWindow::CreateChildControls()
{
	HINSTANCE hInst = m_hInstance;

	// Left panel: TreeView for lesson navigation
	m_hwndLessonTree = CreateWindowExW(
		0, WC_TREEVIEWW, L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_LESSON_TREE), hInst, nullptr);

	// Right upper: RichEdit for lesson content (read-only)
	LoadLibraryW(L"Msftedit.dll");
	m_hwndLessonContent = CreateWindowExW(
		0, L"RichEdit50W", L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
		ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_LESSON_CONTENT), hInst, nullptr);

	// Set default font for content area
	CHARFORMAT2W cf = {};
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_FACE | CFM_SIZE | CFM_BOLD;
	cf.yHeight = 240; // 12pt
	wcscpy_s(cf.szFaceName, L"Segoe UI");
	SendMessage(m_hwndLessonContent, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

	// Right lower: ListBox for chat history
	m_hwndChatHistory = CreateWindowExW(
		0, L"LISTBOX", L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
		LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_CHAT_HISTORY), hInst, nullptr);

	// Chat input
	m_hwndChatInput = CreateWindowExW(
		0, L"EDIT", L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_CHAT_INPUT), hInst, nullptr);

	// Send button
	m_hwndSendBtn = CreateWindowExW(
		0, L"BUTTON", L"Send ✉",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_SEND_BTN), hInst, nullptr);

	// Record / Practice button
	m_hwndRecordBtn = CreateWindowExW(
		0, L"BUTTON", L"🎤 Start Practice",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_RECORD_BTN), hInst, nullptr);

	// Play / Read aloud button
	m_hwndPlayBtn = CreateWindowExW(
		0, L"BUTTON", L"🔊 Read Aloud",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_PLAY_BTN), hInst, nullptr);

	// Status bar
	m_hwndStatusBar = CreateWindowExW(
		0, STATUSCLASSNAMEW, L"",
		WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_STATUS_BAR), hInst, nullptr);

	// Set fonts for child controls
	HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, FF_DONTCARE, L"Segoe UI");
	SendMessage(m_hwndLessonTree, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	SendMessage(m_hwndChatInput, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	SendMessage(m_hwndSendBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	SendMessage(m_hwndRecordBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	SendMessage(m_hwndPlayBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	// Note: font handle not freed — owned by window
}

// ─── WM_SIZE — Layout ────────────────────────────

LRESULT MainWindow::OnSize(int width, int height)
{
	LayoutControls(width, height);
	return 0;
}

void MainWindow::LayoutControls(int width, int height)
{
	if (!m_hwndLessonTree || width == 0 || height == 0) return;

	// Status bar at bottom
	SendMessage(m_hwndStatusBar, WM_SIZE, 0, 0);
	RECT sbRect;
	GetClientRect(m_hwndStatusBar, &sbRect);
	int sbHeight = sbRect.bottom - sbRect.top;

	int clientHeight = height - sbHeight;
	int toolbarY = 0;
	int contentTop = toolbarY + TOOLBAR_HEIGHT + MARGIN;

	// ── Toolbar row ──────────────────────────────
	int btnY = toolbarY + MARGIN;
	int btnHeight = TOOLBAR_HEIGHT - 2 * MARGIN;

	SetWindowPos(m_hwndRecordBtn, nullptr,
		MARGIN, btnY, 140, btnHeight, SWP_NOZORDER);
	SetWindowPos(m_hwndPlayBtn, nullptr,
		MARGIN + 144, btnY, 120, btnHeight, SWP_NOZORDER);

	// ── Left panel: Lesson Tree ──────────────────
	int treeLeft = MARGIN;
	int treeTop = contentTop;
	int treeWidth = LEFT_PANEL_WIDTH;
	int treeHeight = clientHeight - contentTop - MARGIN;

	SetWindowPos(m_hwndLessonTree, nullptr,
		treeLeft, treeTop, treeWidth, treeHeight, SWP_NOZORDER);

	// ── Right panel ──────────────────────────────
	int rightLeft = treeLeft + treeWidth + MARGIN;
	int rightWidth = width - rightLeft - MARGIN;
	if (rightWidth < 100) rightWidth = 100;

	int contentHeight = (treeHeight - MARGIN) * 3 / 5;
	int chatTop = treeTop + contentHeight + MARGIN;
	int chatHeight = treeHeight - contentHeight - MARGIN;

	// Lesson content (upper right)
	SetWindowPos(m_hwndLessonContent, nullptr,
		rightLeft, treeTop, rightWidth, contentHeight, SWP_NOZORDER);

	// Chat history (lower right, above input)
	int inputTop = clientHeight - MARGIN - INPUT_AREA_HEIGHT - sbHeight;
	int chatListHeight = inputTop - chatTop - MARGIN;

	SetWindowPos(m_hwndChatHistory, nullptr,
		rightLeft, chatTop, rightWidth, chatListHeight, SWP_NOZORDER);

	// Chat input + send button (bottom right)
	int inputWidth = rightWidth - BUTTON_WIDTH - MARGIN;
	SetWindowPos(m_hwndChatInput, nullptr,
		rightLeft, inputTop, inputWidth, INPUT_AREA_HEIGHT, SWP_NOZORDER);
	SetWindowPos(m_hwndSendBtn, nullptr,
		rightLeft + inputWidth + MARGIN, inputTop,
		BUTTON_WIDTH, INPUT_AREA_HEIGHT, SWP_NOZORDER);
}

// ─── Populate Lesson Tree ────────────────────────

void MainWindow::PopulateLessonTree()
{
	TreeView_DeleteAllItems(m_hwndLessonTree);
	m_treeItemData.clear();

	auto& lm = LessonManager::Instance();
	auto& books = lm.GetBooks();

	size_t totalNodes = 0;
	for (auto& b : books)
		totalNodes += 1 + b.lessons.size();
	m_treeItemData.reserve(totalNodes + 4);

	for (auto& book : books)
	{
		// ── Step 1: Insert book node (COLLAPSED) ──
		auto pBookData = std::make_unique<TreeItemData>();
		pBookData->isBook = true;
		pBookData->bookId = book.id;
		pBookData->lessonNumber = 0;
		TreeItemData* pRawBook = pBookData.get();
		m_treeItemData.push_back(std::move(pBookData));

		TVINSERTSTRUCTW tviBook = {};
		tviBook.hParent = TVI_ROOT;
		tviBook.hInsertAfter = TVI_LAST;
		tviBook.item.mask = TVIF_TEXT | TVIF_PARAM;
		tviBook.item.pszText = const_cast<LPWSTR>(book.name.c_str());
		tviBook.item.lParam = reinterpret_cast<LPARAM>(pRawBook);
		// NO TVIS_EXPANDED — we expand AFTER children are added
		HTREEITEM hBook = TreeView_InsertItem(m_hwndLessonTree, &tviBook);

		// ── Step 2: Insert lesson nodes under the book ──
		for (auto& lesson : book.lessons)
		{
			auto pData = std::make_unique<TreeItemData>();
			pData->isBook = false;
			pData->bookId = book.id;
			pData->lessonNumber = lesson.lessonNumber;
			TreeItemData* pRaw = pData.get();
			m_treeItemData.push_back(std::move(pData));

			std::wstring label = L"Lesson " + std::to_wstring(lesson.lessonNumber) +
				L" — " + lesson.title;

			TVINSERTSTRUCTW tviL = {};
			tviL.hParent = hBook;
			tviL.hInsertAfter = TVI_LAST;
			tviL.item.mask = TVIF_TEXT | TVIF_PARAM;
			tviL.item.pszText = const_cast<LPWSTR>(label.c_str());
			tviL.item.lParam = reinterpret_cast<LPARAM>(pRaw);
			TreeView_InsertItem(m_hwndLessonTree, &tviL);
		}

		// ── Step 3: Expand AFTER children exist ──
		TreeView_Expand(m_hwndLessonTree, hBook, TVE_EXPAND);
	}

	// Force full redraw
	InvalidateRect(m_hwndLessonTree, nullptr, TRUE);
	UpdateWindow(m_hwndLessonTree);

	// Select first lesson (safe)
	if (!books.empty() && !books[0].lessons.empty())
		SelectLesson(books[0].id, books[0].lessons[0].lessonNumber);
}

// ─── Lesson Selection ────────────────────────────

void MainWindow::SelectLesson(const std::wstring& bookId, int lessonNumber)
{
	auto& lm = LessonManager::Instance();
	lm.SetCurrentLesson(bookId, lessonNumber);
	auto* pLesson = lm.GetCurrentLesson();
	if (pLesson)
	{
		m_pCurrentLesson = pLesson;
		m_currentBookId = bookId;
		DisplayLesson(*pLesson);

		// Visually select the tree item
		// Walk tree to find the matching node by lParam
		HTREEITEM hRoot = TreeView_GetRoot(m_hwndLessonTree);
		while (hRoot)
		{
			TVITEMW item = {};
			item.hItem = hRoot;
			item.mask = TVIF_PARAM;
			TreeView_GetItem(m_hwndLessonTree, &item);
			auto* pData = reinterpret_cast<TreeItemData*>(item.lParam);
			if (pData && pData->isBook && pData->bookId == bookId)
			{
				// Found the book — expand it, then walk children
				TreeView_Expand(m_hwndLessonTree, hRoot, TVE_EXPAND);

				HTREEITEM hChild = TreeView_GetChild(m_hwndLessonTree, hRoot);
				while (hChild)
				{
					TVITEMW childItem = {};
					childItem.hItem = hChild;
					childItem.mask = TVIF_PARAM;
					TreeView_GetItem(m_hwndLessonTree, &childItem);
					auto* pChildData = reinterpret_cast<TreeItemData*>(childItem.lParam);
					if (pChildData && !pChildData->isBook && pChildData->lessonNumber == lessonNumber)
					{
						TreeView_SelectItem(m_hwndLessonTree, hChild);
						TreeView_EnsureVisible(m_hwndLessonTree, hChild);
						break;
					}
					hChild = TreeView_GetNextSibling(m_hwndLessonTree, hChild);
				}
				break;
			}
			hRoot = TreeView_GetNextSibling(m_hwndLessonTree, hRoot);
		}

		// Clear chat history when switching lessons
		m_pAiService->ClearHistory();
		SendMessage(m_hwndChatHistory, LB_RESETCONTENT, 0, 0);

		// Show context message
		std::wstring ctxMsg = L"📖 Switched to: " + pLesson->bookName +
			L" — Lesson " + std::to_wstring(lessonNumber) +
			L": " + pLesson->title;
		AppendToChatLog(ctxMsg);

		SetStatus(L"Ready — " + pLesson->bookName +
			L" Lesson " + std::to_wstring(lessonNumber));
	}
}

void MainWindow::DisplayLesson(const Lesson& lesson)
{
	std::wstring text;
	text += lesson.bookName + L"\r\n";
	text += L"Lesson " + std::to_wstring(lesson.lessonNumber) + L": " + lesson.title + L"\r\n\r\n";
	text += L"Key Grammar:\r\n" + lesson.keyGrammar + L"\r\n\r\n";
	text += L"Dialogue / Text:\r\n" + lesson.dialogueText + L"\r\n\r\n";

	if (!lesson.vocabulary.empty())
	{
		text += L"Key Vocabulary:\r\n";
		for (auto& v : lesson.vocabulary)
		{
			text += L"  " + v.word + L"  " + v.phonetic + L" - " + v.translation + L"\r\n";
			if (!v.exampleSentence.empty())
			{
				text += L"    Example: " + v.exampleSentence + L"\r\n";
			}
		}
		text += L"\r\n";
	}

	text += L"Practice Focus:\r\n" + lesson.practicePrompt + L"\r\n";

	SetWindowTextW(m_hwndLessonContent, text.c_str());

	// Scroll to top
	SendMessage(m_hwndLessonContent, EM_SETSEL, 0, 0);
	SendMessage(m_hwndLessonContent, EM_SCROLLCARET, 0, 0);
}

// ─── WM_NOTIFY — Tree Selection ──────────────────

LRESULT MainWindow::OnNotify(NMHDR* pnmh)
{
	if (pnmh->idFrom == IDC_LESSON_TREE && pnmh->code == TVN_SELCHANGEDW)
	{
		NMTREEVIEWW* pnmtv = reinterpret_cast<NMTREEVIEWW*>(pnmh);
		OnTreeSelectionChanged(pnmtv->itemNew.hItem);
		return 0;
	}
	return 0;
}

void MainWindow::OnTreeSelectionChanged(HTREEITEM hItem)
{
	if (!hItem) return;

	TVITEMW item = {};
	item.hItem = hItem;
	item.mask = TVIF_PARAM | TVIF_HANDLE;
	if (!TreeView_GetItem(m_hwndLessonTree, &item)) return;

	auto* pData = reinterpret_cast<TreeItemData*>(item.lParam);
	if (!pData) return;
	if (pData->isBook) return;  // Book node → ignore

	SelectLesson(pData->bookId, pData->lessonNumber);
}

// ─── WM_COMMAND ──────────────────────────────────

LRESULT MainWindow::OnCommand(WPARAM wp, LPARAM lp)
{
	int id = LOWORD(wp);
	int code = HIWORD(wp);

	switch (id)
	{
	case IDM_FILE_EXIT:
		DestroyWindow(m_hwnd);
		return 0;

	case IDM_PRACTICE_START:
	case IDC_RECORD_BTN:
		if (code == BN_CLICKED || code == 0)
		{
			StartPractice();
		}
		return 0;

	case IDM_PRACTICE_STOP:
		if (code == 0) StopPractice();
		return 0;

	case IDC_PLAY_BTN:
		if (code == BN_CLICKED) ReadLessonAloud();
		return 0;

	case IDC_SEND_BTN:
		if (code == BN_CLICKED) SendChatMessage();
		return 0;

	case IDM_SETTINGS_APIKEY:
		ShowSettingsDialog();
		return 0;

	case IDM_SETTINGS_VOICE:
		ShowVoiceSettingsDialog();
		return 0;

	case IDM_HELP_ABOUT:
		ShowAboutDialog();
		return 0;

		// Handle Enter key in chat input
	case IDC_CHAT_INPUT:
		if (code == EN_MAXTEXT)
		{
			// User pressed Enter in chat input — but EN_MAXTEXT isn't for Enter
			// We handle Enter in the edit control subclass, but for now
			// the Send button is the primary send mechanism
		}
		return 0;
	}

	return 0;
}

// ─── Chat ────────────────────────────────────────

void MainWindow::SendChatMessage()
{
	int len = GetWindowTextLengthW(m_hwndChatInput);
	if (len == 0) return;

	std::wstring msg(len + 1, L'\0');
	GetWindowTextW(m_hwndChatInput, &msg[0], len + 1);
	msg.resize(len);

	// Clear input
	SetWindowTextW(m_hwndChatInput, L"");

	// Show user message
	AppendChatMessage(L"You", msg);

	// Build context from current lesson
	std::wstring systemCtx;
	if (m_pCurrentLesson)
	{
		systemCtx = LessonManager::Instance().BuildPracticeContext(*m_pCurrentLesson);
	}

	// Send to AI
	SetStatus(L"🤔 AI is thinking...");
	if (!m_pAiService->SendMessage(msg, systemCtx))
	{
		AppendToChatLog(L"⚠️ AI service is busy. Please wait for the current response.");
		SetStatus(L"Ready");
	}
}

void MainWindow::AppendChatMessage(const std::wstring& role, const std::wstring& content)
{
	std::wstring prefix;
	if (role == L"You" || role == L"user")
	{
		prefix = L"🧑 You: ";
	}
	else if (role == L"assistant" || role == L"AI")
	{
		prefix = L"🤖 AI: ";
	}
	else
	{
		prefix = role + L": ";
	}

	// Add to list box — handle multi-line by splitting
	std::wstring fullMsg = prefix + content;
	AppendToChatLog(fullMsg);
}

void MainWindow::AppendToChatLog(const std::wstring& text)
{
	// Split by newlines and add each line to the listbox
	std::wstring remaining = text;
	size_t pos;
	while ((pos = remaining.find(L'\n')) != std::wstring::npos)
	{
		std::wstring line = remaining.substr(0, pos);
		if (!line.empty())
		{
			SendMessageW(m_hwndChatHistory, LB_ADDSTRING, 0,
				reinterpret_cast<LPARAM>(line.c_str()));
		}
		remaining = remaining.substr(pos + 1);
	}
	if (!remaining.empty())
	{
		SendMessageW(m_hwndChatHistory, LB_ADDSTRING, 0,
			reinterpret_cast<LPARAM>(remaining.c_str()));
	}

	// Scroll to bottom
	int count = static_cast<int>(SendMessage(m_hwndChatHistory, LB_GETCOUNT, 0, 0));
	if (count > 0)
	{
		SendMessage(m_hwndChatHistory, LB_SETTOPINDEX, count - 1, 0);
	}
}

// ─── Practice ────────────────────────────────────

void MainWindow::StartPractice()
{
	if (!m_pCurrentLesson)
	{
		MessageBoxW(m_hwnd, L"Please select a lesson first.",
			L"No Lesson Selected", MB_OK | MB_ICONINFORMATION);
		return;
	}

	if (m_practiceState != PracticeState::Idle)
	{
		StopPractice();
		return;
	}

	m_practiceState = PracticeState::Listening;

	// Generate a practice prompt
	std::wstring prompt = LessonManager::Instance().GeneratePracticePrompt(*m_pCurrentLesson);

	// Speak the prompt
	m_pSpeechService->SpeakAsync(prompt, m_hwnd);

	// Update UI
	SetWindowTextW(m_hwndRecordBtn, L"⏹ Stop Practice");
	SetStatus(L"🔊 AI is speaking... Listen and respond.");

	// Show prompt in chat
	AppendToChatLog(L"📣 Practice: " + prompt);

	// Send initial practice message to AI
	std::wstring practiceMsg = L"I'm ready to practice this lesson. " + prompt;
	std::wstring systemCtx = LessonManager::Instance().BuildPracticeContext(*m_pCurrentLesson);
	m_pAiService->SendMessage(practiceMsg, systemCtx);
}

void MainWindow::StopPractice()
{
	m_practiceState = PracticeState::Idle;
	m_pSpeechService->StopSpeaking();
	m_pAiService->Cancel();

	SetWindowTextW(m_hwndRecordBtn, L"🎤 Start Practice");
	SetStatus(L"Ready — Practice stopped");
}

void MainWindow::ReadLessonAloud()
{
	if (!m_pCurrentLesson)
	{
		MessageBoxW(m_hwnd, L"Please select a lesson first.",
			L"No Lesson Selected", MB_OK | MB_ICONINFORMATION);
		return;
	}

	// Read the dialogue text
	std::wstring textToRead = m_pCurrentLesson->dialogueText;
	if (textToRead.empty())
	{
		MessageBoxW(m_hwnd, L"No dialogue text available for this lesson.",
			L"Nothing to Read", MB_OK | MB_ICONINFORMATION);
		return;
	}

	SetStatus(L"🔊 Reading lesson aloud...");
	m_pSpeechService->SpeakAsync(textToRead, m_hwnd);
}

// ─── AI Response Handler ─────────────────────────

LRESULT MainWindow::OnAiResponse(WPARAM wp, LPARAM lp)
{
	std::wstring* pMsg = reinterpret_cast<std::wstring*>(wp);
	if (!pMsg) return 0;

	bool success = (lp == 1);

	if (success && pMsg->find(L"ERROR:") != 0)
	{
		AppendChatMessage(L"AI", *pMsg);

		if (m_practiceState == PracticeState::Listening)
		{
			// Speak the AI response
			m_pSpeechService->SpeakAsync(*pMsg, m_hwnd);
			m_practiceState = PracticeState::Responding;
		}
	}
	else
	{
		AppendToChatLog(L"❌ " + *pMsg);
	}

	SetStatus(L"Ready");
	delete pMsg;
	return 0;
}

// ─── Speech Complete Handler ─────────────────────

LRESULT MainWindow::OnSpeechComplete(WPARAM wp, LPARAM lp)
{
	if (m_practiceState == PracticeState::Responding)
	{
		SetStatus(L"🎤 Your turn! Speak your answer...");
		m_practiceState = PracticeState::Listening;
	}
	else
	{
		SetStatus(L"Ready");
	}
	return 0;
}

// ─── Settings Dialog ─────────────────────────────

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_INITDIALOG: {
		auto& cfg = ConfigManager::Instance();
		SetDlgItemTextW(hDlg, IDC_API_ENDPOINT, cfg.GetApiEndpoint().c_str());
		SetDlgItemTextW(hDlg, IDC_API_KEY, cfg.GetApiKey().c_str());
		SetDlgItemTextW(hDlg, IDC_MODEL_NAME, cfg.GetModelName().c_str());
		return TRUE;
	}

	case WM_COMMAND:
		if (LOWORD(wp) == IDC_SAVE_SETTINGS || LOWORD(wp) == IDOK)
		{
			auto& cfg = ConfigManager::Instance();
			wchar_t buf[4096];

			GetDlgItemTextW(hDlg, IDC_API_ENDPOINT, buf, 4096);
			cfg.SetApiEndpoint(buf);

			GetDlgItemTextW(hDlg, IDC_API_KEY, buf, 4096);
			cfg.SetApiKey(buf);

			GetDlgItemTextW(hDlg, IDC_MODEL_NAME, buf, 4096);
			cfg.SetModelName(buf);

			cfg.Save();
			EndDialog(hDlg, IDOK);
			return TRUE;
		}

		if (LOWORD(wp) == IDCANCEL)
		{
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}

		if (LOWORD(wp) == IDC_TEST_CONNECTION)
		{
			// Save temp settings first
			wchar_t buf[4096];
			auto& cfg = ConfigManager::Instance();
			GetDlgItemTextW(hDlg, IDC_API_ENDPOINT, buf, 4096);
			cfg.SetApiEndpoint(buf);
			GetDlgItemTextW(hDlg, IDC_API_KEY, buf, 4096);
			cfg.SetApiKey(buf);
			GetDlgItemTextW(hDlg, IDC_MODEL_NAME, buf, 4096);
			cfg.SetModelName(buf);

			// Need access to MainWindow's AI service
			std::wstring error;
			// Create a temporary AIService to test
			AIService tempService;
			HWND hwndMain = GetParent(hDlg);
			tempService.Initialize(hwndMain);
			if (tempService.TestConnection(error))
			{
				MessageBoxW(hDlg, L"✅ Connection successful! The API is working.",
					L"Test Result", MB_OK | MB_ICONINFORMATION);
			}
			else
			{
				MessageBoxW(hDlg, (L"❌ Connection failed:\n" + error).c_str(),
					L"Test Result", MB_OK | MB_ICONERROR);
			}
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void MainWindow::ShowSettingsDialog()
{
	DialogBoxW(m_hInstance, MAKEINTRESOURCEW(IDD_SETTINGS),
		m_hwnd, SettingsDlgProc);
}

// ─── Voice Settings Dialog ────────────────────────

INT_PTR CALLBACK VoiceSettingsDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	static SpeechService* pSpeech = nullptr;

	switch (msg)
	{
	case WM_INITDIALOG: {
		// Use temp service for listing voices (independent of main window)
		SpeechService tempSvc;
		tempSvc.Initialize();
		auto voices = tempSvc.GetAvailableVoices();
		HWND hList = GetDlgItem(hDlg, IDC_VOICE_LIST);
		for (auto& v : voices)
		{
			SendMessageW(hList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(v.c_str()));
		}
		auto& cfg = ConfigManager::Instance();
		std::wstring curVoice = cfg.GetVoiceToken();
		if (!curVoice.empty())
		{
			LRESULT idx = SendMessageW(hList, LB_FINDSTRINGEXACT, 0,
				reinterpret_cast<LPARAM>(curVoice.c_str()));
			if (idx != LB_ERR)
				SendMessage(hList, LB_SETCURSEL, idx, 0);
		}
		SetDlgItemInt(hDlg, IDC_VOICE_RATE, cfg.GetSpeechRate(), TRUE);
		return TRUE;
	}

	case WM_COMMAND:
		if (LOWORD(wp) == IDC_VOICE_TEST)
		{
			HWND hList = GetDlgItem(hDlg, IDC_VOICE_LIST);
			int idx = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
			if (idx == LB_ERR)
			{
				MessageBoxW(hDlg, L"Please select a voice first.",
					L"Voice Test", MB_OK | MB_ICONINFORMATION);
				return TRUE;
			}

			wchar_t buf[256] = {};
			SendMessageW(hList, LB_GETTEXT, idx, reinterpret_cast<LPARAM>(buf));
			if (g_pMainWindow)
			{
				auto* pSpeech = g_pMainWindow->GetSpeechService();
				if (pSpeech)
				{
					int rate = (int)GetDlgItemInt(hDlg, IDC_VOICE_RATE, nullptr, TRUE);
					pSpeech->StopSpeaking();
					pSpeech->Initialize();
					pSpeech->SetRate(rate);
					if (!pSpeech->SetVoice(buf) ||
						!pSpeech->SpeakAsync(L"Hello. This is a voice test for SpeakCraft.", hDlg))
					{
						MessageBoxW(hDlg, L"Voice playback failed. Check Windows speech settings and audio output.",
							L"Voice Test", MB_OK | MB_ICONERROR);
					}
				}
			}
			return TRUE;
		}

		if (LOWORD(wp) == IDOK)
		{
			auto& cfg = ConfigManager::Instance();
			HWND hList = GetDlgItem(hDlg, IDC_VOICE_LIST);
			int idx = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
			if (idx != LB_ERR)
			{
				wchar_t buf[256] = {};
				SendMessageW(hList, LB_GETTEXT, idx, reinterpret_cast<LPARAM>(buf));
				cfg.SetVoiceToken(buf);
			}
			int rate = (int)GetDlgItemInt(hDlg, IDC_VOICE_RATE, nullptr, TRUE);
			cfg.SetSpeechRate(std::clamp(rate, -10, 10));
			cfg.Save();
			EndDialog(hDlg, IDOK);
			return TRUE;
		}

		if (LOWORD(wp) == IDCANCEL)
		{
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void MainWindow::ShowVoiceSettingsDialog()
{
	DialogBoxW(m_hInstance, MAKEINTRESOURCEW(IDD_VOICE_SETTINGS),
		m_hwnd, VoiceSettingsDlgProc);
}

// ─── About Dialog ────────────────────────────────

INT_PTR CALLBACK AboutDlgProc(HWND hDlg, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_INITDIALOG: {
		SetDlgItemTextW(hDlg, IDOK, L"OK");
		return TRUE;
	}
	case WM_COMMAND:
		if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)
		{
			EndDialog(hDlg, 0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

void MainWindow::ShowAboutDialog()
{
	DialogBoxW(m_hInstance, MAKEINTRESOURCEW(IDD_ABOUT),
		m_hwnd, AboutDlgProc);
}

// ─── Status Bar ──────────────────────────────────

void MainWindow::SetStatus(const std::wstring& text)
{
	SendMessageW(m_hwndStatusBar, SB_SETTEXTW, 0,
		reinterpret_cast<LPARAM>(text.c_str()));
}

// ─── WM_DESTROY ──────────────────────────────────

LRESULT MainWindow::OnDestroy()
{
	m_pAiService->Cancel();
	m_pSpeechService->StopSpeaking();
	PostQuitMessage(0);
	return 0;
}

// ─── Run Message Loop ────────────────────────────

int MainWindow::Run()
{
	MSG msg = {};
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return static_cast<int>(msg.wParam);
}
