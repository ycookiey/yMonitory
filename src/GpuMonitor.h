#pragma once
#include <windows.h>
#include <pdh.h>
#include <vector>

// Overall GPU usage in percent, from one of two sources:
// - NVML (NVIDIA's management library, installed with the NVIDIA driver). Reading it
//   costs almost no CPU. The value is the share of time a kernel was running on the GPU,
//   as nvidia-smi shows it. With several NVIDIA GPUs, the busiest one is shown.
// - Otherwise the "GPU Engine" performance counters, which Task Manager also uses. The
//   value is the busiest engine (3D, copy, video and so on) across all adapters. Reading
//   these counters costs a few milliseconds of CPU per sample.
// D3DKMTQueryStatistics, which some process monitors use, is not used: Microsoft documents
// its structures as reserved for system use, and it cost more CPU per read than the counters.
class GpuMonitor {
public:
    GpuMonitor() = default;
    ~GpuMonitor();
    GpuMonitor(const GpuMonitor&) = delete;
    GpuMonitor& operator=(const GpuMonitor&) = delete;

    // Returns false when neither source is available, for example on a PC without a GPU driver.
    bool Initialize();

    // Returns false when the value could not be read this time.
    bool Read(double& percent);

private:
    bool InitNvml();
    bool InitPdh();
    bool ReadNvml(double& percent);
    bool ReadPdh(double& percent);
    void ShutdownNvml();

    // NVML is loaded at run time so the program also starts on PCs without an NVIDIA driver.
    typedef int (*NvmlInitFn)();
    typedef int (*NvmlShutdownFn)();
    typedef int (*NvmlGetCountFn)(unsigned int*);
    typedef int (*NvmlGetHandleFn)(unsigned int, void**);
    struct NvmlUtilization { unsigned int gpu; unsigned int memory; };
    typedef int (*NvmlGetUtilizationFn)(void*, NvmlUtilization*);

    HMODULE m_nvml = NULL;
    NvmlShutdownFn m_nvmlShutdown = nullptr;
    NvmlGetUtilizationFn m_nvmlGetUtilization = nullptr;
    std::vector<void*> m_nvmlDevices;

    PDH_HQUERY m_pdhQuery = NULL;
    PDH_HCOUNTER m_pdhCounter = NULL;
    std::vector<BYTE> m_pdhBuffer;
};
