#include <iostream>
#include <Windows.h>
#include <TlHelp32.h>
#include <shellapi.h>
#include <filesystem>
#include <string>
#include <sstream>

#include "AutoUpdater.h"

std::string fdmExe = "4DM.exe";
inline static std::string modloaderCore = ".\\4DModLoader-Core.dll";

bool attachedToConsole = false;
PROCESS_INFORMATION startup(LPCSTR lpApplicationName, const std::vector<std::string>& args, bool suspended = true)
{
	// additional information
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;

	// set the size of the structures
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	// create the shit for arguments
	std::ostringstream argsStream;
	argsStream << "\"" << lpApplicationName << "\"";

	for (auto& arg : args)
		argsStream << " \"" << arg << "\"";

	// start the process
	CreateProcessA
	(
		lpApplicationName,
		const_cast<char*>(argsStream.str().c_str()),
		NULL,
		NULL,
		TRUE,
		(!attachedToConsole ? CREATE_NEW_CONSOLE : 0) | (suspended ? CREATE_SUSPENDED : 0),
		NULL,
		NULL,
		&si,
		&pi
	);

	return pi;
}

bool Inject(HANDLE process, HANDLE thread)
{
	if (!process) return false;
	if (!std::filesystem::exists(modloaderCore)) return false;

	LPVOID LoadLibAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

	LPVOID LoadLocation = VirtualAllocEx(process, 0, modloaderCore.size(), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	HANDLE RemoteThread = CreateRemoteThread(process, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibAddr, LoadLocation, 0, 0);
	WriteProcessMemory(process, LoadLocation, modloaderCore.c_str(), modloaderCore.size(), NULL);
	WaitForSingleObject(RemoteThread, INFINITE);

	VirtualFreeEx(process, LoadLocation, modloaderCore.size(), MEM_RELEASE);
	CloseHandle(RemoteThread);
	CloseHandle(process);
	
	return true;
}

#define DEFAULT_JIT_DEBUGGER_CMD "C:\\Windows\\System32\\vsjitdebugger.exe -p"
int main_(const std::vector<std::string>& args)
{
	if (std::filesystem::exists("4D Server.exe"))
	{
		fdmExe = "4D Server.exe";
	}

	bool fdmOverride = false;
	std::string fdmOverrideStr = "";
	bool jitDebug = false;
	std::string debugger = DEFAULT_JIT_DEBUGGER_CMD;
	bool debuggerOverride = false;
	bool offline = false;
	for (auto& arg : args)
	{
		// -help
		{
			if (arg == "-help" || arg == "--help" || arg == "help")
			{
				print(std::format(
					"4DModLoader.exe flags/launch arguments:\n"
					" -help or --help or help  -  Outputs this and does not launch anything.\n"
					" -4dm <path to an .exe>  -  Overrides the \"4D Miner executable\".\n\tDefault: {}\n"
					" -debug  -  Attaches a debugger to the just started 4D Miner process.\n"
					" -debugger <cmd for a debugger with a PID>  -  Overrides the debugger cmd to be used with -debug.\n\tDefault: \"" DEFAULT_JIT_DEBUGGER_CMD "\"\n"
					" -offline  -  Disable Auto-Updater Online Checks.\n"
					" 4DModLoader-Core specific launch arguments:\n"
					"  -console  -  Starts the debug console. (since v2.2)\n  there might be more if you are on an outdated 4DModLoader.exe that doesn't know about them.\n"
					" + whatever mod launch arguments there might be\n"
					, fdmExe)
				);
				std::cin.get();
				return 0;
			}
		}
		// -4dm
		{
			if (!fdmOverride && arg == "-4dm")
			{
				fdmOverride = true;
				continue;
			}
			if (fdmOverride &&
				std::filesystem::exists(arg) && arg.ends_with(".exe"))
			{
				fdmOverrideStr = arg;
				fdmOverride = false;
				continue;
			}
		}
		// -debug
		{
			if (!jitDebug && arg == "-debug")
			{
				jitDebug = true;
				continue;
			}
		}
		// -debugger
		{
			if (!debuggerOverride && arg == "-debugger")
			{
				debuggerOverride = true;
				continue;
			}
			if (debuggerOverride)
			{
				debugger = arg;
				debuggerOverride = false;
				continue;
			}
		}
		// -offline
		{
			if (!offline && arg == "-offline")
			{
				offline = true;
				continue;
			}
		}
	}
	if (!fdmOverrideStr.empty())
		fdmExe = fdmOverrideStr.c_str();

	if (!offline)
	{
		AutoUpdate();
		CheckForLibs();
	}

	auto gameProcessInfo = startup(fdmExe.c_str(), args);
	HANDLE& gameProcess = gameProcessInfo.hProcess;
	HANDLE& gameThread = gameProcessInfo.hThread;

	if (!gameProcess)
	{
		MessageBoxA(0, std::format("Couldn't open the 4D Miner executable (expected to be at \"{}\")", fdmExe).c_str(), "4DModLoader", MB_OK | MB_ICONERROR);

		return -2;
	}
	if (jitDebug)
	{
		debugger = std::format("{} {}", debugger, gameProcessInfo.dwProcessId);
		int r = system(debugger.c_str());
		if (r)
		{
			MessageBoxA(0, std::format("Couldn't launch the debugger with cmd \"{}\"", debugger).c_str(), "4DModLoader", MB_OK | MB_ICONWARNING);
		}
	}
	if (!Inject(gameProcess, gameThread))
	{
		MessageBoxA(0, "Couldn't load 4DModLoader-Core.dll", "4DModLoader", MB_OK | MB_ICONERROR);

		ResumeThread(gameThread);

		return -1;
	}

	ResumeThread(gameThread);

	if (attachedToConsole)
	{
		WaitForSingleObject(gameProcess, INFINITE);
		WaitForSingleObject(gameThread, INFINITE);
	}

	CloseHandle(gameProcess);
	CloseHandle(gameThread);

	if (consoleOpen && !attachedToConsole)
	{
		printf("You can close this window if it is still open for some reason :)\n");
		FreeConsole();
	}

	return 0;
}

/*
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	if (attachedToConsole = AttachConsole(ATTACH_PARENT_PROCESS))
	{
		AllocConsole();

		FILE* fpout;
		FILE* fpin;
		freopen_s(&fpout, "CONOUT$", "wt", stdout);
		freopen_s(&fpin, "CONIN$", "rt", stdin);

		SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
	}

	std::vector<std::string> args;

	int argc = 0;
	LPWSTR* w_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (w_argv)
	{
		args.reserve(argc);

		for (int i = 0; i < argc; i++)
		{
			std::wstring wideStr(w_argv[i]);

			args.emplace_back(wideStr.begin(), wideStr.end());
		}
	}

	LocalFree(w_argv);

	args.erase(args.begin());

	int returnV = main_(args);

	exit(returnV);
	return returnV;
}
*/

int main(int argc, char* argv[])
{
	DWORD processList[2];
	DWORD processCount = GetConsoleProcessList(processList, 2);

	if (processCount <= 1)
	{
		HWND hwnd = GetConsoleWindow();
		if (hwnd)
		{
			FreeConsole();
		}
	}
	else
	{
		attachedToConsole = true;
		SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);
	}

	std::vector<std::string> args;
	args.reserve(argc - 1);
	for (int i = 1; i < argc; ++i)
	{
		args.emplace_back(argv[i]);
	}

	return main_(args);
}
