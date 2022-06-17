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

template <typename Atomic>
inline void futexWait(Atomic &futex, typename Atomic::Type expectedValue)
{
    static const auto pWaitOnAddress =
        reinterpret_cast<decltype(&::WaitOnAddress)>(
            QApiCache::instance().get(QApiCache::SD_KernelBase, "WaitOnAddress"_L1));
    if (!pWaitOnAddress)
        return;
    QtTsan::futexRelease(&futex);
    pWaitOnAddress(&futex, &expectedValue, sizeof(expectedValue), INFINITE);
    QtTsan::futexAcquire(&futex);
}
template <typename Atomic>
inline bool futexWait(Atomic &futex, typename Atomic::Type expectedValue, QDeadlineTimer deadline)
{
    static const auto pWaitOnAddress =
        reinterpret_cast<decltype(&::WaitOnAddress)>(
            QApiCache::instance().get(QApiCache::SD_KernelBase, "WaitOnAddress"_L1));
    if (!pWaitOnAddress)
        return false;
    using namespace std::chrono;
    BOOL r = pWaitOnAddress(&futex, &expectedValue, sizeof(expectedValue), DWORD(deadline.remainingTime()));
    return r || GetLastError() != ERROR_TIMEOUT;
}
template <typename Atomic> inline void futexWakeAll(Atomic &futex)
{
    static const auto pWakeByAddressAll =
        reinterpret_cast<decltype(&::WakeByAddressAll)>(
            QApiCache::instance().get(QApiCache::SD_KernelBase, "WakeByAddressAll"_L1));
    if (!pWakeByAddressAll)
        return;
    pWakeByAddressAll(&futex);
}
template <typename Atomic> inline void futexWakeOne(Atomic &futex)
{
    static const auto pWakeByAddressSingle =
        reinterpret_cast<decltype(&::WakeByAddressSingle)>(
            QApiCache::instance().get(QApiCache::SD_KernelBase, "WakeByAddressSingle"_L1));
    if (!pWakeByAddressSingle)
        return;
    pWakeByAddressSingle(&futex);
}
} // namespace QtWindowsFutex
namespace QtFutex = QtWindowsFutex;

QT_END_NAMESPACE

#endif // QFUTEX_WIN_P_H
