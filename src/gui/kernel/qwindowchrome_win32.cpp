/****************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Copyright (C) 2021 Yuhang Zhao, 2546789017@qq.com
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qwindowchrome_p.h"
#include "qwindow.h"

#ifdef Q_OS_WINDOWS

#include <QtCore/qoperatingsystemversion.h> // TODO: Remove once Win7 support is officially dropped.
//#include <QtCore/private/qsystemlibrary_p.h>
#include <QtCore/qt_windows.h>
#include <dwmapi.h> // TODO: Remove once Win7 support is officially dropped.

// TODO: Remove once Win7 support is officially dropped.
#ifndef WM_NCUAHDRAWCAPTION
// Not documented, only available since Windows Vista
#  define WM_NCUAHDRAWCAPTION 0x00AE
#endif

// TODO: Remove once Win7 support is officially dropped.
#ifndef WM_NCUAHDRAWFRAME
// Not documented, only available since Windows Vista
#  define WM_NCUAHDRAWFRAME 0x00AF
#endif

#if 0
#ifndef USER_DEFAULT_SCREEN_DPI
#  define USER_DEFAULT_SCREEN_DPI 96
#endif
#endif

#ifndef IsMaximized
#  define IsMaximized(hwnd) IsZoomed(hwnd)
#endif

QT_BEGIN_NAMESPACE

#if 0
using MONITOR_DPI_TYPE = enum _MONITOR_DPI_TYPE
{
    MDT_EFFECTIVE_DPI = 0
};

using PROCESS_DPI_AWARENESS = enum _PROCESS_DPI_AWARENESS
{
    PROCESS_DPI_UNAWARE = 0,
    PROCESS_SYSTEM_DPI_AWARE = 1,
    PROCESS_PER_MONITOR_DPI_AWARE = 2
};

struct Win32Data
{
    using GetDpiForMonitorPtr = HRESULT(WINAPI *)(HMONITOR, MONITOR_DPI_TYPE, UINT *, UINT *);
    using GetProcessDpiAwarenessPtr = HRESULT(WINAPI *)(HANDLE, PROCESS_DPI_AWARENESS *);
    using GetSystemDpiForProcessPtr = UINT(WINAPI *)(HANDLE);
    using GetDpiForWindowPtr = UINT(WINAPI *)(HWND);
    using GetDpiForSystemPtr = UINT(WINAPI *)();
    using GetSystemMetricsForDpiPtr = int(WINAPI *)(int, UINT);
    using AdjustWindowRectExForDpiPtr = BOOL(WINAPI *)(LPRECT, DWORD, BOOL, DWORD, UINT);

    GetDpiForMonitorPtr GetDpiForMonitorPFN = nullptr;
    GetProcessDpiAwarenessPtr GetProcessDpiAwarenessPFN = nullptr;
    GetSystemDpiForProcessPtr GetSystemDpiForProcessPFN = nullptr;
    GetDpiForWindowPtr GetDpiForWindowPFN = nullptr;
    GetDpiForSystemPtr GetDpiForSystemPFN = nullptr;
    GetSystemMetricsForDpiPtr GetSystemMetricsForDpiPFN = nullptr;
    AdjustWindowRectExForDpiPtr AdjustWindowRectExForDpiPFN = nullptr;

    explicit Win32Data()
    {
        QSystemLibrary User32Dll(QStringLiteral("User32"));
        GetDpiForWindowPFN = reinterpret_cast<GetDpiForWindowPtr>(User32Dll.resolve("GetDpiForWindow"));
        GetDpiForSystemPFN = reinterpret_cast<GetDpiForSystemPtr>(User32Dll.resolve("GetDpiForSystem"));
        GetSystemMetricsForDpiPFN = reinterpret_cast<GetSystemMetricsForDpiPtr>(User32Dll.resolve("GetSystemMetricsForDpi"));
        AdjustWindowRectExForDpiPFN = reinterpret_cast<AdjustWindowRectExForDpiPtr>(User32Dll.resolve("AdjustWindowRectExForDpi"));
        GetSystemDpiForProcessPFN = reinterpret_cast<GetSystemDpiForProcessPtr>(User32Dll.resolve("GetSystemDpiForProcess"));

        QSystemLibrary SHCoreDll(QStringLiteral("SHCore"));
        GetDpiForMonitorPFN = reinterpret_cast<GetDpiForMonitorPtr>(SHCoreDll.resolve("GetDpiForMonitor"));
        GetProcessDpiAwarenessPFN = reinterpret_cast<GetProcessDpiAwarenessPtr>(SHCoreDll.resolve("GetProcessDpiAwareness"));
    }
};

Q_GLOBAL_STATIC(Win32Data, win32Data)

static inline quint32 getWindowDpi(const QWindow *window)
{
    Q_ASSERT(window);
    if (!window)
        return USER_DEFAULT_SCREEN_DPI;
    const auto hwnd = reinterpret_cast<HWND>(window->winId());
    if (!hwnd)
        return USER_DEFAULT_SCREEN_DPI;
    if (win32Data()->GetDpiForWindowPFN) {
        return win32Data()->GetDpiForWindowPFN(hwnd);
    } else if (win32Data()->GetSystemDpiForProcessPFN) {
        return win32Data()->GetSystemDpiForProcessPFN(GetCurrentProcess());
    } else if (win32Data()->GetDpiForSystemPFN) {
        return win32Data()->GetDpiForSystemPFN();
    } else if (win32Data()->GetDpiForMonitorPFN) {
        const HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        if (monitor) {
            UINT dpiX = USER_DEFAULT_SCREEN_DPI, dpiY = USER_DEFAULT_SCREEN_DPI;
            if (SUCCEEDED(win32Data()->GetDpiForMonitorPFN(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY))) {
                return dpiX;
            } else {
                qWarning() << "GetDpiForMonitor failed";
            }
        } else {
            qWarning() << "MonitorFromWindow failed.";
        }
    }
    // We can query DPI through Direct2D since Windows 7, but it's deprecated and will cause
    // compilation warnings, so we don't use it here.
    const HDC hdc = GetDC(nullptr);
    if (hdc) {
        const int dpiX = GetDeviceCaps(hdc, LOGPIXELSX);
        ReleaseDC(nullptr, hdc);
        if (dpiX > 0) {
            return dpiX;
        } else {
            qWarning() << "GetDeviceCaps failed.";
        }
    }
    return USER_DEFAULT_SCREEN_DPI;
}

static inline int getSizeFrameWidth(const QWindow *window, const bool x)
{
    Q_ASSERT(window);
    if (!window)
        return 8;
    // Although Microsoft claims that GetSystemMetrics() is not DPI-aware,
    // it still returns a scaled value on Windows 7~10.
    int sizeFrameWidth = GetSystemMetrics(x ? SM_CXSIZEFRAME : SM_CYSIZEFRAME);
    int paddedBorderWidth = GetSystemMetrics(SM_CXPADDEDBORDER); // There is no SM_CYPADDEDBORDER
    // But we still use GetSystemMetricsForDpi() if it's available.
    if (win32Data()->GetSystemMetricsForDpiPFN) {
        const UINT dpi = getWindowDpi(window);
        sizeFrameWidth = win32Data()->GetSystemMetricsForDpiPFN(x ? SM_CXSIZEFRAME : SM_CYSIZEFRAME, dpi);
        paddedBorderWidth = win32Data()->GetSystemMetricsForDpiPFN(SM_CXPADDEDBORDER, dpi);
    }
    return (sizeFrameWidth + paddedBorderWidth);
}
#endif

static inline QPointF getGlobalMousePosition(const QWindow *window)
{
    Q_ASSERT(window);
    if (!window)
        return {};
    return (QCursor::pos(window->screen()) * window->devicePixelRatio());
}

static inline QPointF getLocalMousePosition(const QWindow *window)
{
    Q_ASSERT(window);
    if (!window)
        return {};
    const QPointF gmp = getGlobalMousePosition(window);
    POINT winLocalMouse = {qRound(gmp.x()), qRound(gmp.y())};
    if (ScreenToClient(reinterpret_cast<HWND>(window->winId()), &winLocalMouse) == FALSE)
        return {};
    return {static_cast<qreal>(winLocalMouse.x), static_cast<qreal>(winLocalMouse.y)};
}

// TODO: Remove once Win7 support is officially dropped.
static inline bool dwmIsCompositionEnabled()
{
    // DwmIsCompositionEnabled() always return true since Win8.
    BOOL enabled = FALSE;
    return (SUCCEEDED(DwmIsCompositionEnabled(&enabled)) && (enabled == TRUE));
}

// TODO: Remove once Win7 support is officially dropped.
static inline bool shouldHaveWindowFrame()
{
    return (QOperatingSystemVersion::current() >= QOperatingSystemVersion::Windows10);
}

static inline void triggerFrameChange(const HWND hwnd)
{
    Q_ASSERT(hwnd);
    if (!hwnd)
        return;
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

bool QWindow::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (!(flags() & Qt::FramelessWindowHint))
        return false;
    if ((eventType != QByteArrayLiteral("windows_generic_MSG")) || !message || !result)
        return false;
    const auto msg = static_cast<LPMSG>(message);
    const HWND hWnd = msg->hwnd;
    if (!hWnd)
        return false;
    const auto winid = reinterpret_cast<HWND>(winId());
    if (!winid || (winid != hWnd))
        return false;
    switch (msg->message) {
    // These undocumented messages are sent to draw themed window
    // borders. Block them to prevent drawing borders over the client
    // area.
    // TODO: Remove once Win7 support is officially dropped.
    case WM_NCUAHDRAWCAPTION:
    case WM_NCUAHDRAWFRAME: {
        if (shouldHaveWindowFrame()) {
            break;
        } else {
            *result = 0;
            return true;
        }
    }
    // TODO: Remove once Win7 support is officially dropped.
    case WM_NCPAINT: {
        if (!dwmIsCompositionEnabled() && !shouldHaveWindowFrame()) {
            // Only block WM_NCPAINT when DWM composition is disabled. If
            // it's blocked when DWM composition is enabled, the frame
            // shadow won't be drawn.
            *result = 0;
            return true;
        } else {
            break;
        }
    }
    // TODO: Remove once Win7 support is officially dropped.
    case WM_NCACTIVATE: {
        if (shouldHaveWindowFrame()) {
            break;
        } else {
            if (dwmIsCompositionEnabled()) {
                // DefWindowProc won't repaint the window border if lParam
                // (normally a HRGN) is -1. See the following link's "lParam"
                // section:
                // https://docs.microsoft.com/en-us/windows/win32/winmsg/wm-ncactivate
                // Don't use "*result = 0" otherwise the window won't respond
                // to the window active state change.
                *result = DefWindowProcW(hWnd, msg->message, msg->wParam, -1);
            } else {
                if (static_cast<BOOL>(msg->wParam)) {
                    *result = FALSE;
                } else {
                    *result = TRUE;
                }
            }
            return true;
        }
    }
    case WM_SETICON:
    case WM_SETTEXT: {
        // Disable painting while these messages are handled to prevent them
        // from drawing a window caption over the client area.
        const auto oldStyle = GetWindowLongPtrW(hWnd, GWL_STYLE);
        // Prevent Windows from drawing the default title bar by temporarily
        // toggling the WS_VISIBLE style.
        SetWindowLongPtrW(hWnd, GWL_STYLE, oldStyle & ~WS_VISIBLE);
        triggerFrameChange(hWnd);
        const LRESULT ret = DefWindowProcW(hWnd, msg->message, msg->wParam, msg->lParam);
        SetWindowLongPtrW(hWnd, GWL_STYLE, oldStyle);
        triggerFrameChange(hWnd);
        *result = ret;
        return true;
    }
    case WM_NCHITTEST: {
        if (WindowChrome::useUnixCodePath())
            break;
        const QPointF localMousePosition = getLocalMousePosition(this);
        const qreal dpr = devicePixelRatio();
        const int sizeFrameWidth = /*getSizeFrameWidth(this, true)*/ qRound(resizeBorderThickness() * dpr);
        const int sizeFrameHeight = /*getSizeFrameWidth(this, false)*/ qRound(resizeBorderThickness() * dpr);
        const bool isInTitleBarArea = (localMousePosition.y() <= qRound(titleBarHeight() * dpr))
                                                               && !WindowChrome::isHitTestVisibleInChrome(this);
        const bool isInTopArea = (localMousePosition.y() <= sizeFrameHeight);
        if (shouldHaveWindowFrame()) {
            // This will handle the left, right and bottom parts of the frame
            // because we didn't change them.
            const LRESULT originalRet = DefWindowProcW(hWnd, WM_NCHITTEST, msg->wParam, msg->lParam);
            if (originalRet != HTCLIENT) {
                *result = originalRet;
                return true;
            }
            // At this point, we know that the cursor is inside the client area
            // so it has to be either the little border at the top of our custom
            // title bar or the drag bar. Apparently, it must be the drag bar or
            // the little border at the top which the user can use to move or
            // resize the window.
            if (!IsMaximized(hWnd) && isInTopArea) {
                *result = HTTOP;
                return true;
            }
            if (isInTitleBarArea) {
                *result = HTCAPTION;
                return true;
            }
            *result = HTCLIENT;
            return true;
        } else {
            const auto getHitTestResult = [this, hWnd, isInTitleBarArea, &localMousePosition,
                    sizeFrameWidth, sizeFrameHeight, isInTopArea]() -> LRESULT {
                RECT clientRect = {0, 0, 0, 0};
                if (GetClientRect(hWnd, &clientRect) == FALSE)
                    return HTNOWHERE;
                const LONG windowWidth = clientRect.right;
                const LONG windowHeight = clientRect.bottom;
                if (IsMaximized(hWnd)) {
                    if (isInTitleBarArea) {
                        return HTCAPTION;
                    }
                    return HTCLIENT;
                }
                const bool isInBottomArea = (localMousePosition.y() >= (windowHeight - sizeFrameHeight));
                // Make the border a little wider to let the user easy to resize on corners.
                const qreal factor = (isInTopArea || isInBottomArea) ? 2.0 : 1.0;
                const bool isInLeftArea = (localMousePosition.x() <= (sizeFrameWidth * factor));
                const bool isInRightArea = (localMousePosition.x() >= (windowWidth - (sizeFrameWidth * factor)));
                const bool fixedSize = WindowChrome::isWindowFixedSize(this);
                const auto getBorderValue = [fixedSize](int value) -> int {
                    return fixedSize ? HTCLIENT : value;
                };
                if (isInTopArea) {
                    if (isInLeftArea) {
                        return getBorderValue(HTTOPLEFT);
                    }
                    if (isInRightArea) {
                        return getBorderValue(HTTOPRIGHT);
                    }
                    return getBorderValue(HTTOP);
                }
                if (isInBottomArea) {
                    if (isInLeftArea) {
                        return getBorderValue(HTBOTTOMLEFT);
                    }
                    if (isInRightArea) {
                        return getBorderValue(HTBOTTOMRIGHT);
                    }
                    return getBorderValue(HTBOTTOM);
                }
                if (isInLeftArea) {
                    return getBorderValue(HTLEFT);
                }
                if (isInRightArea) {
                    return getBorderValue(HTRIGHT);
                }
                if (isInTitleBarArea) {
                    return HTCAPTION;
                }
                return HTCLIENT;
            };
            *result = getHitTestResult();
            return true;
        }
    } break;
    default:
        break;
    }
    return false;
}

QT_END_NAMESPACE

#endif // Q_OS_WINDOWS
