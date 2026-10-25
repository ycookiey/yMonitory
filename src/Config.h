#pragma once
#include <string>
#include <windows.h>

struct AppConfig {
    int x = -1; // -1 places the window near the top-right corner
    int y = -1;
    int width = 200;
    int height = 100;
    int opacity = 200; // 0-255
    int updateIntervalMs = 1000;

    // "#RRGGBBAA" or "#RRGGBB", kept as written in config.ini and parsed when drawing.
    std::wstring colorText = L"#FFFFFFFF";
    std::wstring colorBackground = L"#00000080";
    std::wstring colorGraphCpu = L"#00FFFF80";
    std::wstring colorGraphRam = L"#FF00FF";

    bool ghostModeEnabled = true;
    bool runOnStartup = false;

    // config.ini sits next to the executable.
    void Load();
    // Also adds or removes the HKCU Run value according to runOnStartup.
    void Save() const;
};

extern AppConfig g_Config;
