#include "SystemMonitor.h"
#include <tchar.h>
#include <algorithm>

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

    // Counter paths are localized; PdhAddEnglishCounter accepts the English
    // path on any display language. PdhAddCounter is the fallback.
    status = PdhAddEnglishCounter(m_queryHandle, L"\\Processor(_Total)\\% Processor Time", 0, &m_cpuCounter);
    if (status != ERROR_SUCCESS) {
         status = PdhAddCounter(m_queryHandle, L"\\Processor(_Total)\\% Processor Time", 0, &m_cpuCounter);
         if (status != ERROR_SUCCESS) return false;
    }

    // Committed memory as a share of the commit limit, not physical memory in use.
    status = PdhAddEnglishCounter(m_queryHandle, L"\\Memory\\% Committed Bytes In Use", 0, &m_memCounter);
    if (status != ERROR_SUCCESS) {
         status = PdhAddCounter(m_queryHandle, L"\\Memory\\% Committed Bytes In Use", 0, &m_memCounter);
         if (status != ERROR_SUCCESS) return false;
    }

    // Rate counters such as % Processor Time need a first sample to compare against.
    PdhCollectQueryData(m_queryHandle);
    return true;
}

void SystemMonitor::Update() {
    if (!m_queryHandle) return;

    PDH_STATUS status = PdhCollectQueryData(m_queryHandle);
    if (status != ERROR_SUCCESS) return;

    PDH_FMT_COUNTERVALUE counterVal;

    status = PdhGetFormattedCounterValue(m_cpuCounter, PDH_FMT_DOUBLE, nullptr, &counterVal);
    if (status == ERROR_SUCCESS) {
        m_cpuUsage = counterVal.doubleValue;
    }

    status = PdhGetFormattedCounterValue(m_memCounter, PDH_FMT_DOUBLE, nullptr, &counterVal);
    if (status == ERROR_SUCCESS) {
        m_memoryUsagePercent = counterVal.doubleValue;
    }

    PushHistory(m_cpuUsage, m_memoryUsagePercent);
}

void SystemMonitor::PushHistory(double cpu, double mem) {
    // Shifting instead of a ring buffer keeps the arrays in drawing order.
    // Moving 60 values per sample is negligible work.
    for (int i = 0; i < HISTORY_SIZE - 1; i++) {
        m_cpuHistory[i] = m_cpuHistory[i + 1];
        m_memoryHistory[i] = m_memoryHistory[i + 1];
    }
    m_cpuHistory[HISTORY_SIZE - 1] = cpu;
    m_memoryHistory[HISTORY_SIZE - 1] = mem;
}
