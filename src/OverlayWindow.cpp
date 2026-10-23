#include "OverlayWindow.h"
#include "yMonitory.h"
#include "resource.h"
#include <math.h>
#include <sstream>
#include <iomanip>

// Accepts #RRGGBBAA or #RRGGBB. Anything else falls back to white.
static D2D1_COLOR_F ParseColor(const std::wstring& hex) {
    if (hex.empty() || hex[0] != L'#') return D2D1::ColorF(D2D1::ColorF::White);
    
    unsigned long value = 0;
    try {
        value = std::stoul(hex.substr(1), nullptr, 16);
    } catch(...) { return D2D1::ColorF(D2D1::ColorF::White); }

    float r, g, b, a = 1.0f;
    if (hex.length() > 7) {
        r = ((value >> 24) & 0xFF) / 255.0f;
        g = ((value >> 16) & 0xFF) / 255.0f;
        b = ((value >> 8) & 0xFF) / 255.0f;
        a = (value & 0xFF) / 255.0f;
    } else {
        r = ((value >> 16) & 0xFF) / 255.0f;
        g = ((value >> 8) & 0xFF) / 255.0f;
        b = (value & 0xFF) / 255.0f;
    }
    return D2D1::ColorF(r, g, b, a);
}

OverlayWindow::OverlayWindow() :
    m_hwnd(NULL),
    m_hInstance(NULL),
    m_pD2DFactory(NULL),
    m_pRenderTarget(NULL),
    m_pTextBrush(NULL),
    m_pGraphCpuBrush(NULL),
    m_pGraphRamBrush(NULL),
    m_pGraphCpuGradient(NULL),
    m_pGraphRamGradient(NULL),
    m_pDWriteFactory(NULL),
    m_pTextFormat(NULL),
    m_currentOpacity(255)
{
}

OverlayWindow::~OverlayWindow() {
    DiscardDeviceResources();
    SafeRelease(&m_pDWriteFactory);
    SafeRelease(&m_pD2DFactory);
    SafeRelease(&m_pTextFormat);
}

bool OverlayWindow::Initialize(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    m_systemMonitor.Initialize();

    if (FAILED(CreateDeviceIndependentResources())) {
        return false;
    }

    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = OverlayWindow::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = hInstance;
    wcex.hbrBackground = NULL; // The background is drawn with Direct2D
    wcex.lpszMenuName = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = yMonitory::WINDOW_CLASS_NAME;

    RegisterClassEx(&wcex);

    // -1 places the window near the top-right corner of the primary monitor.
    int x = (g_Config.x == -1) ? (GetSystemMetrics(SM_CXSCREEN) - g_Config.width - 20) : g_Config.x;
    int y = (g_Config.y == -1) ? 20 : g_Config.y;

    // WS_EX_TOOLWINDOW keeps the window off the taskbar. WS_EX_TRANSPARENT lets
    // clicks pass through until Ctrl is held (see UpdateGhostMode).
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT;
    
    m_hwnd = CreateWindowEx(
        exStyle,
        yMonitory::WINDOW_CLASS_NAME,
        yMonitory::APP_NAME,
        WS_POPUP,
        x, y,
        g_Config.width, g_Config.height,
        NULL,
        NULL,
        hInstance,
        this
    );

    if (!m_hwnd) {
        return false;
    }

    // Timer 1 samples the counters. Timer 2 runs faster so that the fade and
    // the Ctrl check react without waiting for the next sample.
    SetTimer(m_hwnd, 1, g_Config.updateIntervalMs, NULL);
    SetTimer(m_hwnd, 2, 50, NULL);

    ShowWindow(m_hwnd, SW_SHOW);
    SetLayeredWindowAttributes(m_hwnd, 0, (BYTE)m_currentOpacity, LWA_ALPHA);
    UpdateWindow(m_hwnd);

    AddTrayIcon();

    return true;
}

HRESULT OverlayWindow::CreateDeviceIndependentResources() {
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &m_pD2DFactory);
    if (SUCCEEDED(hr)) {
        hr = DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&m_pDWriteFactory)
        );
    }
    if (SUCCEEDED(hr)) {
        hr = m_pDWriteFactory->CreateTextFormat(
            L"Segoe UI",
            NULL,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            12.0f,
            L"en-us",
            &m_pTextFormat
        );
    }
    return hr;
}

HRESULT OverlayWindow::CreateDeviceResources() {
    HRESULT hr = S_OK;
    if (!m_pRenderTarget) {
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

        hr = m_pD2DFactory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
            ),
            D2D1::HwndRenderTargetProperties(m_hwnd, size),
            &m_pRenderTarget
        );

        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(ParseColor(g_Config.colorText), &m_pTextBrush);
        }
        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(ParseColor(g_Config.colorGraphCpu), &m_pGraphCpuBrush);
        }
        if (SUCCEEDED(hr)) {
            hr = m_pRenderTarget->CreateSolidColorBrush(ParseColor(g_Config.colorGraphRam), &m_pGraphRamBrush);
        }

        // The gradient's start and end points are set per graph in OnRender.
        if (SUCCEEDED(hr)) {
            D2D1_GRADIENT_STOP stops[2];
            D2D1_COLOR_F col = ParseColor(g_Config.colorGraphCpu);
            stops[0].color = col;
            stops[0].position = 0.0f;
            stops[1].color = col;
            stops[1].color.a = 0.0f;
            stops[1].position = 1.0f;

            ID2D1GradientStopCollection* pStops = NULL;
            hr = m_pRenderTarget->CreateGradientStopCollection(stops, 2, &pStops);
            if (SUCCEEDED(hr)) {
                hr = m_pRenderTarget->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, size.height)),
                    pStops,
                    &m_pGraphCpuGradient
                );
                pStops->Release();
            }
        }
        if (SUCCEEDED(hr)) {
            D2D1_GRADIENT_STOP stops[2];
            D2D1_COLOR_F col = ParseColor(g_Config.colorGraphRam);
            stops[0].color = col;
            stops[0].position = 0.0f;
            stops[1].color = col;
            stops[1].color.a = 0.0f;
            stops[1].position = 1.0f;

            ID2D1GradientStopCollection* pStops = NULL;
            hr = m_pRenderTarget->CreateGradientStopCollection(stops, 2, &pStops);
            if (SUCCEEDED(hr)) {
                hr = m_pRenderTarget->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, size.height)), 
                    pStops,
                    &m_pGraphRamGradient
                );
                pStops->Release();
            }
        }
    }
    return hr;
}

void OverlayWindow::DiscardDeviceResources() {
    SafeRelease(&m_pRenderTarget);
    SafeRelease(&m_pTextBrush);
    SafeRelease(&m_pGraphCpuBrush);
    SafeRelease(&m_pGraphRamBrush);
    SafeRelease(&m_pGraphCpuGradient);
    SafeRelease(&m_pGraphRamGradient);
}

void OverlayWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

LRESULT CALLBACK OverlayWindow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    OverlayWindow* pThis = NULL;
    if (message == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (OverlayWindow*)pCreate->lpCreateParams;
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (OverlayWindow*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        switch (message) {
        case WM_PAINT:
            pThis->OnRender();
            ValidateRect(hWnd, NULL);
            return 0;
        case WM_SIZE:
            pThis->OnResize(LOWORD(lParam), HIWORD(lParam));
            return 0;
        case WM_TIMER:
            if (wParam == 1) pThis->OnTimer();
            if (wParam == 2) pThis->UpdateGhostMode();
            return 0;
        case WM_DESTROY:
            pThis->RemoveTrayIcon();
            PostQuitMessage(0);
            return 0;
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hWnd);
                pThis->ShowContextMenu(pt.x, pt.y);
            }
            return 0;
        case WM_RBUTTONUP:
            if (pThis->m_isDragEnabled) {
                POINT pt;
                GetCursorPos(&pt);
                pThis->ShowContextMenu(pt.x, pt.y);
            }
            return 0;
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_MENU_EXIT) {
                DestroyWindow(hWnd);
            }
            else if (LOWORD(wParam) == ID_MENU_STARTUP) {
                g_Config.runOnStartup = !g_Config.runOnStartup;
                g_Config.Save(); // Save also writes the Run registry value
            }
            return 0;
        case WM_NCHITTEST:
             // HTCLIENT rather than HTCAPTION, so that a right click reaches
             // WM_RBUTTONUP and opens the menu. Dragging starts from WM_LBUTTONDOWN.
             return HTCLIENT;
        case WM_LBUTTONDOWN:
             if (pThis->m_isDragEnabled) {
                 ReleaseCapture();
                 SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
             }
             return 0;
        }
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

void OverlayWindow::OnResize(UINT width, UINT height) {
    if (m_pRenderTarget) {
        m_pRenderTarget->Resize(D2D1::SizeU(width, height));
    }
}

HRESULT OverlayWindow::OnRender() {
    HRESULT hr = S_OK;
    hr = CreateDeviceResources();
    if (FAILED(hr)) {
        return hr;
    }
    
    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    
    // The window-wide opacity (Opacity and the ghost fade) comes from
    // SetLayeredWindowAttributes. The background is a filled rectangle drawn here.
    D2D1_COLOR_F bgCol = ParseColor(g_Config.colorBackground);

    m_pRenderTarget->Clear(D2D1::ColorF(0, 0.0f));

    D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();

    ID2D1SolidColorBrush* pBgBrush = NULL;
    hr = m_pRenderTarget->CreateSolidColorBrush(bgCol, &pBgBrush);
    
    if (SUCCEEDED(hr) && pBgBrush) {
        m_pRenderTarget->FillRectangle(D2D1::RectF(0, 0, rtSize.width, rtSize.height), pBgBrush);
        pBgBrush->Release();
    }

    std::wstringstream ss;
    ss << L"CPU: " << std::fixed << std::setprecision(1) << m_systemMonitor.GetCpuUsage() << L"%\n";
    ss << L"RAM: " << std::fixed << std::setprecision(1) << m_systemMonitor.GetMemoryUsagePercent() << L"%";
    std::wstring text = ss.str();

    m_pRenderTarget->DrawText(
        text.c_str(),
        (UINT32)text.length(),
        m_pTextFormat,
        D2D1::RectF(5, 5, rtSize.width, rtSize.height),
        m_pTextBrush
    );

    // CPU in the top half, memory in the bottom half. 0-100% maps to the half's height.
    const double* cpuHist = m_systemMonitor.GetCpuHistory();
    float xStep = (rtSize.width - 10) / (SystemMonitor::HISTORY_SIZE - 1);
    float yBase = rtSize.height / 2 - 5;
    float yScale = (rtSize.height / 2 - 10) / 100.0f;

    auto DrawSmoothGraph = [&](const double* data, float yBasePos, ID2D1Brush* strokeBrush, ID2D1Brush* fillBrush) {
        if (!strokeBrush) return;

        ID2D1PathGeometry* pPath = NULL;
        m_pD2DFactory->CreatePathGeometry(&pPath);
        if (!pPath) return;

        ID2D1GeometrySink* pSink = NULL;
        pPath->Open(&pSink);

        D2D1_POINT_2F p0 = D2D1::Point2F(5, yBasePos - (float)data[0] * yScale);
        pSink->BeginFigure(p0, D2D1_FIGURE_BEGIN_FILLED);

        // Control points half a step away horizontally give a curve that is flat at
        // every sample, so it never overshoots 0% or 100% between samples.
        // The open curve is stroked; a separate closed path below is filled.
        for(int i=0; i<SystemMonitor::HISTORY_SIZE - 1; i++) {
             D2D1_POINT_2F pStart = D2D1::Point2F(5 + i * xStep, yBasePos - (float)data[i] * yScale);
             D2D1_POINT_2F pEnd = D2D1::Point2F(5 + (i+1) * xStep, yBasePos - (float)data[i+1] * yScale);
             
             D2D1_POINT_2F c1 = D2D1::Point2F(pStart.x + xStep * 0.5f, pStart.y);
             D2D1_POINT_2F c2 = D2D1::Point2F(pEnd.x - xStep * 0.5f, pEnd.y);
             
             pSink->AddBezier(D2D1::BezierSegment(c1, c2, pEnd));
        }

        pSink->EndFigure(D2D1_FIGURE_END_OPEN);
        pSink->Close();
        pSink->Release();
        
        m_pRenderTarget->DrawGeometry(pPath, strokeBrush, 1.5f);

        if (fillBrush) {
            ID2D1PathGeometry* pFillPath = NULL;
            m_pD2DFactory->CreatePathGeometry(&pFillPath);
            if(pFillPath) {
                pFillPath->Open(&pSink);
                pSink->BeginFigure(D2D1::Point2F(5, yBasePos), D2D1_FIGURE_BEGIN_FILLED);
                pSink->AddLine(p0);

                for(int i=0; i<SystemMonitor::HISTORY_SIZE - 1; i++) {
                     D2D1_POINT_2F pStart = D2D1::Point2F(5 + i * xStep, yBasePos - (float)data[i] * yScale);
                     D2D1_POINT_2F pEnd = D2D1::Point2F(5 + (i+1) * xStep, yBasePos - (float)data[i+1] * yScale);
                     D2D1_POINT_2F c1 = D2D1::Point2F(pStart.x + xStep * 0.5f, pStart.y);
                     D2D1_POINT_2F c2 = D2D1::Point2F(pEnd.x - xStep * 0.5f, pEnd.y);
                     pSink->AddBezier(D2D1::BezierSegment(c1, c2, pEnd));
                }
                
                pSink->AddLine(D2D1::Point2F(5 + (SystemMonitor::HISTORY_SIZE - 1) * xStep, yBasePos));
                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pSink->Close(); // TODO: Check result
                pSink->Release();


                // static_cast, not dynamic_cast: Direct2D objects carry no RTTI that
                // dynamic_cast can rely on. Both callers pass a linear gradient brush.
                if (fillBrush) {
                     ID2D1LinearGradientBrush* grad = static_cast<ID2D1LinearGradientBrush*>(fillBrush);
                     grad->SetStartPoint(D2D1::Point2F(0, yBasePos - 100 * yScale));
                     grad->SetEndPoint(D2D1::Point2F(0, yBasePos));
                }

                m_pRenderTarget->FillGeometry(pFillPath, fillBrush);
                pFillPath->Release();
            }
        }
        pPath->Release();
    };

    DrawSmoothGraph(cpuHist, yBase, m_pGraphCpuBrush, m_pGraphCpuGradient);
    
    yBase = rtSize.height - 5;
    DrawSmoothGraph(m_systemMonitor.GetMemoryHistory(), yBase, m_pGraphRamBrush, m_pGraphRamGradient);
    
    hr = m_pRenderTarget->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET) {
        hr = S_OK;
        DiscardDeviceResources();
    }
    return hr;
}

void OverlayWindow::OnTimer() {
    m_systemMonitor.Update();
    InvalidateRect(m_hwnd, NULL, FALSE);
}

void OverlayWindow::UpdateGhostMode() {
    // GetAsyncKeyState, because the overlay usually has no keyboard focus to receive WM_KEYDOWN.
    bool isCtrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

    if (isCtrlPressed != m_isDragEnabled) {
        m_isDragEnabled = isCtrlPressed;
        LONG exStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);
        if (m_isDragEnabled) {
             exStyle &= ~WS_EX_TRANSPARENT;
        } else {
             exStyle |= WS_EX_TRANSPARENT;
        }
        SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle);
    }

    int targetOpacity = g_Config.opacity;
    if (g_Config.ghostModeEnabled && !m_isDragEnabled) {
        POINT pt;
        GetCursorPos(&pt);
        RECT rc;
        GetWindowRect(m_hwnd, &rc);
        
        // Horizontal plus vertical distance from the window's edge; 0 inside it.
        int dist = 0;
        if (pt.x < rc.left) dist += (rc.left - pt.x);
        else if (pt.x > rc.right) dist += (pt.x - rc.right);
        
        if (pt.y < rc.top) dist += (rc.top - pt.y);
        else if (pt.y > rc.bottom) dist += (pt.y - rc.bottom);

        // Within 100 px, fade linearly from Opacity down to 10.
        if (dist < 100) {
            targetOpacity = 10 + (int)((g_Config.opacity - 10) * (dist / 100.0f));
        }
    }

    if (m_currentOpacity != targetOpacity) {
        int step = 25;
        if (m_currentOpacity < targetOpacity) m_currentOpacity = min(m_currentOpacity + step, targetOpacity);
        else m_currentOpacity = max(m_currentOpacity - step, targetOpacity);
        
        SetLayeredWindowAttributes(m_hwnd, 0, (BYTE)m_currentOpacity, LWA_ALPHA);
        
        // TODO: The position is recorded only while the opacity is still changing,
        // which ends a few hundred ms after Ctrl is pressed. The end of a longer drag,
        // or a drag that starts after the fade has finished, is not saved.
        if (m_isDragEnabled) {
            RECT rc;
            GetWindowRect(m_hwnd, &rc);
            g_Config.x = rc.left;
            g_Config.y = rc.top;
        }
    }
}

void OverlayWindow::AddTrayIcon() {
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = m_hwnd;
    nid.uID = ID_TRAY_ICON;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // TODO: Use the app's own icon once yMonitory.rc has one
    wcscpy_s(nid.szTip, L"yMonitory");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

void OverlayWindow::RemoveTrayIcon() {
    NOTIFYICONDATA nid = { sizeof(nid) };
    nid.hWnd = m_hwnd;
    nid.uID = ID_TRAY_ICON;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void OverlayWindow::ShowContextMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    
    AppendMenu(hMenu, MF_STRING | (g_Config.runOnStartup ? MF_CHECKED : MF_UNCHECKED), ID_MENU_STARTUP, L"スタートアップ登録");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_MENU_EXIT, L"Exit");
    
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);
}
