// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QFUTEX_WIN_P_H
#define QFUTEX_WIN_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <private/qglobal_p.h>
#include <qdeadlinetimer.h>
#include <qtsan_impl.h>

#include <QtCore/qoperatingsystemversion.h>
#include <QtCore/private/qsystemlibrary_p.h>

#include <qt_windows.h>

#ifdef QT_ALWAYS_USE_FUTEX
#  undef QT_ALWAYS_USE_FUTEX
#endif

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

namespace QtWindowsFutex {
inline bool futexAvailable()
{
    return QOperatingSystemVersion::isWin8OrGreater();
}

struct QFutexApi final
{
    decltype(&::WaitOnAddress) pWaitOnAddress = nullptr;
    decltype(&::WakeByAddressAll) pWakeByAddressAll = nullptr;
    decltype(&::WakeByAddressSingle) pWakeByAddressSingle = nullptr;

    [[nodiscard]] static QFutexApi *instance()
    {
        static QFutexApi api;
        return &api;
    }

private:
    Q_DISABLE_COPY_MOVE(QFutexApi)

    QFutexApi()
    {
        if (!futexAvailable())
            return;
        QSystemLibrary library(u"kernelbase"_s);
        pWaitOnAddress = reinterpret_cast<decltype(pWaitOnAddress)>(library.resolve("WaitOnAddress"));
        pWakeByAddressAll = reinterpret_cast<decltype(pWakeByAddressAll)>(library.resolve("WakeByAddressAll"));
        pWakeByAddressSingle = reinterpret_cast<decltype(pWakeByAddressSingle)>(library.resolve("WakeByAddressSingle"));
    }

    ~QFutexApi() = default;
};

template <typename Atomic>
inline void futexWait(Atomic &futex, typename Atomic::Type expectedValue)
{
    if (!QFutexApi::instance()->pWaitOnAddress)
        return;
    QtTsan::futexRelease(&futex);
    QFutexApi::instance()->pWaitOnAddress(&futex, &expectedValue, sizeof(expectedValue), INFINITE);
    QtTsan::futexAcquire(&futex);
}
template <typename Atomic>
inline bool futexWait(Atomic &futex, typename Atomic::Type expectedValue, QDeadlineTimer deadline)
{
    if (!QFutexApi::instance()->pWaitOnAddress)
        return false;
    using namespace std::chrono;
    BOOL r = QFutexApi::instance()->pWaitOnAddress(&futex, &expectedValue, sizeof(expectedValue), DWORD(deadline.remainingTime()));
    return r || GetLastError() != ERROR_TIMEOUT;
}
template <typename Atomic> inline void futexWakeAll(Atomic &futex)
{
    if (!QFutexApi::instance()->pWakeByAddressAll)
        return;
    QFutexApi::instance()->pWakeByAddressAll(&futex);
}
template <typename Atomic> inline void futexWakeOne(Atomic &futex)
{
    if (!QFutexApi::instance()->pWakeByAddressSingle)
        return;
    QFutexApi::instance()->pWakeByAddressSingle(&futex);
}
} // namespace QtWindowsFutex
namespace QtFutex = QtWindowsFutex;

QT_END_NAMESPACE

#endif // QFUTEX_WIN_P_H
