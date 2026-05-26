@echo off
REM ============================================================
REM  rodar.bat — Compila e executa o Smart Data Center
REM  Requisitos: MSYS2 MinGW64 instalado em C:\msys64
REM              Raylib 5.5 instalado via pacman no MinGW64
REM              CMake e Ninja disponíveis no PATH do MinGW64
REM ============================================================

setlocal enabledelayedexpansion

REM Adiciona MinGW64 ao PATH desta sessão
set "MINGW=C:\msys64\mingw64\bin"
set "PATH=%MINGW%;%PATH%"

REM Verifica se gcc existe
where gcc >nul 2>&1
if errorlevel 1 (
    echo [ERRO] gcc nao encontrado em %MINGW%
    echo Instale o MSYS2 e execute no terminal MSYS2 MinGW64:
    echo   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake
    echo   pacman -S mingw-w64-x86_64-ninja mingw-w64-x86_64-raylib
    pause
    exit /b 1
)

REM Verifica se cmake existe
where cmake >nul 2>&1
if errorlevel 1 (
    echo [ERRO] cmake nao encontrado.
    echo Instale via: pacman -S mingw-w64-x86_64-cmake
    pause
    exit /b 1
)

REM Diretório de build
set "BUILD_DIR=%~dp0build"

REM Limpa build anterior se existir (descomente para rebuild limpo)
REM if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"

REM Cria diretório de build
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo.
echo [1/3] Configurando CMake com Ninja...
cmake -S "%~dp0" -B "%BUILD_DIR%" ^
      -G "Ninja" ^
      -DCMAKE_C_COMPILER=gcc ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DRAYLIB_ROOT=C:/msys64/mingw64

if errorlevel 1 (
    echo [ERRO] Falha na configuracao do CMake.
    pause
    exit /b 1
)

echo.
echo [2/3] Compilando...
cmake --build "%BUILD_DIR%" --config Release

if errorlevel 1 (
    echo [ERRO] Falha na compilacao.
    pause
    exit /b 1
)

echo.
echo [3/3] Iniciando simulacao...
echo.
"%~dp0smart_data_center.exe"

endlocal
