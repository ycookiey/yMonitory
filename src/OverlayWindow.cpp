#include "OverlayWindow.h"
#include "yMonitory.h"
#include "resource.h" 
#include <math.h>
#include <sstream>
#include <iomanip>

// Helper to parse hex color string to D2D1_COLOR_F
// Format: #RRGGBBAA or #RRGGBB
static D2D1_COLOR_F ParseColor(const std::wstring& hex) {
    if (hex.empty() || hex[0] != L'#') return D2D1::ColorF(D2D1::ColorF::White);
    
    unsigned long value = 0;
    try {
        value = std::stoul(hex.substr(1), nullptr, 16);
    } catch(...) { return D2D1::ColorF(D2D1::ColorF::White); }

    float r, g, b, a = 1.0f;
    if (hex.length() > 7) { // With Alpha
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

    if (FAILED(CreateDeviceIndependentResources())) return false;

    // Window Class
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = OverlayWindow::WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(LONG_PTR);
    wcex.hInstance = hInstance;
    wcex.hbrBackground = NULL; // Handle background in D2D
    wcex.lpszMenuName = NULL;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.lpszClassName = yMonitory::WINDOW_CLASS_NAME;

    RegisterClassEx(&wcex);

    // Initial Position
    int x = (g_Config.x == -1) ? (GetSystemMetrics(SM_CXSCREEN) - g_Config.width - 20) : g_Config.x;
    int y = (g_Config.y == -1) ? 20 : g_Config.y;

    // Window Style: Layered, ToolWindow (no taskbar), TopMost
    // Initial style includes TRANSPARENT for click-through
    DWORD exStyle = WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_TRANSPARENT;
    
    m_hwnd = CreateWindowEx(
        exStyle,
        yMonitory::WINDOW_CLASS_NAME,
        yMonitory::APP_NAME,
        WS_POPUP, // Frameless
        x, y,
        g_Config.width, g_Config.height,
        NULL,
        NULL,
        hInstance,
        this
    );

    if (!m_hwnd) return false;

    // Setup Timer for Update
    SetTimer(m_hwnd, 1, g_Config.updateIntervalMs, NULL);
    // Setup Timer for Ghost Mode / Animation (higher fps)
    SetTimer(m_hwnd, 2, 50, NULL); 

    ShowWindow(m_hwnd, SW_SHOW);
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
            L"Segoe UI", // Fallback font, ideally use config
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

        // Create Gradients
        if (SUCCEEDED(hr)) {
            D2D1_GRADIENT_STOP stops[2];
            D2D1_COLOR_F col = ParseColor(g_Config.colorGraphCpu);
            stops[0].color = col;
            stops[0].position = 0.0f;
            stops[1].color = col; 
            stops[1].color.a = 0.0f; // Fade out
            stops[1].position = 1.0f;

            ID2D1GradientStopCollection* pStops = NULL;
            hr = m_pRenderTarget->CreateGradientStopCollection(stops, 2, &pStops);
            if (SUCCEEDED(hr)) {
                hr = m_pRenderTarget->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, size.height)), // Temp range
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
            stops[1].color.a = 0.0f; // Fade out
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
            if (wParam == 2) pThis->UpdateGhostMode(); // Separated for smooth opacity/checking
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
                g_Config.Save(); // Triggers registry update
            }
            return 0;
        case WM_NCHITTEST:
             // To allow dragging, we need to return HTCAPTION when clicking background
             // But we are TRANSPARENT by default.
             // When Ctrl-mode is active, we remove TRANSPARENT, so this is called.
             // We can return HTCAPTION to allow dragging anywhere.
             // return HTCAPTION; // Disabled to handle clicks manually
             return HTCLIENT; 
        case WM_LBUTTONDOWN:
             if (pThis->m_isDragEnabled) {
                 ReleaseCapture();
                 SendMessage(hWnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
             }
             return 0;
        case WM_KEYDOWN:
             // This might not be received if we don't have focus, 
             // but UpdateGhostMode checks GetAsyncKeyState for global Ctrl detection.
             break;
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
    if (FAILED(hr)) return hr;

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->SetTransform(D2D1::Matrix3x2F::Identity());
    
    // Background color
    D2D1_COLOR_F bgCol = ParseColor(g_Config.colorBackground);
    // Apply opacity (premultiplied alpha manual calculation not needed if we use Clear with a color that has alpha?
    // D2D1 RenderTarget "Clear" ignores the alpha if window is Layered?
    // For UpdateLayeredWindow with alpha, we usually need to draw to GDI surface or use D3D/D2D interop.
    // BUT, SetLayeredWindowAttributes uses a global alpha.
    // We want per-pixel alpha (for rounded corners or transparent bg).
    // Simple approach: Use SetLayeredWindowAttributes for the global "Ghost Mode" opacity.
    // And for background color, just draw a rectangle.
    
    m_pRenderTarget->Clear(D2D1::ColorF(0, 0.0f)); // Clear to fully transparent

    D2D1_SIZE_F rtSize = m_pRenderTarget->GetSize();
    
    // Draw Background
    ID2D1SolidColorBrush* pBgBrush = NULL;
    hr = m_pRenderTarget->CreateSolidColorBrush(bgCol, &pBgBrush);
    
    if (SUCCEEDED(hr) && pBgBrush) {
        m_pRenderTarget->FillRectangle(D2D1::RectF(0, 0, rtSize.width, rtSize.height), pBgBrush);
        pBgBrush->Release();
    }

    // Draw Text
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

    // Draw Graphs (Smooth Bezier)
    
    // CPU Graph
    const double* cpuHist = m_systemMonitor.GetCpuHistory();
    float xStep = (rtSize.width - 10) / (SystemMonitor::HISTORY_SIZE - 1);
    float yBase = rtSize.height / 2 - 5;
    float yScale = (rtSize.height / 2 - 10) / 100.0f; // Scale to fit half height

    auto DrawSmoothGraph = [&](const double* data, float yBasePos, ID2D1Brush* strokeBrush, ID2D1Brush* fillBrush) {
        if (!strokeBrush) return;

        // Create Path
        ID2D1PathGeometry* pPath = NULL;
        m_pD2DFactory->CreatePathGeometry(&pPath);
        if (!pPath) return;

        ID2D1GeometrySink* pSink = NULL;
        pPath->Open(&pSink);

        D2D1_POINT_2F p0 = D2D1::Point2F(5, yBasePos - (float)data[0] * yScale);
        pSink->BeginFigure(p0, D2D1_FIGURE_BEGIN_FILLED); // Filled for gradient, we will just stroke it later if fill is null? 
        // Actually we need two paths: one for stroke (HOLLOW), one for fill (FILLED)
        
        // Simplified Logic: Just draw straight lines for now, but use Bezier?
        // True Bezier requires control points.
        // Simple smoothing: Catmull-Rom or just midpoint bezier.
        // Let's use a simple approach: Line to each point? No, user asked for smooth.
        
        // Bezier interpolation logic:
        // For p[i] to p[i+1], control points c1, c2.
        // c1 = p[i] + (p[i+1] - p[i-1]) * tension
        // But we need previous points.
        
        // Let's implement a simple "add line" for now, but if we want smooth, we use AddBezier.
        // Since implementing full spline in one go without helper class is verbose, 
        // I will stick to the previous implementation but add a small smoothing if possible,
        // OR essentially, replace the loop with AddBezier.
        
        // RE-IMPLEMENTATION: Using a simple heuristic for control points (Horizontal smoothing)
        // cp1 = current + (next.x - prev.x) * favor, etc.
        // For simplicity in this tool, assume X is constant step.
        
        // Let's try drawing with Beziers from i to i+1.
        // Control points: (x_i + step/2, y_i), (x_{i+1} - step/2, y_{i+1})
        // This creates a smooth curve that is horizontal at each data point.
        
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
        
        // Stroke
        m_pRenderTarget->DrawGeometry(pPath, strokeBrush, 1.5f); // Thicker line

        // Fill (Gradient)
        if (fillBrush) {
            ID2D1PathGeometry* pFillPath = NULL;
            m_pD2DFactory->CreatePathGeometry(&pFillPath);
            if(pFillPath) {
                pFillPath->Open(&pSink);
                pSink->BeginFigure(D2D1::Point2F(5, yBasePos), D2D1_FIGURE_BEGIN_FILLED); // Start bottom-left
                pSink->AddLine(p0); // Up to first point
                
                // Trace curve again
                for(int i=0; i<SystemMonitor::HISTORY_SIZE - 1; i++) {
                     D2D1_POINT_2F pStart = D2D1::Point2F(5 + i * xStep, yBasePos - (float)data[i] * yScale);
                     D2D1_POINT_2F pEnd = D2D1::Point2F(5 + (i+1) * xStep, yBasePos - (float)data[i+1] * yScale);
                     D2D1_POINT_2F c1 = D2D1::Point2F(pStart.x + xStep * 0.5f, pStart.y);
                     D2D1_POINT_2F c2 = D2D1::Point2F(pEnd.x - xStep * 0.5f, pEnd.y);
                     pSink->AddBezier(D2D1::BezierSegment(c1, c2, pEnd));
                }
                
                pSink->AddLine(D2D1::Point2F(5 + (SystemMonitor::HISTORY_SIZE - 1) * xStep, yBasePos)); // Down to bottom-right
                pSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                pSink->Close();
                pSink->Release();
                
                // Adjust gradient
                if (auto grad = dynamic_cast<ID2D1LinearGradientBrush*>(fillBrush)) {
                     grad->SetStartPoint(D2D1::Point2F(0, yBasePos - 100 * yScale));
                     grad->SetEndPoint(D2D1::Point2F(0, yBasePos));
                }
                // We passed generic brush, but we know it's gradient in context. 
                // Casting might be unsafe if not checked, but here we control usage.
                // However, ID2D1Brush doesn't have SetStartPoint. We need to pass the gradient brush specifically or cast carefully.
                // Let's rely on caller (DrawSmoothGraph) assuming the brush is already configured or we cast strictly.
                // C-style cast for interface pointer is valid for D2D derived standard.
                ((ID2D1LinearGradientBrush*)fillBrush)->SetStartPoint(D2D1::Point2F(0, yBasePos - 50)); 
                ((ID2D1LinearGradientBrush*)fillBrush)->SetEndPoint(D2D1::Point2F(0, yBasePos));

                m_pRenderTarget->FillGeometry(pFillPath, fillBrush);
                pFillPath->Release();
            }
        }
        pPath->Release();
    };

    DrawSmoothGraph(cpuHist, yBase, m_pGraphCpuBrush, m_pGraphCpuGradient);
    
    // RAM Graph
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
    // 1. Check Ctrl key for interaction mode
    bool isCtrlPressed = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    
    if (isCtrlPressed != m_isDragEnabled) {
        m_isDragEnabled = isCtrlPressed;
        // Toggle TRANSPARENT style
        LONG exStyle = GetWindowLong(m_hwnd, GWL_EXSTYLE);
        if (m_isDragEnabled) {
             exStyle &= ~WS_EX_TRANSPARENT; // Enable mouse input
        } else {
             exStyle |= WS_EX_TRANSPARENT; // Click through
        }
        SetWindowLong(m_hwnd, GWL_EXSTYLE, exStyle);
        // Force repaint/update to ensure style change takes effect? usually automatic
    }

    // 2. Ghost Mode Opacity Calculation
    int targetOpacity = g_Config.opacity;
    if (g_Config.ghostModeEnabled && !m_isDragEnabled) {
        POINT pt;
        GetCursorPos(&pt);
        RECT rc;
        GetWindowRect(m_hwnd, &rc);
        
        // Calculate distance from window
        int dist = 0;
        if (pt.x < rc.left) dist += (rc.left - pt.x);
        else if (pt.x > rc.right) dist += (pt.x - rc.right);
        
        if (pt.y < rc.top) dist += (rc.top - pt.y);
        else if (pt.y > rc.bottom) dist += (pt.y - rc.bottom);

        // If distance < 100px, fade out
        if (dist < 100) {
            // Map 0-100 distance to 10-Config.opacity
            // Closer = more transparent
            // dist=0 -> opacity=10 (almost invisible)
            // dist=100 -> opacity=Config.opacity
            targetOpacity = 10 + (int)((g_Config.opacity - 10) * (dist / 100.0f));
        }
    }

    // Smooth transition
    if (m_currentOpacity != targetOpacity) {
        int step = 25; // Faster fade (was 15)
        if (m_currentOpacity < targetOpacity) m_currentOpacity = min(m_currentOpacity + step, targetOpacity);
        else m_currentOpacity = max(m_currentOpacity - step, targetOpacity);
        
        SetLayeredWindowAttributes(m_hwnd, 0, (BYTE)m_currentOpacity, LWA_ALPHA);
        
        // Save position if dragging happened? 
        // Better to check if window moved OnExit or periodically if drag is done.
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
    nid.hIcon = LoadIcon(NULL, IDI_APPLICATION); // Default icon
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
    
    // Startup Checkbox
    AppendMenu(hMenu, MF_STRING | (g_Config.runOnStartup ? MF_CHECKED : MF_UNCHECKED), ID_MENU_STARTUP, L"スタートアップ登録");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_STRING, ID_MENU_EXIT, L"Exit");
    
    TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, x, y, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);
}
