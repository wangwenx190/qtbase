// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qsystemlibrary_p.h"
#include <QtCore/qvarlengtharray.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qhash.h>
#include <QtCore/qloggingcategory.h>
#include <QtCore/private/wcharhelpers_win_p.h>
#include <array>

/*!

    \internal
    \class QSystemLibrary
    \inmodule QtCore

    The purpose of this class is to load only libraries that are located in
    well-known and trusted locations on the filesystem. It does not suffer from
    the security problem that QLibrary has, therefore it will never search in
    the current directory.

    The search order is the same as the order in DLL Safe search mode Windows,
    except that we don't search:
    * The current directory
    * The 16-bit system directory. (normally \c{c:\windows\system})
    * The Windows directory.  (normally \c{c:\windows})

    This means that the effective search order is:
    1. Application path.
    2. System libraries path.
    3. Trying all paths inside the PATH environment variable.

    Note, when onlySystemDirectory is true it will skip 1) and 3).

    DLL Safe search mode is documented in the "Dynamic-Link Library Search
    Order" document on MSDN.
*/

QT_BEGIN_NAMESPACE

Q_STATIC_LOGGING_CATEGORY(lcQSysLib, "qt.core.systemlibrary", QtWarningMsg);

using namespace Qt::StringLiterals;

#if !defined(QT_BOOTSTRAPPED)
extern QString qAppFileName();
#endif

static QString qSystemDirectory()
{
    static const QString result = []() -> QString {
        QVarLengthArray<wchar_t, MAX_PATH> fullPath = {};
        UINT retLen = ::GetSystemDirectoryW(fullPath.data(), MAX_PATH);
        if (retLen > MAX_PATH) {
            fullPath.resize(retLen);
            retLen = ::GetSystemDirectoryW(fullPath.data(), retLen);
        }
        // in some rare cases retLen might be 0
        return QString::fromWCharArray(fullPath.constData(), retLen);
    }();
    return result;
}

HINSTANCE QSystemLibrary::load(const wchar_t *libraryName, bool onlySystemDirectory /* = true */)
{
    {
        if (HMODULE instance = ::GetModuleHandleW(libraryName)) {
            return instance;
        }
    }

    const QString fileName = QString::fromWCharArray(libraryName);

    if (onlySystemDirectory) {
        const QString path = qSystemDirectory() + u'\\' + fileName;
        return ::LoadLibraryW(qt_castToWchar(path));
    }

    QStringList searchOrder;

#if !defined(QT_BOOTSTRAPPED)
    searchOrder << QFileInfo(qAppFileName()).path();
#endif
    searchOrder << qSystemDirectory();

    const QString PATH(QLatin1StringView(qgetenv("PATH")));
    searchOrder << PATH.split(u';', Qt::SkipEmptyParts);

    // Start looking in the order specified
    for (int i = 0; i < searchOrder.count(); ++i) {
        QString fullPathAttempt = searchOrder.at(i);
        if (!fullPathAttempt.endsWith(u'\\')) {
            fullPathAttempt.append(u'\\');
        }
        fullPathAttempt.append(fileName);
        HINSTANCE inst = ::LoadLibrary(qt_castToWchar(fullPathAttempt));
        if (inst != nullptr)
            return inst;
    }
    return nullptr;
}

struct QApiCache::D final
{
    D(QApiCache *qq);
    ~D();
    Q_DISABLE_COPY_MOVE(D)

    QApiCache *q = nullptr;
    std::array<QString, QApiCache::SD_MAX> dllNameMap{};
    std::array<QHash<QString, QFunctionPointer>, QApiCache::SD_MAX> funcMap{};
};

QApiCache::D::D(QApiCache *qq) : q(qq)
{
    Q_ASSERT(q);
    const QString dllPrefix = qSystemDirectory() + u'\\';
    dllNameMap[QApiCache::SD_Kernel32] = dllPrefix + "kernel32.dll"_L1;
    dllNameMap[QApiCache::SD_User32] = dllPrefix + "user32.dll"_L1;
    dllNameMap[QApiCache::SD_Shell32] = dllPrefix + "shell32.dll"_L1;
    dllNameMap[QApiCache::SD_GDI32] = dllPrefix + "gdi32.dll"_L1;
    dllNameMap[QApiCache::SD_SHCore] = dllPrefix + "shcore.dll"_L1;
    dllNameMap[QApiCache::SD_DWMAPI] = dllPrefix + "dwmapi.dll"_L1;
    dllNameMap[QApiCache::SD_DWrite] = dllPrefix + "dwrite.dll"_L1;
    dllNameMap[QApiCache::SD_DXGI] = dllPrefix + "dxgi.dll"_L1;
    dllNameMap[QApiCache::SD_DComp] = dllPrefix + "dcomp.dll"_L1;
    dllNameMap[QApiCache::SD_D3D9] = dllPrefix + "d3d9.dll"_L1;
    dllNameMap[QApiCache::SD_D3D11] = dllPrefix + "d3d11.dll"_L1;
    dllNameMap[QApiCache::SD_D3D12] = dllPrefix + "d3d12.dll"_L1;
    dllNameMap[QApiCache::SD_PSAPI] = dllPrefix + "psapi.dll"_L1;
    dllNameMap[QApiCache::SD_NTDLL] = dllPrefix + "ntdll.dll"_L1;
    dllNameMap[QApiCache::SD_DNSAPI] = dllPrefix + "dnsapi.dll"_L1;
    dllNameMap[QApiCache::SD_KernelBase] = dllPrefix + "kernelbase.dll"_L1;
    dllNameMap[QApiCache::SD_ComBase] = dllPrefix + "combase.dll"_L1;
    dllNameMap[QApiCache::SD_OpenGL32] = dllPrefix + "opengl32.dll"_L1;
}

QApiCache::D::~D() = default;

QApiCache::QApiCache() : d(std::make_unique<D>(this)) {}

QApiCache::~QApiCache() = default;

const QApiCache &QApiCache::instance()
{
    static const QApiCache inst{};
    return inst;
}

QFunctionPointer QApiCache::get(const qsizetype dll, const QString &funcName) const
{
    Q_ASSERT(dll >= 0 && dll < SD_MAX);
    Q_ASSERT(!funcName.isEmpty());
    if (dll < 0 || dll >= SD_MAX || funcName.isEmpty()) {
        qCWarning(lcQSysLib) << Q_FUNC_INFO << ": parameter invalid.";
        return nullptr;
    }
    auto &funcMap = d->funcMap[dll];
    const auto it = funcMap.constFind(funcName);
    if (it != funcMap.constEnd()) {
        return it.value();
    }
    const QString &dllName = d->dllNameMap[dll];
    auto &funcAddr = funcMap[funcName];
    qCDebug(lcQSysLib) << "Trying to load" << funcName << "from" << dllName << "...";
    const auto dllNameC = qt_castToWchar(dllName);
    HMODULE hDll = ::GetModuleHandleW(dllNameC);
    if (!hDll) {
        hDll = ::LoadLibraryW(dllNameC);
    }
    if (!hDll) {
        qCWarning(lcQSysLib) << "Failed to load library" << dllName;
        return nullptr;
    }
    funcAddr = reinterpret_cast<QFunctionPointer>(::GetProcAddress(hDll, funcName.toUtf8().constData()));
    if (!funcAddr) {
        qCWarning(lcQSysLib) << "Failed to resolve symbol" << funcName;
        return nullptr;
    }
    return funcAddr;
}

QT_END_NAMESPACE
