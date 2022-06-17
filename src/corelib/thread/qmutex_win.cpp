// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qmutex.h"
#include "qmutex_p.h"
#include "qatomic.h"
#include "qt_windows.h"

QT_BEGIN_NAMESPACE

QMutexPrivate::QMutexPrivate()
{
    event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!event)
        qWarning("QMutexPrivate::QMutexPrivate: Cannot create event");
}

QMutexPrivate::~QMutexPrivate()
{
    CloseHandle(event);
}

bool QMutexPrivate::wait(QDeadlineTimer deadline)
{
    while (true) {
        const DWORD timeout = deadline.isForever()
                ? INFINITE
                : DWORD(std::min(deadline.remainingTime(), qint64(INFINITE - 1)));
        switch (WaitForSingleObjectEx(event, timeout, FALSE)) {
        case WAIT_OBJECT_0:
            return true;
        case WAIT_TIMEOUT:
            if (deadline.hasExpired())
                return false;
            break;
        default:
            return false;
        }
    }
}

void QMutexPrivate::wakeUp() noexcept
{
    SetEvent(event);
}

QT_END_NAMESPACE
