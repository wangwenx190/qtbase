# QtBase with Windows Vista support

This repo brings Windows Vista support to latest Qt git master code. It's also possible to support Windows XP as well, but it requires much more modification to Qt source code, namely: provide DWM function fallbacks, totally disable non-OpenGL rendering functionalities, totally disable DirectWrite font engine, ensure all bundled 3rd-party libraries also supports Windows XP, etc... and this repo will never try to do such thing (but you can fork this repo and try to add WinXP support yourself).

You can just use this repo to build your own Qt application like the way you use the original official Qt SDK, but please make sure you have enabled the usage of [YY-Thunks](https://github.com/Chuyu-Team/YY-Thunks/). It's also highly recommended to enable [VC-LTL](https://github.com/Chuyu-Team/VC-LTL5/) at the same time to get rid of the annoying API-Sets and MSVC runtime. Old systems such as Win7 lack some API-Set DLLs so enabling VC-LTL would save you much time. In theory this repo can be used alone without any external tools or libraries, but YY-Thunks and VC-LTL will help you cover some edge cases so it's really a good choice to use them altogether.

**IMPORTANT NOTE**

This repo will try its best to use all possible and reliable solutions to workaround missing APIs and functionalities, however, you yourself should make sure you don't request for unsupported technologies on unsupported platforms, such as:

- Try to use D3D 11/12 on systems below Win10.
- Try to use DirectWrite on systems below Win10.
- Try to use any Qt API that relies unsupported Win32 APIs.

You need to either don't use them at all or use different APIs on different platforms explicitly.

Some common scenarios (not a complete list, just to explain how this repo works):

1. QtQuick will default to use D3D11 on Windows, however, Qt requires D3D 11.1 and it's not well supported on Win7. So you should explicitly change the 3D graphics API to Vulkan or OpenGL when your application is running on Win7, otherwise your QtQuick application won't run properly. This repo won't do it for you automatically, because changing the QtQuick scene graph API is a major behavior change and affect too much.

2. `void QThread::setServiceLevel(QThread::QualityOfService serviceLevel)` internally uses a Win32 API which is only available on Win10 and there is no replacement for it on older systems, so you should avoid using this function or at least don't use it on older systems. This repo will load such APIs dynamically on demand at run-time when you call it, so your application can run normally even when the API doesn't exist on current platform and there are no overhead at all if you don't use related functions. But if you really called this function on unsupported platforms, this repo has safe guards for such use cases so nothing will happen, your application won't crash or stuck, but as just mentioned, the missing API won't work at all so your code should have a second path for this scenario.

3. Qt6 by default uses DirectWrite to render text on Windows. However, it's not supported until Win10, so this repo will change Qt's default font engine to GDI automatically when your application is running on Win7, and this repo will also refuse to enable DirectWrite even if you request Qt to use it (to avoid application crash and other critical issues). This action is done internally and automatically, no user actions are needed.

4. QThread will use futex APIs to get better performance, however, these APIs are only available since Win8.1, this repo will fallback to traditional Win32 threading APIs when running on Win7. There will be some performance drop in theory for sure, but the replacements are also MS recommended solutions so the performance will still be in normal range. This action is done internally and automatically, no user actions are needed.

## Useful Environment Variables

- **QT_FORCE_LOAD_PLUGINS_FROM_CORE_DIR**: set it to a non-zero value to force Qt load plugins from QtCore DLL's directory instead of the executable's directory. Mostly useful for products that use sub-directories to distinguish between different versions. Set it before any QCoreApplication instance is created. You can't change it's value at run-time.
- **QT_RHI_DWM_FLUSH**: set it to a non-zero value to request DWM flush every time after DXGI/Vulkan/OpenGL swap chain presentation/swap buffer. You may use this feature to try to reduce window resize flicker on Windows, but the result can't be guaranteed. It will behave very differently on different environments. Set it before any QWindow is created. You can't change it's value at run-time.
- **QT_D3D_PRESENT_RESTART**: set it to a non-zero value to enable "*DXGI_PRESENT_RESTART*" when DXGI swap chain is presenting. Please refer to Microsoft Docs to see it's functionality. Set it before any QWindow is created. You can't change it's value at run-time.
- **QT_QPA_DISABLE_REDIRECTION_SURFACE**: set it to a non-zero value to avoid window render to a redirection surface. Enabling this feature will also totally break classic swap chain model, so you may not use it for QWidget applications or QtQuick applications that use Vulkan or OpenGL to render. For Direct3D applications this feature may increase the drawing performance for a little bit (VERY LITTLE). Set it before any QWindow is created. You can't change it's value at run-time.
- **QT_WINDOWS_SYSTEM_MENU_NEED_OFFSET**: set it to a non-zero value to let the system menu appear below the window top border for a native title bar's height (when DPI is 96, it should be 32px). Mostly useful for frameless windows. Set it before any QWindow is created. You can't change it's value at run-time.
- **QT_ASSUME_STDERR_HAS_CONSOLE**/**QT_FORCE_STDERR_LOGGING**: set it to a non-zero value to let Qt debugging output functions print to `stderr`. Only use it when you have already redirected your `stderr` output to a console, otherwise you can't see any difference.
- **QT_WIN_DEBUG_CONSOLE**: set it to "*new*" to let a GUI application create a separate console window on startup, or set it to "*attach*" if you start the GUI application from a console, Qt will redirect all debug messages to the console window automatically. You don't need to enable it for CLI applications apparently.
- **QT_PLUGIN_PATH**: set it to the file system location where you want Qt to load it's plugins.
- **QT_HIGHDPI_DISABLE_2X_IMAGE_LOADING**: set it to a non-zero value to avoid Qt automatically choose different files if they have the "*@*" character in their file names, eg, `my-image@2x.png`.
- **QT_QPA_PLATFORM_PLUGIN_PATH**: set it to the file system location where you want Qt to load it's platform abstraction plugin, eg, `qwindows.dll`.
- **QT_WIDGETS_HIGHDPI_DOWNSCALE**: set it to a non-zero value to let Qt first render to a larger resolution and then scale it to the expected resolution, which may increase the final image quality, but will have some performance overhead. Only available for QWidget applications.
- **QSG_RHI_PREFER_SOFTWARE_RENDERER**: set it to a non-zero value to force QtQuick use it's internal software rasterizer.
- **QT_WIDGETS_RHI**: set it to a non-zero value to let QWidget render through 3D APIs such as D3D11/D3D12/Vulkan/OpenGL.
- **QT_WIDGETS_RHI_BACKEND**: set it to "*d3d11*"/"*d3d12*"/"*vulkan*"/"*opengl*" to switch between different 3D APIs. Only available when you have enabled QWidget's RHI rendering.
- **QSG_INFO**: set it to a non-zero value to enable basic debugging information output for Qt SceneGraph engine.
- **QSG_RHI_BACKEND**: similar to *QT_WIDGETS_RHI_BACKEND*, but for QtQuick applications.
- **QT_D3D_MAX_FRAME_LATENCY**: set it to an appropriate value to change Qt RHI's default maximum frame latency value (2) for Direct3D.
- **QT_D3D_ADAPTER_INDEX**: set it to an appropriate value to change Qt RHI's default adapter for Direct3D.
- **QT_VK_PHYSICAL_DEVICE_INDEX**: set it to an appropriate value to change Qt RHI's default adapter for Vulkan.
- **QT_NO_OPENGL_BUGLIST**: set it to a non-zero value to disable Qt's internal graphics driver black list, otherwise Qt may use it's internal software rasterizer to render OpenGL content.
- **QT_OPENGL_FORCE_ANGLE**: set it to a non-zero value to force Qt use ANGLE instead of desktop OpenGL. Will be ignored if the graphics backend is not OpenGL. Please make sure libEGL.dll & libGLESv2.dll is in your executable's folder. You can copy them from Google Chrome/Microsoft Edge's installation folder, or build them yourself using vcpkg.
- **QT_ANGLE_BACKEND**: set it to "*d3d11*"/"*warp*"/"*d3d11on12*"/"*vulkan*"/"*swiftshader*" to choose ANGLE's backend. Be careful, you have to ensure your application has necessary libraries and configurations to make the requested ANGLE backend work, for example, if you want to use the Vulkan backend, you have to put Vulkan's runtime library in your executable folder (putting it in your system directory won't work at all! ANGLE will only look for it from your executable directory!).
- **QT_QPA_UPDATE_IDLE_TIME**: `QWindow` will automatically trigger a window repaint event according to your monitor's maximum refresh rate (eg, `QWindow` will always try to repaint the window every 4 ms if your monitor's max refresh rate is 240 Hz), you can tweak the update interval through this env var. The unit is *ms*.
- **QT_IMAGEIO_MAXALLOC**: `QImageReader` will refuse to load an image if the file size is greater than the maximum file size limit, and you can tweak the max size through this env var. The unit is *MB*. You can set it to 0 to disable `QImageReader`'s file size check. By default, `QImageReader` uses *256 MB* as the max file size.
- **QT_FONT_DPI**: you can use this env var to override Qt's DPI setting (will apply to all `QScreen`s, not quite related to `QFont` actually).
- **QT_D3D_NO_VBLANK_THREAD**: set it to 0 to disable QRhi's V-Blank refresh thread. By default, QRhi will create a separate thread to trigger frame updates according to your monitor's maximum refresh rate, you can disable this thread through this env var.

### IMPORTANT NOTES

- Please make sure you have toally understand their usage, otherwise don't attempt to set any of these internal environment variables! 
- Most of them need to be set at a very early stage in the application's whole life time.
- Most of them will only be read once and then cached, so you can't change them dynamically at run-time.
