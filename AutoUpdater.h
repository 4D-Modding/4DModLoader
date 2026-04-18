#pragma once

#define VERSION "2026.04.19.2"

//#define DEV

#ifndef DEV
	#define SERVER_PHP "https://4d-modding.com"
	#define SERVER_FILES "https://4d-modding.com"
	//#define SERVER_HTTP
#else
	#define SERVER_PHP "http://localhost"
	#define SERVER_FILES "http://localhost"
	#define SERVER_HTTP
#endif

#include "requests.h"
#include "json.hpp"
#include <fstream>
#include <thread>
#include <atomic>

inline static constexpr uint64_t fdm014xOffset = 0x159BB8;
inline static constexpr int fdm0140InitCrashRptStartValue = 0x74696E49;
inline static constexpr int fdm0141AValue = 0x4005B220;
inline static constexpr uint64_t fdm0214VerOffset = 0x2434C8; // "0.2.1.4 Alpha" used in the title screen
inline static constexpr std::string fdm0214VerName = "0.2.1.4 Alpha";

inline static bool consoleOpen = false;
inline static FILE* fpout;
inline static FILE* fpin;

extern std::string fdmExe;
extern bool attachedToConsole;
extern std::string thisPath;
extern std::vector<std::string> args;
PROCESS_INFORMATION startup(LPCSTR lpApplicationName, const std::vector<std::string>& args, bool suspended = true);

inline static void print(const std::string& msg)
{
	if (!consoleOpen)
	{
		AllocConsole();

		freopen_s(&fpout, "CONOUT$", "wt", stdout);
		freopen_s(&fpin, "CONIN$", "rt", stdin);

		SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT);

		consoleOpen = true;
	}
	printf("%s", msg.c_str());
}

// [0] is modloaderVer, [1] is gameVer
std::vector<std::string> GetVers()
{
	if(std::filesystem::exists("./core.ver"))
	{
		std::ifstream file("core.ver");
		if (file.is_open()) 
		{
			std::string modloaderVer, gameVer;
			std::getline(file, modloaderVer);
			std::getline(file, gameVer);
			file.close();

			return { modloaderVer, gameVer };
		}
	}
	else // detection of game version for 0.1.4.0, 0.1.4.1 and 0.2.1.4. also yea it forces to update modloader lol
	{
		std::ifstream file(fdmExe, std::ios::binary);
		if (file.is_open())
		{
			// 0.1.4.0 check
			file.seekg(fdm014xOffset);

			int number;
			file.read(reinterpret_cast<char*>(&number), sizeof(number));
			if (number == fdm0140InitCrashRptStartValue)
			{
				file.close();
				return { "0.0", "0.1.4.0" };
			}

			// 0.1.4.1 check (same offset lol)
			if (number == fdm0141AValue)
			{
				file.close();
				return { "0.0", "0.1.4.1" };
			}
			
			// 0.2.1.4 check
			file.seekg(fdm0214VerOffset);
			char buffer[14]{0};
			file.read(buffer, sizeof(buffer) - 1);
			std::string ver(buffer);
			if (ver == fdm0214VerName)
			{
				file.close();
				return { "0.0", "0.2.1.4" };
			}
		}

		// download latest core.ver from the website lmfao. also yea it forces to update the modloader lol
		DownloadFile(SERVER_PHP "/updater/latestGameVer.php", "core.ver");
		std::ifstream a("core.ver");
		if (a.is_open())
		{
			std::string modloaderVer, gameVer;
			std::getline(a, modloaderVer);
			std::getline(a, gameVer);
			a.close();

			return { "0.0", gameVer };
		}
		return { "0.0", "0.0" };
	}
}

void createDirectories(const std::string& path, const std::string& pathPrefix)
{
	std::string dirs = path;
	std::string currentPath = "";
	size_t pos = 0;
	while ((pos = dirs.find_first_of("/")) != std::string::npos)
	{
		std::string directory = dirs.substr(0, pos);
		dirs = dirs.erase(0, pos + 1);

		if (!directory.empty() && !std::filesystem::path(directory).has_extension())
		{
			if (currentPath.empty())
				currentPath = directory;
			else
				currentPath = (std::filesystem::path(currentPath) / directory).string();

			auto fullPath = std::filesystem::path(pathPrefix) / currentPath;
			if (!std::filesystem::exists(fullPath))
				std::filesystem::create_directory(fullPath);
		}
		else
		{
			break;
		}
	}
}

inline std::vector<std::string> split(const std::string& str, char delim)
{
	std::vector<std::string> tokens;
	size_t pos = 0;
	size_t len = str.length();
	tokens.reserve(len / 2);  // allocate memory for expected number of tokens

	while (pos < len)
	{
		size_t end = str.find_first_of(delim, pos);
		if (end == std::string::npos)
		{
			tokens.emplace_back(str.substr(pos));
			break;
		}
		tokens.emplace_back(str.substr(pos, end - pos));
		pos = end + 1;
	}

	return tokens;
}
inline void trimStart(std::string& s)
{
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](uint8_t c) {
		return !std::isspace(c);
		}));
}
inline void trimEnd(std::string& s)
{
	s.erase(std::find_if(s.rbegin(), s.rend(), [](uint8_t c) {
		return !std::isspace(c);
		}).base(), s.end());
}
inline void trim(std::string& s)
{
	trimStart(s);
	trimEnd(s);
}
/*
	Compares two version strings using an operator string
	operators are "==" | ">=" | "<=" | "<" | ">" | "!="
	it is used by the modloader to check dependency versions
*/
bool versionCompare(const std::string& aVer, const std::string& op, const std::string& bVer)
{
	if (op.empty()) return true;

	auto splitA = split(aVer, '.');
	auto splitB = split(bVer, '.');

	int len = std::max(splitA.size(), splitB.size());
	int a, b;
	for (int i = 0; i < len; i++)
	{
		a = (i < splitA.size() ? std::stoi(splitA[i]) : 0);
		b = (i < splitB.size() ? std::stoi(splitB[i]) : 0);

		if (a != b)
			break;
	}
	if (op == "==") return a == b;
	else if (op == ">=") return a >= b;
	else if (op == "<=") return a <= b;
	else if (op == "<") return a < b;
	else if (op == ">") return a > b;
	else if (op == "!=") return a != b;
	return false;
}

bool updateModLoader()
{
	std::string result = GET(SERVER_FILES "/core-files/modloader.ver", "");
	trim(result);

	if (result.size() <= 6) return false;
	if (result[0] != '2') return false;

	try
	{
		if (!versionCompare(VERSION, "<", result))
		{
			return false;
		}

		if (!attachedToConsole)
		{
			if (MessageBoxA(0, std::format("New 4DModLoader v{} is available (current is v{})\n", result, VERSION).c_str(), "Auto-Updater", MB_ICONQUESTION | MB_OKCANCEL) != IDOK)
			{
				return false;
			}
		}
		else
		{
			print(std::format("New 4DModLoader v{} is available (current is v{})\n", result, VERSION));
			print(std::format("Would you like to install the update? [Y/N]\n"));
			std::atomic<bool> answered = false;
			std::string answer;
			std::thread inputThread{ [&]()
				{
					std::cin >> answer;
					answered = true;
				}
			};

			for (int i = 0; i < 5; ++i)
			{
				print(".");
				std::this_thread::sleep_for(std::chrono::seconds(1));
				if (answered) break;
			}

			inputThread.detach();

			if (!answered || answer.empty())
			{
				print(std::format("\ni guess not\n"));
			}

			if (!answered || answer.empty() || std::tolower(answer[0]) != 'y')
			{
				return false;
			}
		}

		auto thisPath_ = std::filesystem::path(thisPath);

		auto thisPathOld = thisPath_;
		thisPathOld.replace_extension(".exe.old");

		auto pdbPath_ = thisPath_.root_directory() / "4DModLoader.pdb";
		pdbPath_.replace_extension(".pdb");

		auto pdbPathOld = pdbPath_;
		pdbPathOld.replace_extension(".pdb.old");

		try { std::filesystem::rename(thisPath_, thisPathOld); }
		catch (const std::exception& e) { print("Failed to update self .exe\n"); return false; }

		int64_t bytesDownloaded = 0;
		print("Downloading 4DModLoader.exe...\n");
		bytesDownloaded = 0;
		if (DownloadFile(SERVER_FILES "/core-files/4DModLoader.exe", thisPath_.string(), bytesDownloaded))
		{
			print(std::format("\tDownloaded ({}).\n", bytesDownloaded));
		}
		else
		{
			print("\tFailed.\n");
			return false;
		}

		if (std::filesystem::exists(pdbPath_))
		{
			try
			{
				std::filesystem::rename(pdbPath_, pdbPathOld);

				print("Downloading 4DModLoader.pdb...\n");
				bytesDownloaded = 0;
				if (DownloadFile(SERVER_FILES "/core-files/4DModLoader.pdb", pdbPath_.string(), bytesDownloaded))
				{
					print(std::format("\tDownloaded ({}).\n", bytesDownloaded));
				}
				else
				{
					print("\tFailed.\n");
					return false;
				}
			}
			catch (const std::exception& e) { print("Failed to update self .pdb\n"); return false; }
		}

		return true;
	}
	catch (const std::exception&)
	{
		return false;
	}

	return false;
}
void updateCore()
{
	// check for game's ver
	auto vers = GetVers();

	std::string result = POST(
		SERVER_PHP "/updater/updateAvailable.php",
		std::format("curGameVer={}&curModLoaderVer={}", vers[1], vers[0]));

	if (result.size() <= 6)
		return;

	result.erase(std::remove(result.begin(), result.end(), '\\'), result.end());

	if (result == "WRONG_GAME_VERSION" || result == "SAME_MODLOADER_VERSION")
		return;

	nlohmann::json updateJson = nlohmann::json::parse(result);

	std::string verNum = updateJson["versionNumber"];

	if (!attachedToConsole)
	{
		if (MessageBoxA(0, std::format("New 4DModLoader-Core v{} is available for 4D Miner v{}. (current is v{})\nWould you like to update?\n(you can turn this off in settings or use -offline)", verNum, vers[1], vers[0]).c_str(), "Auto-Updater", MB_ICONQUESTION | MB_OKCANCEL) != IDOK)
		{
			return;
		}
	}
	else
	{
		print(std::format("New 4DModLoader-Core v{} is available for 4D Miner v{} (current is v{})\n", verNum, vers[1], vers[0]));
		print(std::format("Would you like to install the update? [Y/N]\n"));
		std::atomic<bool> answered = false;
		std::string answer;
		std::thread inputThread{ [&]()
			{
				std::cin >> answer;
				answered = true;
			}
		};

		for (int i = 0; i < 5; ++i)
		{
			print(".");
			std::this_thread::sleep_for(std::chrono::seconds(1));
			if (answered) break;
		}

		inputThread.detach();

		if (!answered || answer.empty())
		{
			print(std::format("\ni guess not\n"));
		}

		if (!answered || answer.empty() || std::tolower(answer[0]) != 'y')
		{
			return;
		}
	}

	print("Downloading new files...\n\n");

	int64_t bytesDownloaded = 0;
	for (auto& fileUrl : updateJson["versionFiles"])
	{
		std::string str = fileUrl;
		bytesDownloaded = 0;

		std::string hostname;
		std::string path;
		splitUrl(str, hostname, path);

		std::filesystem::path filePath(path);
		std::string fileName = filePath.filename().string();
		size_t lToCut;
		std::string outPath = fileName;
		std::string a = std::format("core-files/{}/", vers[1]);
		lToCut = path.find(a);
		if (lToCut != std::string::npos)
		{
			outPath = path.substr(lToCut + a.size());
			outPath = outPath.substr(outPath.find('/') + 1);
		}
		else
		{
			lToCut = path.find("core-files/");
			if (lToCut != std::string::npos)
				outPath = path.substr(lToCut + 11);
		}

		print(std::format("Downloading {}\n", outPath));
		createDirectories(outPath, "./");
		if (DownloadFile(str, outPath, bytesDownloaded))
			print(std::format("\tDownloaded ({}).\n", bytesDownloaded));
		else
			print("\tFailed.\n");
	}
}

void AutoUpdate()
{
	// check if the server is down or the network is not available
#ifndef SERVER_HTTP
	{
		if (!InternetCheckConnectionA(SERVER_PHP, FLAG_ICC_FORCE_CONNECTION, 0))
			return;
		if (!InternetCheckConnectionA(SERVER_FILES, FLAG_ICC_FORCE_CONNECTION, 0))
			return;
	}
#endif

	// don't auto-update if auto-updater option is toggled off
	if(std::filesystem::exists("./settings.json"))
	{
		std::ifstream file("settings.json");
		if (file.is_open()) 
		{
			nlohmann::json settingsJson = nlohmann::json::parse(file);

			file.close();

			if (settingsJson.contains("auto-updater") && settingsJson["auto-updater"].is_boolean() && !((bool)settingsJson["auto-updater"]))
				return;

			if(!settingsJson.contains("auto-updater"))
			{
				settingsJson["auto-updater"] = true;

				std::ofstream outputFile("settings.json");

				outputFile << settingsJson.dump() << std::endl;
				outputFile.close();
			}
		}
	}

	if (updateModLoader())
	{
		auto pi = startup(thisPath.c_str(), args, false);
		if (attachedToConsole)
		{
			WaitForSingleObject(pi.hProcess, INFINITE);
		}
		
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);

		exit(0);

		return;
	}
	else
	{
		auto thisPath_ = std::filesystem::path(thisPath);

		auto thisPathOld = thisPath_;
		thisPathOld.replace_extension(".exe.old");

		auto pdbPath_ = thisPath_.root_directory() / "4DModLoader.pdb";
		pdbPath_.replace_extension(".pdb");

		auto pdbPathOld = pdbPath_;
		pdbPathOld.replace_extension(".pdb.old");

		if (std::filesystem::exists(thisPathOld))
		{
			std::filesystem::remove(thisPathOld);
		}
		if (std::filesystem::exists(pdbPathOld))
		{
			std::filesystem::remove(pdbPathOld);
		}
	}
	updateCore();
}
void CheckForLibs()
{
	// check if the server is down or the network is not available
#ifndef SERVER_HTTP
	{
		if (!InternetCheckConnectionA(SERVER_PHP, FLAG_ICC_FORCE_CONNECTION, 0))
			return;
		if (!InternetCheckConnectionA(SERVER_FILES, FLAG_ICC_FORCE_CONNECTION, 0))
			return;
	}
#endif

	std::string result = GET(SERVER_FILES "/core-files/versions.json", "");

	if (result.size() <= 6) return;

	nlohmann::json versionsJson = nlohmann::json::parse(result);

	int64_t bytesDownloaded = 0;
	if (versionsJson.contains("libs"))
	{
		for (auto& lib : versionsJson["libs"])
		{
			std::string libStr = lib;
			if (!std::filesystem::exists(libStr))
			{
				print(std::format("Missing {}! Downloading it now...\n", libStr));
				bytesDownloaded = 0;
				if (DownloadFile(std::format(SERVER_FILES "/core-files/{}", libStr), libStr, bytesDownloaded))
					print(std::format("\tDownloaded ({}).\n", bytesDownloaded));
				else
					print("\tFailed.\n");
			}
		}
	}
}