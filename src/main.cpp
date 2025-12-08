#include "yMonitory.h"
#include "Config.h"
#include "OverlayWindow.h"

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Load Config
    g_Config.Load();

    OverlayWindow overlay;
    if (!overlay.Initialize(hInstance)) {
        return FALSE;
    }

    overlay.RunMessageLoop();
    // Message loop is inside RunMessageLoop

    // Save Config on exit
    g_Config.Save();

    return 0;
}
