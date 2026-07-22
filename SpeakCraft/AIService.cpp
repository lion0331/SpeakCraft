#include "AIService.h"
#include "ConfigManager.h"
#include "Resource.h"

#pragma comment(lib, "winhttp.lib")

AIService::AIService()
{
	m_hSession = WinHttpOpen(L"SpeakCraft/1.0",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME,
		WINHTTP_NO_PROXY_BYPASS, 0);
}

AIService::~AIService()
{
	Cancel();
	if (m_hSession)
	{
		WinHttpCloseHandle(m_hSession);
		m_hSession = nullptr;
	}
}

void AIService::Initialize(HWND hwndNotify)
{
	m_hwndNotify = hwndNotify;
}

void AIService::Cancel()
{
	m_cancelled.store(true);
	if (m_workerThread && m_workerThread->joinable())
	{
		m_workerThread->join();
	}
	m_workerThread.reset();
	m_processing.store(false);
	m_cancelled.store(false);
}

void AIService::ClearHistory()
{
	std::lock_guard<std::mutex> lock(m_historyMutex);
	m_history.clear();
}

bool AIService::SendMessage(const std::wstring& userMessage,
	const std::wstring& systemContext)
{
	if (m_processing.load()) return false;

	// Add user message to history
	{
		std::lock_guard<std::mutex> lock(m_historyMutex);
		m_history.push_back({ L"user", userMessage,
							 std::chrono::system_clock::now() });
	}

	std::wstring body = BuildChatRequestBody(userMessage, systemContext);
	m_processing.store(true);
	m_cancelled.store(false);

	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}


bool AIService::TestConnection(std::wstring& outError)
{
	auto& cfg = ConfigManager::Instance();
	if (cfg.GetApiKey().empty())
	{
		outError = L"API key is not configured. Please set it in Settings.";
		return false;
	}

	// Simple test: send a minimal request
	std::wstring testBody = L"{"
		L"\"model\":\"" + cfg.GetModelName() + L"\","
		L"\"messages\":[{\"role\":\"user\",\"content\":\"Say hello in one word.\"}],"
		L"\"max_tokens\":10"
		L"}";

	bool success = false;
	std::wstring response = SendHttpRequest(testBody, success);

	if (!success)
	{
		outError = response.empty() ? L"Connection failed. Check your endpoint and network." : response;
		return false;
	}

	return true;
}

// ─── Private Helpers ─────────────────────────────

void AIService::ProcessRequest(std::wstring requestBody)
{
	bool success = false;
	std::wstring response = SendHttpRequest(requestBody, success);

	if (m_cancelled.load())
	{
		m_processing.store(false);
		return;
	}

	std::wstring* pResult = new std::wstring();
	if (success)
	{
		*pResult = ParseResponseContent(response);
	}
	else
	{
		*pResult = L"ERROR: " + response;
	}

	// Add to history
	if (success && !pResult->empty())
	{
		std::lock_guard<std::mutex> lock(m_historyMutex);
		m_history.push_back({ L"assistant", *pResult,
							 std::chrono::system_clock::now() });
	}

	m_processing.store(false);

	if (m_hwndNotify)
	{
		PostMessage(m_hwndNotify, WM_AI_RESPONSE,
			reinterpret_cast<WPARAM>(pResult),
			success ? 1 : 0);
	}
	else
	{
		delete pResult;
	}
}

std::wstring AIService::BuildChatRequestBody(const std::wstring& userMessage,
	const std::wstring& systemContext) const
{
	auto& cfg = ConfigManager::Instance();
	std::wstring systemPrompt = systemContext.empty()
		? cfg.GetSystemPrompt() : systemContext;

	std::wstring body = L"{";
	body += L"\"model\":\"" + cfg.GetModelName() + L"\",";
	body += L"\"messages\":[";

	// System message
	body += L"{\"role\":\"system\",\"content\":\"" + JsonEscape(systemPrompt) + L"\"},";

	// History (last 10 messages to keep context manageable)
	{
		std::lock_guard<std::mutex> lock(m_historyMutex);
		size_t start = m_history.size() > 10 ? m_history.size() - 10 : 0;
		for (size_t i = start; i < m_history.size(); i++)
		{
			body += L"{\"role\":\"" + m_history[i].role +
				L"\",\"content\":\"" + JsonEscape(m_history[i].content) + L"\"},";
		}
	}

	body += L"{\"role\":\"user\",\"content\":\"" + JsonEscape(userMessage) + L"\"}";
	body += L"],";
	body += L"\"max_tokens\":1024,";
	body += L"\"temperature\":0.7";
	body += L"}";

	return body;
}

std::wstring AIService::BuildEvalRequestBody(const std::wstring& userSpeech,
	const std::wstring& referenceText) const
{
	auto& cfg = ConfigManager::Instance();
	std::wstring evalPrompt = L"You are a professional English pronunciation evaluator. "
		L"Compare the user's spoken text with the reference text. "
		L"Evaluate pronunciation accuracy, fluency, and provide specific feedback. "
		L"Respond in JSON format: "
		L"{\"overall_score\":85,\"fluency_score\":80,\"accuracy_score\":90,"
		L"\"feedback\":\"Your feedback here\",\"problem_words\":[\"word1\",\"word2\"],"
		L"\"corrected_text\":\"corrected version\"}";

	std::wstring body = L"{";
	body += L"\"model\":\"" + cfg.GetModelName() + L"\",";
	body += L"\"messages\":[";
	body += L"{\"role\":\"system\",\"content\":\"" + JsonEscape(evalPrompt) + L"\"},";
	body += L"{\"role\":\"user\",\"content\":\"Reference: " + JsonEscape(referenceText) +
		L"\\nUser said: " + JsonEscape(userSpeech) + L"\"}";
	body += L"],";
	body += L"\"max_tokens\":512,";
	body += L"\"temperature\":0.3";
	body += L"}";

	return body;
}

std::wstring AIService::SendHttpRequest(const std::wstring& body, bool& success)
{
	success = false;
	auto& cfg = ConfigManager::Instance();

	// Parse URL
	std::wstring url = cfg.GetApiEndpoint();
	URL_COMPONENTS urlComp = { 0 };
	urlComp.dwStructSize = sizeof(urlComp);

	wchar_t hostName[256] = { 0 };
	wchar_t urlPath[1024] = { 0 };
	urlComp.lpszHostName = hostName;
	urlComp.dwHostNameLength = 256;
	urlComp.lpszUrlPath = urlPath;
	urlComp.dwUrlPathLength = 1024;

	if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.length()), 0, &urlComp))
	{
		return L"Failed to parse API endpoint URL.";
	}

	bool useHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

	HINTERNET hConnect = WinHttpConnect(m_hSession, hostName,
		urlComp.nPort, 0);
	if (!hConnect)
	{
		return L"Failed to connect to API server.";
	}

	DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
	HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath,
		nullptr, WINHTTP_NO_REFERER,
		WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
	if (!hRequest)
	{
		WinHttpCloseHandle(hConnect);
		return L"Failed to create HTTP request.";
	}

	// Set headers
	std::wstring headers = L"Content-Type: application/json\r\n";
	std::wstring authHeader = L"Authorization: Bearer " + cfg.GetApiKey();
	headers += authHeader + L"\r\n";

	WinHttpAddRequestHeaders(hRequest, headers.c_str(),
		static_cast<DWORD>(headers.length()),
		WINHTTP_ADDREQ_FLAG_ADD);

	// Send request
	std::string utf8Body;
	int len = WideCharToMultiByte(CP_UTF8, 0, body.c_str(), static_cast<int>(body.length()),
		nullptr, 0, nullptr, nullptr);
	utf8Body.resize(len);
	WideCharToMultiByte(CP_UTF8, 0, body.c_str(), static_cast<int>(body.length()),
		&utf8Body[0], len, nullptr, nullptr);

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
		const_cast<char*>(utf8Body.c_str()),
		static_cast<DWORD>(utf8Body.length()),
		static_cast<DWORD>(utf8Body.length()), 0))
	{
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return L"Failed to send request. Check your API endpoint.";
	}

	if (!WinHttpReceiveResponse(hRequest, nullptr))
	{
		WinHttpCloseHandle(hRequest);
		WinHttpCloseHandle(hConnect);
		return L"Failed to receive response.";
	}

	// Read response
	std::string responseUTF8;
	DWORD bytesAvailable = 0;
	while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
	{
		if (m_cancelled.load()) break;

		std::vector<char> buffer(bytesAvailable + 1);
		DWORD bytesRead = 0;
		if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead))
		{
			responseUTF8.append(buffer.data(), bytesRead);
		}
	}

	WinHttpCloseHandle(hRequest);
	WinHttpCloseHandle(hConnect);

	if (m_cancelled.load())
	{
		return L"Cancelled.";
	}

	// Convert UTF-8 response to wstring
	int wlen = MultiByteToWideChar(CP_UTF8, 0, responseUTF8.c_str(),
		static_cast<int>(responseUTF8.length()), nullptr, 0);
	std::wstring responseW(wlen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, responseUTF8.c_str(),
		static_cast<int>(responseUTF8.length()), &responseW[0], wlen);

	success = true;
	return responseW;
}

std::wstring AIService::ParseResponseContent(const std::wstring& jsonResponse) const
{
	// Extract "content" from OpenAI response:
	// {"choices":[{"message":{"content":"..."}}]}
	std::wstring search = L"\"content\"";
	size_t pos = jsonResponse.find(search);
	if (pos == std::wstring::npos)
	{
		// Check for error
		size_t errPos = jsonResponse.find(L"\"error\"");
		if (errPos != std::wstring::npos)
		{
			// Try to extract error message
			size_t msgPos = jsonResponse.find(L"\"message\"", errPos);
			if (msgPos != std::wstring::npos)
			{
				size_t start = jsonResponse.find(L'"', msgPos + 10);
				if (start != std::wstring::npos)
				{
					size_t end = jsonResponse.find(L'"', start + 1);
					if (end != std::wstring::npos)
					{
						return L"API Error: " + jsonResponse.substr(start + 1, end - start - 1);
					}
				}
			}
		}
		return L"Unexpected API response format.";
	}

	pos = jsonResponse.find(L'"', pos + search.length());
	if (pos == std::wstring::npos) return L"Parse error: no content start";
	size_t start = jsonResponse.find(L'"', pos + 1);
	if (start == std::wstring::npos) return L"Parse error: no content value";

	// Find end — handle escaped quotes
	size_t end = start + 1;
	while (end < jsonResponse.length())
	{
		if (jsonResponse[end] == L'"' && jsonResponse[end - 1] != L'\\')
		{
			break;
		}
		end++;
	}

	std::wstring content = jsonResponse.substr(start + 1, end - start - 1);

	// Unescape JSON: \" → ", \\ → \, \n → newline, etc.
	std::wstring result;
	for (size_t i = 0; i < content.length(); i++)
	{
		if (content[i] == L'\\' && i + 1 < content.length())
		{
			switch (content[i + 1])
			{
			case L'"': result += L'"'; i++; break;
			case L'\\': result += L'\\'; i++; break;
			case L'n': result += L'\n'; i++; break;
			case L'r': result += L'\r'; i++; break;
			case L't': result += L'\t'; i++; break;
			default: result += content[i]; break;
			}
		}
		else
		{
			result += content[i];
		}
	}

	return result;
}

std::wstring AIService::JsonEscape(const std::wstring& s)
{
	std::wstring out;
	for (wchar_t c : s)
	{
		switch (c)
		{
		case L'"': out += L"\\\""; break;
		case L'\\': out += L"\\\\"; break;
		case L'\n': out += L"\\n"; break;
		case L'\r': out += L"\\r"; break;
		case L'\t': out += L"\\t"; break;
		default: out += c; break;
		}
	}
	return out;
}

// ─── 1. Text Shadowing — Pronunciation Evaluation ──

bool AIService::EvaluatePronunciation(const std::wstring& userSpeech,
	const std::wstring& referenceText)
{
	if (m_processing.load()) return false;
	m_currentMode = L"text_shadowing";

	std::wstring body = BuildEvalRequestBody(userSpeech, referenceText);
	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

// ─── 2. Role Play ──────────────────────────────────

bool AIService::StartRolePlay(const std::wstring& scenario,
	const std::wstring& userCharacter,
	const std::wstring& aiCharacter,
	const std::wstring& lessonContext)
{
	if (m_processing.load()) return false;
	m_currentMode = L"role_play";

	std::wstring systemPrompt = L"You are an English conversation partner for role-play practice.\n\n";
	systemPrompt += L"SCENARIO: " + scenario + L"\n";
	systemPrompt += L"Your character: " + aiCharacter + L"\n";
	systemPrompt += L"User's character: " + userCharacter + L"\n\n";
	systemPrompt += L"RULES:\n";
	systemPrompt += L"1. Stay in character at all times. Speak as " + aiCharacter + L".\n";
	systemPrompt += L"2. If the user makes a grammar or pronunciation error, gently correct them by modeling the correct form in your next line.\n";
	systemPrompt += L"3. Keep the conversation natural and engaging.\n";
	systemPrompt += L"4. Guide the user to use vocabulary and grammar from the lesson.\n";
	systemPrompt += L"5. Begin the dialogue with the first line as " + aiCharacter + L".\n\n";
	systemPrompt += L"Lesson context:\n" + lessonContext;

	std::wstring userMsg = L"I'm ready to start the role-play. I am " + userCharacter + L".";

	std::wstring body = BuildRolePlayRequestBody(userMsg, systemPrompt);

	// Add to history
	{
		std::lock_guard<std::mutex> lock(m_historyMutex);
		m_history.push_back({ L"system", systemPrompt, std::chrono::system_clock::now() });
		m_history.push_back({ L"user", userMsg, std::chrono::system_clock::now() });
	}

	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

bool AIService::ContinueRolePlay(const std::wstring& userLine)
{
	if (m_processing.load()) return false;
	m_currentMode = L"role_play";

	std::wstring systemPrompt = L"Continue the role-play dialogue. Stay in character. "
		L"If the user made errors, model the correct form naturally in your response. "
		L"Keep the conversation going and gently guide.";

	std::wstring body = BuildRolePlayRequestBody(userLine, systemPrompt);

	{
		std::lock_guard<std::mutex> lock(m_historyMutex);
		m_history.push_back({ L"user", userLine, std::chrono::system_clock::now() });
	}

	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

// ─── 3. Sentence Pattern Substitution ──────────────

bool AIService::GeneratePatternExercise(const std::wstring& corePattern,
	const std::wstring& lessonContext)
{
	if (m_processing.load()) return false;
	m_currentMode = L"sentence_pattern";

	std::wstring systemPrompt = L"You are an English grammar exercise generator.\n";
	systemPrompt += L"Generate a sentence pattern substitution exercise.\n";
	systemPrompt += L"Core pattern: " + corePattern + L"\n";
	systemPrompt += L"Lesson context: " + lessonContext + L"\n\n";
	systemPrompt += L"Respond with a JSON object containing:\n";
	systemPrompt += L"{\"pattern\":\"<core pattern>\",\"exercise\":{\"keyword\":\"<replacement word>\",";
	systemPrompt += L"\"expected_answer\":\"<correct sentence using the keyword>\",";
	systemPrompt += L"\"hint\":\"<hint if user struggles>\"}}\n";
	systemPrompt += L"Make sure the exercise is appropriate for the lesson level.";

	std::wstring userMsg = L"Generate a sentence pattern exercise for: " + corePattern;

	std::wstring body = BuildPatternCheckBody(userMsg, systemPrompt);
	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

bool AIService::CheckPatternAnswer(const std::wstring& userAnswer,
	const std::wstring& pattern,
	const std::wstring& keyword,
	const std::wstring& expectedAnswer)
{
	if (m_processing.load()) return false;
	m_currentMode = L"sentence_pattern";

	std::wstring systemPrompt = L"You are an English grammar exercise checker.\n";
	systemPrompt += L"Pattern: " + pattern + L"\n";
	systemPrompt += L"Keyword to use: " + keyword + L"\n";
	systemPrompt += L"Expected answer: " + expectedAnswer + L"\n";
	systemPrompt += L"User's answer: " + userAnswer + L"\n\n";
	systemPrompt += L"Evaluate the answer. Respond in JSON:\n";
	systemPrompt += L"{\"correct\":true/false,\"feedback\":\"<explain what's right/wrong>\",";
	systemPrompt += L"\"hint\":\"<hint if wrong>\",\"correct_answer\":\"<the correct sentence>\"}";

	std::wstring body = BuildPatternCheckBody(userAnswer, systemPrompt);
	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

// ─── 4. Free Conversation ──────────────────────────

bool AIService::StartFreeConversation(const std::wstring& topic,
	const std::vector<std::wstring>& targetVocab,
	const std::wstring& lessonContext)
{
	if (m_processing.load()) return false;
	m_currentMode = L"free_conversation";

	std::wstring systemPrompt = L"You are a friendly English conversation partner.\n";
	systemPrompt += L"TOPIC: " + topic + L"\n\n";
	systemPrompt += L"TARGET VOCABULARY (guide the user to use these words):\n";
	for (auto& v : targetVocab) {
		systemPrompt += L"  - " + v + L"\n";
	}
	systemPrompt += L"\nRULES:\n";
	systemPrompt += L"1. Have a natural, engaging conversation about the topic.\n";
	systemPrompt += L"2. Subtly guide the user to use the target vocabulary words.\n";
	systemPrompt += L"3. Ask open-ended questions to encourage longer responses.\n";
	systemPrompt += L"4. Gently correct major grammar errors by modeling correct usage.\n";
	systemPrompt += L"5. Keep responses conversational and encourage the user.\n";
	systemPrompt += L"6. Track which vocabulary words the user uses.\n";
	systemPrompt += L"7. Start by introducing the topic naturally.\n\n";
	systemPrompt += L"Lesson context:\n" + lessonContext;

	std::wstring userMsg = L"Let's talk about: " + topic;

	std::wstring body = BuildRolePlayRequestBody(userMsg, systemPrompt);

	{
		std::lock_guard<std::mutex> lock(m_historyMutex);
		m_history.clear();
		m_history.push_back({ L"system", systemPrompt, std::chrono::system_clock::now() });
		m_history.push_back({ L"user", userMsg, std::chrono::system_clock::now() });
	}

	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

bool AIService::ContinueFreeConversation(const std::wstring& userMessage)
{
	if (m_processing.load()) return false;
	m_currentMode = L"free_conversation";

	std::wstring systemPrompt = L"Continue the free conversation. Be engaging. "
		L"Guide the user to use target vocabulary naturally. Ask follow-up questions.";

	std::wstring body = BuildRolePlayRequestBody(userMessage, systemPrompt);

	{
		std::lock_guard<std::mutex> lock(m_historyMutex);
		m_history.push_back({ L"user", userMessage, std::chrono::system_clock::now() });
	}

	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

bool AIService::EndFreeConversation()
{
	if (m_processing.load()) return false;
	m_currentMode = L"free_conversation_end";

	std::wstring systemPrompt = L"The free conversation has ended. "
		L"Review the conversation and provide a vocabulary usage summary. "
		L"List which target vocabulary words the user used, which they missed, "
		L"and give an overall assessment of their performance. "
		L"Respond in JSON:\n"
		L"{\"vocab_used\":[\"word1\",\"word2\"],\"vocab_missed\":[\"word3\"],"
		L"\"vocab_usage_rate\":75,\"summary\":\"<feedback>\","
		L"\"grammar_score\":80,\"vocab_score\":75,\"fluency_score\":80}";

	std::wstring userMsg = L"Please summarize my vocabulary usage from this conversation.";

	std::wstring body = BuildPatternCheckBody(userMsg, systemPrompt);
	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

// ─── 5. Grammar Correction ─────────────────────────

bool AIService::CorrectGrammar(const std::wstring& userSpeech,
	const std::wstring& topicContext)
{
	if (m_processing.load()) return false;
	m_currentMode = L"grammar_correction";

	std::wstring systemPrompt = L"You are a professional English grammar tutor.\n";
	systemPrompt += L"Analyze the user's speech sentence by sentence and correct all grammar errors.\n\n";
	systemPrompt += L"Topic context: " + topicContext + L"\n\n";
	systemPrompt += L"User's speech:\n\"\"\"\n" + userSpeech + L"\n\"\"\"\n\n";
	systemPrompt += L"Respond in JSON format:\n";
	systemPrompt += L"{\n";
	systemPrompt += L"  \"overall_grammar_score\": 75,\n";
	systemPrompt += L"  \"corrections\": [\n";
	systemPrompt += L"    {\n";
	systemPrompt += L"      \"original\": \"<original sentence>\",\n";
	systemPrompt += L"      \"corrected\": \"<corrected version>\",\n";
	systemPrompt += L"      \"explanation\": \"<grammar point>\",\n";
	systemPrompt += L"      \"error_type\": \"<tense/article/preposition/word_order/etc>\"\n";
	systemPrompt += L"    }\n";
	systemPrompt += L"  ],\n";
	systemPrompt += L"  \"summary\": \"<overall assessment>\",\n";
	systemPrompt += L"  \"strengths\": [\"<what user did well>\"],\n";
	systemPrompt += L"  \"weak_areas\": [\"<what needs improvement>\"]\n";
	systemPrompt += L"}\n";
	systemPrompt += L"Only include sentences that have errors. If a sentence is correct, don't include it.";

	std::wstring body = BuildGrammarCorrectionBody(userSpeech, systemPrompt);
	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

// ─── 6. Learning Report ────────────────────────────

bool AIService::GenerateLearningReport(const std::wstring& statsJson)
{
	if (m_processing.load()) return false;
	m_currentMode = L"learning_report";

	std::wstring body = BuildReportBody(statsJson);
	m_processing.store(true);
	m_cancelled.store(false);
	m_workerThread = std::make_unique<std::thread>(&AIService::ProcessRequest, this, body);
	return true;
}

// ─── Helper Builders ───────────────────────────────

std::wstring AIService::BuildRolePlayRequestBody(const std::wstring& content,
	const std::wstring& systemPrompt) const
{
	auto& cfg = ConfigManager::Instance();
	std::wstring body = L"{";
	body += L"\"model\":\"" + cfg.GetModelName() + L"\",";
	body += L"\"messages\":[";
	body += L"{\"role\":\"system\",\"content\":\"" + JsonEscape(systemPrompt) + L"\"},";

	{
		std::lock_guard<std::mutex> lock(m_historyMutex);
		size_t start = m_history.size() > 8 ? m_history.size() - 8 : 0;
		for (size_t i = start; i < m_history.size(); i++) {
			if (m_history[i].role != L"system") {
				body += L"{\"role\":\"" + m_history[i].role +
					L"\",\"content\":\"" + JsonEscape(m_history[i].content) + L"\"},";
			}
		}
	}

	body += L"{\"role\":\"user\",\"content\":\"" + JsonEscape(content) + L"\"}";
	body += L"],";
	body += L"\"max_tokens\":1024,";
	body += L"\"temperature\":0.8";
	body += L"}";
	return body;
}

std::wstring AIService::BuildPatternCheckBody(const std::wstring& content,
	const std::wstring& systemPrompt) const
{
	auto& cfg = ConfigManager::Instance();
	std::wstring body = L"{";
	body += L"\"model\":\"" + cfg.GetModelName() + L"\",";
	body += L"\"messages\":[";
	body += L"{\"role\":\"system\",\"content\":\"" + JsonEscape(systemPrompt) + L"\"},";
	body += L"{\"role\":\"user\",\"content\":\"" + JsonEscape(content) + L"\"}";
	body += L"],";
	body += L"\"max_tokens\":512,";
	body += L"\"temperature\":0.5";
	body += L"}";
	return body;
}

std::wstring AIService::BuildGrammarCorrectionBody(const std::wstring& content,
	const std::wstring& systemPrompt) const
{
	auto& cfg = ConfigManager::Instance();
	std::wstring body = L"{";
	body += L"\"model\":\"" + cfg.GetModelName() + L"\",";
	body += L"\"messages\":[";
	body += L"{\"role\":\"system\",\"content\":\"" + JsonEscape(systemPrompt) + L"\"},";
	body += L"{\"role\":\"user\",\"content\":\"" + JsonEscape(content) + L"\"}";
	body += L"],";
	body += L"\"max_tokens\":1024,";
	body += L"\"temperature\":0.3";
	body += L"}";
	return body;
}

std::wstring AIService::BuildReportBody(const std::wstring& statsJson) const
{
	auto& cfg = ConfigManager::Instance();
	std::wstring systemPrompt = L"You are a language learning analytics assistant. "
		L"Based on the provided statistics, generate a personalized learning report. "
		L"Include: progress summary, skill breakdown analysis, areas for improvement, "
		L"and encouraging next steps. Respond in a natural, motivational tone.";

	std::wstring body = L"{";
	body += L"\"model\":\"" + cfg.GetModelName() + L"\",";
	body += L"\"messages\":[";
	body += L"{\"role\":\"system\",\"content\":\"" + JsonEscape(systemPrompt) + L"\"},";
	body += L"{\"role\":\"user\",\"content\":\"Learning statistics:\\n" + JsonEscape(statsJson) + L"\"}";
	body += L"],";
	body += L"\"max_tokens\":800,";
	body += L"\"temperature\":0.7";
	body += L"}";
	return body;
}
