// Copyright (C) 2023 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only

#include "qrhid3dhelpers_p.h"
#include <QtCore/private/qsystemlibrary_p.h>
#include <QtCore/private/qsystemerror_p.h>

QT_BEGIN_NAMESPACE

using namespace Qt::StringLiterals;

namespace QRhiD3D {

bool output6ForWindow(QWindow *w, IDXGIAdapter1 *adapter, IDXGIOutput6 **result)
{
    bool ok = false;
    QRect wr = w->geometry();
    wr = QRect(wr.topLeft() * w->devicePixelRatio(), wr.size() * w->devicePixelRatio());
    const QPoint center = wr.center();
    IDXGIOutput *currentOutput = nullptr;
    IDXGIOutput *output = nullptr;
    for (UINT i = 0; adapter->EnumOutputs(i, &output) != DXGI_ERROR_NOT_FOUND; ++i) {
        if (!output)
            continue;
        DXGI_OUTPUT_DESC desc;
        output->GetDesc(&desc);
        const RECT r = desc.DesktopCoordinates;
        const QRect dr(QPoint(r.left, r.top), QPoint(r.right - 1, r.bottom - 1));
        if (dr.contains(center)) {
            currentOutput = output;
            break;
        } else {
            output->Release();
        }
    }
    if (currentOutput) {
        ok = SUCCEEDED(currentOutput->QueryInterface(IID_PPV_ARGS(result)));
        currentOutput->Release();
    }
    return ok;
}

bool outputDesc1ForWindow(QWindow *w, IDXGIAdapter1 *adapter, DXGI_OUTPUT_DESC1 *result)
{
    bool ok = false;
    IDXGIOutput6 *out6 = nullptr;
    if (output6ForWindow(w, adapter, &out6)) {
        ok = SUCCEEDED(out6->GetDesc1(result));
        out6->Release();
    }
    return ok;
}

float sdrWhiteLevelInNits(const DXGI_OUTPUT_DESC1 &outputDesc)
{
    static constexpr const auto kDefaultWhiteLevel = 100.0f;

    using GetDisplayConfigBufferSizesPFN = decltype(&::GetDisplayConfigBufferSizes);
    using QueryDisplayConfigPFN = decltype(&::QueryDisplayConfig);
    using DisplayConfigGetDeviceInfoPFN = decltype(&::DisplayConfigGetDeviceInfo);

    static const auto pGetDisplayConfigBufferSizes = reinterpret_cast<GetDisplayConfigBufferSizesPFN>(QSystemLibrary::resolve(u"user32"_s, "GetDisplayConfigBufferSizes"));
    static const auto pQueryDisplayConfig = reinterpret_cast<QueryDisplayConfigPFN>(QSystemLibrary::resolve(u"user32"_s, "QueryDisplayConfig"));
    static const auto pDisplayConfigGetDeviceInfo = reinterpret_cast<DisplayConfigGetDeviceInfoPFN>(QSystemLibrary::resolve(u"user32"_s, "DisplayConfigGetDeviceInfo"));

    if (!pGetDisplayConfigBufferSizes || !pQueryDisplayConfig || !pDisplayConfigGetDeviceInfo)
        return kDefaultWhiteLevel;

    QVector<DISPLAYCONFIG_PATH_INFO> pathInfos;
    uint32_t pathInfoCount, modeInfoCount;
    LONG result;
    do {
        if (pGetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathInfoCount, &modeInfoCount) == ERROR_SUCCESS) {
            pathInfos.resize(pathInfoCount);
            QVector<DISPLAYCONFIG_MODE_INFO> modeInfos(modeInfoCount);
            result = pQueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathInfoCount, pathInfos.data(), &modeInfoCount, modeInfos.data(), nullptr);
        } else {
            return kDefaultWhiteLevel;
        }
    } while (result == ERROR_INSUFFICIENT_BUFFER);

    MONITORINFOEX monitorInfo = {};
    monitorInfo.cbSize = sizeof(monitorInfo);
    GetMonitorInfo(outputDesc.Monitor, &monitorInfo);

    for (const DISPLAYCONFIG_PATH_INFO &info : pathInfos) {
        DISPLAYCONFIG_SOURCE_DEVICE_NAME deviceName = {};
        deviceName.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
        deviceName.header.size = sizeof(deviceName);
        deviceName.header.adapterId = info.sourceInfo.adapterId;
        deviceName.header.id = info.sourceInfo.id;
        if (pDisplayConfigGetDeviceInfo(&deviceName.header) == ERROR_SUCCESS) {
            if (!wcscmp(monitorInfo.szDevice, deviceName.viewGdiDeviceName)) {
                DISPLAYCONFIG_SDR_WHITE_LEVEL whiteLevel = {};
                whiteLevel.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
                whiteLevel.header.size = sizeof(DISPLAYCONFIG_SDR_WHITE_LEVEL);
                whiteLevel.header.adapterId = info.targetInfo.adapterId;
                whiteLevel.header.id = info.targetInfo.id;
                if (pDisplayConfigGetDeviceInfo(&whiteLevel.header) == ERROR_SUCCESS)
                    return whiteLevel.SDRWhiteLevel * 80 / 1000.0f;
            }
        }
    }

    return kDefaultWhiteLevel;
}

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
        qWarning("Failed to load D3DCompiler_47/43.dll");
        return nullptr;
    }();
    return d3dCompile;
}

IDCompositionDevice *createDirectCompositionDevice()
{
    using DCompositionCreateDevicePtr = decltype(&::DCompositionCreateDevice);
    static const auto pDCompositionCreateDevice =
        reinterpret_cast<DCompositionCreateDevicePtr>(
            QSystemLibrary::resolve(u"dcomp"_s, "DCompositionCreateDevice"));
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
    QSystemLibrary dxclib(u"dxcompiler"_s);
    // this will not be in the system library location, hence onlySystemDirectory==false
    if (!dxclib.load(false)) {
        qWarning("Failed to load dxcompiler.dll");
        return {};
    }
    const auto func = reinterpret_cast<DxcCreateInstanceProc>(dxclib.resolve("DxcCreateInstance"));
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
