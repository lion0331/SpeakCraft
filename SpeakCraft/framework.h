#pragma once

// Windows version targeting — must be before windows.h
#include "targetver.h"

// Prevent windows.h from defining min/max macros (breaks std::min/std::max)
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Suppress deprecation warnings in SAPI headers
#pragma warning(push)
#pragma warning(disable: 4996)

// Windows SDK headers
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <winhttp.h>
#include <shellapi.h>
#include <shlobj.h>        // SHGetFolderPathW, CSIDL_APPDATA
#include <richedit.h>      // CHARFORMAT2W, SETTEXTEX, EM_SETCHARFORMAT, etc.
#include <sapi.h>
#include <sphelper.h>

#pragma warning(pop)

// C++ Standard Library
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <functional>
#include <atomic>
#include <mutex>
#include <thread>
#include <future>
#include <chrono>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <random>
#include <optional>

// COM helper
#include <comdef.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "sapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "shell32.lib")

// Common control manifest
#if defined _M_IX86
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
