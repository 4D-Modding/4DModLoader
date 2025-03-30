#pragma once

#define SERVER_PHP "https://4d-modding.com"
#define SERVER_FILES "https://4d-modding.com"

#include "requests.h"
#include "json.hpp"
#include <fstream>

inline static constexpr uint64_t fdm014xOffset = 0x159BB8;
inline static constexpr int fdm0140InitCrashRptStartValue = 0x74696E49;
inline static constexpr int fdm0141AValue = 0x4005B220;
inline static constexpr uint64_t fdm0214VerOffset = 0x2434C8; // "0.2.1.4 Alpha" used in the title screen
inline static constexpr std::string fdm0214VerName = "0.2.1.4 Alpha";

inline static bool consoleOpen = false;

extern std::string fdmExe;

inline static void print(const std::string& msg)
{
	if (!consoleOpen)
	{
		AllocConsole();

		FILE* fpout;
		FILE* fpin;
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

void AutoUpdate()
{
	// check if the server is down or the network is not available
	{
		if (!InternetCheckConnectionA(SERVER_PHP, FLAG_ICC_FORCE_CONNECTION, 0))
			return;
		if (!InternetCheckConnectionA(SERVER_FILES, FLAG_ICC_FORCE_CONNECTION, 0))
			return;
	}

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

	printf("New 4DModLoader v%s is available for 4D Miner v%s\n", verNum.c_str(), vers[1].c_str());

	if (MessageBoxA(0, std::format("New 4DModLoader v{} is available for 4D Miner v{}.\nWould you like to update?\n(you can turn this off in the game Settings)", verNum, vers[1]).c_str(), "Auto-Updater", MB_ICONQUESTION | MB_OKCANCEL) != IDOK)
		return;

	print("Downloading new files...\n\n");

	int64_t bytesDownloaded = 0;
	for(auto& fileUrl : updateJson["versionFiles"])
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
		if(DownloadFile(str, outPath, bytesDownloaded))
			print(std::format("\tDownloaded ({}).\n", bytesDownloaded));
		else
			print("\tFailed.\n");
	}
}
void CheckForLibs()
{
	// check if the server is down or the network is not available
	{
		if (!InternetCheckConnectionA(SERVER_PHP, FLAG_ICC_FORCE_CONNECTION, 0))
			return;
		if (!InternetCheckConnectionA(SERVER_FILES, FLAG_ICC_FORCE_CONNECTION, 0))
			return;
	}

	std::string result = GET(SERVER_FILES "/core-files/versions.json", "");

	if (result.size() <= 6) return;

	nlohmann::json versionsJson = nlohmann::json::parse(result);

	int64_t bytesDownloaded = 0;
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