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

#ifndef QWINDOWSEGLCONTEXT_H
#define QWINDOWSEGLCONTEXT_H

#include <QtCore/qt_windows.h>
#include "qwindowsopenglcontext.h"

#define EGL_CAST(type, value) (static_cast<type>(value))

#define EGL_NO_CONTEXT                    EGL_CAST(EGLContext,0)
#define EGL_NO_DISPLAY                    EGL_CAST(EGLDisplay,0)
#define EGL_NO_SURFACE                    EGL_CAST(EGLSurface,0)

#define EGLAPIENTRY APIENTRY

#define EGL_OPENGL_ES_API                 0x30A0

typedef __int32 EGLint;
typedef unsigned int EGLBoolean;
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLSurface;
typedef void *EGLContext;
typedef unsigned int EGLenum;
typedef void *EGLClientBuffer;

typedef HDC EGLNativeDisplayType;
typedef HBITMAP EGLNativePixmapType;
typedef HWND EGLNativeWindowType;

QT_BEGIN_NAMESPACE

struct QWindowsLibEGL
{
    bool init();

    EGLint (EGLAPIENTRY * eglGetError)(void);
    EGLDisplay (EGLAPIENTRY * eglGetDisplay)(EGLNativeDisplayType display_id);
    EGLBoolean (EGLAPIENTRY * eglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
    EGLBoolean (EGLAPIENTRY * eglTerminate)(EGLDisplay dpy);
    EGLBoolean (EGLAPIENTRY * eglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list,
                                               EGLConfig *configs, EGLint config_size,
                                               EGLint *num_config);
    EGLBoolean (EGLAPIENTRY * eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config,
                                                  EGLint attribute, EGLint *value);
    EGLSurface (EGLAPIENTRY * eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                                      EGLNativeWindowType win,
                                                      const EGLint *attrib_list);
    EGLSurface (EGLAPIENTRY * eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                                       const EGLint *attrib_list);
    EGLBoolean (EGLAPIENTRY * eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
    EGLBoolean (EGLAPIENTRY * eglBindAPI)(EGLenum api);
    EGLBoolean (EGLAPIENTRY * eglSwapInterval)(EGLDisplay dpy, EGLint interval);
    EGLContext (EGLAPIENTRY * eglCreateContext)(EGLDisplay dpy, EGLConfig config,
                                                EGLContext share_context,
                                                const EGLint *attrib_list);
    EGLBoolean (EGLAPIENTRY * eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
    EGLBoolean (EGLAPIENTRY * eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw,
                                              EGLSurface read, EGLContext ctx);
    EGLContext (EGLAPIENTRY * eglGetCurrentContext)(void);
    EGLSurface (EGLAPIENTRY * eglGetCurrentSurface)(EGLint readdraw);
    EGLDisplay (EGLAPIENTRY * eglGetCurrentDisplay)(void);
    EGLBoolean (EGLAPIENTRY * eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
    QFunctionPointer (EGLAPIENTRY *eglGetProcAddress)(const char *procname);

    EGLDisplay (EGLAPIENTRY * eglGetPlatformDisplayEXT)(EGLenum platform, void *native_display, const EGLint *attrib_list);

private:
    QFunctionPointer resolve(const char *name) const;
    HMODULE m_lib = nullptr;
};

struct QWindowsLibGLESv2
{
    bool init();

    inline void *moduleHandle() const { return m_lib; }

    const GLubyte * (APIENTRY * glGetString)(GLenum name);

    QFunctionPointer resolve(const char *name) const;

private:
    HMODULE m_lib = nullptr;
};

class QWindowsEGLStaticContext : public QWindowsStaticOpenGLContext
{
    Q_DISABLE_COPY_MOVE(QWindowsEGLStaticContext)

public:
    static QWindowsEGLStaticContext *create();
    ~QWindowsEGLStaticContext() override;

    inline EGLDisplay display() const { return m_display; }

    QWindowsOpenGLContext *createContext(QOpenGLContext *context) override;
    QWindowsOpenGLContext *createContext(HGLRC context, HWND window) override;
    inline void *moduleHandle() const override { return libGLESv2.moduleHandle(); }
    inline QOpenGLContext::OpenGLModuleType moduleType() const override { return QOpenGLContext::LibGLES; }
    inline bool supportsThreadedOpenGL() const override { return true; }

    void *createWindowSurface(void *nativeWindow, void *nativeConfig, int *err) override;
    void destroyWindowSurface(void *nativeSurface) override;

    QSurfaceFormat formatFromConfig(EGLDisplay display, EGLConfig config, const QSurfaceFormat &referenceFormat);

    static QWindowsLibEGL libEGL;
    static QWindowsLibGLESv2 libGLESv2;

private:
    QWindowsEGLStaticContext(EGLDisplay display);
    static void initializeAngle(HDC dc, EGLDisplay *display, EGLint *major, EGLint *minor);

    const EGLDisplay m_display;
};

class QWindowsEGLContext : public QWindowsOpenGLContext
{
public:
    QWindowsEGLContext(QWindowsEGLStaticContext *staticContext, QOpenGLContext *context);
    QWindowsEGLContext(QWindowsEGLStaticContext *staticContext, HGLRC context, HWND window);
    ~QWindowsEGLContext() override;

    bool makeCurrent(QPlatformSurface *surface) override;
    void doneCurrent() override;
    void swapBuffers(QPlatformSurface *surface) override;
    QFunctionPointer getProcAddress(const char *procName) override;

    inline QSurfaceFormat format() const override { return m_format; }
    inline bool isSharing() const override { return m_shareContext != EGL_NO_CONTEXT; }
    inline bool isValid() const override { return m_eglContext != EGL_NO_CONTEXT; }

    inline void *nativeContext() const /*override*/ { return m_eglContext; }
    inline void *nativeDisplay() const override { return m_eglDisplay; }
    inline void *nativeConfig() const override { return m_eglConfig; }

private:
    EGLConfig chooseConfig(const QSurfaceFormat &format);

    QWindowsEGLStaticContext *m_staticContext;
    EGLContext m_eglContext;
    EGLContext m_shareContext;
    EGLDisplay m_eglDisplay;
    EGLConfig m_eglConfig;
    QSurfaceFormat m_format;
    EGLenum m_api = EGL_OPENGL_ES_API;
    int m_swapInterval = -1;
};

QT_END_NAMESPACE

#endif // QWINDOWSEGLCONTEXT_H
