// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qrhid3dhelpers_p.h"
#include <QtCore/private/qsystemlibrary_p.h>
#include <QtCore/private/qsystemerror_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

namespace QRhiD3D {

pD3DCompile resolveD3DCompile()
{
    static const auto d3dCompile = []() -> pD3DCompile {
        for (const wchar_t *libraryName : {L"D3DCompiler_47", L"D3DCompiler_43"}) {
            QSystemLibrary library(libraryName);
            if (library.load()) {
                if (const auto symbol = library.resolve("D3DCompile"))
                    return reinterpret_cast<pD3DCompile>(symbol);
            }
        }
        qWarning("Failed to resolve D3DCompile() from D3DCompiler_47/43.dll");
        return nullptr;
    }();
    return d3dCompile;
}

IDCompositionDevice *createDirectCompositionDevice()
{
    static const auto pDCompositionCreateDevice =
        reinterpret_cast<decltype(&::DCompositionCreateDevice)>(
            QApiCache::instance().get(QApiCache::SD_DComp, "DCompositionCreateDevice"_L1));
    if (!pDCompositionCreateDevice) {
        return nullptr;
    }
    IDCompositionDevice *device = nullptr;
    const HRESULT hr = pDCompositionCreateDevice(nullptr, IID_PPV_ARGS(&device));
    if (FAILED(hr)) {
        qWarning("Failed to create Direct Composition device: %s",
                 qPrintable(QSystemError::windowsComString(hr)));
        return nullptr;
    }
    return device;
}

#ifdef QRHI_D3D12_HAS_DXC
std::pair<IDxcCompiler *, IDxcLibrary *> createDxcCompiler()
{
    QSystemLibrary dxclib(QStringLiteral("dxcompiler"));
    // this will not be in the system library location, hence onlySystemDirectory==false
    if (!dxclib.load(false)) {
        qWarning("Failed to load dxcompiler.dll");
        return {};
    }
    DxcCreateInstanceProc func = reinterpret_cast<DxcCreateInstanceProc>(dxclib.resolve("DxcCreateInstance"));
    if (!func) {
        qWarning("Unable to resolve DxcCreateInstance");
        return {};
    }
    IDxcCompiler *compiler = nullptr;
    HRESULT hr = func(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    if (FAILED(hr)) {
        qWarning("Failed to create dxc compiler instance: %s",
                 qPrintable(QSystemError::windowsComString(hr)));
        return {};
    }
    IDxcLibrary *library = nullptr;
    hr = func(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    if (FAILED(hr)) {
        qWarning("Failed to create dxc library instance: %s",
                 qPrintable(QSystemError::windowsComString(hr)));
        return {};
    }
    return { compiler, library };
}
#endif

void fillDriverInfo(QRhiDriverInfo *info, const DXGI_ADAPTER_DESC1 &desc)
{
    const QString name = QString::fromUtf16(reinterpret_cast<const char16_t *>(desc.Description));
    info->deviceName = name.toUtf8();
    info->deviceId = desc.DeviceId;
    info->vendorId = desc.VendorId;
    info->deviceType = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) ? QRhiDriverInfo::CpuDevice
                                                                 : QRhiDriverInfo::UnknownDevice;
}

} // namespace

QT_END_NAMESPACE
