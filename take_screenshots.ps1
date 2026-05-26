<#
  take_screenshots.ps1
  Captura screenshots de cada estado do simulador e gera animated GIF.
  Uso: .\take_screenshots.ps1
#>

Set-StrictMode -Off
$ErrorActionPreference = "Continue"

$ROOT   = $PSScriptRoot
$ASSETS = Join-Path $ROOT "assets"
New-Item -ItemType Directory -Force $ASSETS | Out-Null

# ---------------------------------------------------------------------------
# Win32 helpers
# ---------------------------------------------------------------------------
Add-Type -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class Win {
    [DllImport("user32.dll")] public static extern bool  SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool  ShowWindow(IntPtr h, int n);
    [DllImport("user32.dll")] public static extern bool  GetWindowRect(IntPtr h, ref RECT r);
    [DllImport("user32.dll")] public static extern bool  GetClientRect(IntPtr h, ref RECT r);
    [DllImport("user32.dll")] public static extern bool  ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool  PrintWindow(IntPtr h, IntPtr hdc, uint f);
    [DllImport("user32.dll")] public static extern bool  SetCursorPos(int x, int y);
    [DllImport("user32.dll")] public static extern uint  SendInput(uint n, INPUT[] inp, int sz);

    [StructLayout(LayoutKind.Sequential)] public struct RECT  { public int L,T,R,B; }
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x,y; }

    // SendInput structures
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT {
        public int dx, dy; public uint mouseData, dwFlags, time; public UIntPtr extraInfo;
    }
    [StructLayout(LayoutKind.Sequential)] public struct INPUT {
        public uint type; public MOUSEINPUT mi;
    }
    const uint INPUT_MOUSE      = 0;
    const uint MOUSEEVENTF_MOVE = 0x0001;
    const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    const uint MOUSEEVENTF_LEFTUP   = 0x0004;
    const uint MOUSEEVENTF_ABSOLUTE = 0x8000;

    public static System.Drawing.Size GetClientSize(IntPtr hwnd) {
        RECT r = new RECT();
        GetClientRect(hwnd, ref r);
        return new System.Drawing.Size(r.R - r.L, r.B - r.T);
    }

    public static Bitmap Capture(IntPtr hwnd) {
        RECT r = new RECT();
        GetWindowRect(hwnd, ref r);
        int w = r.R - r.L, h = r.B - r.T;
        if (w <= 0 || h <= 0) return null;
        var bmp = new Bitmap(w, h, PixelFormat.Format32bppArgb);
        using (var g = Graphics.FromImage(bmp)) {
            IntPtr hdc = g.GetHdc();
            PrintWindow(hwnd, hdc, 2);   // PW_RENDERFULLCONTENT
            g.ReleaseHdc(hdc);
        }
        return bmp;
    }

    public static void Click(IntPtr hwnd, int clientX, int clientY) {
        // Converter coords de cliente para coords de ecrã
        POINT p = new POINT { x = clientX, y = clientY };
        ClientToScreen(hwnd, ref p);

        // Converter para coordenadas absolutas (0-65535)
        int sw = System.Windows.Forms.Screen.PrimaryScreen.Bounds.Width;
        int sh = System.Windows.Forms.Screen.PrimaryScreen.Bounds.Height;
        int ax = (p.x * 65535) / sw;
        int ay = (p.y * 65535) / sh;

        var inputs = new INPUT[3];

        // Mover cursor
        inputs[0].type = INPUT_MOUSE;
        inputs[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        inputs[0].mi.dx = ax; inputs[0].mi.dy = ay;

        // Botao esquerdo baixo
        inputs[1].type = INPUT_MOUSE;
        inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_ABSOLUTE;
        inputs[1].mi.dx = ax; inputs[1].mi.dy = ay;

        // Botao esquerdo cima
        inputs[2].type = INPUT_MOUSE;
        inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP | MOUSEEVENTF_ABSOLUTE;
        inputs[2].mi.dx = ax; inputs[2].mi.dy = ay;

        SetForegroundWindow(hwnd);
        System.Threading.Thread.Sleep(80);
        SendInput(3, inputs, System.Runtime.InteropServices.Marshal.SizeOf(typeof(INPUT)));
    }
}
"@ -ReferencedAssemblies System.Drawing,System.Windows.Forms

# ---------------------------------------------------------------------------
# Iniciar o simulador
# ---------------------------------------------------------------------------
Write-Host "Iniciando simulador..."
$proc = Start-Process -FilePath (Join-Path $ROOT "smart_data_center.exe") `
                      -WorkingDirectory $ROOT -PassThru

$timeout = 0
do {
    Start-Sleep -Milliseconds 500
    $proc.Refresh()
    $timeout++
} while (($proc.MainWindowHandle -eq [IntPtr]::Zero) -and ($timeout -lt 24))

if ($proc.MainWindowHandle -eq [IntPtr]::Zero) {
    Write-Error "Janela nao apareceu."; exit 1
}

$hwnd = $proc.MainWindowHandle

# Mostrar normal (NAO maximizado) e trazer para frente
[Win]::ShowWindow($hwnd, 9) | Out-Null    # SW_RESTORE
Start-Sleep -Milliseconds 200
[Win]::SetForegroundWindow($hwnd) | Out-Null
Start-Sleep -Milliseconds 3000            # aguardar render inicial completo

# Obter tamanho real do cliente
$cs = [Win]::GetClientSize($hwnd)
$cw = $cs.Width
$ch = $cs.Height
Write-Host "Janela: $hwnd  Cliente: ${cw}x${ch}"

# ---------------------------------------------------------------------------
# Calcular posicoes dos botoes dinamicamente
# ---------------------------------------------------------------------------
# btn_y = ch - 54  (top do botao)
# centro y = ch - 54 + 14
$BTN_CY = $ch - 54 + 14

$BTNS = @{
    Neural  = @(64,  $BTN_CY)
    Compare = @(148, $BTN_CY)
    Heatmap = @(190, $BTN_CY)
    ARM     = @(106, $BTN_CY)
    Crac    = @(232, $BTN_CY)
}
Write-Host "Botoes em y=$BTN_CY  (ch=$ch)"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Save-Shot([string]$name) {
    Start-Sleep -Milliseconds 900
    $bmp = [Win]::Capture($hwnd)
    if ($bmp) {
        $path = Join-Path $ASSETS "$name.png"
        $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        Write-Host "  -> $path"
        return $path
    }
    Write-Warning "  FALHOU: $name"; return $null
}

function Click-Btn([string]$key) {
    [Win]::Click($hwnd, $BTNS[$key][0], $BTNS[$key][1])
    Start-Sleep -Milliseconds 150
}

# ---------------------------------------------------------------------------
# Screenshots
# ---------------------------------------------------------------------------
Write-Host "`n[1/6] Vista principal..."
$null = Save-Shot "01_main"

Write-Host "[2/6] Rede Neural MLP..."
Click-Btn "Neural"; $null = Save-Shot "02_neural"
Click-Btn "Neural"; Start-Sleep -Milliseconds 400

Write-Host "[3/6] Fuzzy vs Histerese..."
Click-Btn "Compare"; $null = Save-Shot "03_compare"
Click-Btn "Compare"; Start-Sleep -Milliseconds 400

Write-Host "[4/6] Mapa de Calor..."
Click-Btn "Heatmap"; $null = Save-Shot "04_heatmap"
Click-Btn "Heatmap"; Start-Sleep -Milliseconds 400

Write-Host "[5/6] ARM vs AVR..."
Click-Btn "ARM"; $null = Save-Shot "05_arm"
Click-Btn "ARM"; Start-Sleep -Milliseconds 400

Write-Host "[6/6] Falha CRAC..."
Click-Btn "Crac"                    # 0->1
Start-Sleep -Milliseconds 1800
$null = Save-Shot "06_crac_fail"
Click-Btn "Crac"; Click-Btn "Crac"  # restaurar

# ---------------------------------------------------------------------------
# GIF: 18 frames × 0.45 s
# ---------------------------------------------------------------------------
Write-Host "`nCapturando frames para GIF..."
[Win]::SetForegroundWindow($hwnd) | Out-Null
$framePaths = @()
for ($i = 0; $i -lt 18; $i++) {
    $bmp = [Win]::Capture($hwnd)
    if ($bmp) {
        $p = Join-Path $ASSETS ("gif_{0:D2}.png" -f $i)
        $bmp.Save($p, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        $framePaths += $p
        Write-Host ("  frame {0:D2}" -f $i)
    }
    Start-Sleep -Milliseconds 450
}

# ---------------------------------------------------------------------------
# Montar GIF com Python + Pillow
# ---------------------------------------------------------------------------
Write-Host "`nMontando demo.gif..."
$pyScript = Join-Path $ASSETS "_make_gif.py"
$frameListStr = ($framePaths | ForEach-Object { "`"$($_ -replace '\\','/')`"" }) -join ",`n    "

@"
from PIL import Image
import os

paths = [
    $frameListStr
]
paths = [p for p in paths if os.path.exists(p)]

imgs   = [Image.open(p).convert("RGBA") for p in paths]
imgs   = [img.resize((960, 540), Image.LANCZOS) for img in imgs]
imgs_q = [img.quantize(colors=192, dither=Image.Dither.NONE) for img in imgs]

out = r"$($ASSETS -replace '\\','/')/demo.gif"
imgs_q[0].save(out, save_all=True, append_images=imgs_q[1:],
               duration=380, loop=0, optimize=False)

sz = os.path.getsize(out) // 1024
print(f"GIF: {out}  ({sz} KB, {len(imgs_q)} frames)")

for p in paths:
    try: os.remove(p)
    except: pass
os.remove(os.path.abspath(__file__))
"@ | Set-Content -Path $pyScript -Encoding UTF8

python $pyScript

Write-Host "`n=== CONCLUIDO ==="
Get-ChildItem $ASSETS | ForEach-Object { "  $($_.Name)  ($([int]($_.Length/1024)) KB)" }
