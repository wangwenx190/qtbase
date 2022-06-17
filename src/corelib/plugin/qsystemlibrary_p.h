// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#ifndef QSYSTEMLIBRARY_P_H
#define QSYSTEMLIBRARY_P_H

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

#include <QtCore/private/qglobal_p.h>
#ifdef Q_OS_WIN
#  include <QtCore/qstring.h>
#  include <qt_windows.h>
#  include <memory>

QT_BEGIN_NAMESPACE

class QSystemLibrary
{
public:
    explicit QSystemLibrary(const QString &libraryName)
    {
        m_libraryName = libraryName;
    }

    explicit QSystemLibrary(const wchar_t *libraryName)
    {
        m_libraryName = QString::fromWCharArray(libraryName);
    }

    bool load(bool onlySystemDirectory = true)
    {
        m_handle = load((const wchar_t *)m_libraryName.utf16(), onlySystemDirectory);
        m_didLoad = true;
        return (m_handle != nullptr);
    }

    bool isLoaded()
    {
        return (m_handle != nullptr);
    }

    QFunctionPointer resolve(const char *symbol)
    {
        if (!m_didLoad)
            load();
        if (!m_handle)
            return nullptr;
        return QFunctionPointer(GetProcAddress(m_handle, symbol));
    }

    static QFunctionPointer resolve(const QString &libraryName, const char *symbol)
    {
        return QSystemLibrary(libraryName).resolve(symbol);
    }

    static Q_CORE_EXPORT HINSTANCE load(const wchar_t *lpFileName, bool onlySystemDirectory = true);

private:
    HINSTANCE m_handle = nullptr;
    QString m_libraryName = {};
    bool m_didLoad = false;
};

struct QApiCache final
{
    enum SystemDLL : qsizetype {
        SD_Kernel32 = 0, // We are using it as an array index so must begin with 0.
        SD_User32,
        SD_Shell32,
        SD_GDI32,
        SD_SHCore,
        SD_DWMAPI,
        SD_DWrite,
        SD_DXGI,
        SD_DComp,
        SD_D3D9,
        SD_D3D11,
        SD_D3D12,
        SD_PSAPI,
        SD_NTDLL,
        SD_DNSAPI,
        SD_KernelBase,
        SD_ComBase,
        SD_OpenGL32,
        SD_MAX
    };

    [[nodiscard]] Q_CORE_EXPORT static const QApiCache &instance();
    [[nodiscard]] Q_CORE_EXPORT QFunctionPointer get(const qsizetype dll, const QString &funcName) const;

private:
    QApiCache();
    ~QApiCache();
    Q_DISABLE_COPY_MOVE(QApiCache)
    struct D;
    friend struct D;
    const std::unique_ptr<D> d;
};

QT_END_NAMESPACE

#endif // Q_OS_WIN

#endif // QSYSTEMLIBRARY_P_H
