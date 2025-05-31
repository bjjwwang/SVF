@echo off
setlocal

:: 检查必要的工具是否存在
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: CMake not found. Please install CMake first.
    exit /b 1
)

where git >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: Git not found. Please install Git first.
    exit /b 1
)

where ninja >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: Ninja not found. Please install Ninja first.
    exit /b 1
)

:: 设置环境变量
set GITHUB_WORKSPACE=%cd%
set INSTALL_DIR=%GITHUB_WORKSPACE%\install
set LLVM_URL=https://github.com/bjjwwang/SVF-LLVM/releases/download/16.0.0/llvm-windows-build.zip
set Z3_URL=https://github.com/Z3Prover/z3/releases/download/z3-4.15.0/z3-4.15.0-x64-win.zip
set LLVM_DIR=%GITHUB_WORKSPACE%\llvm-windows-build
set Z3_DIR=%GITHUB_WORKSPACE%\z3-4.15.0-x64-win

:: 下载并解压 LLVM
if not exist "%LLVM_DIR%" (
    echo Downloading LLVM...
    curl -L -o llvm.zip %LLVM_URL%
    mkdir "%LLVM_DIR%"
    tar -xf llvm.zip -C "%LLVM_DIR%"
    del llvm.zip
)

:: 下载并解压 Z3
if not exist "%Z3_DIR%" (
    echo Downloading Z3...
    curl -L -o z3.zip %Z3_URL%
    tar -xf z3.zip -C "%GITHUB_WORKSPACE%"
    del z3.zip
)

:: 构建 SVF
echo Building SVF...
mkdir build
cd build
cmake -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DLLVM_DIR=%LLVM_DIR% -DZ3_DIR=%Z3_DIR% -DSVF_USE_PIC=ON -DBUILD_SHARED_LIBS=OFF -DSVF_ENABLE_RTTI=ON -DCMAKE_INSTALL_PREFIX=%INSTALL_DIR% ..
cmake --build . --config Debug
cmake --install .
cd ..

endlocal