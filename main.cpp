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
		FALSE,
		CREATE_NEW_CONSOLE | (suspended ? CREATE_SUSPENDED : 0),
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
	for (auto& arg : args)
	{
		// -help
		{
			if (arg == "-help" || arg == "--help" || arg == "help")
			{
				print(std::format(
					"4DModLoader.exe flags/launch arguments:\r\n"
					" -help or --help or help  -  Outputs this and does not launch anything.\r\n"
					" -4dm <path to an .exe>  -  Overrides the \"4D Miner executable\".\r\n\tDefault: {}\r\n"
					" -debug  -  Attaches a debugger to the just started 4D Miner process.\r\n"
					" -debugger <cmd for a debugger with a PID>  -  Overrides the debugger cmd to be used with -debug.\r\n\tDefault: \"" DEFAULT_JIT_DEBUGGER_CMD "\"\r\n"
					" 4DModLoader-Core specific launch arguments:\r\n"
					"  -console  -  Starts the debug console. (since v2.2)\r\n  there might be more if you are on an outdated 4DModLoader.exe that doesn't know about them.\r\n"
					" + whatever mod launch arguments there might be\r\n"
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
	}
	if (!fdmOverrideStr.empty())
		fdmExe = fdmOverrideStr.c_str();

	AutoUpdate();
	CheckForLibs();
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

	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
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

int main()
{
	return main_({});
}
