<#
  build_release.ps1
  Compila o projeto, coleta as DLLs necessárias e empacota tudo em ZIP.
  O ZIP resultante roda em qualquer Windows sem instalar nada.
  Uso: .\build_release.ps1 [-Version "1.0.0"]
#>
param(
    [string]$Version = "1.0.0"
)

$ErrorActionPreference = "Stop"
$ROOT     = $PSScriptRoot
$BUILD    = Join-Path $ROOT "build"
$DIST     = Join-Path $ROOT "dist"
$MINGW    = "C:\msys64\mingw64\bin"
$ZIPNAME  = "smart_data_center_v${Version}_windows_x64.zip"
$ZIPPATH  = Join-Path $ROOT $ZIPNAME

# 1. Compilar (build normal, sem flags especiais)
Write-Host "=== Compilando..."
cmake -S $ROOT -B $BUILD -G Ninja -DCMAKE_BUILD_TYPE=Release | Out-Null
cmake --build $BUILD --config Release
if ($LASTEXITCODE -ne 0) { Write-Error "Build falhou."; exit 1 }

$exe = Join-Path $ROOT "smart_data_center.exe"
if (-not (Test-Path $exe)) { Write-Error "EXE nao encontrado: $exe"; exit 1 }

# 2. Montar pasta dist/
Write-Host "`n=== Montando dist/..."
if (Test-Path $DIST) { Remove-Item -Recurse -Force $DIST }
New-Item -ItemType Directory $DIST | Out-Null

Copy-Item $exe          $DIST
Copy-Item "$MINGW\libraylib.dll" $DIST
Copy-Item "$MINGW\glfw3.dll"     $DIST

Get-ChildItem $DIST | ForEach-Object {
    "  $($_.Name)  ($([int]($_.Length / 1KB)) KB)"
}

# 3. Empacotar ZIP
Write-Host "`n=== Empacotando $ZIPNAME ..."
if (Test-Path $ZIPPATH) { Remove-Item $ZIPPATH -Force }
Compress-Archive -Path "$DIST\*" -DestinationPath $ZIPPATH
$zipKB = [int]((Get-Item $ZIPPATH).Length / 1KB)
Write-Host "  OK: $ZIPPATH  ($zipKB KB)"

# 4. Limpar pasta temporária
Remove-Item -Recurse -Force $DIST

Write-Host "`n=== CONCLUIDO ==="
Write-Host "Distribuivel: $ZIPPATH"
Write-Host "Conteudo: smart_data_center.exe + libraylib.dll + glfw3.dll"
