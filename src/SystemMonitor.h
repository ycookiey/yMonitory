#pragma once
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <string>

// Link with pdh.lib (handled in build.bat)

class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    // Initialize PDH counters
    bool Initialize();

    // Update counters (call this periodically, usually once per second)
    void Update();

    // Get latest values
    double GetCpuUsage() const { return m_cpuUsage; }
    double GetMemoryUsagePercent() const { return m_memoryUsagePercent; }
    
    // History for graph (last N points)
    // For simplicity, we can store history here or in OverlayWindow.
    // Storing here makes SystemMonitor the source of truth.
    // Assuming 60 points of history.
    static const int HISTORY_SIZE = 60;
    const double* GetCpuHistory() const { return m_cpuHistory; }
    const double* GetMemoryHistory() const { return m_memoryHistory; }

private:
    PDH_HQUERY m_queryHandle = nullptr;
    PDH_HCOUNTER m_cpuCounter = nullptr;
    PDH_HCOUNTER m_memCounter = nullptr;

    double m_cpuUsage = 0.0;
    double m_memoryUsagePercent = 0.0;

    double m_cpuHistory[HISTORY_SIZE] = {0};
    double m_memoryHistory[HISTORY_SIZE] = {0};
    int m_historyIndex = 0; // Ring buffer index or shift? Shift is easier for drawing.

    void PushHistory(double cpu, double mem);
};
