#include "MainWindow.h"
#include "ConfigManager.h"
#include "LessonManager.h"

// Forward declaration
static std::wstring GetBarString(double score);

// Global instance for window proc forwarding
static MainWindow* g_pMainWindow = nullptr;

// ─── Window Class Name ───────────────────────────
static constexpr const wchar_t* WINDOW_CLASS = L"SpeakCraftMainWindow";

// ─── Mode button labels ──────────────────────────
static constexpr const wchar_t* MODE_LABELS[] = {
	L"📖 课文跟读",
	L"🎭 角色扮演",
	L"🔄 句型替换",
	L"💬 自由对话",
	L"🎙️ 发音纠错",
	L"📊 学习追踪"
};

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

	WNDCLASSEXW wc = {};
	wc.cbSize = sizeof(wc);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = m_hInstance;
	wc.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_APP_ICON));
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	wc.lpszClassName = WINDOW_CLASS;
	wc.hIconSm = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_APP_ICON));

	if (!RegisterClassExW(&wc)) return false;

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
	case WM_USER_STT_EVENT:
		// Route SAPI recognition events to speech service
		if (m_pSpeechService) m_pSpeechService->HandleSttEvent();
		return 0;
	case WM_USER_STT_RESULT:
		// Recognition phrase arrived — append to input box
	{
		std::wstring* pText = reinterpret_cast<std::wstring*>(wp);
		if (pText)
		{
			// Append to chat input so user can see/edit before submitting
			int curLen = GetWindowTextLengthW(m_hwndChatInput);
			SendMessageW(m_hwndChatInput, EM_SETSEL, curLen, curLen);
			std::wstring toInsert = *pText + L" ";
			SendMessageW(m_hwndChatInput, EM_REPLACESEL, TRUE,
				reinterpret_cast<LPARAM>(toInsert.c_str()));
			delete pText;
		}
	}
	return 0;
	case WM_KEYDOWN:
		// Enter key in chat input → submit typed response
		if (wp == VK_RETURN && GetFocus() == g_pMainWindow->m_hwndChatInput)
		{
			g_pMainWindow->SendChatMessage();
			return 0;
		}
		break;
	}
	return DefWindowProc(m_hwnd, msg, wp, lp);
}

// ─── WM_CREATE ───────────────────────────────────

LRESULT MainWindow::OnCreate()
{
	INITCOMMONCONTROLSEX icc = {};
	icc.dwSize = sizeof(icc);
	icc.dwICC = ICC_TREEVIEW_CLASSES | ICC_LISTVIEW_CLASSES |
		ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
	InitCommonControlsEx(&icc);

	ConfigManager::Instance().Load();
	m_pSpeechService->Initialize();
	m_pSpeechService->SetPreferredRecognizer(ConfigManager::Instance().GetRecognizerToken());
	m_pAiService->Initialize(m_hwnd);
	LessonManager::Instance().Initialize();
	LearningTracker::Instance().Load();

	CreateChildControls();
	PopulateLessonTree();
	SwitchMode(PracticeModeType::TextShadowing);

	auto& lm = LessonManager::Instance();
	if (lm.GetBookCount() > 0)
	{
		auto& books = lm.GetBooks();
		if (!books[0].lessons.empty())
		{
			SelectLesson(books[0].id, books[0].lessons[0].lessonNumber);
		}
	}

	SetStatus(L"Ready — Select a lesson and practice mode to begin");
	return 0;
}

// ─── Create Child Controls ───────────────────────

void MainWindow::CreateChildControls()
{
	HINSTANCE hInst = m_hInstance;

	// Left panel: TreeView
	m_hwndLessonTree = CreateWindowExW(
		0, WC_TREEVIEWW, L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER |
		TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_LESSON_TREE), hInst, nullptr);

	// Right upper: RichEdit for lesson content
	LoadLibraryW(L"Msftedit.dll");
	m_hwndLessonContent = CreateWindowExW(
		0, L"RichEdit50W", L"",
		WS_CHILD | WS_VISIBLE | WS_BORDER | WS_VSCROLL |
		ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_LESSON_CONTENT), hInst, nullptr);

	CHARFORMAT2W cf = {};
	cf.cbSize = sizeof(cf);
	cf.dwMask = CFM_FACE | CFM_SIZE | CFM_BOLD;
	cf.yHeight = 240;
	wcscpy_s(cf.szFaceName, L"Segoe UI");
	SendMessage(m_hwndLessonContent, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&cf));

	// Chat history
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

	// Main action button: per-sentence submit ("我说完了") / Start
	m_hwndRecordBtn = CreateWindowExW(
		0, L"BUTTON", L"▶ Start",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_RECORD_BTN), hInst, nullptr);

	// End Practice button (hidden until practice starts)
	m_hwndEndBtn = CreateWindowExW(
		0, L"BUTTON", L"⏹ 结束练习",
		WS_CHILD | BS_PUSHBUTTON,   // NOT visible initially
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_END_BTN), hInst, nullptr);

	// Play / Read aloud button
	m_hwndPlayBtn = CreateWindowExW(
		0, L"BUTTON", L"🔊 Read Aloud",
		WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_PLAY_BTN), hInst, nullptr);

	// ── Mode Buttons (6 practice modes) ──────────
	for (int i = 0; i < 6; i++)
	{
		m_hwndModeBtns[i] = CreateWindowExW(
			0, L"BUTTON", MODE_LABELS[i],
			WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_PUSHLIKE,
			0, 0, 0, 0,
			m_hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_MODE_BTN_0 + i)), hInst, nullptr);
	}

	// Status bar
	m_hwndStatusBar = CreateWindowExW(
		0, STATUSCLASSNAMEW, L"",
		WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
		0, 0, 0, 0,
		m_hwnd, reinterpret_cast<HMENU>(IDC_STATUS_BAR), hInst, nullptr);

	// Set fonts
	HFONT hFont = CreateFontW(-12, 0, 0, 0, FW_NORMAL,
		FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
		DEFAULT_QUALITY, FF_DONTCARE, L"Segoe UI");
	SendMessage(m_hwndLessonTree, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	SendMessage(m_hwndChatInput, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	SendMessage(m_hwndRecordBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	SendMessage(m_hwndEndBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	SendMessage(m_hwndPlayBtn, WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	for (int i = 0; i < 6; i++)
	{
		SendMessage(m_hwndModeBtns[i], WM_SETFONT, reinterpret_cast<WPARAM>(hFont), TRUE);
	}
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

	// Status bar
	SendMessage(m_hwndStatusBar, WM_SIZE, 0, 0);
	RECT sbRect;
	GetClientRect(m_hwndStatusBar, &sbRect);
	int sbHeight = sbRect.bottom - sbRect.top;

	int clientHeight = height - sbHeight;
	int toolbarY = 0;
	int contentTop = toolbarY + TOOLBAR_HEIGHT + MARGIN;

	// ── Toolbar row: mode buttons ────────────────
	int btnY = toolbarY + MARGIN;
	int btnHeight = TOOLBAR_HEIGHT - 2 * MARGIN;

	// Mode buttons (first row)
	int modeX = MARGIN;
	for (int i = 0; i < 6; i++)
	{
		SetWindowPos(m_hwndModeBtns[i], nullptr,
			modeX, btnY, MODE_BTN_WIDTH, btnHeight, SWP_NOZORDER);
		modeX += MODE_BTN_WIDTH + 2;
	}

	// Action buttons row
	int actionY = toolbarY + TOOLBAR_HEIGHT + MARGIN;
	int actionBtnHeight = TOOLBAR_HEIGHT - 2 * MARGIN;
	SetWindowPos(m_hwndRecordBtn, nullptr,
		MARGIN, actionY, 160, actionBtnHeight, SWP_NOZORDER);
	SetWindowPos(m_hwndEndBtn, nullptr,
		MARGIN + 164, actionY, 140, actionBtnHeight,
		m_practiceState != PracticeState::Idle ? SWP_NOZORDER : SWP_NOZORDER | SWP_HIDEWINDOW);
	ShowWindow(m_hwndEndBtn, m_practiceState != PracticeState::Idle ? SW_SHOW : SW_HIDE);
	SetWindowPos(m_hwndPlayBtn, nullptr,
		MARGIN + 308, actionY, 120, actionBtnHeight, SWP_NOZORDER);

	// Adjust content top for second toolbar row
	int adjustedContentTop = actionY + TOOLBAR_HEIGHT;

	// ── Left panel: Lesson Tree ──────────────────
	int treeLeft = MARGIN;
	int treeTop = adjustedContentTop;
	int treeWidth = LEFT_PANEL_WIDTH;
	int treeHeight = clientHeight - treeTop - MARGIN;

	SetWindowPos(m_hwndLessonTree, nullptr,
		treeLeft, treeTop, treeWidth, treeHeight, SWP_NOZORDER);

	// ── Right panel ──────────────────────────────
	int rightLeft = treeLeft + treeWidth + MARGIN;
	int rightWidth = width - rightLeft - MARGIN;
	if (rightWidth < 100) rightWidth = 100;

	int contentHeight = (treeHeight - MARGIN) * 3 / 5;
	int chatTop = treeTop + contentHeight + MARGIN;
	int chatHeight = treeHeight - contentHeight - MARGIN;

	// Lesson content
	SetWindowPos(m_hwndLessonContent, nullptr,
		rightLeft, treeTop, rightWidth, contentHeight, SWP_NOZORDER);

	// Chat area
	int inputTop = clientHeight - MARGIN - INPUT_AREA_HEIGHT - sbHeight;
	int chatListHeight = inputTop - chatTop - MARGIN;

	SetWindowPos(m_hwndChatHistory, nullptr,
		rightLeft, chatTop, rightWidth, chatListHeight, SWP_NOZORDER);

	// Chat input (optional text fallback)
	SetWindowPos(m_hwndChatInput, nullptr,
		rightLeft, inputTop, rightWidth, INPUT_AREA_HEIGHT, SWP_NOZORDER);
}

// ─── Mode Management ────────────────────────────

std::wstring MainWindow::GetCurrentModeLabel() const
{
	return MODE_LABELS[static_cast<int>(m_currentMode)];
}

void MainWindow::SwitchMode(PracticeModeType newMode)
{
	m_currentMode = newMode;
	m_practiceState = PracticeState::Idle;
	ClearChatLog();
	UpdateModeButtons();
	UpdateModePanel();

	// Reset mode-specific state
	m_shadowSentences.clear();
	m_shadowIndex = 0;
	m_pRolePlaySession.reset();
	m_pPatternSession.reset();
	m_pFreeConvSession.reset();
	m_pronunciationSpeechBuffer.clear();

	SetStatus(L"Mode: " + GetCurrentModeLabel() + L" — Ready");
}

void MainWindow::UpdateModeButtons()
{
	for (int i = 0; i < 6; i++)
	{
		bool isActive = (static_cast<int>(m_currentMode) == i);
		SendMessage(m_hwndModeBtns[i], BM_SETSTYLE,
			isActive ? BS_PUSHBUTTON : BS_PUSHBUTTON | BS_PUSHLIKE, TRUE);
		InvalidateRect(m_hwndModeBtns[i], nullptr, TRUE);
	}

	bool active = (m_practiceState != PracticeState::Idle);
	ShowWindow(m_hwndEndBtn, active ? SW_SHOW : SW_HIDE);
	EnableWindow(m_hwndEndBtn, active);

	if (!active)
	{
		const wchar_t* label = L"▶ Start";
		switch (m_currentMode)
		{
		case PracticeModeType::TextShadowing:        label = L"▶ 开始跟读"; break;
		case PracticeModeType::RolePlay:             label = L"▶ 开始角色扮演"; break;
		case PracticeModeType::SentencePattern:       label = L"▶ 出题"; break;
		case PracticeModeType::FreeConversation:      label = L"▶ 开始对话"; break;
		case PracticeModeType::PronunciationCorrection: label = L"▶ 开始发音练习"; break;
		case PracticeModeType::LearningReport:        label = L"📊 查看报告"; break;
		}
		SetWindowTextW(m_hwndRecordBtn, label);
		EnableWindow(m_hwndRecordBtn, TRUE);
	}
	else if (m_practiceState == PracticeState::Processing)
	{
		SetWindowTextW(m_hwndRecordBtn, L"⏳ 处理中...");
		EnableWindow(m_hwndRecordBtn, FALSE);
	}
	else
	{
		SetWindowTextW(m_hwndRecordBtn, L"✅ 我说完了");
		EnableWindow(m_hwndRecordBtn, TRUE);
	}
}

void MainWindow::UpdateModePanel()
{
	// Show mode-specific introduction in chat log
	std::wstring intro;
	switch (m_currentMode)
	{
	case PracticeModeType::TextShadowing:
		intro = L"📖 【课文跟读模式】逐句跟读课文，AI 评估发音\n"
			L"使用方法：点击「开始跟读」→ 听原声 → 跟着朗读 → AI 逐词评分\n"
			L"评分标记：🟢准确 / 🟡一般 / 🔴需改进";
		break;
	case PracticeModeType::RolePlay:
		intro = L"🎭 【角色扮演模式】和 AI 按教材剧情对话\n"
			L"使用方法：点击「开始角色扮演」→ 选择一个角色 → AI 扮演另一个角色\n"
			L"AI 会实时纠正你的语法错误并引导对话";
		break;
	case PracticeModeType::SentencePattern:
		intro = L"🔄 【句型替换模式】核心句型 + 换关键词造句\n"
			L"使用方法：点击「出题」→ AI 给出句型和关键词 → 你输入造句\n"
			L"AI 判对错 → 给 Hint → 给正确答案";
		break;
	case PracticeModeType::FreeConversation:
		intro = L"💬 【自由对话模式】围绕话题和 AI 自由聊天\n"
			L"使用方法：点击「开始对话」→ 自由回答 AI 的开放问题\n"
			L"AI 引导使用本课词汇，结束后总结用词情况";
		break;
	case PracticeModeType::PronunciationCorrection:
		intro = L"🎙️ 【发音纠错模式】自由说话，AI 评估发音准确度\n"
			L"使用方法：点击「开始发音练习」→ 对着麦克风说一段英语\n"
			L"→ 再点「⏹ 结束并评估」→ AI 评估发音（音素/重音/语调/流利度）";
		break;
	case PracticeModeType::LearningReport:
		intro = L"📊 【学习追踪模式】\n"
			L"自动评分（语法/词汇/发音/流利度）\n"
			L"记录错误模式，自动归纳薄弱环节\n"
			L"学习报告：进步曲线 + 技能分解图 + 里程碑";
		ShowLearningReport();
		break;
	}
	if (!intro.empty())
	{
		AppendToChatLog(intro);
	}
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
		HTREEITEM hBook = TreeView_InsertItem(m_hwndLessonTree, &tviBook);

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

		TreeView_Expand(m_hwndLessonTree, hBook, TVE_EXPAND);
	}

	InvalidateRect(m_hwndLessonTree, nullptr, TRUE);
	UpdateWindow(m_hwndLessonTree);

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

		m_pAiService->ClearHistory();
		ClearChatLog();

		std::wstring ctxMsg = L"📖 Switched to: " + pLesson->bookName +
			L" — Lesson " + std::to_wstring(lessonNumber) +
			L": " + pLesson->title;
		AppendToChatLog(ctxMsg);
		UpdateModePanel();

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
	if (pData->isBook) return;

	SelectLesson(pData->bookId, pData->lessonNumber);
}

// ─── WM_COMMAND ──────────────────────────────────

LRESULT MainWindow::OnCommand(WPARAM wp, LPARAM lp)
{
	int id = LOWORD(wp);
	int code = HIWORD(wp);

	// Mode button clicks
	if (id >= IDC_MODE_BTN_0 && id <= IDC_MODE_BTN_5)
	{
		if (code == BN_CLICKED)
		{
			SwitchMode(static_cast<PracticeModeType>(id - IDC_MODE_BTN_0));
		}
		return 0;
	}

	switch (id)
	{
	case IDM_FILE_EXIT:
		DestroyWindow(m_hwnd);
		return 0;

	case IDM_PRACTICE_START:
	case IDC_RECORD_BTN:
		if (code == BN_CLICKED || code == 0)
		{
			if (m_practiceState != PracticeState::Idle)
			{
				OnSubmitUtterance();          // active → submit this sentence
			}
			else
			{
				StartPractice();              // idle → start session
			}
		}
		return 0;

	case IDC_END_BTN:
		if (code == BN_CLICKED) StopPractice();
		return 0;

	case IDM_PRACTICE_STOP:
		if (code == 0) StopPractice();
		return 0;

	case IDC_PLAY_BTN:
		if (code == BN_CLICKED) ReadLessonAloud();
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
	}

	return 0;
}

// ─── Chat ────────────────────────────────────────

void MainWindow::SendChatMessage()
{
	// Enter key = same as clicking "✅ 我说完了"
	if (m_practiceState != PracticeState::Idle)
	{
		OnSubmitUtterance();
	}
}

void MainWindow::AppendChatMessage(const std::wstring& role, const std::wstring& content)
{
	std::wstring prefix;
	if (role == L"You" || role == L"user")
		prefix = L"🧑 You: ";
	else if (role == L"assistant" || role == L"AI")
		prefix = L"🤖 AI: ";
	else
		prefix = role + L": ";

	AppendToChatLog(prefix + content);
}

void MainWindow::AppendToChatLog(const std::wstring& text)
{
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

	int count = static_cast<int>(SendMessage(m_hwndChatHistory, LB_GETCOUNT, 0, 0));
	if (count > 0)
	{
		SendMessage(m_hwndChatHistory, LB_SETTOPINDEX, count - 1, 0);
	}
}

void MainWindow::ClearChatLog()
{
	SendMessage(m_hwndChatHistory, LB_RESETCONTENT, 0, 0);
}

// ─── Practice Entry Point ────────────────────────

void MainWindow::StartPractice()
{
	if (!m_pCurrentLesson)
	{
		MessageBoxW(m_hwnd, L"Please select a lesson first.",
			L"No Lesson Selected", MB_OK | MB_ICONINFORMATION);
		return;
	}

	m_sessionStartTime = std::chrono::steady_clock::now();

	switch (m_currentMode)
	{
	case PracticeModeType::TextShadowing:
		StartTextShadowing();
		break;
	case PracticeModeType::RolePlay:
		StartRolePlay();
		break;
	case PracticeModeType::SentencePattern:
		StartSentencePattern();
		break;
	case PracticeModeType::FreeConversation:
		StartFreeConversation();
		break;
	case PracticeModeType::PronunciationCorrection:
		if (m_pronunciationSpeechBuffer.empty())
		{
			StartPronunciationCorrection();
		}
		else
		{
			SubmitPronunciationCheck(L"");
		}
		break;
	case PracticeModeType::LearningReport:
		ShowLearningReport();
		break;
	}
}

void MainWindow::StopPractice()
{
	if (m_practiceState == PracticeState::Idle) return;

	m_pSpeechService->StopRecognition();  // stop microphone
	m_pSpeechService->StopSpeaking();     // stop TTS
	m_pAiService->Cancel();               // kill any in-flight AI request
	m_pAiService->ClearHistory();

	auto makeRec = [&](PracticeModeType mode, SkillScores scores) {
		SessionRecord rec;
		rec.timestamp = std::chrono::system_clock::now();
		rec.mode = mode;
		rec.bookId = m_currentBookId;
		rec.lessonNumber = m_pCurrentLesson ? m_pCurrentLesson->lessonNumber : 0;
		rec.lessonTitle = m_pCurrentLesson ? m_pCurrentLesson->title : L"";
		rec.scores = scores;
		if (m_sessionStartTime.time_since_epoch().count() > 0)
		{
			rec.durationSeconds = (int)std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - m_sessionStartTime).count();
		}
		LearningTracker::Instance().RecordSession(rec);
		};

	switch (m_currentMode)
	{
	case PracticeModeType::TextShadowing:
		AppendToChatLog(L"⏹ 课文跟读已结束 — 已完成 " +
			std::to_wstring(m_shadowIndex) + L"/" +
			std::to_wstring(m_shadowSentences.size()) + L" 句");
		makeRec(PracticeModeType::TextShadowing, { 60.0, 50.0, 70.0, 65.0 });
		break;

	case PracticeModeType::RolePlay:
		AppendToChatLog(L"⏹ 角色扮演已结束");
		makeRec(PracticeModeType::RolePlay, { 65.0, 60.0, 55.0, 65.0 });
		break;

	case PracticeModeType::SentencePattern:
		if (m_pPatternSession)
		{
			AppendToChatLog(L"⏹ 句型替换已结束 — 成绩: " +
				std::to_wstring(m_pPatternSession->correctCount) + L"/" +
				std::to_wstring(m_pPatternSession->totalAttempts) + L" 正确");
		}
		else
		{
			AppendToChatLog(L"⏹ 句型替换已结束");
		}
		makeRec(PracticeModeType::SentencePattern, { 60.0, 55.0, 50.0, 60.0 });
		break;

	case PracticeModeType::FreeConversation:
		AppendToChatLog(L"⏹ 自由对话已结束");
		makeRec(PracticeModeType::FreeConversation, { 70.0, 65.0, 60.0, 70.0 });
		break;

	case PracticeModeType::PronunciationCorrection:
		// This mode needs AI evaluation — submit what we have
		if (!m_pronunciationSpeechBuffer.empty())
		{
			SubmitPronunciationCheck(L"");
			return;  // wait for AI evaluation result
		}
		// check typed input as fallback
		{
			int len = GetWindowTextLengthW(m_hwndChatInput);
			if (len > 0)
			{
				std::wstring input(len + 1, L'\0');
				GetWindowTextW(m_hwndChatInput, &input[0], len + 1);
				input.resize(len);
				SetWindowTextW(m_hwndChatInput, L"");
				if (!input.empty())
				{
					m_pronunciationSpeechBuffer = input;
					SubmitPronunciationCheck(L"");
					return;
				}
			}
		}
		AppendToChatLog(L"⏹ 发音纠错已结束（无语音输入）");
		break;

	case PracticeModeType::LearningReport:
		break;
	}

	// ── common reset ──────────────────────────
	m_practiceState = PracticeState::Idle;
	UpdateModeButtons();
	SetWindowTextW(m_hwndChatInput, L"");
	SetStatus(L"Practice ended — Ready");
}

// ─── Mode 1: Text Shadowing ──────────────────────

void MainWindow::StartTextShadowing()
{
	if (!m_pCurrentLesson) return;

	// Split dialogue into sentences
	m_shadowSentences.clear();
	std::wstring dialogue = m_pCurrentLesson->dialogueText;
	size_t start = 0;
	for (size_t i = 0; i < dialogue.length(); i++)
	{
		if (dialogue[i] == L'.' || dialogue[i] == L'?' || dialogue[i] == L'!')
		{
			std::wstring sentence = dialogue.substr(start, i - start + 1);
			// Trim
			while (!sentence.empty() && iswspace(sentence.front())) sentence.erase(0, 1);
			while (!sentence.empty() && iswspace(sentence.back())) sentence.pop_back();
			if (!sentence.empty() && sentence.length() > 1)
			{
				m_shadowSentences.push_back(sentence);
			}
			start = i + 1;
		}
	}
	// Catch remaining
	if (start < dialogue.length())
	{
		std::wstring remainder = dialogue.substr(start);
		while (!remainder.empty() && iswspace(remainder.front())) remainder.erase(0, 1);
		while (!remainder.empty() && iswspace(remainder.back())) remainder.pop_back();
		if (!remainder.empty()) m_shadowSentences.push_back(remainder);
	}

	// Also split on \n
	std::vector<std::wstring> splitByNewline;
	for (auto& s : m_shadowSentences)
	{
		size_t nlPos = 0, prev = 0;
		while ((nlPos = s.find(L'\n', prev)) != std::wstring::npos)
		{
			std::wstring sub = s.substr(prev, nlPos - prev);
			while (!sub.empty() && iswspace(sub.front())) sub.erase(0, 1);
			while (!sub.empty() && iswspace(sub.back())) sub.pop_back();
			if (!sub.empty()) splitByNewline.push_back(sub);
			prev = nlPos + 1;
		}
		std::wstring sub = s.substr(prev);
		while (!sub.empty() && iswspace(sub.front())) sub.erase(0, 1);
		while (!sub.empty() && iswspace(sub.back())) sub.pop_back();
		if (!sub.empty()) splitByNewline.push_back(sub);
	}
	m_shadowSentences = splitByNewline;

	if (m_shadowSentences.empty())
	{
		AppendToChatLog(L"⚠️ No sentences found in the dialogue for shadowing.");
		return;
	}

	m_shadowIndex = 0;
	m_practiceState = PracticeState::Listening;

	AppendToChatLog(L"📖 【课文跟读】共 " + std::to_wstring(m_shadowSentences.size()) + L" 句。开始第 1 句：");
	AppendToChatLog(L"📣 Original: " + m_shadowSentences[0]);

	// Speak the first sentence
	m_pSpeechService->SpeakAsync(m_shadowSentences[0], m_hwnd);

	m_practiceState = PracticeState::Listening;
	UpdateModeButtons();
	SetStatus(L"🔊 Listen to the original, then speak and click [我说完了]");
}

void MainWindow::ProcessPronunciationResult(const std::wstring& aiResponse)
{
	AppendToChatLog(L"📊 AI Pronunciation Feedback: " + aiResponse);

	m_shadowIndex++;
	if (m_shadowIndex < m_shadowSentences.size())
	{
		AppendToChatLog(L"📣 Next sentence (" + std::to_wstring(m_shadowIndex + 1) +
			L"/" + std::to_wstring(m_shadowSentences.size()) + L"): " +
			m_shadowSentences[m_shadowIndex]);
		m_pSpeechService->SpeakAsync(m_shadowSentences[m_shadowIndex], m_hwnd);
		m_practiceState = PracticeState::Listening;
		UpdateModeButtons();
		SetStatus(L"🔊 Listen, then speak and click [我说完了]");
	}
	else
	{
		AppendToChatLog(L"✅ All sentences completed! Great practice!");
		m_practiceState = PracticeState::Idle;
		UpdateModeButtons();

		SessionRecord rec;
		rec.timestamp = std::chrono::system_clock::now();
		rec.mode = PracticeModeType::TextShadowing;
		rec.bookId = m_currentBookId;
		rec.lessonNumber = m_pCurrentLesson ? m_pCurrentLesson->lessonNumber : 0;
		rec.lessonTitle = m_pCurrentLesson ? m_pCurrentLesson->title : L"";
		rec.scores = SkillScores{ 60.0, 50.0, 70.0, 65.0 };
		if (m_sessionStartTime.time_since_epoch().count() > 0)
		{
			rec.durationSeconds = (int)std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - m_sessionStartTime).count();
		}
		LearningTracker::Instance().RecordSession(rec);
		SetStatus(L"Text shadowing complete!");
	}
}

// ─── Mode 2: Role Play ───────────────────────────

void MainWindow::StartRolePlay()
{
	if (!m_pCurrentLesson) return;

	std::wstring scenario = m_pCurrentLesson->dialogueText;
	std::wstring lessonCtx = LessonManager::Instance().BuildPracticeContext(*m_pCurrentLesson);

	// Determine characters from the dialogue
	std::wstring aiChar = L"Character A";
	std::wstring userChar = L"Character B";

	// Try to extract from dialogue format "A: ..." / "B: ..."
	if (scenario.find(L"A:") != std::wstring::npos && scenario.find(L"B:") != std::wstring::npos)
	{
		aiChar = L"Character B (responder)";
		userChar = L"Character A (initiator)";
	}

	m_practiceState = PracticeState::Responding;
	AppendToChatLog(L"🎭 Starting role play based on: " + m_pCurrentLesson->title);
	AppendToChatLog(L"   You play: " + userChar);
	AppendToChatLog(L"   AI plays: " + aiChar);

	m_pAiService->StartRolePlay(scenario, userChar, aiChar, lessonCtx);

	m_practiceState = PracticeState::Processing;
	UpdateModeButtons();
	SetStatus(L"🎭 Role play in progress...");
}

void MainWindow::ProcessRolePlayResponse(const std::wstring& aiResponse)
{
	AppendChatMessage(L"🎭 AI", aiResponse);
	m_pSpeechService->SpeakAsync(aiResponse, m_hwnd);
	m_practiceState = PracticeState::Responding;
	UpdateModeButtons();
	SetStatus(L"🎭 Your turn! Speak and click [我说完了]");
}

// ─── Mode 3: Sentence Pattern ────────────────────

void MainWindow::StartSentencePattern()
{
	if (!m_pCurrentLesson) return;

	m_pPatternSession = std::make_unique<PatternSession>();
	m_pPatternSession->corePattern = m_pCurrentLesson->keyGrammar;
	m_pPatternSession->explanation = m_pCurrentLesson->practicePrompt;
	m_pPatternSession->correctCount = 0;
	m_pPatternSession->totalAttempts = 0;

	std::wstring lessonCtx = LessonManager::Instance().BuildPracticeContext(*m_pCurrentLesson);

	AppendToChatLog(L"🔄 Generating sentence pattern exercise...");
	AppendToChatLog(L"   Pattern: " + m_pPatternSession->corePattern);

	m_pAiService->GeneratePatternExercise(m_pPatternSession->corePattern, lessonCtx);

	m_practiceState = PracticeState::Processing;
	UpdateModeButtons();
	SetStatus(L"🔄 Generating exercise...");
}

void MainWindow::ProcessPatternResult(const std::wstring& aiResponse)
{
	AppendToChatLog(L"📝 Result: " + aiResponse);

	// Check if AI response indicates correct
	if (aiResponse.find(L"\"correct\":true") != std::wstring::npos ||
		aiResponse.find(L"correct") != std::wstring::npos)
	{
		m_pPatternSession->correctCount++;
	}

	// Generate next exercise
	std::wstring lessonCtx = m_pCurrentLesson
		? LessonManager::Instance().BuildPracticeContext(*m_pCurrentLesson) : L"";
	m_pAiService->GeneratePatternExercise(m_pPatternSession->corePattern, lessonCtx);

	m_practiceState = PracticeState::Processing;
	UpdateModeButtons();
	SetStatus(L"Score: " + std::to_wstring(m_pPatternSession->correctCount) + L"/" +
		std::to_wstring(m_pPatternSession->totalAttempts));
}

// ─── Mode 4: Free Conversation ────────────────────

void MainWindow::StartFreeConversation()
{
	if (!m_pCurrentLesson) return;

	m_pFreeConvSession = std::make_unique<FreeConversationSession>();
	m_pFreeConvSession->topic = m_pCurrentLesson->title;
	for (auto& v : m_pCurrentLesson->vocabulary)
	{
		m_pFreeConvSession->targetVocab.push_back(v.word);
	}

	std::wstring lessonCtx = LessonManager::Instance().BuildPracticeContext(*m_pCurrentLesson);

	AppendToChatLog(L"💬 Starting free conversation about: " + m_pFreeConvSession->topic);
	AppendToChatLog(L"   Target vocabulary: " +
		[&]() { std::wstring s; for (auto& v : m_pFreeConvSession->targetVocab)
	{
		if (!s.empty()) s += L", "; s += v;
	} return s; }());

	m_pAiService->StartFreeConversation(m_pFreeConvSession->topic,
		m_pFreeConvSession->targetVocab, lessonCtx);

	m_practiceState = PracticeState::Processing;
	UpdateModeButtons();
	SetStatus(L"💬 Free conversation in progress...");
}

void MainWindow::EndFreeConversation()
{
	AppendToChatLog(L"📊 Ending conversation, generating vocabulary summary...");
	m_pAiService->EndFreeConversation();
	SetStatus(L"📊 Generating summary...");
}

void MainWindow::ProcessFreeConvResult(const std::wstring& aiResponse)
{
	AppendChatMessage(L"AI", aiResponse);
	m_pSpeechService->SpeakAsync(aiResponse, m_hwnd);
	m_practiceState = PracticeState::Responding;
	UpdateModeButtons();
	SetStatus(L"💬 Listen, then speak your response and click [我说完了]");
}

// ─── Mode 5: Pronunciation Correction ──────────────

void MainWindow::StartPronunciationCorrection()
{
	m_pronunciationSpeechBuffer.clear();
	m_practiceState = PracticeState::Listening;
	m_sessionStartTime = std::chrono::steady_clock::now();

	AppendToChatLog(L"🎙️ 【发音纠错】请对着麦克风说一段英语。");
	AppendToChatLog(L"   可以是自由说话，也可以朗读当前课文中的句子。");
	AppendToChatLog(L"   说完后点击「⏹ 结束并评估」，AI 将评估你的发音。");
	AppendToChatLog(L"   💡 提示：如果麦克风不可用，也可以在下方输入框输入你说的内容。");

	// Start speech recognition if available
	if (m_pSpeechService)
	{
		if (!m_pSpeechService->StartRecognition(m_hwnd))
		{
			std::wstring lang = m_pSpeechService->GetRecognizerLanguage();
			AppendToChatLog(L"⚠️ 语音识别启动失败 (引擎: " + lang +
				L")。请检查 Windows 麦克风权限和语音识别组件。仍可在输入框输入文本后提交。");
		}
	}

	m_practiceState = PracticeState::Listening;
	UpdateModeButtons();
	SetStatus(L"🎙️ Speak, then click [我说完了] or [结束练习] to evaluate");
}

void MainWindow::SubmitPronunciationCheck(const std::wstring& speech)
{
	if (m_pSpeechService)
		m_pSpeechService->StopRecognition();

	// Collect recognized text
	std::wstring sttText;
	if (m_pSpeechService) sttText = m_pSpeechService->PopRecognizedText();

	// The live STT result is mirrored into the input box. If the user edited it,
	// the input box is the source of truth for the submitted utterance.
	int len = GetWindowTextLengthW(m_hwndChatInput);
	if (len > 0)
	{
		std::wstring input(len + 1, L'\0');
		GetWindowTextW(m_hwndChatInput, &input[0], len + 1);
		input.resize(len);
		SetWindowTextW(m_hwndChatInput, L"");
		sttText = input;
	}
	if (!sttText.empty())
	{
		if (!m_pronunciationSpeechBuffer.empty()) m_pronunciationSpeechBuffer += L" ";
		m_pronunciationSpeechBuffer += sttText;
	}

	if (m_pronunciationSpeechBuffer.empty())
	{
		MessageBoxW(m_hwnd,
			L"未检测到语音输入。请对着麦克风说话后点击 [结束练习]，或在输入框打字后按 Enter。",
			L"No Speech Input", MB_OK | MB_ICONINFORMATION);
		m_practiceState = PracticeState::Idle;
		UpdateModeButtons();
		return;
	}

	std::wstring textToEval = m_pronunciationSpeechBuffer;
	AppendToChatLog(L"🎙️ Submitted for pronunciation evaluation:\n" + textToEval);
	m_pronunciationSpeechBuffer.clear();

	std::wstring reference = m_pCurrentLesson ? m_pCurrentLesson->dialogueText : L"";
	m_pAiService->EvaluateFreeSpeechPronunciation(textToEval, reference);

	m_practiceState = PracticeState::Processing;
	UpdateModeButtons();
	SetStatus(L"🎙️ Evaluating pronunciation...");
}

void MainWindow::ProcessPronunciationCorrectionResult(const std::wstring& aiResponse)
{
	AppendToChatLog(L"🎙️ Pronunciation Evaluation Results:");
	AppendToChatLog(aiResponse);

	m_practiceState = PracticeState::Idle;
	m_pronunciationSpeechBuffer.clear();
	UpdateModeButtons();
	SetStatus(L"Pronunciation evaluation complete!");

	// Record session
	SessionRecord rec;
	rec.mode = PracticeModeType::PronunciationCorrection;
	rec.bookId = m_currentBookId;
	rec.lessonNumber = m_pCurrentLesson ? m_pCurrentLesson->lessonNumber : 0;
	rec.lessonTitle = m_pCurrentLesson ? m_pCurrentLesson->title : L"";
	rec.scores = SkillScores{ 50.0, 50.0, 70.0, 60.0 };  // emphasis on pronunciation
	rec.durationSeconds = (int)std::chrono::duration_cast<std::chrono::seconds>(
		std::chrono::steady_clock::now() - m_sessionStartTime).count();
	rec.aiFeedback = aiResponse;
	LearningTracker::Instance().RecordSession(rec);
}

// ─── Mode 6: Learning Report ──────────────────────

void MainWindow::ShowLearningReport()
{
	auto& tracker = LearningTracker::Instance();
	auto& profile = tracker.GetProfile();

	ClearChatLog();
	AppendToChatLog(L"📊 ═══════════════════════════════════");
	AppendToChatLog(L"   📈 SPEAKCRAFT LEARNING REPORT");
	AppendToChatLog(L"═══════════════════════════════════");

	AppendToChatLog(L"");
	AppendToChatLog(L"👤 User: " + profile.displayName);

	auto now = std::chrono::system_clock::now();
	auto created = std::chrono::system_clock::to_time_t(profile.createdDate);
	std::tm tm;
	localtime_s(&tm, &created);
	wchar_t dateBuf[64];
	wcsftime(dateBuf, 64, L"%Y-%m-%d", &tm);
	AppendToChatLog(L"📅 Learning since: " + std::wstring(dateBuf));

	AppendToChatLog(L"");
	AppendToChatLog(L"── 📊 Overall Statistics ──");
	AppendToChatLog(L"   Total Sessions: " + std::to_wstring(profile.totalSessions));
	AppendToChatLog(L"   Total Minutes: " + std::to_wstring(profile.totalMinutes));
	AppendToChatLog(L"   Current Streak: " + std::to_wstring(profile.currentStreak) + L" days 🔥");
	AppendToChatLog(L"   Longest Streak: " + std::to_wstring(profile.longestStreak) + L" days");

	AppendToChatLog(L"");
	AppendToChatLog(L"── 🎯 Skill Breakdown ──");
	AppendToChatLog(L"   Grammar:      " + std::to_wstring((int)profile.averageScores.grammar) + L"/100 " +
		GetBarString(profile.averageScores.grammar));
	AppendToChatLog(L"   Vocabulary:   " + std::to_wstring((int)profile.averageScores.vocabulary) + L"/100 " +
		GetBarString(profile.averageScores.vocabulary));
	AppendToChatLog(L"   Pronunciation:" + std::to_wstring((int)profile.averageScores.pronunciation) + L"/100 " +
		GetBarString(profile.averageScores.pronunciation));
	AppendToChatLog(L"   Fluency:      " + std::to_wstring((int)profile.averageScores.fluency) + L"/100 " +
		GetBarString(profile.averageScores.fluency));
	AppendToChatLog(L"   ───────────────────────");
	double overall = tracker.GetOverallProgress();
	AppendToChatLog(L"   ★ Overall:     " + std::to_wstring((int)overall) + L"/100 " +
		GetBarString(overall));

	AppendToChatLog(L"");
	AppendToChatLog(L"── ⚠️ Weak Areas (Top Errors) ──");
	auto weakAreas = tracker.GetWeakAreas();
	if (weakAreas.empty())
	{
		AppendToChatLog(L"   No error data yet. Keep practicing!");
	}
	else
	{
		for (size_t i = 0; i < weakAreas.size(); i++)
		{
			AppendToChatLog(L"   " + std::to_wstring(i + 1) + L". " + weakAreas[i]);
		}
	}

	AppendToChatLog(L"");
	AppendToChatLog(L"── 🏆 Milestones ──");
	if (profile.milestones.empty())
	{
		AppendToChatLog(L"   No milestones yet. Complete more sessions!");
	}
	else
	{
		for (auto& m : profile.milestones)
		{
			auto mt = std::chrono::system_clock::to_time_t(m.date);
			localtime_s(&tm, &mt);
			wcsftime(dateBuf, 64, L"%Y-%m-%d", &tm);
			AppendToChatLog(L"   🏅 " + m.title + L" — " + m.description +
				L" (" + std::wstring(dateBuf) + L")");
		}
	}

	AppendToChatLog(L"");
	AppendToChatLog(L"── 📈 Progress History (Last 10) ──");
	auto& history = profile.progressHistory;
	size_t histStart = history.size() > 10 ? history.size() - 10 : 0;
	for (size_t i = histStart; i < history.size(); i++)
	{
		double avg = (history[i].second.grammar + history[i].second.vocabulary +
			history[i].second.pronunciation + history[i].second.fluency) / 4.0;
		AppendToChatLog(L"   " + history[i].first + L": " +
			std::to_wstring((int)avg) + L"/100 " + GetBarString(avg));
	}

	AppendToChatLog(L"");
	AppendToChatLog(L"═══════════════════════════════════");
	AppendToChatLog(L"💪 Keep practicing! Every session counts!");

	SetStatus(L"📊 Learning report displayed");
}

// ─── Helper: progress bar string ──────────────────
static std::wstring GetBarString(double score)
{
	std::wstring bar;
	int blocks = (int)(score / 10.0);
	for (int i = 0; i < 10; i++)
	{
		bar += (i < blocks) ? L"█" : L"░";
	}
	return bar;
}

// ─── Read Aloud ───────────────────────────────────


// ─── Per-Sentence Submit ──────────────────────────
// Called when user clicks "✅ 我说完了" — sends current utterance to AI

void MainWindow::OnSubmitUtterance()
{
	// ── Read recognized text WITHOUT killing recognition ──
	// Recognition stays alive continuously; we just poll the buffer
	std::wstring sttText;
	if (m_pSpeechService) sttText = m_pSpeechService->PopRecognizedText();

	std::wstring utterance = sttText;
	int len = GetWindowTextLengthW(m_hwndChatInput);
	if (len > 0)
	{
		std::wstring typed(len + 1, L'\0');
		GetWindowTextW(m_hwndChatInput, &typed[0], len + 1);
		typed.resize(len);
		SetWindowTextW(m_hwndChatInput, L"");
		utterance = typed;
	}
	if (utterance.empty() && !m_pronunciationSpeechBuffer.empty())
	{
		utterance = m_pronunciationSpeechBuffer;
		m_pronunciationSpeechBuffer.clear();
	}
	if (utterance.empty())
	{
		if (m_pSpeechService && !m_pSpeechService->IsRecognizing())
		{
			m_pSpeechService->StartRecognition(m_hwnd);
			SetStatus(L"⚠️ 语音识别刚重新启动，请再说一遍后点击 [我说完了]。也可以直接在输入框打字然后按 Enter。");
		}
		else if (m_pSpeechService && !m_pSpeechService->HasDetectedSound())
		{
			SetStatus(L"⚠️ 语音识别已启动，但没有收到麦克风声音。请检查 Windows 麦克风权限、默认输入设备和输入音量。");
		}
		else
		{
			// Sound was detected but SAPI produced no usable recognition text.
			// Show a detailed dialog with the recognizer language so the user can diagnose.
			std::wstring lang = m_pSpeechService ? m_pSpeechService->GetRecognizerLanguage() : L"Unknown";
			std::wstring msg = L"已收到麦克风声音，但语音识别引擎没有返回文字结果。\n\n"
				L"当前识别引擎: " + lang + L"\n\n"
				L"常见原因:\n"
				L"  • 未安装英语语音识别语言包\n"
				L"    → 请前往 Windows 设置 → 时间和语言 → 语言和区域\n"
				L"    → 添加语言 → English (United States) → 安装\"语音识别\"功能\n"
				L"  • 当前识别引擎不是英语，无法识别英语语音\n"
				L"  • 麦克风输入音量过低或环境噪声过大\n\n"
				L"解决方法: 在下方输入框输入你说的英语内容后按 Enter 提交。";

			MessageBoxW(m_hwnd, msg.c_str(),
				L"语音识别失败", MB_OK | MB_ICONWARNING);

			// Restart recognition to give it another chance with fresh state
			if (m_pSpeechService)
			{
				m_pSpeechService->StopRecognition();
				if (!m_pSpeechService->StartRecognition(m_hwnd))
				{
					AppendToChatLog(L"⚠️ 语音识别重新启动失败，请在输入框打字后提交。");
				}
				else
				{
					AppendToChatLog(L"🔄 语音识别已重新启动 (引擎: " +
						m_pSpeechService->GetRecognizerLanguage() +
						L")，请再试一次。或在输入框打字后按 Enter 提交。");
				}
			}
			SetStatus(L"⚠️ 语音识别已重新启动，请重试或打字提交。");
		}
		return;
	}

	AppendChatMessage(L"You", utterance);

	m_practiceState = PracticeState::Processing;
	UpdateModeButtons();
	SetStatus(L"⏳ AI is evaluating...");

	switch (m_currentMode)
	{
	case PracticeModeType::TextShadowing:
		// Send user's spoken text + reference sentence for pronunciation eval
		if (m_shadowIndex < m_shadowSentences.size())
		{
			m_pAiService->EvaluatePronunciation(utterance,
				m_shadowSentences[m_shadowIndex]);
		}
		break;

	case PracticeModeType::RolePlay:
		m_pAiService->ContinueRolePlay(utterance);
		break;

	case PracticeModeType::SentencePattern:
	{
		auto& s = m_pPatternSession;
		m_pAiService->CheckPatternAnswer(utterance,
			s ? s->corePattern : L"",
			(s && !s->exercises.empty()) ? s->exercises.back().keyword : L"",
			(s && !s->exercises.empty()) ? s->exercises.back().expectedAnswer : L"");
		if (s) s->totalAttempts++;
	}
	break;

	case PracticeModeType::FreeConversation:
		m_pAiService->ContinueFreeConversation(utterance);
		break;

	case PracticeModeType::PronunciationCorrection:
		m_pronunciationSpeechBuffer += utterance + L" ";
		m_practiceState = PracticeState::Listening;
		UpdateModeButtons();
		if (m_pSpeechService && !m_pSpeechService->StartRecognition(m_hwnd))
		{
			AppendToChatLog(L"⚠️ 语音识别未能重新启动，可继续在输入框输入文本。");
		}
		SetStatus(L"🎙️ Added. Keep speaking, click [结束练习] to evaluate.");
		break;

	default:
	{
		std::wstring ctx = m_pCurrentLesson
			? LessonManager::Instance().BuildPracticeContext(*m_pCurrentLesson) : L"";
		m_pAiService->SendMessage(utterance, ctx);
	}
	break;
	}
}

void MainWindow::ReadLessonAloud()
{
	if (!m_pCurrentLesson)
	{
		MessageBoxW(m_hwnd, L"Please select a lesson first.",
			L"No Lesson Selected", MB_OK | MB_ICONINFORMATION);
		return;
	}

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

// ─── AI Response Handler (mode-aware) ─────────────

LRESULT MainWindow::OnAiResponse(WPARAM wp, LPARAM lp)
{
	std::wstring* pMsg = reinterpret_cast<std::wstring*>(wp);
	if (!pMsg) return 0;

	bool success = (lp == 1);

	if (success && pMsg->find(L"ERROR:") != 0)
	{
		std::wstring modeTag = m_pAiService->GetCurrentMode();

		if (modeTag == L"text_shadowing")
		{
			ProcessPronunciationResult(*pMsg);
		}
		else if (modeTag == L"role_play")
		{
			ProcessRolePlayResponse(*pMsg);
		}
		else if (modeTag == L"sentence_pattern")
		{
			ProcessPatternResult(*pMsg);
		}
		else if (modeTag == L"free_conversation")
		{
			ProcessFreeConvResult(*pMsg);
		}
		else if (modeTag == L"pronunciation_correction")
		{
			ProcessPronunciationCorrectionResult(*pMsg);
		}
		else
		{
			// Default: general chat response
			AppendChatMessage(L"AI", *pMsg);
			m_pSpeechService->SpeakAsync(*pMsg, m_hwnd);
			m_practiceState = PracticeState::Responding;
			UpdateModeButtons();
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
	bool shouldListen = (m_practiceState != PracticeState::Idle &&
		m_practiceState != PracticeState::Processing);

	if (shouldListen)
	{
		// Start recognition only if not already running
		if (m_pSpeechService && !m_pSpeechService->IsRecognizing())
		{
			if (!m_pSpeechService->StartRecognition(m_hwnd))
			{
				std::wstring lang = m_pSpeechService->GetRecognizerLanguage();
				AppendToChatLog(L"⚠️ 语音识别启动失败 (引擎: " + lang +
					L")。请检查 Windows 麦克风权限和语音识别组件。仍可在输入框输入文本后提交。");
			}
		}
		m_practiceState = PracticeState::Responding;
		UpdateModeButtons();
		SetStatus(L"🎤 Speak now, then click [我说完了]");
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
			wchar_t buf[4096];
			auto& cfg = ConfigManager::Instance();
			GetDlgItemTextW(hDlg, IDC_API_ENDPOINT, buf, 4096);
			cfg.SetApiEndpoint(buf);
			GetDlgItemTextW(hDlg, IDC_API_KEY, buf, 4096);
			cfg.SetApiKey(buf);
			GetDlgItemTextW(hDlg, IDC_MODEL_NAME, buf, 4096);
			cfg.SetModelName(buf);

			AIService tempService;
			HWND hwndMain = GetParent(hDlg);
			tempService.Initialize(hwndMain);
			std::wstring error;
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

		// Populate recognizer list (first entry = "Auto (prefer English)")
		HWND hRecoList = GetDlgItem(hDlg, IDC_RECOGNIZER_LIST);
		SendMessageW(hRecoList, LB_ADDSTRING, 0,
			reinterpret_cast<LPARAM>(L"🔄 Auto (prefer English, fallback to default)"));
		auto recognizers = tempSvc.GetAvailableRecognizers();
		for (auto& r : recognizers)
		{
			SendMessageW(hRecoList, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(r.c_str()));
		}
		// Select current recognizer preference
		std::wstring curReco = cfg.GetRecognizerToken();
		if (curReco.empty())
		{
			SendMessage(hRecoList, LB_SETCURSEL, 0, 0);   // "Auto"
		}
		else
		{
			LRESULT idx = SendMessageW(hRecoList, LB_FINDSTRINGEXACT, 0,
				reinterpret_cast<LPARAM>(curReco.c_str()));
			if (idx != LB_ERR)
				SendMessage(hRecoList, LB_SETCURSEL, idx, 0);
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

			// Save recognizer preference
			HWND hRecoList = GetDlgItem(hDlg, IDC_RECOGNIZER_LIST);
			int recoIdx = (int)SendMessage(hRecoList, LB_GETCURSEL, 0, 0);
			if (recoIdx == 0 || recoIdx == LB_ERR)
			{
				cfg.SetRecognizerToken(L"");   // "Auto" → empty = use English-preference logic
			}
			else
			{
				wchar_t buf[256] = {};
				SendMessageW(hRecoList, LB_GETTEXT, recoIdx, reinterpret_cast<LPARAM>(buf));
				cfg.SetRecognizerToken(buf);
			}

			// Apply recognizer preference to the live SpeechService
			if (g_pMainWindow)
			{
				auto* pSpeech = g_pMainWindow->GetSpeechService();
				if (pSpeech)
				{
					pSpeech->SetPreferredRecognizer(cfg.GetRecognizerToken());
				}
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
	LearningTracker::Instance().Save();
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
