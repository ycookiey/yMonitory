#pragma once

#include <windows.h>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#include "Config.h"
#include "SystemMonitor.h"

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_ICON 1

template<class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease) {
    if (*ppInterfaceToRelease != NULL) {
        (*ppInterfaceToRelease)->Release();
        (*ppInterfaceToRelease) = NULL;
    }
}

class OverlayWindow {
public:
    OverlayWindow();
    ~OverlayWindow();

    bool Initialize(HINSTANCE hInstance);
    void RunMessageLoop();

private:
    // Window Management
    static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
    void OnResize(UINT width, UINT height);
    static void CALLBACK OnForegroundChanged(HWINEVENTHOOK hook, DWORD event, HWND hWnd, LONG idObject, LONG idChild, DWORD thread, DWORD time);
    void EnsureTopmost();
    
    // Direct2D / DirectWrite
    HRESULT CreateDeviceIndependentResources();
    HRESULT CreateDeviceResources();
    void DiscardDeviceResources();
    HRESULT OnRender();

    // Logic
    void OnTimer();
    // Toggles click-through with Ctrl and fades the window near the pointer.
    void UpdateGhostMode();
    void ShowContextMenu(int x, int y);
    void AddTrayIcon();
    void RemoveTrayIcon();
    void UpdateTrayIcon(); // Swap icon after a taskbar theme change
    HICON LoadTrayIcon();

    static OverlayWindow* s_instance; // For OnForegroundChanged, which has no user data
    HWINEVENTHOOK m_foregroundHook = NULL;
    HWND m_hwnd;
    HINSTANCE m_hInstance;
    HICON m_hTrayIcon;

    // Direct2D Resources
    ID2D1Factory* m_pD2DFactory;
    ID2D1HwndRenderTarget* m_pRenderTarget;
    ID2D1SolidColorBrush* m_pTextBrush;
    ID2D1SolidColorBrush* m_pGraphCpuBrush;
    ID2D1SolidColorBrush* m_pGraphRamBrush;
    ID2D1LinearGradientBrush* m_pGraphCpuGradient;
    ID2D1LinearGradientBrush* m_pGraphRamGradient;
    
    // DirectWrite Resources
    IDWriteFactory* m_pDWriteFactory;
    IDWriteTextFormat* m_pTextFormat;

    // Components
    SystemMonitor m_systemMonitor;

    // State
    bool m_isDragEnabled = false; // True while Ctrl is held
    POINT m_lastMousePos = {0, 0};
    int m_currentOpacity = 255;
};
