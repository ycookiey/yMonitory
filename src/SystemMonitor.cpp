#include "SystemMonitor.h"
#include <tchar.h>
#include <algorithm>

// Use PdhAddEnglishCounter if available (Vista+) to avoid localization issues
// Typedef for dynamic linking if we wanted to be super safe, but we link pdh.lib so it should be fine.
// Actually PdhAddEnglishCounterW is standard in Windows 10 SDK.

SystemMonitor::SystemMonitor() : 
    m_queryHandle(NULL),
    m_cpuCounter(NULL),
    m_memCounter(NULL),
    m_cpuUsage(0.0),
    m_memoryUsagePercent(0.0)
{
    std::fill(std::begin(m_cpuHistory), std::end(m_cpuHistory), 0.0);
    std::fill(std::begin(m_memoryHistory), std::end(m_memoryHistory), 0.0);
}

SystemMonitor::~SystemMonitor() {
    if (m_queryHandle) {
        PdhCloseQuery(m_queryHandle);
        m_queryHandle = nullptr;
    }
}

bool SystemMonitor::Initialize() {
    PDH_STATUS status = PdhOpenQuery(nullptr, 0, &m_queryHandle);
    if (status != ERROR_SUCCESS) return false;

    // CPU Usage (Total)
    // English name: \Processor(_Total)\% Processor Time
    status = PdhAddEnglishCounter(m_queryHandle, L"\\Processor(_Total)\\% Processor Time", 0, &m_cpuCounter);
    if (status != ERROR_SUCCESS) {
        // Fallback to standard AddCounter if English one fails (unlikely on modern Windows)
         status = PdhAddCounter(m_queryHandle, L"\\Processor(_Total)\\% Processor Time", 0, &m_cpuCounter);
         if (status != ERROR_SUCCESS) return false;
    }

    // Memory Usage (Committed Bytes In Use %)
    // English name: \Memory\% Committed Bytes In Use
    status = PdhAddEnglishCounter(m_queryHandle, L"\\Memory\\% Committed Bytes In Use", 0, &m_memCounter);
    if (status != ERROR_SUCCESS) {
         status = PdhAddCounter(m_queryHandle, L"\\Memory\\% Committed Bytes In Use", 0, &m_memCounter);
         if (status != ERROR_SUCCESS) return false;
    }

    // Initial collect
    PdhCollectQueryData(m_queryHandle);
    return true;
}

void SystemMonitor::Update() {
    if (!m_queryHandle) return;

    PDH_STATUS status = PdhCollectQueryData(m_queryHandle);
    if (status != ERROR_SUCCESS) return;

    PDH_FMT_COUNTERVALUE counterVal;

    // CPU
    status = PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, nullptr, &counterVal);
    if (status == ERROR_SUCCESS) {
        m_cpuUsage = counterVal.doubleValue;
    }

    // Memory
    status = PdhGetFormattedCounterValue(m_memCounter, PDH_FMT_DOUBLE, nullptr, &counterVal);
    if (status == ERROR_SUCCESS) {
        m_memoryUsagePercent = counterVal.doubleValue;
    }

    PushHistory(m_cpuUsage, m_memoryUsagePercent);
}

void SystemMonitor::PushHistory(double cpu, double mem) {
    // Shift history (simple implementation)
    // Optimization: Ring buffer is better, but shifting 60 doubles is negligible.
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        m_cpuHistory[i] = m_cpuHistory[i + 1];
        m_memoryHistory[i] = m_memoryHistory[i + 1];
    }
    m_cpuHistory[HISTORY_SIZE - 1] = cpu;
    m_memoryHistory[HISTORY_SIZE - 1] = mem;
}
