@echo off
chcp 65001
title Qt build test
setlocal enabledelayedexpansion
cls
set /p __compiler="Please choose the compiler: [1] MSVC [2] Clang-CL [3] MinGW"
set __msvc=0
set __clang=0
set __mingw=0
if /i "%__compiler%" == "2" (
    set __msvc=1
    set __clang=1
) else if /i "%__compiler%" == "3" (
    set __mingw=1
) else (
    set __msvc=1
)
set __msvc_edition=1
if /i "%__msvc%" == "1" set /p __msvc_edition="Please choose the MSVC edition: [1] 2022 [2] 2019 [3] 2017"
if /i "%__msvc_edition%" == "2" (
    set __msvc_edition=2019
) else if /i "%__msvc_edition%" == "3" (
    set __msvc_edition=2017
) else (
    set __msvc_edition=2022
)
set __llvm_bin=
if /i "%__clang%" == "1" (
    if /i "%LLVM_INSTALL_DIR%" == "" (
        set /p __llvm_bin="Please input your LLVM bin directory path:"
        if /i "!__llvm_bin!" == "" (
            echo You didn't input a valid path. Please make sure you have add it to the PATH environment variable.
        ) else (
            set PATH=!__llvm_bin!;!PATH!
        )
    ) else (
        set PATH=%LLVM_INSTALL_DIR%\bin;!PATH!
    )
)
set __mingw_bin=
if /i "%__mingw%" == "1" (
    set /p __mingw_bin="Please input your MinGW bin directory path:"
    if /i "!__mingw_bin!" == "" (
        echo You didn't input a valid path. Please make sure you have add it to the PATH environment variable.
    ) else (
        set PATH=!__mingw_bin!;!PATH!
    )
)
set /p __arch="Please choose the architecture: [1] x64 [2] x86 [3] arm64"
if /i "%__arch%" == "2" (
    set __arch=x86
) else if /i "%__arch%" == "3" (
    set __arch=arm64
) else (
    set __arch=x64
)
echo =================================================
if /i "%__mingw%" == "1" (
    echo Compiler: MinGW
) else if /i "%__clang%" == "1" (
    echo Compiler: Clang-CL
) else (
    echo Compiler: MSVC
)
if /i "%__msvc%" == "1" echo MSVC edition: %__msvc_edition%
if /i "%__mingw_bin%" == "" (
    echo a>nul
) else (
    echo MinGW bin directory: %__mingw_bin%
)
echo Architecture: %__arch%
echo =================================================
echo If everything is OK, please press the [ENTER] key to continue.
echo Or if anything is wrong, please close this window directly and re-run this script.
pause
set __vcvars_suffix=Microsoft Visual Studio\%__msvc_edition%\Community\VC\Auxiliary\Build\vcvars
if /i "%__arch%" == "x86" (
    set __vcvars_suffix=%__vcvars_suffix%32
) else if /i "%__arch%" == "arm64" (
    set __vcvars_suffix=%__vcvars_suffix%arm64
) else (
    set __vcvars_suffix=%__vcvars_suffix%64
)
set __vcvars_suffix=%__vcvars_suffix%.bat
set __vcvars_path1=%ProgramFiles%\%__vcvars_suffix%
set __vcvars_path2=%ProgramFiles(x86)%\%__vcvars_suffix%
if /i "%__msvc%" == "1" (
    if exist "%__vcvars_path1%" (
        call "%__vcvars_path1%"
    ) else if exist "%__vcvars_path2%" (
        call "%__vcvars_path2%"
    ) else (
        echo Cannot find vcvarsall.bat! Please install the MSVC toolchain first.
        goto end
    )
)
set PATH=%~dp0bin;%PATH%
set QT_ENABLE_VCLTL=1
set QT_ENABLE_YYTHUNKS=1
cd /d "%~dp0"
if exist build-test rd /s /q build-test
md build-test
cd build-test
md cmake
cd cmake
set __toolchain_flags=
if /i "%__mingw%" == "1" (
    set __toolchain_flags=-DCMAKE_C_COMPILER=gcc.exe -DCMAKE_CXX_COMPILER=g++.exe
) else if /i "%__clang%" == "1" (
    set __toolchain_flags=-DCMAKE_C_COMPILER=clang-cl.exe -DCMAKE_CXX_COMPILER=clang-cl.exe
) else (
    set __toolchain_flags=-DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe
)
:: -DINPUT_intelcet=yes -DINPUT_spectre=yes -DINPUT_ehcont=yes -DINPUT_cfguard=yes
set __common_flags=-Wno-dev %__toolchain_flags% -DCMAKE_MESSAGE_LOG_LEVEL=STATUS -DCMAKE_BUILD_TYPE=Release -GNinja -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF -DFEATURE_relocatable=ON -DFEATURE_cxx20=ON -DQT_DISABLE_DEPRECATED_UP_TO=0x070000
cmake %__common_flags% -DCMAKE_INSTALL_PREFIX="%~dp0build-test\shared" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=ON -DBUILD_SHARED_LIBS=ON "%~dp0"
echo =================================================
echo Configuration done.
echo Please press the [ENTER] key to start compilation if everything is OK,
echo or close this window directly and re-run this script if there is anything wrong.
echo =================================================
pause
cmake --build . --config Release --target all --parallel
cmake --install . --config Release --strip
cd ..
rd /s /q cmake
echo =================================================
echo The shared library edition has been built. You can check the final binary files now.
echo Please press the [ENTER] key to start building the static library edition,
echo or close this window directly to cancel it.
echo =================================================
pause
md cmake
cd cmake
cmake %__common_flags% -DCMAKE_INSTALL_PREFIX="%~dp0build-test\static" -DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE=OFF -DBUILD_SHARED_LIBS=OFF "%~dp0"
echo =================================================
echo Configuration done.
echo Please press the [ENTER] key to start compilation if everything is OK,
echo or close this window directly and re-run this script if there is anything wrong.
echo =================================================
pause
cmake --build . --config Release --target all --parallel
cmake --install . --config Release --strip
cd ..
rd /s /q cmake
echo =================================================
echo The static library edition has been built. You can check the final binary files now.
echo Please press the [ENTER] key to exit from this script, or close this window directly.
echo =================================================
goto end

:end
endlocal
cd /d "%~dp0"
pause
exit /b 0
