#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

// Application Constants
namespace yMonitory {
    constexpr const TCHAR* APP_NAME = _T("yMonitory");
    constexpr const TCHAR* WINDOW_CLASS_NAME = _T("yMonitory_OverlayClass");
    constexpr const TCHAR* CONFIG_FILENAME = _T("config.ini");
}
