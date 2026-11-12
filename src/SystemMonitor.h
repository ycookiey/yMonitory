#pragma once
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <string>
#include "GpuMonitor.h"

// Requires linking with pdh.lib.

class SystemMonitor {
public:
    SystemMonitor();
    ~SystemMonitor();

    // With enableGpu false, no GPU source is loaded or read.
    bool Initialize(bool enableGpu);

    void Update();

    double GetCpuUsage() const { return m_cpuUsage; }
    // Share of physical memory in use, as Task Manager shows it.
    double GetMemoryUsagePercent() const { return m_memoryUsagePercent; }
    // Committed memory as a share of the commit limit (physical memory plus page files).
    double GetCommitUsagePercent() const { return m_commitUsagePercent; }
    // False when GPU usage is turned off or no GPU source could be loaded.
    bool HasGpu() const { return m_hasGpu; }
    // False when the latest GPU read failed; GetGpuUsage then has no current value.
    bool IsGpuValid() const { return m_gpuValid; }
    double GetGpuUsage() const { return m_gpuUsage; }

    // Oldest sample first; the newest is at HISTORY_SIZE - 1.
    static const int HISTORY_SIZE = 60;
    const double* GetCpuHistory() const { return m_cpuHistory; }
    const double* GetMemoryHistory() const { return m_memoryHistory; }
    const double* GetCommitHistory() const { return m_commitHistory; }
    const double* GetGpuHistory() const { return m_gpuHistory; }

private:
    PDH_HQUERY m_queryHandle = nullptr;
    PDH_HCOUNTER m_cpuCounter = nullptr;
    GpuMonitor m_gpu;
    bool m_hasGpu = false;
    bool m_gpuValid = false;
    double m_gpuUsage = 0.0;
    double m_gpuHistory[HISTORY_SIZE] = {0};

    double m_cpuUsage = 0.0;
    double m_memoryUsagePercent = 0.0;
    double m_commitUsagePercent = 0.0;

    double m_cpuHistory[HISTORY_SIZE] = {0};
    double m_memoryHistory[HISTORY_SIZE] = {0};
    double m_commitHistory[HISTORY_SIZE] = {0};

    void PushHistory(double cpu, double mem, double commit, double gpu);
};
