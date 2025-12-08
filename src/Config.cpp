#include "Config.h"
#include "yMonitory.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <map>

AppConfig g_Config;

// Helper to trim whitespace
static std::wstring Trim(const std::wstring& str) {
    size_t first = str.find_first_not_of(L" \t\r\n");
    if (std::wstring::npos == first) return L"";
    size_t last = str.find_last_not_of(L" \t\r\n");
    return str.substr(first, (last - first + 1));
}

// Helper to convert wstring to int with default
static int ParseInt(const std::wstring& str, int def) {
    try {
        return std::stoi(str);
    } catch (...) {
        return def;
    }
}

// Helper to parse bool
static bool ParseBool(const std::wstring& str, bool def) {
    std::wstring lower = str;
    for (auto& c : lower) c = towlower(c);
    if (lower == L"true" || lower == L"1") return true;
    if (lower == L"false" || lower == L"0") return false;
    return def;
}

void AppConfig::Load() {
    // Get executable path
    WCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::filesystem::path exePath(path);
    std::filesystem::path configPath = exePath.parent_path() / yMonitory::CONFIG_FILENAME;

    std::wifstream file(configPath);
    if (!file.is_open()) return;

    std::wstring line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == L';' || line[0] == L'#') continue;

        size_t delimiterPos = line.find(L'=');
        if (delimiterPos == std::wstring::npos) continue;

        std::wstring key = Trim(line.substr(0, delimiterPos));
        std::wstring value = Trim(line.substr(delimiterPos + 1));

        if (key == L"X") x = ParseInt(value, x);
        else if (key == L"Y") y = ParseInt(value, y);
        else if (key == L"Width") width = ParseInt(value, width);
        else if (key == L"Height") height = ParseInt(value, height);
        else if (key == L"Opacity") opacity = ParseInt(value, opacity);
        else if (key == L"UpdateIntervalMs") updateIntervalMs = ParseInt(value, updateIntervalMs);
        else if (key == L"ColorText") colorText = value;
        else if (key == L"ColorBackground") colorBackground = value;
        else if (key == L"ColorGraphCpu") colorGraphCpu = value;
        else if (key == L"ColorGraphRam") colorGraphRam = value;
        else if (key == L"GhostModeEnabled") ghostModeEnabled = ParseBool(value, ghostModeEnabled);
    }
}

void AppConfig::Save() const {
    WCHAR path[MAX_PATH];
    GetModuleFileName(NULL, path, MAX_PATH);
    std::filesystem::path exePath(path);
    std::filesystem::path configPath = exePath.parent_path() / yMonitory::CONFIG_FILENAME;

    std::wofstream file(configPath);
    if (!file.is_open()) return;

    file << L"; yMonitory Configuration\n";
    file << L"X=" << x << L"\n";
    file << L"Y=" << y << L"\n";
    file << L"Width=" << width << L"\n";
    file << L"Height=" << height << L"\n";
    file << L"Opacity=" << opacity << L"\n";
    file << L"UpdateIntervalMs=" << updateIntervalMs << L"\n";
    file << L"ColorText=" << colorText << L"\n";
    file << L"ColorBackground=" << colorBackground << L"\n";
    file << L"ColorGraphCpu=" << colorGraphCpu << L"\n";
    file << L"ColorGraphRam=" << colorGraphRam << L"\n";
    file << L"GhostModeEnabled=" << (ghostModeEnabled ? L"true" : L"false") << L"\n";
}
