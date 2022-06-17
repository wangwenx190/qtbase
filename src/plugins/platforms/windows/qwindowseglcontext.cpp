/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qwindowseglcontext.h"
#include "qwindowscontext.h"
#include "qwindowswindow.h"

#include <QtCore/qdebug.h>
#include <QtGui/qopenglcontext.h>
#include <QtCore/private/qsystemlibrary_p.h>

#define EGL_NONE                          0x3038
#define EGL_NOT_INITIALIZED               0x3001

#define EGL_PLATFORM_ANGLE_ANGLE          0x3202
#define EGL_PLATFORM_ANGLE_TYPE_ANGLE     0x3203
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MAJOR_ANGLE 0x3204
#define EGL_PLATFORM_ANGLE_MAX_VERSION_MINOR_ANGLE 0x3205
#define EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE 0x3206
#define EGL_PLATFORM_ANGLE_DEBUG_LAYERS_ENABLED_ANGLE 0x3451
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE 0x3209
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_HARDWARE_ANGLE 0x320A
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_NULL_ANGLE 0x345E
#define EGL_PLATFORM_ANGLE_NATIVE_PLATFORM_TYPE_ANGLE 0x348F

#define EGL_PLATFORM_ANGLE_TYPE_D3D9_ANGLE 0x3207
#define EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE 0x3208
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE 0x320B
#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_REFERENCE_ANGLE 0x320C
#define EGL_PLATFORM_ANGLE_ENABLE_AUTOMATIC_TRIM_ANGLE 0x320F

#define EGL_PLATFORM_ANGLE_D3D11ON12_ANGLE 0x3488

#define EGL_PLATFORM_ANGLE_TYPE_OPENGL_ANGLE 0x320D
#define EGL_PLATFORM_ANGLE_TYPE_OPENGLES_ANGLE 0x320E
#define EGL_PLATFORM_ANGLE_EGL_HANDLE_ANGLE 0x3480

#define EGL_PLATFORM_ANGLE_TYPE_NULL_ANGLE 0x33AE

#define EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE 0x3450
#define EGL_PLATFORM_VULKAN_DISPLAY_MODE_SIMPLE_ANGLE 0x34A4
#define EGL_PLATFORM_VULKAN_DISPLAY_MODE_HEADLESS_ANGLE 0x34A5

#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE 0x3487

#define EGL_PLATFORM_ANGLE_DEVICE_TYPE_EGL_ANGLE 0x348E

#define EGL_RED_SIZE                      0x3024
#define EGL_GREEN_SIZE                    0x3023
#define EGL_BLUE_SIZE                     0x3022
#define EGL_ALPHA_SIZE                    0x3021
#define EGL_DEPTH_SIZE                    0x3025
#define EGL_STENCIL_SIZE                  0x3026
#define EGL_SAMPLES                       0x3031
#define EGL_SAMPLE_BUFFERS                0x3032

#define EGL_CONTEXT_MAJOR_VERSION         0x3098
#define EGL_CONTEXT_MINOR_VERSION         0x30FB

#define EGL_BAD_ACCESS                    0x3002
#define EGL_BAD_ALLOC                     0x3003

#define EGL_WIDTH                         0x3057
#define EGL_HEIGHT                        0x3056
#define EGL_LARGEST_PBUFFER               0x3058

#define EGL_FALSE                         0
#define EGL_TRUE                          1

#define EGL_DRAW                          0x3059
#define EGL_READ                          0x305A

#define EGL_CONTEXT_LOST                  0x300E

#define EGL_SWAP_BEHAVIOR                 0x3093
#define EGL_BUFFER_SIZE                   0x3020

#define EGL_BIND_TO_TEXTURE_RGB           0x3039
#define EGL_BIND_TO_TEXTURE_RGBA          0x303A

#define EGL_SURFACE_TYPE                  0x3033
#define EGL_WINDOW_BIT                    0x0004
#define EGL_RENDERABLE_TYPE               0x3040
#define EGL_OPENGL_ES2_BIT                0x0004
#define EGL_OPENGL_ES3_BIT                0x00000040

QT_BEGIN_NAMESPACE

/*!
    \class QWindowsEGLStaticContext
    \brief Static data for QWindowsEGLContext.

    Keeps the display. The class is shared via QSharedPointer in the windows, the
    contexts and in QWindowsIntegration. The display will be closed if the last instance
    is deleted.

    No EGL or OpenGL functions are called directly. Instead, they are resolved
    dynamically. This works even if the plugin links directly to libegl/libglesv2 so
    there is no need to differentiate between dynamic or Angle-only builds in here.

    \internal
*/

using namespace Qt::StringLiterals;

QWindowsLibEGL QWindowsEGLStaticContext::libEGL;
QWindowsLibGLESv2 QWindowsEGLStaticContext::libGLESv2;

#ifdef Q_CC_MINGW
static inline QFunctionPointer resolveFunc(HMODULE lib, const char *name)
{
    QString baseNameStr = QString::fromUtf8(name);
    QString nameStr;
    QFunctionPointer proc = nullptr;

    // Play nice with 32-bit mingw: Try func first, then func@0, func@4,
    // func@8, func@12, ..., func@64. The def file does not provide any aliases
    // in libEGL and libGLESv2 in these builds which results in exporting
    // function names like eglInitialize@12. This cannot be fixed without
    // breaking binary compatibility. So be flexible here instead.

    int argSize = -1;
    while (!proc && argSize <= 64) {
        nameStr = baseNameStr;
        if (argSize >= 0)
            nameStr += u'@' + QString::number(argSize);
        argSize = argSize < 0 ? 0 : argSize + 4;
        proc = reinterpret_cast<QFunctionPointer>(::GetProcAddress(lib, nameStr.toUtf8().constData()));
    }
    return proc;
}
#else
static inline QFunctionPointer resolveFunc(HMODULE lib, const char *name)
{
    return reinterpret_cast<QFunctionPointer>(::GetProcAddress(lib, name));
}
#endif // Q_CC_MINGW

QFunctionPointer QWindowsLibEGL::resolve(const char *name) const
{
    return m_lib ? resolveFunc(m_lib, name) : nullptr;
}

#define ANGLE_RESOLVE(Func) Func = reinterpret_cast<decltype(Func)>(resolve(#Func));

bool QWindowsLibEGL::init()
{
    qCDebug(lcQpaGl) << "QWindowsEGLContext: using EGL from libEGL.dll ...";

    m_lib = QSystemLibrary::load(L"libEGL.dll", false);
    if (!m_lib) {
        qCWarning(lcQpaGl) << "QWindowsEGLContext: failed to load libEGL.dll.";
        return false;
    }

    ANGLE_RESOLVE(eglGetError)
    ANGLE_RESOLVE(eglGetDisplay)
    ANGLE_RESOLVE(eglInitialize)
    ANGLE_RESOLVE(eglGetProcAddress)
    ANGLE_RESOLVE(eglTerminate)
    ANGLE_RESOLVE(eglChooseConfig)
    ANGLE_RESOLVE(eglGetConfigAttrib)
    ANGLE_RESOLVE(eglCreateWindowSurface)
    ANGLE_RESOLVE(eglCreatePbufferSurface)
    ANGLE_RESOLVE(eglDestroySurface)
    ANGLE_RESOLVE(eglBindAPI)
    ANGLE_RESOLVE(eglSwapInterval)
    ANGLE_RESOLVE(eglCreateContext)
    ANGLE_RESOLVE(eglDestroyContext)
    ANGLE_RESOLVE(eglMakeCurrent)
    ANGLE_RESOLVE(eglGetCurrentContext)
    ANGLE_RESOLVE(eglGetCurrentSurface)
    ANGLE_RESOLVE(eglGetCurrentDisplay)
    ANGLE_RESOLVE(eglSwapBuffers)
    ANGLE_RESOLVE(eglGetPlatformDisplayEXT)

    if (!eglGetError || !eglGetDisplay || !eglInitialize || !eglGetProcAddress)
        return false;

    return true;
}

QFunctionPointer QWindowsLibGLESv2::resolve(const char *name) const
{
    return m_lib ? resolveFunc(m_lib, name) : nullptr;
}

bool QWindowsLibGLESv2::init()
{
    qCDebug(lcQpaGl) << "QWindowsEGLContext: using OpenGL ES 2.0 from libGLESv2.dll ...";

    m_lib = QSystemLibrary::load(L"libGLESv2.dll", false);
    if (!m_lib) {
        qCWarning(lcQpaGl) << "QWindowsEGLContext: failed to load libGLESv2.dll.";
        return false;
    }

    void (APIENTRY * glBindTexture)(GLenum target, GLuint texture);
    ANGLE_RESOLVE(glBindTexture)
    GLuint (APIENTRY * glCreateShader)(GLenum type);
    ANGLE_RESOLVE(glCreateShader)
    void (APIENTRY * glClearDepthf)(GLclampf depth);
    ANGLE_RESOLVE(glClearDepthf)
    ANGLE_RESOLVE(glGetString)

    return glBindTexture && glCreateShader && glClearDepthf;
}

QWindowsEGLStaticContext::QWindowsEGLStaticContext(EGLDisplay display)
    : m_display(display)
{
}

void QWindowsEGLStaticContext::initializeAngle(HDC dc, EGLDisplay *display, EGLint *major, EGLint *minor)
{
    if (!libEGL.eglGetPlatformDisplayEXT) {
        qCWarning(lcQpaGl) << "QWindowsEGLContext: eglGetPlatformDisplayEXT() from libEGL.dll is not available, can't initialize ANGLE.";
        return;
    }
    static constexpr const EGLint anglePlatformAttributes[][5] = {
        { EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_NONE },
        { EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_D3D_WARP_ANGLE, EGL_NONE },
        { EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_D3D11ON12_ANGLE, EGL_NONE },
        { EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_VULKAN_ANGLE, EGL_NONE },
        { EGL_PLATFORM_ANGLE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE, EGL_PLATFORM_ANGLE_DEVICE_TYPE_SWIFTSHADER_ANGLE, EGL_NONE },
    };
    const EGLint *attributes = nullptr;
    const QString angleBackend = qEnvironmentVariable("QT_ANGLE_BACKEND", "d3d11"_L1).toLower();
    if (angleBackend == "d3d11"_L1) {
        qCDebug(lcQpaGl) << "QWindowsEGLContext: user request to use ANGLE (D3D11).";
        attributes = anglePlatformAttributes[0];
    } else if (angleBackend == "warp"_L1) {
        qCDebug(lcQpaGl) << "QWindowsEGLContext: user request to use ANGLE (WARP).";
        attributes = anglePlatformAttributes[1];
    } else if (angleBackend == "d3d11on12"_L1) {
        qCDebug(lcQpaGl) << "QWindowsEGLContext: user request to use ANGLE (D3D11On12).";
        attributes = anglePlatformAttributes[2];
    } else if (angleBackend == "vulkan"_L1) {
        qCDebug(lcQpaGl) << "QWindowsEGLContext: user request to use ANGLE (Vulkan).";
        attributes = anglePlatformAttributes[3];
    } else if (angleBackend == "swiftshader"_L1) {
        qCDebug(lcQpaGl) << "QWindowsEGLContext: user request to use ANGLE (SwiftShader).";
        attributes = anglePlatformAttributes[4];
    } else {
        qCWarning(lcQpaGl) << "QWindowsEGLContext: unrecognized ANGLE backend from user:" << angleBackend;
    }
    if (!attributes) {
        qCDebug(lcQpaGl) << "QWindowsEGLContext: user doesn't request any specific ANGLE backend, using default configuration ...";
        return;
    }
    *display = libEGL.eglGetPlatformDisplayEXT(EGL_PLATFORM_ANGLE_ANGLE, dc, attributes);
    if (libEGL.eglInitialize(*display, major, minor)) {
        qCDebug(lcQpaGl) << "QWindowsEGLContext: the requested ANGLE backend initialized successfully.";
    } else {
        qCWarning(lcQpaGl) << "QWindowsEGLContext: failed to initialize the requested ANGLE backend, using default configuration instead ...";
        libEGL.eglTerminate(*display);
        *display = EGL_NO_DISPLAY;
        *major = *minor = 0;
    }
}

QWindowsEGLStaticContext *QWindowsEGLStaticContext::create()
{
    const HDC dc = QWindowsContext::instance()->displayContext();
    if (!dc){
        qCWarning(lcQpaGl, "%s: No Display", __FUNCTION__);
        return nullptr;
    }

    if (!libEGL.init()) {
        qCWarning(lcQpaGl, "%s: Failed to load and resolve libEGL functions", __FUNCTION__);
        return nullptr;
    }
    if (!libGLESv2.init()) {
        qCWarning(lcQpaGl, "%s: Failed to load and resolve libGLESv2 functions", __FUNCTION__);
        return nullptr;
    }

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLint major = 0;
    EGLint minor = 0;

    initializeAngle(dc, &display, &major, &minor);

    if (display == EGL_NO_DISPLAY)
        display = libEGL.eglGetDisplay(dc);
    if (!display) {
        qCWarning(lcQpaGl, "%s: Could not obtain EGL display", __FUNCTION__);
        return nullptr;
    }

    if (!major && !libEGL.eglInitialize(display, &major, &minor)) {
        int err = libEGL.eglGetError();
        qCWarning(lcQpaGl, "%s: Could not initialize EGL display: error 0x%x", __FUNCTION__, err);
        if (err == EGL_NOT_INITIALIZED)
            qCWarning(lcQpaGl, "%s: When using ANGLE, check if d3dcompiler_4x.dll is available", __FUNCTION__);
        return nullptr;
    }

    qCDebug(lcQpaGl) << __FUNCTION__ << "Created EGL display" << display << 'v' <<major << '.' << minor;
    return new QWindowsEGLStaticContext(display);
}

QWindowsEGLStaticContext::~QWindowsEGLStaticContext()
{
    qCDebug(lcQpaGl) << __FUNCTION__ << "Releasing EGL display " << m_display;
    libEGL.eglTerminate(m_display);
}

QWindowsOpenGLContext *QWindowsEGLStaticContext::createContext(QOpenGLContext *context)
{
    return new QWindowsEGLContext(this, context);
}

QWindowsOpenGLContext *QWindowsEGLStaticContext::createContext(HGLRC context, HWND window)
{
#if 0
    return new QWindowsEGLContext(this, context, window);
#else
    // ### wangwenx190: don't know how to initialize the QWindowsEGLContext struct without
    // a valid QOpenGLContext. It's a lot safer to return a NULL pointer here.
    qCWarning(lcQpaGl, "%s is not implemented.", __FUNCTION__);
    return nullptr;
#endif
}

void *QWindowsEGLStaticContext::createWindowSurface(void *nativeWindow, void *nativeConfig, int *err)
{
    *err = 0;
    EGLSurface surface = libEGL.eglCreateWindowSurface(m_display, nativeConfig,
                                                       static_cast<EGLNativeWindowType>(nativeWindow), nullptr);
    if (surface == EGL_NO_SURFACE) {
        *err = libEGL.eglGetError();
        qCWarning(lcQpaGl, "%s: Could not create the EGL window surface: 0x%x", __FUNCTION__, *err);
    }

    return surface;
}

void QWindowsEGLStaticContext::destroyWindowSurface(void *nativeSurface)
{
    libEGL.eglDestroySurface(m_display, nativeSurface);
}

QSurfaceFormat QWindowsEGLStaticContext::formatFromConfig(EGLDisplay display, EGLConfig config,
                                                          const QSurfaceFormat &referenceFormat)
{
    QSurfaceFormat format;
    EGLint redSize     = 0;
    EGLint greenSize   = 0;
    EGLint blueSize    = 0;
    EGLint alphaSize   = 0;
    EGLint depthSize   = 0;
    EGLint stencilSize = 0;
    EGLint sampleCount = 0;

    libEGL.eglGetConfigAttrib(display, config, EGL_RED_SIZE,     &redSize);
    libEGL.eglGetConfigAttrib(display, config, EGL_GREEN_SIZE,   &greenSize);
    libEGL.eglGetConfigAttrib(display, config, EGL_BLUE_SIZE,    &blueSize);
    libEGL.eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE,   &alphaSize);
    libEGL.eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE,   &depthSize);
    libEGL.eglGetConfigAttrib(display, config, EGL_STENCIL_SIZE, &stencilSize);
    libEGL.eglGetConfigAttrib(display, config, EGL_SAMPLES,      &sampleCount);

    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(referenceFormat.majorVersion(), referenceFormat.minorVersion());
    format.setProfile(referenceFormat.profile());
    format.setOptions(referenceFormat.options());

    format.setRedBufferSize(redSize);
    format.setGreenBufferSize(greenSize);
    format.setBlueBufferSize(blueSize);
    format.setAlphaBufferSize(alphaSize);
    format.setDepthBufferSize(depthSize);
    format.setStencilBufferSize(stencilSize);
    format.setSamples(sampleCount);
    format.setStereo(false);
    format.setSwapInterval(referenceFormat.swapInterval());

    // Clear the EGL error state because some of the above may
    // have errored out because the attribute is not applicable
    // to the surface type.  Such errors don't matter.
    libEGL.eglGetError();

    return format;
}

/*!
    \class QWindowsEGLContext
    \brief Open EGL context.

    \section1 Using QWindowsEGLContext for Desktop with ANGLE
    \section2 Build Instructions
    \list
    \o Install the Direct X SDK
    \o Checkout and build ANGLE (SVN repository) as explained here:
       \l{https://chromium.googlesource.com/angle/angle/+/master/README.md}
       When building for 64bit, de-activate the "WarnAsError" option
       in every project file (as otherwise integer conversion
       warnings will break the build).
    \o Run configure.exe with the options "-opengl es2".
    \o Build qtbase and test some examples.
    \endlist

    \internal
*/

QWindowsEGLContext::QWindowsEGLContext(QWindowsEGLStaticContext *staticContext, QOpenGLContext *context)
    : m_staticContext(staticContext)
{
    if (!m_staticContext)
        return;
    m_eglDisplay = m_staticContext->display();

    const QSurfaceFormat format = context->format();
    const QPlatformOpenGLContext *share = context->shareHandle();

    m_eglConfig = chooseConfig(format);
    m_format = m_staticContext->formatFromConfig(m_eglDisplay, m_eglConfig, format);
    m_shareContext = share ? static_cast<const QWindowsEGLContext *>(share)->nativeContext() : nullptr;

    QVector<EGLint> contextAttrs;
    const int major = m_format.majorVersion();
    const int minor = m_format.minorVersion();
    if (major > 3 || (major == 3 && minor > 0))
        qCWarning(lcQpaGl, "QWindowsEGLContext: ANGLE only partially supports OpenGL ES > 3.0");
    contextAttrs.append(EGL_CONTEXT_MAJOR_VERSION);
    contextAttrs.append(major);
    contextAttrs.append(EGL_CONTEXT_MINOR_VERSION);
    contextAttrs.append(minor);
    contextAttrs.append(EGL_NONE);

    QWindowsEGLStaticContext::libEGL.eglBindAPI(m_api);
    m_eglContext = QWindowsEGLStaticContext::libEGL.eglCreateContext(m_eglDisplay, m_eglConfig, m_shareContext, contextAttrs.constData());
    if (m_eglContext == EGL_NO_CONTEXT && m_shareContext != EGL_NO_CONTEXT) {
        m_shareContext = nullptr;
        m_eglContext = QWindowsEGLStaticContext::libEGL.eglCreateContext(m_eglDisplay, m_eglConfig, nullptr, contextAttrs.constData());
    }

    if (m_eglContext == EGL_NO_CONTEXT) {
        int err = QWindowsEGLStaticContext::libEGL.eglGetError();
        qCWarning(lcQpaGl, "QWindowsEGLContext: Failed to create context, eglError: %x, this: %p", err, this);
        // ANGLE gives bad alloc when it fails to reset a previously lost D3D device.
        // A common cause for this is disabling the graphics adapter used by the app.
        if (err == EGL_BAD_ALLOC)
            qCWarning(lcQpaGl, "QWindowsEGLContext: Graphics device lost. (Did the adapter get disabled?)");
        return;
    }

    // Make the context current to ensure the GL version query works. This needs a surface too.
    const EGLint pbufferAttributes[] = {
        EGL_WIDTH, EGL_TRUE,
        EGL_HEIGHT, EGL_TRUE,
        EGL_LARGEST_PBUFFER, EGL_FALSE,
        EGL_NONE
    };
    EGLSurface pbuffer = QWindowsEGLStaticContext::libEGL.eglCreatePbufferSurface(m_eglDisplay, m_eglConfig, pbufferAttributes);
    if (pbuffer == EGL_NO_SURFACE)
        return;

    EGLDisplay prevDisplay = QWindowsEGLStaticContext::libEGL.eglGetCurrentDisplay();
    if (prevDisplay == EGL_NO_DISPLAY) // when no context is current
        prevDisplay = m_eglDisplay;
    EGLContext prevContext = QWindowsEGLStaticContext::libEGL.eglGetCurrentContext();
    EGLSurface prevSurfaceDraw = QWindowsEGLStaticContext::libEGL.eglGetCurrentSurface(EGL_DRAW);
    EGLSurface prevSurfaceRead = QWindowsEGLStaticContext::libEGL.eglGetCurrentSurface(EGL_READ);

    if (QWindowsEGLStaticContext::libEGL.eglMakeCurrent(m_eglDisplay, pbuffer, pbuffer, m_eglContext)) {
        const GLubyte *s = QWindowsEGLStaticContext::libGLESv2.glGetString(GL_VERSION);
        if (s) {
            QByteArray version = QByteArray(reinterpret_cast<const char *>(s));
            int major, minor;
            if (QPlatformOpenGLContext::parseOpenGLVersion(version, major, minor)) {
                m_format.setMajorVersion(major);
                m_format.setMinorVersion(minor);
            }
        }
        m_format.setProfile(QSurfaceFormat::NoProfile);
        m_format.setOptions(QSurfaceFormat::FormatOptions());
        QWindowsEGLStaticContext::libEGL.eglMakeCurrent(prevDisplay, prevSurfaceDraw, prevSurfaceRead, prevContext);
    }
    QWindowsEGLStaticContext::libEGL.eglDestroySurface(m_eglDisplay, pbuffer);
}

QWindowsEGLContext::QWindowsEGLContext(QWindowsEGLStaticContext *staticContext, HGLRC context, HWND window)
    : m_staticContext(staticContext)
{
    // ### wangwenx190: how to initialize this struct without a valid QOpenGLContext ???
    qCWarning(lcQpaGl, "%s is not implemented.", __FUNCTION__);
}

QWindowsEGLContext::~QWindowsEGLContext()
{
    if (m_eglContext != EGL_NO_CONTEXT) {
        QWindowsEGLStaticContext::libEGL.eglDestroyContext(m_eglDisplay, m_eglContext);
        m_eglContext = EGL_NO_CONTEXT;
    }
}

bool QWindowsEGLContext::makeCurrent(QPlatformSurface *surface)
{
    Q_ASSERT(surface->surface()->supportsOpenGL());

    QWindowsEGLStaticContext::libEGL.eglBindAPI(m_api);

    auto *window = static_cast<QWindowsWindow *>(surface);

    int err = 0;
    auto eglSurface = static_cast<EGLSurface>(window->surface(m_eglConfig, &err));
    if (eglSurface == EGL_NO_SURFACE) {
        if (err == EGL_CONTEXT_LOST) {
            m_eglContext = EGL_NO_CONTEXT;
            qCDebug(lcQpaGl) << "QWindowsEGLContext: Got EGL context lost in createWindowSurface() for context" << this;
        } else if (err == EGL_BAD_ACCESS) {
            // With ANGLE this means no (D3D) device and can happen when disabling/changing graphics adapters.
            qCDebug(lcQpaGl) << "QWindowsEGLContext: Bad access (missing device?) in createWindowSurface() for context" << this;
            // Simulate context loss as the context is useless.
            QWindowsEGLStaticContext::libEGL.eglDestroyContext(m_eglDisplay, m_eglContext);
            m_eglContext = EGL_NO_CONTEXT;
        }
        return false;
    }

    // shortcut: on some GPUs, eglMakeCurrent is not a cheap operation
    if (QWindowsEGLStaticContext::libEGL.eglGetCurrentContext() == m_eglContext &&
            QWindowsEGLStaticContext::libEGL.eglGetCurrentDisplay() == m_eglDisplay &&
            QWindowsEGLStaticContext::libEGL.eglGetCurrentSurface(EGL_READ) == eglSurface &&
            QWindowsEGLStaticContext::libEGL.eglGetCurrentSurface(EGL_DRAW) == eglSurface) {
        return true;
    }

    const bool ok = QWindowsEGLStaticContext::libEGL.eglMakeCurrent(m_eglDisplay, eglSurface, eglSurface, m_eglContext);
    if (ok) {
        const int requestedSwapInterval = surface->format().swapInterval();
        if (requestedSwapInterval >= 0 && m_swapInterval != requestedSwapInterval) {
            m_swapInterval = requestedSwapInterval;
            QWindowsEGLStaticContext::libEGL.eglSwapInterval(m_staticContext->display(), m_swapInterval);
        }
    } else {
        err = QWindowsEGLStaticContext::libEGL.eglGetError();
        // EGL_CONTEXT_LOST (loss of the D3D device) is not necessarily fatal.
        // Qt Quick is able to recover for example.
        if (err == EGL_CONTEXT_LOST) {
            m_eglContext = EGL_NO_CONTEXT;
            qCDebug(lcQpaGl) << "QWindowsEGLContext: Got EGL context lost in makeCurrent() for context" << this;
            // Drop the surface. Will recreate on the next makeCurrent.
            window->invalidateSurface();
        } else {
            qCWarning(lcQpaGl, "%s: Failed to make surface current. eglError: %x, this: %p", __FUNCTION__, err, this);
        }
    }

    return ok;
}

void QWindowsEGLContext::doneCurrent()
{
    QWindowsEGLStaticContext::libEGL.eglBindAPI(m_api);
    bool ok = QWindowsEGLStaticContext::libEGL.eglMakeCurrent(m_eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (!ok)
        qCWarning(lcQpaGl, "%s: Failed to make no context/surface current. eglError: %d, this: %p", __FUNCTION__,
                 QWindowsEGLStaticContext::libEGL.eglGetError(), this);
}

void QWindowsEGLContext::swapBuffers(QPlatformSurface *surface)
{
    QWindowsEGLStaticContext::libEGL.eglBindAPI(m_api);
    auto *window = static_cast<QWindowsWindow *>(surface);
    int err = 0;
    auto eglSurface = static_cast<EGLSurface>(window->surface(m_eglConfig, &err));
    if (eglSurface == EGL_NO_SURFACE) {
        if (err == EGL_CONTEXT_LOST) {
            m_eglContext = EGL_NO_CONTEXT;
            qCDebug(lcQpaGl) << "QWindowsEGLContext: Got EGL context lost in createWindowSurface() for context" << this;
        }
        return;
    }

    bool ok = QWindowsEGLStaticContext::libEGL.eglSwapBuffers(m_eglDisplay, eglSurface);
    if (!ok) {
        err =  QWindowsEGLStaticContext::libEGL.eglGetError();
        if (err == EGL_CONTEXT_LOST) {
            m_eglContext = EGL_NO_CONTEXT;
            qCDebug(lcQpaGl) << "QWindowsEGLContext: Got EGL context lost in eglSwapBuffers()";
        } else {
            qCWarning(lcQpaGl, "%s: Failed to swap buffers. eglError: %d, this: %p", __FUNCTION__, err, this);
        }
    }
}

QFunctionPointer QWindowsEGLContext::getProcAddress(const char *procName)
{
    QWindowsEGLStaticContext::libEGL.eglBindAPI(m_api);

    QFunctionPointer procAddress = nullptr;

    // Special logic for ANGLE extensions for blitFramebuffer and
    // renderbufferStorageMultisample. In version 2 contexts the extensions
    // must be used instead of the suffixless, version 3.0 functions.
    if (m_format.majorVersion() < 3) {
        if (!strcmp(procName, "glBlitFramebuffer") || !strcmp(procName, "glRenderbufferStorageMultisample")) {
            char extName[32 + 5 + 1];
            strcpy(extName, procName);
            strcat(extName, "ANGLE");
            procAddress = reinterpret_cast<QFunctionPointer>(QWindowsEGLStaticContext::libEGL.eglGetProcAddress(extName));
        }
    }

    if (!procAddress)
        procAddress = reinterpret_cast<QFunctionPointer>(QWindowsEGLStaticContext::libEGL.eglGetProcAddress(procName));

    // We support AllGLFunctionsQueryable, which means this function must be able to
    // return a function pointer for standard GLES2 functions too. These are not
    // guaranteed to be queryable via eglGetProcAddress().
    if (!procAddress) {
        procAddress = reinterpret_cast<QFunctionPointer>(QWindowsEGLStaticContext::libGLESv2.resolve(procName));
    }

    if (QWindowsContext::verbose > 1)
        qCDebug(lcQpaGl) << __FUNCTION__ <<  procName << QWindowsEGLStaticContext::libEGL.eglGetCurrentContext() << "returns" << procAddress;

    return procAddress;
}

static QVector<EGLint> createConfigAttributesFromFormat(const QSurfaceFormat &format)
{
    int redSize     = format.redBufferSize();
    int greenSize   = format.greenBufferSize();
    int blueSize    = format.blueBufferSize();
    int alphaSize   = format.alphaBufferSize();
    int depthSize   = format.depthBufferSize();
    int stencilSize = format.stencilBufferSize();
    int sampleCount = format.samples();

    QVector<EGLint> configAttributes;
    configAttributes.reserve(16);

    configAttributes.append(EGL_RED_SIZE);
    configAttributes.append(redSize > 0 ? redSize : 0);

    configAttributes.append(EGL_GREEN_SIZE);
    configAttributes.append(greenSize > 0 ? greenSize : 0);

    configAttributes.append(EGL_BLUE_SIZE);
    configAttributes.append(blueSize > 0 ? blueSize : 0);

    configAttributes.append(EGL_ALPHA_SIZE);
    configAttributes.append(alphaSize > 0 ? alphaSize : 0);

    configAttributes.append(EGL_DEPTH_SIZE);
    configAttributes.append(depthSize > 0 ? depthSize : 0);

    configAttributes.append(EGL_STENCIL_SIZE);
    configAttributes.append(stencilSize > 0 ? stencilSize : 0);

    configAttributes.append(EGL_SAMPLES);
    configAttributes.append(sampleCount > 0 ? sampleCount : 0);

    configAttributes.append(EGL_SAMPLE_BUFFERS);
    configAttributes.append(sampleCount > 0);

    return configAttributes;
}

static bool reduceConfigAttributes(QVector<EGLint> *configAttributes)
{
    int i = -1;

    i = configAttributes->indexOf(EGL_SWAP_BEHAVIOR);
    if (i >= 0) {
        configAttributes->remove(i,2);
    }

    i = configAttributes->indexOf(EGL_BUFFER_SIZE);
    if (i >= 0) {
        if (configAttributes->at(i+1) == 16) {
            configAttributes->remove(i,2);
            return true;
        }
    }

    i = configAttributes->indexOf(EGL_SAMPLES);
    if (i >= 0) {
        EGLint value = configAttributes->value(i+1, 0);
        if (value > 1)
            configAttributes->replace(i+1, qMin(EGLint(16), value / 2));
        else
            configAttributes->remove(i, 2);
        return true;
    }

    i = configAttributes->indexOf(EGL_SAMPLE_BUFFERS);
    if (i >= 0) {
        configAttributes->remove(i,2);
        return true;
    }

    i = configAttributes->indexOf(EGL_ALPHA_SIZE);
    if (i >= 0) {
        configAttributes->remove(i,2);
        i = configAttributes->indexOf(EGL_BIND_TO_TEXTURE_RGBA);
        if (i >= 0) {
            configAttributes->replace(i,EGL_BIND_TO_TEXTURE_RGB);
            configAttributes->replace(i+1,true);

        }
        return true;
    }

    i = configAttributes->indexOf(EGL_STENCIL_SIZE);
    if (i >= 0) {
        if (configAttributes->at(i + 1) > 1)
            configAttributes->replace(i + 1, 1);
        else
            configAttributes->remove(i, 2);
        return true;
    }

    i = configAttributes->indexOf(EGL_DEPTH_SIZE);
    if (i >= 0) {
        if (configAttributes->at(i + 1) > 1)
            configAttributes->replace(i + 1, 1);
        else
            configAttributes->remove(i, 2);
        return true;
    }
    i = configAttributes->indexOf(EGL_BIND_TO_TEXTURE_RGB);
    if (i >= 0) {
        configAttributes->remove(i,2);
        return true;
    }

    return false;
}

EGLConfig QWindowsEGLContext::chooseConfig(const QSurfaceFormat &format)
{
    QVector<EGLint> configureAttributes = createConfigAttributesFromFormat(format);
    configureAttributes.append(EGL_SURFACE_TYPE);
    configureAttributes.append(EGL_WINDOW_BIT);
    configureAttributes.append(EGL_RENDERABLE_TYPE);
    if (format.majorVersion() >= 3) {
        configureAttributes.append(EGL_OPENGL_ES3_BIT);
        qCDebug(lcQpaGl) << "QWindowsEGLContext: testing OpenGL ES 3.x context ...";
    } else {
        configureAttributes.append(EGL_OPENGL_ES2_BIT);
        qCDebug(lcQpaGl) << "QWindowsEGLContext: testing OpenGL ES 2.x context ...";
    }
    configureAttributes.append(EGL_NONE);

    EGLDisplay display = m_staticContext->display();
    EGLConfig cfg = nullptr;
    do {
        // Get the number of matching configurations for this set of properties.
        EGLint matching = 0;
        if (!QWindowsEGLStaticContext::libEGL.eglChooseConfig(display, configureAttributes.constData(), nullptr, 0, &matching) || !matching)
            continue;

        // Fetch all of the matching configurations and find the
        // first that matches the pixel format we wanted.
        int i = configureAttributes.indexOf(EGL_RED_SIZE);
        int confAttrRed = configureAttributes.at(i+1);
        i = configureAttributes.indexOf(EGL_GREEN_SIZE);
        int confAttrGreen = configureAttributes.at(i+1);
        i = configureAttributes.indexOf(EGL_BLUE_SIZE);
        int confAttrBlue = configureAttributes.at(i+1);
        i = configureAttributes.indexOf(EGL_ALPHA_SIZE);
        int confAttrAlpha = i == -1 ? 0 : configureAttributes.at(i+1);

        QVector<EGLConfig> configs(matching);
        QWindowsEGLStaticContext::libEGL.eglChooseConfig(display, configureAttributes.constData(), configs.data(), configs.size(), &matching);
        if (!cfg && matching > 0)
            cfg = configs.constFirst();

        EGLint red = 0;
        EGLint green = 0;
        EGLint blue = 0;
        EGLint alpha = 0;
        for (const EGLConfig &config : std::as_const(configs)) {
            if (confAttrRed)
                QWindowsEGLStaticContext::libEGL.eglGetConfigAttrib(display, config, EGL_RED_SIZE, &red);
            if (confAttrGreen)
                QWindowsEGLStaticContext::libEGL.eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &green);
            if (confAttrBlue)
                QWindowsEGLStaticContext::libEGL.eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &blue);
            if (confAttrAlpha)
                QWindowsEGLStaticContext::libEGL.eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &alpha);

            if (red == confAttrRed && green == confAttrGreen
                    && blue == confAttrBlue && alpha == confAttrAlpha)
                return config;
        }
    } while (reduceConfigAttributes(&configureAttributes));

    if (!cfg)
        qCWarning(lcQpaGl, "QWindowsEGLContext: Cannot find EGLConfig, returning null config");

    return cfg;
}

QT_END_NAMESPACE
