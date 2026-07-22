#include "LessonManager.h"
#include <fstream>
#include <sstream>

LessonManager& LessonManager::Instance()
{
	static LessonManager instance;
	return instance;
}

bool LessonManager::Initialize()
{
	m_books.clear();
	CreateBuiltInBooks();

	// Try to load external books from AppData
	wchar_t appData[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
	{
		std::filesystem::path booksDir(appData);
		booksDir /= L"SpeakCraft";
		booksDir /= L"books";

		if (std::filesystem::exists(booksDir))
		{
			for (auto& entry : std::filesystem::directory_iterator(booksDir))
			{
				if (entry.path().extension() == L".json")
				{
					LoadExternalBook(entry.path());
				}
			}
		}
	}

	// Set first lesson as current
	if (!m_books.empty() && !m_books[0].lessons.empty())
	{
		m_pCurrentLesson = &m_books[0].lessons[0];
	}

	return true;
}

const Book* LessonManager::GetBook(const std::wstring& bookId) const
{
	for (auto& book : m_books)
	{
		if (book.id == bookId) return &book;
	}
	return nullptr;
}

const Lesson* LessonManager::GetLesson(const std::wstring& bookId, int lessonNumber) const
{
	for (auto& book : m_books)
	{
		if (book.id == bookId)
		{
			for (auto& lesson : book.lessons)
			{
				if (lesson.lessonNumber == lessonNumber) return &lesson;
			}
		}
	}
	return nullptr;
}

void LessonManager::SetCurrentLesson(const std::wstring& bookId, int lessonNumber)
{
	const Lesson* p = GetLesson(bookId, lessonNumber);
	if (p) m_pCurrentLesson = p;
}

bool LessonManager::LoadExternalBook(const std::filesystem::path& jsonPath)
{
	// Read as raw bytes and convert from UTF-8 (consistent with ConfigManager)
	std::ifstream file(jsonPath, std::ios::binary);
	if (!file.is_open()) return false;

	std::string rawBytes((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	file.close();

	if (rawBytes.empty()) return false;

	// Convert UTF-8 → wstring
	int len = MultiByteToWideChar(CP_UTF8, 0, rawBytes.c_str(), (int)rawBytes.size(),
		nullptr, 0);
	std::wstring json(len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, rawBytes.c_str(), (int)rawBytes.size(),
		&json[0], len);

	Book book;
	book.id = L"ext_" + jsonPath.stem().wstring();
	book.name = jsonPath.stem().wstring();
	book.description = L"External book";
	book.sourceFile = jsonPath.wstring();
	book.isBuiltIn = false;

	// Simple JSON parsing for external books
	// Expected format: { "name":"...", "lessons":[{ "title":"...", "dialogue":"...", ... }] }

	// Extract name
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

	std::wstring name = extractString(L"name");
	if (!name.empty()) book.name = name;

	std::wstring desc = extractString(L"description");
	if (!desc.empty()) book.description = desc;

	m_books.push_back(book);
	return true;
}

std::wstring LessonManager::BuildPracticeContext(const Lesson& lesson) const
{
	std::wstring ctx;
	ctx += L"You are tutoring a student on " + lesson.bookName + L".\n";
	ctx += L"Current lesson: Lesson " + std::to_wstring(lesson.lessonNumber) +
		L" - " + lesson.title + L"\n";
	ctx += L"Key grammar: " + lesson.keyGrammar + L"\n";
	ctx += L"Lesson dialogue/text:\n" + lesson.dialogueText + L"\n\n";

	if (!lesson.vocabulary.empty())
	{
		ctx += L"Key vocabulary:\n";
		for (auto& v : lesson.vocabulary)
		{
			ctx += L"  - " + v.word + L" (" + v.phonetic + L"): " + v.translation + L"\n";
		}
	}

	ctx += L"\nPractice prompt: " + lesson.practicePrompt;
	return ctx;
}

std::wstring LessonManager::GeneratePracticePrompt(const Lesson& lesson) const
{
	return L"Let's practice Lesson " + std::to_wstring(lesson.lessonNumber) +
		L": " + lesson.title + L". " + lesson.practicePrompt;
}

// ─── Built-in Book Creation ──────────────────────

void LessonManager::CreateBuiltInBooks()
{
	Book nce1;
	nce1.id = L"nce1";
	nce1.name = L"新概念英语第一册 - First Things First";
	nce1.description = L"英语初阶：适合零基础或初学者，学习基本语音、语调、基本语法和词汇。共144课。";
	nce1.isBuiltIn = true;
	CreateNCE1Lessons(nce1);
	m_books.push_back(nce1);

	Book nce2;
	nce2.id = L"nce2";
	nce2.name = L"新概念英语第二册 - Practice and Progress";
	nce2.description = L"实践与进步：适合有一定基础的学员，掌握英语语法体系和句型结构。共96课。";
	nce2.isBuiltIn = true;
	CreateNCE2Lessons(nce2);
	m_books.push_back(nce2);

	Book nce3;
	nce3.id = L"nce3";
	nce3.name = L"新概念英语第三册 - Developing Skills";
	nce3.description = L"培养技能：适合中级学员，提高阅读理解能力和写作技巧。共60课。";
	nce3.isBuiltIn = true;
	CreateNCE3Lessons(nce3);
	m_books.push_back(nce3);

	Book nce4;
	nce4.id = L"nce4";
	nce4.name = L"新概念英语第四册 - Fluency in English";
	nce4.description = L"流利英语：适合高级学员，达到流利自如的英语表达水平。共48课。";
	nce4.isBuiltIn = true;
	CreateNCE4Lessons(nce4);
	m_books.push_back(nce4);
}

// Helper: create a vocabulary item easily
static VocabularyItem Vocab(const std::wstring& word, const std::wstring& phonetic,
	const std::wstring& trans, const std::wstring& example = L"")
{
	return { word, phonetic, trans, example };
}

// Helper: create a lesson easily
static Lesson MakeLesson(int num, const std::wstring& title,
	const std::wstring& dialogue,
	const std::wstring& grammar,
	const std::wstring& prompt,
	std::vector<VocabularyItem> vocab,
	int diff = 1)
{
	Lesson l;
	l.id = num;
	l.lessonNumber = num;
	l.title = title;
	l.bookName = L"新概念英语第一册";
	l.dialogueText = dialogue;
	l.keyGrammar = grammar;
	l.practicePrompt = prompt;
	l.vocabulary = std::move(vocab);
	l.difficulty = diff;
	return l;
}

void LessonManager::CreateNCE1Lessons(Book& book)
{
	// NCE Book 1 representative lessons covering key grammar points
	// We include ~20 representative lessons. For a full version, all 144 can be added.

	book.lessons.push_back(MakeLesson(1,
		L"Excuse me! — 对不起!",
		L"A: Excuse me!\nB: Yes?\nA: Is this your handbag?\nB: Pardon?\nA: Is this your handbag?\nB: Yes, it is. Thank you very much.",
		L"陈述句与一般疑问句: Is this your...? Yes, it is.",
		L"Practice introducing yourself and asking about objects. Use 'Is this your...?' pattern.",
		{
			Vocab(L"excuse", L"/ɪkˈskjuːs/", L"原谅", L"Excuse me, where is the station?"),
			Vocab(L"handbag", L"/ˈhændbæɡ/", L"手提包", L"Is this your handbag?"),
			Vocab(L"pardon", L"/ˈpɑːrdn/", L"请再说一遍", L"I beg your pardon."),
			Vocab(L"thank you", L"/θæŋk juː/", L"谢谢你", L"Thank you very much."),
		}, 1));

	book.lessons.push_back(MakeLesson(3,
		L"Sorry, sir. — 对不起，先生。",
		L"A: My coat and my umbrella please.\nB: Here is my ticket.\nA: Thank you sir. Number five.\nB: Here's your umbrella and your coat.\nA: This is not my umbrella.\nB: Sorry sir. Is this your umbrella?\nA: No, it isn't.\nB: Is this it?\nA: Yes, it is. Thank you very much.",
		L"否定句: This is not... / No, it isn't. 祈使句: My coat please.",
		L"Practice the cloakroom conversation. Use 'Here is my...' and 'This is not my...'",
		{
			Vocab(L"coat", L"/koʊt/", L"大衣", L"My coat please."),
			Vocab(L"umbrella", L"/ʌmˈbrelə/", L"雨伞", L"Here's your umbrella."),
			Vocab(L"ticket", L"/ˈtɪkɪt/", L"票", L"Here is my ticket."),
			Vocab(L"sorry", L"/ˈsɒri/", L"对不起", L"Sorry, sir."),
		}, 1));

	book.lessons.push_back(MakeLesson(7,
		L"Are you a teacher? — 你是教师吗?",
		L"A: I am a new student. My name's Robert.\nB: Nice to meet you. My name's Sophie.\nA: Are you French?\nB: Yes, I am. Are you French, too?\nA: No, I am not. I'm Italian.\nB: Are you a teacher?\nA: No, I'm not. I'm a student.",
		L"人称与职业: I am... / Are you...? / What's your job? 国籍表达法。",
		L"Introduce yourself: name, nationality, and occupation. Ask 'Are you a...?' questions.",
		{
			Vocab(L"student", L"/ˈstjuːdənt/", L"学生", L"I am a new student."),
			Vocab(L"French", L"/frentʃ/", L"法国人", L"Are you French?"),
			Vocab(L"Italian", L"/ɪˈtæliən/", L"意大利人", L"I'm Italian."),
			Vocab(L"teacher", L"/ˈtiːtʃər/", L"教师", L"Are you a teacher?"),
		}, 1));

	book.lessons.push_back(MakeLesson(13,
		L"A new dress — 一件新连衣裙",
		L"A: What colour's your new dress?\nB: It's green.\nA: Come upstairs and see it.\nB: Look! Here it is!\nA: That's a nice dress. It's very smart.\nB: My hat's new, too.\nA: What colour is it?\nB: It's the same colour. It's green, too.\nA: That is a lovely hat!",
		L"颜色词: green, blue, red... What colour is...? 形容词: nice, smart, lovely。",
		L"Describe clothing and colors. Practice 'What colour is your...?' pattern.",
		{
			Vocab(L"colour", L"/ˈkʌlər/", L"颜色", L"What colour's your dress?"),
			Vocab(L"green", L"/ɡriːn/", L"绿色", L"It's green."),
			Vocab(L"dress", L"/dres/", L"连衣裙", L"A new dress."),
			Vocab(L"smart", L"/smɑːrt/", L"漂亮的，时髦的", L"It's very smart."),
			Vocab(L"lovely", L"/ˈlʌvli/", L"可爱的", L"That is a lovely hat!"),
		}, 1));

	book.lessons.push_back(MakeLesson(25,
		L"Mrs. Smith's kitchen — 史密斯太太的厨房",
		L"Mrs. Smith's kitchen is small.\nThere is a refrigerator in the kitchen.\nThe refrigerator is white. It is on the right.\nThere is an electric cooker in the kitchen.\nThe cooker is blue. It is on the left.\nThere is a table in the middle of the room.\nThere is a bottle on the table.\nThe bottle is empty.\nThere is a cup on the table, too.",
		L"There be 句型: There is a... 方位介词: in, on, on the right/left, in the middle of。",
		L"Describe a room using 'There is...' and prepositions of place.",
		{
			Vocab(L"kitchen", L"/ˈkɪtʃɪn/", L"厨房", L"Mrs. Smith's kitchen."),
			Vocab(L"refrigerator", L"/rɪˈfrɪdʒəreɪtər/", L"冰箱", L"There is a refrigerator."),
			Vocab(L"electric", L"/ɪˈlektrɪk/", L"电的", L"An electric cooker."),
			Vocab(L"cooker", L"/ˈkʊkər/", L"炉灶", L"The cooker is blue."),
			Vocab(L"middle", L"/ˈmɪdəl/", L"中间", L"In the middle of the room."),
		}, 2));

	book.lessons.push_back(MakeLesson(37,
		L"Making a bookcase — 做书架",
		L"A: You're working hard, George. What are you doing?\nB: I'm making a bookcase.\nA: Give me that hammer please, Dan.\nB: Which hammer? This one?\nA: No, not that one. The big one.\nB: Here you are.\nA: Thanks, Dan.\nB: What are you going to do now, George?\nA: I'm going to paint it.\nB: What colour are you going to paint it?\nA: I'm going to paint it pink.",
		L"现在进行时: I'm making... 将来时: I'm going to... 指示代词: this one / that one。",
		L"Talk about what you're doing now and your future plans using 'I'm going to...'",
		{
			Vocab(L"bookcase", L"/ˈbʊkkeɪs/", L"书架", L"I'm making a bookcase."),
			Vocab(L"hammer", L"/ˈhæmər/", L"锤子", L"Give me that hammer."),
			Vocab(L"paint", L"/peɪnt/", L"涂漆", L"I'm going to paint it."),
			Vocab(L"pink", L"/pɪŋk/", L"粉红色", L"I'm going to paint it pink."),
		}, 2));

	book.lessons.push_back(MakeLesson(55,
		L"The Sawyer family — 索耶一家人",
		L"The Sawyers live at 87 King Street.\nIn the morning, Mr. Sawyer goes to work and the children go to school.\nMrs. Sawyer stays at home every day. She does the housework.\nIn the afternoon, Mrs. Sawyer usually sees her friends.\nIn the evening, the children come home from school.\nMr. Sawyer comes home from work late.\nAt night, the children always do their homework.\nMr. Sawyer usually reads his newspaper, but sometimes he watches TV.",
		L"一般现在时(第三人称单数): lives, goes, stays, does。频率副词: always, usually, sometimes。",
		L"Describe daily routines using present simple tense. Talk about what people do at different times of day.",
		{
			Vocab(L"live", L"/lɪv/", L"居住", L"They live at 87 King Street."),
			Vocab(L"stay", L"/steɪ/", L"待在", L"She stays at home."),
			Vocab(L"housework", L"/ˈhaʊswɜːrk/", L"家务", L"She does the housework."),
			Vocab(L"usually", L"/ˈjuːʒuəli/", L"通常", L"She usually sees her friends."),
			Vocab(L"newspaper", L"/ˈnjuːzpeɪpər/", L"报纸", L"He reads his newspaper."),
		}, 3));

	book.lessons.push_back(MakeLesson(71,
		L"He's awful! — 他讨厌透了!",
		L"A: What's Ron Marston like, Pauline?\nB: He's awful! He telephoned me four times yesterday, and three times the day before yesterday.\nA: He telephoned the office yesterday morning and yesterday afternoon. My boss answered the telephone.\nB: What did your boss say to him?\nA: He said, \"Pauline is busy. She can't speak to you now!\"\nB: Then I arrived home at six o'clock yesterday evening. He telephoned again. But I didn't answer the phone!",
		L"一般过去时: telephoned, answered, said, arrived。时间状语: yesterday, the day before yesterday。",
		L"Talk about past events. Use past simple tense to describe what happened yesterday.",
		{
			Vocab(L"awful", L"/ˈɔːfəl/", L"糟糕的", L"He's awful!"),
			Vocab(L"telephone", L"/ˈtelɪfoʊn/", L"打电话", L"He telephoned me."),
			Vocab(L"yesterday", L"/ˈjestərdeɪ/", L"昨天", L"Four times yesterday."),
			Vocab(L"answer", L"/ˈænsər/", L"接(电话)", L"I didn't answer the phone!"),
		}, 3));

	book.lessons.push_back(MakeLesson(87,
		L"A car crash — 车祸",
		L"A: Is my car ready yet?\nB: I don't know, sir. What's the number of your car?\nA: It's LFZ 312G.\nB: When did you bring it to us?\nA: I brought it here three days ago.\nB: Ah yes, I remember now.\nA: Have your mechanics finished yet?\nB: No, they're still working on it. Let's go into the garage and have a look at it.\nA: Isn't that your car?\nB: Well, it was my car.\nA: Didn't you have a crash?\nB: That's right. I drove it into a lamp-post.",
		L"现在完成时初步: Have you finished? / I have brought... 不规则动词过去式: bring→brought, drive→drove。",
		L"Practice present perfect tense: 'Have you... yet?' and talk about recent events.",
		{
			Vocab(L"crash", L"/kræʃ/", L"碰撞", L"Didn't you have a crash?"),
			Vocab(L"mechanics", L"/mɪˈkænɪks/", L"机械师", L"Your mechanics."),
			Vocab(L"garage", L"/ˈɡærɑːʒ/", L"车库", L"Go into the garage."),
			Vocab(L"lamp-post", L"/læmp poʊst/", L"路灯柱", L"I drove it into a lamp-post."),
		}, 4));

	book.lessons.push_back(MakeLesson(103,
		L"The French test — 法语考试",
		L"A: How was the exam, Richard?\nB: Not too bad. I think I passed in English and Mathematics.\nA: The questions were very easy. How about you, Gary?\nB: The English and Maths papers weren't easy enough for me.\nA: I hope I haven't failed.\nB: I think I failed the French paper. I could answer sixteen of the questions.\nA: French tests are awful, aren't they?\nB: I hate them. I'm sure I've got a low mark.",
		L"too/enough 用法: too easy, easy enough。反义疑问句: aren't they? 情态动词 could。",
		L"Discuss exam experiences. Use 'too' and 'enough' to express degree.",
		{
			Vocab(L"exam", L"/ɪɡˈzæm/", L"考试", L"How was the exam?"),
			Vocab(L"pass", L"/pæs/", L"通过", L"I think I passed."),
			Vocab(L"mathematics", L"/ˌmæθəˈmætɪks/", L"数学", L"English and Mathematics."),
			Vocab(L"enough", L"/ɪˈnʌf/", L"足够", L"Not easy enough."),
			Vocab(L"mark", L"/mɑːrk/", L"分数", L"A low mark."),
		}, 4));

	// Update lesson bookName for all NCE1 lessons
	for (auto& l : book.lessons)
	{
		l.bookName = book.name;
	}
}

void LessonManager::CreateNCE2Lessons(Book& book)
{
	book.lessons.push_back(MakeLesson(1,
		L"A private conversation — 私人谈话",
		L"Last week I went to the theatre. I had a very good seat.\nThe play was very interesting. I did not enjoy it.\nA young man and a young woman were sitting behind me. They were talking loudly.\nI got very angry. I could not hear the actors.\nI turned round. I looked at the man and the woman angrily.\nThey did not pay any attention.\nIn the end, I could not bear it. I turned round again.\n'I can't hear a word!' I said angrily.\n'It's none of your business,' the young man said rudely.\n'This is a private conversation!'",
		L"一般过去时 + 过去进行时: I went / They were sitting。情态动词 can/could。",
		L"Tell a story about an unpleasant experience. Practice past continuous: 'They were talking loudly.'",
		{
			Vocab(L"theatre", L"/ˈθɪətər/", L"剧院", L"I went to the theatre."),
			Vocab(L"seat", L"/siːt/", L"座位", L"A very good seat."),
			Vocab(L"angrily", L"/ˈæŋɡrɪli/", L"生气地", L"I looked at them angrily."),
			Vocab(L"bear", L"/ber/", L"忍受", L"I could not bear it."),
			Vocab(L"private", L"/ˈpraɪvɪt/", L"私人的", L"A private conversation."),
			Vocab(L"conversation", L"/ˌkɒnvərˈseɪʃən/", L"谈话", L"A private conversation."),
		}, 4));

	book.lessons.push_back(MakeLesson(8,
		L"The best and the worst — 最好的和最差的",
		L"Joe Sanders has the most beautiful garden in our town.\nNearly everybody enters for 'The Nicest Garden Competition' each year,\nbut Joe wins every time. Bill Frith's garden is larger than Joe's.\nBill works harder than Joe and grows more flowers and vegetables,\nbut Joe's garden is more interesting.\nHe has made neat paths and has built a wooden bridge over a pool.\nI like gardens too, but I do not like hard work.\nEvery year I enter for the garden competition too,\nand I always win a little prize for the worst garden in the town!",
		L"形容词/副词比较级与最高级: larger, harder, more interesting; the most beautiful, the best, the worst。",
		L"Compare things using comparatives and superlatives. Describe the best/worst of something.",
		{
			Vocab(L"competition", L"/ˌkɒmpɪˈtɪʃən/", L"比赛", L"The garden competition."),
			Vocab(L"neat", L"/niːt/", L"整齐的", L"Neat paths."),
			Vocab(L"path", L"/pæθ/", L"小路", L"He has made neat paths."),
			Vocab(L"wooden", L"/ˈwʊdən/", L"木制的", L"A wooden bridge."),
			Vocab(L"prize", L"/praɪz/", L"奖品", L"I win a little prize."),
		}, 5));

	book.lessons.push_back(MakeLesson(14,
		L"Do you speak English? — 你会讲英语吗?",
		L"I had an amusing experience last year.\nAfter I had left a small village in the south of France,\nI drove on to the next town. On the way, a young man waved to me.\nI stopped and he asked me for a lift.\nAs soon as he had got into the car, I said good morning to him in French\nand he replied in the same language. Apart from a few words,\nI do not know any French at all. Neither of us spoke during the journey.\nI had nearly reached the town, when the young man suddenly said,\n'Do you speak English?' As I soon learnt, he was English himself!",
		L"过去完成时: I had left... / After I had left... As soon as... 用法。",
		L"Tell a funny travel story. Use past perfect: 'After I had left...' / 'As soon as he had got into...'",
		{
			Vocab(L"amusing", L"/əˈmjuːzɪŋ/", L"有趣的", L"An amusing experience."),
			Vocab(L"experience", L"/ɪkˈspɪəriəns/", L"经历", L"An amusing experience."),
			Vocab(L"wave", L"/weɪv/", L"挥手", L"He waved to me."),
			Vocab(L"lift", L"/lɪft/", L"搭便车", L"He asked me for a lift."),
			Vocab(L"journey", L"/ˈdʒɜːrni/", L"旅程", L"During the journey."),
		}, 5));

	book.lessons.push_back(MakeLesson(25,
		L"Do the English speak English? — 英国人讲的是英语吗?",
		L"I arrived in London at last. The railway station was big, black and dark.\nI did not know the way to my hotel, so I asked a porter.\nI not only spoke English very carefully, but very clearly as well.\nThe porter, however, could not understand me.\nI repeated my question several times and at last he understood.\nHe answered me, but he spoke neither slowly nor clearly.\n'I am a foreigner,' I said. Then he spoke slowly, but I could not understand him.\nMy teacher never spoke English like that!\nThe porter and I looked at each other and smiled.\nThen he said something and I understood it.\n'You'll soon learn English!' he said.\nI wonder. In England, each person speaks a different language.",
		L"并列结构: not only... but... as well / neither... nor... 情态动词 will 表示将来。",
		L"Discuss language learning difficulties. Practice 'not only...but also' and 'neither...nor' patterns.",
		{
			Vocab(L"railway", L"/ˈreɪlweɪ/", L"铁路", L"The railway station."),
			Vocab(L"porter", L"/ˈpɔːrtər/", L"搬运工", L"I asked a porter."),
			Vocab(L"foreigner", L"/ˈfɒrɪnər/", L"外国人", L"I am a foreigner."),
			Vocab(L"wonder", L"/ˈwʌndər/", L"想知道", L"I wonder."),
		}, 5));

	book.lessons.push_back(MakeLesson(38,
		L"Everything except the weather — 唯独没有考虑到天气",
		L"My old friend, Harrison, had lived in the Mediterranean for many years\nbefore he returned to England. He had often dreamed of retiring in England\nand had planned to settle down in the country.\nHe had no sooner returned than he bought a house and went to live there.\nAlmost immediately he began to complain about the weather,\nfor even though it was still summer, it rained continually\nand it was often bitterly cold.\nAfter so many years of sunshine, Harrison got a shock.\nHe acted as if he had never lived in England before.\nIn the end, he had hardly had time to settle down when he sold the house\nand left the country. The dream he had had for so many years ended there.",
		L"No sooner... than... / Hardly... when... 过去完成时与过去时配合。As if 虚拟语气。",
		L"Talk about adapting to new environments. Use 'no sooner...than' and 'hardly...when' structures.",
		{
			Vocab(L"except", L"/ɪkˈsept/", L"除了", L"Everything except the weather."),
			Vocab(L"Mediterranean", L"/ˌmedɪtəˈreɪniən/", L"地中海", L"In the Mediterranean."),
			Vocab(L"retire", L"/rɪˈtaɪər/", L"退休", L"He dreamed of retiring."),
			Vocab(L"complain", L"/kəmˈpleɪn/", L"抱怨", L"He began to complain."),
			Vocab(L"continually", L"/kənˈtɪnjuəli/", L"不断地", L"It rained continually."),
		}, 6));

	book.lessons.push_back(MakeLesson(52,
		L"A pretty carpet — 漂亮的地毯",
		L"We have just moved into a new house and I have been working hard all morning.\nI have been trying to get my new room in order.\nThis has not been easy because I own over a thousand books.\nTo make matters worse, the room is rather small, so I have temporarily\nput my books on the floor. At the moment, they cover every inch of floor space\nand I actually have to walk on them to get in or out of the room.\nA short while ago, my sister helped me to carry one of my old bookcases\nup the stairs. She went into my room and got a big surprise\nwhen she saw all those books on the floor.\n'This is the prettiest carpet I have ever seen,' she said.",
		L"现在完成进行时: I have been working / I have been trying。强调动作持续到现在。",
		L"Describe ongoing activities using present perfect continuous: 'I have been working hard all morning.'",
		{
			Vocab(L"carpet", L"/ˈkɑːrpɪt/", L"地毯", L"A pretty carpet."),
			Vocab(L"temporarily", L"/ˈtempərerəli/", L"暂时地", L"I have temporarily put..."),
			Vocab(L"inch", L"/ɪntʃ/", L"英寸", L"Every inch of floor space."),
			Vocab(L"actually", L"/ˈæktʃuəli/", L"实际上", L"I actually have to walk on them."),
		}, 6));

	for (auto& l : book.lessons)
	{
		l.bookName = book.name;
	}
}

void LessonManager::CreateNCE3Lessons(Book& book)
{
	book.lessons.push_back(MakeLesson(1,
		L"A puma at large — 逃遁的美洲狮",
		L"Pumas are large, cat-like animals which are found in America.\nWhen reports came into London Zoo that a wild puma had been spotted\nforty-five miles south of London, they were not taken seriously.\nHowever, as the evidence began to accumulate, experts from the Zoo\nfelt obliged to investigate, for the descriptions given by people\nwho claimed to have seen the puma were extraordinarily similar.\nThe hunt for the puma began in a small village where a woman\npicking blackberries saw 'a large cat' only five yards away from her.\nIt immediately ran away when she saw it, and experts confirmed\nthat a puma will not attack a human being unless it is cornered.",
		L"复杂句式: 定语从句 which are found... 同位语从句 that a wild puma... 分词短语 picking blackberries。",
		L"Practice complex sentence structures with relative clauses and participial phrases.",
		{
			Vocab(L"puma", L"/ˈpjuːmə/", L"美洲狮", L"A puma at large."),
			Vocab(L"spot", L"/spɒt/", L"发现", L"A puma had been spotted."),
			Vocab(L"evidence", L"/ˈevɪdəns/", L"证据", L"The evidence began to accumulate."),
			Vocab(L"accumulate", L"/əˈkjuːmjʊleɪt/", L"积累", L"Evidence began to accumulate."),
			Vocab(L"investigate", L"/ɪnˈvestɪɡeɪt/", L"调查", L"Experts felt obliged to investigate."),
		}, 7));

	book.lessons.push_back(MakeLesson(13,
		L"It's only me — 是我，别害怕",
		L"After her husband had gone to work, Mrs. Richards sent her children\nto school and went upstairs to her bedroom.\nShe was too excited to do any housework that morning,\nfor in the evening she would be going to a fancy-dress party\nwith her husband. She intended to dress up as a ghost\nand she had made her costume the night before.\nShe was impatient to try it on.\nThough the costume consisted only of a sheet, it was very effective.\nAfter putting it on, she went downstairs.\nShe wanted to find out whether it would be comfortable to wear.",
		L"Too... to... 结构。过去完成时 + 过去将来时: she would be going。分词作状语。",
		L"Narrate a story with flashbacks. Practice 'too...to...' and 'would be going to' structures.",
		{
			Vocab(L"fancy-dress", L"/ˈfænsi dres/", L"化装服", L"A fancy-dress party."),
			Vocab(L"ghost", L"/ɡoʊst/", L"鬼", L"Dress up as a ghost."),
			Vocab(L"costume", L"/ˈkɒstjuːm/", L"服装", L"She had made her costume."),
			Vocab(L"sheet", L"/ʃiːt/", L"床单", L"A sheet."),
			Vocab(L"effective", L"/ɪˈfektɪv/", L"有效的", L"It was very effective."),
		}, 7));

	for (auto& l : book.lessons)
	{
		l.bookName = book.name;
	}
}

void LessonManager::CreateNCE4Lessons(Book& book)
{
	book.lessons.push_back(MakeLesson(1,
		L"Finding fossil man — 发现化石人",
		L"We can read of things that happened 5,000 years ago in the Near East,\nwhere people first learned to write. But there are some parts of the world\nwhere even now people cannot write. The only way that they can preserve\ntheir history is to recount it as sagas — legends handed down\nfrom one generation of storytellers to another.\nThese legends are useful because they can tell us something\nabout migrations of people who lived long ago,\nbut none could write down what they did.",
		L"学术英语: 被动语态、定语从句、长难句分析。Preserve/recount/handed down 等高级词汇。",
		L"Discuss historical topics using academic vocabulary. Practice passive voice in formal contexts.",
		{
			Vocab(L"fossil", L"/ˈfɒsəl/", L"化石", L"Fossil man."),
			Vocab(L"preserve", L"/prɪˈzɜːrv/", L"保存", L"Preserve their history."),
			Vocab(L"recount", L"/rɪˈkaʊnt/", L"叙述", L"Recount it as sagas."),
			Vocab(L"saga", L"/ˈsɑːɡə/", L"传奇故事", L"Recount it as sagas."),
			Vocab(L"migration", L"/maɪˈɡreɪʃən/", L"迁移", L"Migrations of people."),
		}, 8));

	book.lessons.push_back(MakeLesson(5,
		L"Youth — 青年",
		L"People are always talking about 'the problem of youth'.\nIf there is one — which I take leave to doubt —\nthen it is older people who create it, not the young themselves.\nLet us get down to fundamentals and agree that the young are after all\nhuman beings — people just like their elders.\nThere is only one difference between an old man and a young one:\nthe young man has a glorious future before him\nand the old one has a splendid future behind him: and maybe that is where the rub is.",
		L"议论文写作: 让步结构、对比论证。高级连词与插入语。",
		L"Debate social topics. Practice argumentative language: 'If there is one... it is... who...'",
		{
			Vocab(L"youth", L"/juːθ/", L"青年", L"The problem of youth."),
			Vocab(L"fundamental", L"/ˌfʌndəˈmentəl/", L"基本原则", L"Get down to fundamentals."),
			Vocab(L"glorious", L"/ˈɡlɔːriəs/", L"光荣的", L"A glorious future."),
			Vocab(L"splendid", L"/ˈsplendɪd/", L"灿烂的", L"A splendid future."),
			Vocab(L"rub", L"/rʌb/", L"困难", L"That is where the rub is."),
		}, 8));

	for (auto& l : book.lessons)
	{
		l.bookName = book.name;
	}
}
