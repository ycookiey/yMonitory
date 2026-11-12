#include "GpuMonitor.h"
#include <pdhmsg.h>
#include <map>
#include <string>

namespace {
const int NVML_SUCCESS = 0;
}

GpuMonitor::~GpuMonitor() {
    ShutdownNvml();
    if (m_pdhQuery) {
        PdhCloseQuery(m_pdhQuery);
        m_pdhQuery = NULL;
    }
}

bool GpuMonitor::Initialize() {
    if (InitNvml()) return true;
    return InitPdh();
}

bool GpuMonitor::InitNvml() {
    // Load only from the system folder or the NVIDIA install folder, never from the
    // program's own folder or the current folder.
    m_nvml = LoadLibraryExW(L"nvml.dll", NULL, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!m_nvml) {
        // Older NVIDIA drivers install NVML here instead of the system folder.
        WCHAR path[MAX_PATH];
        DWORD len = ExpandEnvironmentStringsW(L"%ProgramFiles%\\NVIDIA Corporation\\NVSMI\\nvml.dll", path, MAX_PATH);
        if (len > 0 && len <= MAX_PATH) {
            m_nvml = LoadLibraryExW(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        }
    }
    if (!m_nvml) return false;

    auto init = (NvmlInitFn)GetProcAddress(m_nvml, "nvmlInit_v2");
    auto getCount = (NvmlGetCountFn)GetProcAddress(m_nvml, "nvmlDeviceGetCount_v2");
    auto getHandle = (NvmlGetHandleFn)GetProcAddress(m_nvml, "nvmlDeviceGetHandleByIndex_v2");
    m_nvmlShutdown = (NvmlShutdownFn)GetProcAddress(m_nvml, "nvmlShutdown");
    m_nvmlGetUtilization = (NvmlGetUtilizationFn)GetProcAddress(m_nvml, "nvmlDeviceGetUtilizationRates");
    if (!init || !getCount || !getHandle || !m_nvmlShutdown || !m_nvmlGetUtilization || init() != NVML_SUCCESS) {
        m_nvmlShutdown = nullptr;
        FreeLibrary(m_nvml);
        m_nvml = NULL;
        return false;
    }

    // Keep only GPUs that report usage. Some GPUs or drivers do not support it; if none
    // does, the performance counters are used instead.
    unsigned int count = 0;
    if (getCount(&count) == NVML_SUCCESS) {
        for (unsigned int i = 0; i < count; i++) {
            void* device = nullptr;
            NvmlUtilization util = {};
            if (getHandle(i, &device) == NVML_SUCCESS && m_nvmlGetUtilization(device, &util) == NVML_SUCCESS) {
                m_nvmlDevices.push_back(device);
            }
        }
    }
    if (m_nvmlDevices.empty()) {
        ShutdownNvml();
        return false;
    }
    return true;
}

void GpuMonitor::ShutdownNvml() {
    if (!m_nvml) return;
    if (m_nvmlShutdown) m_nvmlShutdown();
    FreeLibrary(m_nvml);
    m_nvml = NULL;
    m_nvmlShutdown = nullptr;
    m_nvmlGetUtilization = nullptr;
    m_nvmlDevices.clear();
}

bool GpuMonitor::InitPdh() {
    if (PdhOpenQuery(nullptr, 0, &m_pdhQuery) != ERROR_SUCCESS) return false;
    // One instance per process and engine. Instances come and go with processes; PDH
    // expands the wildcard again on each collection.
    if (PdhAddEnglishCounterW(m_pdhQuery, L"\\GPU Engine(*)\\Utilization Percentage", 0, &m_pdhCounter) != ERROR_SUCCESS) {
        PdhCloseQuery(m_pdhQuery);
        m_pdhQuery = NULL;
        return false;
    }
    // Utilization Percentage is a rate counter and needs a first sample to compare against.
    PdhCollectQueryData(m_pdhQuery);
    return true;
}

bool GpuMonitor::Read(double& percent) {
    if (m_nvml) return ReadNvml(percent);
    if (m_pdhQuery) return ReadPdh(percent);
    return false;
}

bool GpuMonitor::ReadNvml(double& percent) {
    // A call fails, for example, while the driver is being updated. The caller then
    // shows no value instead of the last one.
    bool any = false;
    unsigned int best = 0;
    for (void* device : m_nvmlDevices) {
        NvmlUtilization util = {};
        if (m_nvmlGetUtilization(device, &util) != NVML_SUCCESS) continue;
        if (!any || util.gpu > best) best = util.gpu;
        any = true;
    }
    if (any) percent = best;
    return any;
}

bool GpuMonitor::ReadPdh(double& percent) {
    if (PdhCollectQueryData(m_pdhQuery) != ERROR_SUCCESS) return false;

    DWORD size = 0, count = 0;
    PDH_STATUS status = PdhGetFormattedCounterArrayW(m_pdhCounter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count, nullptr);
    if (status != PDH_MORE_DATA || size == 0) return false;
    m_pdhBuffer.resize(size);
    auto items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(m_pdhBuffer.data());
    if (PdhGetFormattedCounterArrayW(m_pdhCounter, PDH_FMT_DOUBLE | PDH_FMT_NOCAP100, &size, &count, items) != ERROR_SUCCESS) return false;

    // Instance names look like pid_1234_luid_0x..._0x..._phys_0_eng_3_engtype_3D.
    // Task Manager adds up the processes on each engine and shows the busiest engine.
    std::map<std::wstring, double> perEngine;
    for (DWORD i = 0; i < count; i++) {
        // A process that appeared since the last sample has no valid value yet.
        if (items[i].FmtValue.CStatus != PDH_CSTATUS_VALID_DATA && items[i].FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) continue;
        const wchar_t* engine = wcsstr(items[i].szName, L"_luid_");
        if (!engine) continue;
        perEngine[engine] += items[i].FmtValue.doubleValue;
    }
    if (perEngine.empty()) return false;

    double best = 0.0;
    for (const auto& entry : perEngine) {
        if (entry.second > best) best = entry.second;
    }
    percent = best > 100.0 ? 100.0 : best;
    return true;
}
