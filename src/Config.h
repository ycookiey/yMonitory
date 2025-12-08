#pragma once
#include <string>
#include <windows.h>

struct AppConfig {
    int x = -1; // -1 means default/center
    int y = -1;
    int width = 200;
    int height = 100;
    int opacity = 200; // 0-255
    int updateIntervalMs = 1000;
    
    // Colors (RGBA 0xRRGGBBAA format for simplicity in internal logic, but hex string in file)
    // Using D2D1_COLOR_F essentially, but storing as hex strings for easy parsing
    std::wstring colorText = L"#FFFFFFFF";
    std::wstring colorBackground = L"#00000080";
    std::wstring colorGraphCpu = L"#00FFFF80";
    std::wstring colorGraphRam = L"#FF00FF80";

    bool ghostModeEnabled = true;

    // Load from config.ini
    void Load();
    // Save to config.ini
    void Save() const;
};

extern AppConfig g_Config;
