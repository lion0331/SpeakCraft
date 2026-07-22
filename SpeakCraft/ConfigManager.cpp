#include "ConfigManager.h"
#include <fstream>
#include <sstream>

// ─── UTF-8 ↔ wstring helpers ─────────────────────

static std::string WsToUtf8(const std::wstring& ws)
{
	if (ws.empty()) return {};
	int len = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
		nullptr, 0, nullptr, nullptr);
	std::string result(len, '\0');
	WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(),
		&result[0], len, nullptr, nullptr);
	return result;
}

static std::wstring Utf8ToWs(const std::string& utf8)
{
	if (utf8.empty()) return {};
	int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(),
		nullptr, 0);
	std::wstring result(len, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(),
		&result[0], len);
	return result;
}

// ─── Simple JSON value extraction ────────────────

namespace
{
	std::wstring ExtractJsonString(const std::wstring& json, const std::wstring& key)
	{
		std::wstring search = L'"' + key + L'"';
		size_t pos = json.find(search);
		if (pos == std::wstring::npos) return L"";

		// Find opening quote of the value (after ": ")
		pos = json.find(L'"', pos + search.length());
		if (pos == std::wstring::npos) return L"";

		// Find closing quote of the value
		size_t end = json.find(L'"', pos + 1);
		if (end == std::wstring::npos) return L"";

		// Extract value between the quotes
		return json.substr(pos + 1, end - pos - 1);
	}

	int ExtractJsonInt(const std::wstring& json, const std::wstring& key, int defaultVal = 0)
	{
		std::wstring search = L'"' + key + L'"';
		size_t pos = json.find(search);
		if (pos == std::wstring::npos) return defaultVal;

		pos = json.find(L':', pos);
		if (pos == std::wstring::npos) return defaultVal;

		pos++;
		while (pos < json.length() && iswspace(json[pos])) pos++;

		std::wstring numStr;
		bool negative = false;
		if (pos < json.length() && json[pos] == L'-')
		{
			negative = true;
			pos++;
		}
		while (pos < json.length() && iswdigit(json[pos]))
		{
			numStr += json[pos];
			pos++;
		}

		if (numStr.empty()) return defaultVal;
		int val = std::stoi(numStr);
		return negative ? -val : val;
	}
}

// ─── Singleton ───────────────────────────────────

ConfigManager& ConfigManager::Instance()
{
	static ConfigManager instance;
	return instance;
}

std::filesystem::path ConfigManager::GetConfigPath() const
{
	wchar_t appData[MAX_PATH];
	if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
	{
		std::filesystem::path dir(appData);
		dir /= L"SpeakCraft";
		return dir / L"config.json";
	}
	return L"config.json";
}

// ─── Load (UTF-8 file → wstring) ─────────────────

bool ConfigManager::Load()
{
	auto path = GetConfigPath();
	if (!std::filesystem::exists(path)) return false;

	// Read raw bytes
	std::ifstream file(path, std::ios::binary);
	if (!file.is_open()) return false;

	std::string rawBytes((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	file.close();

	if (rawBytes.empty()) return false;

	// ── Detect old/broken UTF-16LE files (produced by old wofstream) ──
	// UTF-16LE BOM = 0xFF 0xFE; UTF-16BE BOM = 0xFE 0xFF
	// Also: if even bytes are mostly 0x00 → UTF-16LE without BOM
	bool isUtf16 = false;
	if (rawBytes.size() >= 2)
	{
		unsigned char b0 = static_cast<unsigned char>(rawBytes[0]);
		unsigned char b1 = static_cast<unsigned char>(rawBytes[1]);
		if ((b0 == 0xFF && b1 == 0xFE) || (b0 == 0xFE && b1 == 0xFF))
		{
			isUtf16 = true;
		}
	}
	if (!isUtf16 && rawBytes.size() > 4)
	{
		// Heuristic: if every other byte is 0x00 in the first 256 bytes, it's UTF-16LE
		int nullCount = 0;
		size_t check = std::min(rawBytes.size(), size_t(256));
		for (size_t i = 1; i < check; i += 2)
		{
			if (rawBytes[i] == 0) nullCount++;
		}
		if (nullCount > check / 4) isUtf16 = true; // >50% odd bytes are null
	}

	if (isUtf16)
	{
		// Old broken file — delete it so next Save creates a clean UTF-8 one
		file.close();
		std::filesystem::remove(path);
		return false;
	}

	std::wstring json = Utf8ToWs(rawBytes);

	std::wstring ep = ExtractJsonString(json, L"api_endpoint");
	if (!ep.empty()) m_apiEndpoint = ep;

	std::wstring key = ExtractJsonString(json, L"api_key");
	if (!key.empty()) m_apiKey = key;

	std::wstring model = ExtractJsonString(json, L"model_name");
	if (!model.empty()) m_modelName = model;

	std::wstring voice = ExtractJsonString(json, L"voice_token");
	if (!voice.empty()) m_voiceToken = voice;

	m_speechRate = ExtractJsonInt(json, L"speech_rate", 0);
	m_speechVolume = ExtractJsonInt(json, L"speech_volume", 100);

	std::wstring prompt = ExtractJsonString(json, L"system_prompt");
	if (!prompt.empty()) m_systemPrompt = prompt;

	return true;
}

// ─── Save (wstring → UTF-8 file) ─────────────────

bool ConfigManager::Save()
{
	auto path = GetConfigPath();
	std::filesystem::create_directories(path.parent_path());

	// Build JSON as wstring, then convert to UTF-8
	std::wstring json;
	json += L"{\n";
	json += L"  \"api_endpoint\": \"" + m_apiEndpoint + L"\",\n";
	json += L"  \"api_key\": \"" + m_apiKey + L"\",\n";
	json += L"  \"model_name\": \"" + m_modelName + L"\",\n";
	json += L"  \"voice_token\": \"" + m_voiceToken + L"\",\n";
	json += L"  \"speech_rate\": " + std::to_wstring(m_speechRate) + L",\n";
	json += L"  \"speech_volume\": " + std::to_wstring(m_speechVolume) + L",\n";
	json += L"  \"system_prompt\": \"" + m_systemPrompt + L"\"\n";
	json += L"}\n";

	std::string utf8 = WsToUtf8(json);

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) return false;
	file.write(utf8.c_str(), utf8.size());
	file.close();
	return true;
}

// ─── Getters / Setters ──────────────────────────

std::wstring ConfigManager::GetApiEndpoint() const
{
	return m_apiEndpoint;
}
void ConfigManager::SetApiEndpoint(const std::wstring& v)
{
	m_apiEndpoint = v;
}

std::wstring ConfigManager::GetApiKey() const
{
	return m_apiKey;
}
void ConfigManager::SetApiKey(const std::wstring& v)
{
	m_apiKey = v;
}

std::wstring ConfigManager::GetModelName() const
{
	return m_modelName;
}
void ConfigManager::SetModelName(const std::wstring& v)
{
	m_modelName = v;
}

std::wstring ConfigManager::GetVoiceToken() const
{
	return m_voiceToken;
}
void ConfigManager::SetVoiceToken(const std::wstring& v)
{
	m_voiceToken = v;
}

int ConfigManager::GetSpeechRate() const
{
	return m_speechRate;
}
void ConfigManager::SetSpeechRate(int v)
{
	m_speechRate = v;
}

int ConfigManager::GetSpeechVolume() const
{
	return m_speechVolume;
}
void ConfigManager::SetSpeechVolume(int v)
{
	m_speechVolume = v;
}

std::wstring ConfigManager::GetSystemPrompt() const
{
	return m_systemPrompt;
}
void ConfigManager::SetSystemPrompt(const std::wstring& v)
{
	m_systemPrompt = v;
}
