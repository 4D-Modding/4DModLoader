#pragma once

#include <iostream>
#include <wininet.h>
#include <string>
#include <Windows.h>
#include <fstream>

void splitUrl(const std::string& url, std::string& hostname, std::string& path) {
    size_t protocolEnd = url.find("://");
    size_t pathStart = url.find('/', protocolEnd + 3);

    if (protocolEnd != std::string::npos && pathStart != std::string::npos) {
        hostname = url.substr(protocolEnd + 3, pathStart - protocolEnd - 3);
        path = url.substr(pathStart + 1);
    }
}

std::vector<unsigned char> SendPostRequest(const std::string& hostname, const std::string& path, const std::string& postData, int64_t& bytesCount)
{
    const char* headers = "Content-Type: application/x-www-form-urlencoded\r\n";
    LPVOID myMessage = (LPVOID)postData.c_str();
    HINTERNET hInternet = InternetOpenA("4DModLoader", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (hInternet == NULL)
    {
        bytesCount = -1;
        return {};
    }

    HINTERNET hConnection = InternetConnectA(hInternet, hostname.c_str(), 443, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
    if (hConnection == NULL)
    {
        InternetCloseHandle(hInternet);
        bytesCount = -2;
        return {};
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnection, "POST", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE, 1);
    if (hRequest == NULL)
    {
        InternetCloseHandle(hConnection);
        InternetCloseHandle(hInternet);
        bytesCount = -3;
        return {};
    }

    if (!HttpSendRequestA(hRequest, headers, strlen(headers), myMessage, postData.length()))
    {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnection);
        InternetCloseHandle(hInternet);
        bytesCount = -4;
        return {};
    }

    std::vector<unsigned char> response;
    const int nBuffSize = 4096;
    unsigned char buff[nBuffSize];

    BOOL bKeepReading = true;
    DWORD dwBytesRead = -1;

    while (bKeepReading && dwBytesRead != 0)
    {
        bKeepReading = InternetReadFile(hRequest, buff, nBuffSize, &dwBytesRead);
        if (bKeepReading && dwBytesRead > 0)
        {
            response.insert(response.end(), buff, buff + dwBytesRead);
            bytesCount += dwBytesRead;
        }
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnection);
    InternetCloseHandle(hInternet);

    response.push_back('\0');

    return response;
}
std::vector<unsigned char> SendGetRequest(const std::string& hostname, const std::string& path, const std::string& postData, int64_t& bytesCount)
{
    const char* headers = "Content-Type: application/x-www-form-urlencoded\r\n";
    LPVOID myMessage = (LPVOID)postData.c_str();
    HINTERNET hInternet = InternetOpenA("4DModLoader", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (hInternet == NULL)
    {
        bytesCount = -1;
        return {};
    }

    HINTERNET hConnection = InternetConnectA(hInternet, hostname.c_str(), 443, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 1);
    if (hConnection == NULL)
    {
        InternetCloseHandle(hInternet);
        bytesCount = -2;
        return {};
    }

    HINTERNET hRequest = HttpOpenRequestA(hConnection, "GET", path.c_str(), NULL, NULL, NULL, INTERNET_FLAG_SECURE, 1);
    if (hRequest == NULL)
    {
        InternetCloseHandle(hConnection);
        InternetCloseHandle(hInternet);
        bytesCount = -3;
        return {};
    }

    if (!HttpSendRequestA(hRequest, headers, strlen(headers), myMessage, postData.length()))
    {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnection);
        InternetCloseHandle(hInternet);
        bytesCount = -4;
        return {};
    }

    std::vector<unsigned char> response;
    const int nBuffSize = 4096;
    unsigned char buff[nBuffSize];

    BOOL bKeepReading = true;
    DWORD dwBytesRead = -1;

    while (bKeepReading && dwBytesRead != 0)
    {
        bKeepReading = InternetReadFile(hRequest, buff, nBuffSize, &dwBytesRead);
        if (bKeepReading && dwBytesRead > 0)
        {
            response.insert(response.end(), buff, buff + dwBytesRead);
            bytesCount += dwBytesRead;
        }
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnection);
    InternetCloseHandle(hInternet);

    response.push_back('\0');

    return response;
}

std::string SendPostRequestStr(const std::string& hostname, const std::string& path, const std::string& postData)
{
    int64_t bytesCount = 0;
    std::vector<unsigned char> data = SendPostRequest(
        hostname,
        path,
        postData, 
        bytesCount);
    if (bytesCount <= 0)
        return "";
    return { reinterpret_cast<char*>(data.data()) };
}
std::string SendGetRequestStr(const std::string& hostname, const std::string& path, const std::string& postData)
{
    int64_t bytesCount = 0;
    std::vector<unsigned char> data = SendGetRequest(
        hostname,
        path,
        postData,
        bytesCount);
    if (bytesCount <= 0)
        return "";
    return { reinterpret_cast<char*>(data.data()) };
}

std::string POST(const std::string& url, const std::string& data)
{
    std::string hostname;
    std::string path;
    splitUrl(url, hostname, path);
    return SendPostRequestStr(hostname, path, data);
}

std::string GET(const std::string& url, const std::string& data)
{
    std::string hostname;
    std::string path;
    splitUrl(url, hostname, path);
    return SendGetRequestStr(hostname, path, data);
}

bool DownloadFile(const std::string& url, const std::string& fileName, int64_t& bytesDownloaded)
{
    int64_t v = 0;
    std::string hostname;
    std::string path;
    splitUrl(url, hostname, path);

    std::vector<unsigned char> fileData = SendGetRequest(hostname, path, "", v);
    if (v <= 8)
        return false;

    std::ofstream a(fileName, std::ios::binary);
    if (a.is_open())
    {
        a.write(reinterpret_cast<const char*>(fileData.data()), v);
        a.close();
    	bytesDownloaded = v;
        return true;
    }
    return false;
}
bool DownloadFile(const std::string& url, const std::string& fileName)
{
    int64_t v = 0;
    return DownloadFile(url, fileName, v);
}