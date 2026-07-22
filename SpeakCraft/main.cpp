// SpeakCraft — AI-Powered English Speaking Practice Assistant
// Windows Desktop Application — x64, C++20, WinAPI
//
// Built with New Concept English (新概念英语) as the primary curriculum,
// with extensible support for other textbooks via JSON.

#include "framework.h"
#include "MainWindow.h"
#include "ConfigManager.h"
#include "LessonManager.h"

int APIENTRY wWinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR lpCmdLine,
	_In_ int nCmdShow)
{
	UNREFERENCED_PARAMETER(hInstance);
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Initialize COM for speech services
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	if (FAILED(hr) && hr != RPC_E_CHANGED_MODE)
	{
		MessageBoxW(nullptr,
			L"Failed to initialize COM. The application cannot start.",
			L"SpeakCraft — Fatal Error",
			MB_OK | MB_ICONERROR);
		return 1;
	}

	// Load configuration
	ConfigManager::Instance().Load();

	// Create main window and run
	MainWindow app;
	if (!app.Create(nCmdShow))
	{
		MessageBoxW(nullptr,
			L"Failed to create the main application window.",
			L"SpeakCraft — Fatal Error",
			MB_OK | MB_ICONERROR);
		CoUninitialize();
		return 1;
	}

	int result = MainWindow::Run();

	CoUninitialize();
	return result;
}
