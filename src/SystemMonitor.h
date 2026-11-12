#pragma once
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <string>

// Requires linking with pdh.lib.

class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    bool Initialize();

    void Update();

    double GetCpuUsage() const { return m_cpuUsage; }
    // Share of physical memory in use, as Task Manager shows it.
    double GetMemoryUsagePercent() const { return m_memoryUsagePercent; }
    // Committed memory as a share of the commit limit (physical memory plus page files).
    double GetCommitUsagePercent() const { return m_commitUsagePercent; }

    // Oldest sample first; the newest is at HISTORY_SIZE - 1.
    static const int HISTORY_SIZE = 60;
    const double* GetCpuHistory() const { return m_cpuHistory; }
    const double* GetMemoryHistory() const { return m_memoryHistory; }
    const double* GetCommitHistory() const { return m_commitHistory; }

private:
    PDH_HQUERY m_queryHandle = nullptr;
    PDH_HCOUNTER m_cpuCounter = nullptr;

    double m_cpuUsage = 0.0;
    double m_memoryUsagePercent = 0.0;
    double m_commitUsagePercent = 0.0;

    double m_cpuHistory[HISTORY_SIZE] = {0};
    double m_memoryHistory[HISTORY_SIZE] = {0};
    double m_commitHistory[HISTORY_SIZE] = {0};

    void PushHistory(double cpu, double mem, double commit);
};
