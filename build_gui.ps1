param([Parameter(Mandatory=$true)][string]$Python)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
& mingw32-make all
if ($LASTEXITCODE -ne 0) { throw 'C build failed' }
$env:PYTHONPATH = Join-Path $PSScriptRoot 'build/packaging-deps'
$env:PYTHONIOENCODING = 'utf-8'
$env:PYINSTALLER_CONFIG_DIR = Join-Path $PSScriptRoot 'build/pyinstaller-cache'
# В среде Codex Tcl размещён в защищённом каталоге WindowsApps.
# В обычной установке Python копирование не требуется; для сборки используем локальную копию.
$pythonDir = Split-Path -Parent $Python
if (Test-Path -LiteralPath (Join-Path $pythonDir 'tcl/tcl8.6')) {
    New-Item -ItemType Directory -Force build/tcl-runtime | Out-Null
    Copy-Item -LiteralPath (Join-Path $pythonDir 'tcl/tcl8.6') -Destination build/tcl-runtime -Recurse -Force
    Copy-Item -LiteralPath (Join-Path $pythonDir 'tcl/tk8.6') -Destination build/tcl-runtime -Recurse -Force
    $env:TCL_LIBRARY = Join-Path $PSScriptRoot 'build/tcl-runtime/tcl8.6'
    $env:TK_LIBRARY = Join-Path $PSScriptRoot 'build/tcl-runtime/tk8.6'
}
$binary = (Join-Path $PSScriptRoot 'build/rsa_core.dll') + ':.'
$examples = (Join-Path $PSScriptRoot 'examples') + ':examples'
$assets = (Join-Path $PSScriptRoot 'assets') + ':assets'
$icon = Join-Path $PSScriptRoot 'assets/rsa.ico'
& $Python -m PyInstaller --noconfirm --windowed --onedir --name RSA-Analyzer --noupx `
    --distpath release --workpath build/pyinstaller --specpath build `
    --add-binary $binary --add-data $examples --add-data $assets --icon $icon gui/main.py
if ($LASTEXITCODE -ne 0) { throw 'GUI packaging failed' }
Copy-Item -LiteralPath 'README.md' -Destination 'release/RSA-Analyzer/README.md' -Force
Copy-Item -LiteralPath 'docs/Запуск приложения.txt' -Destination 'release/RSA-Analyzer/Начните здесь.txt' -Force
$licenseDir = Join-Path $PSScriptRoot 'release/RSA-Analyzer/licenses'
New-Item -ItemType Directory -Force $licenseDir | Out-Null
foreach ($entry in @(@('LICENSE.txt', 'Python-LICENSE.txt'), @('tcl/tk8.6/license.terms', 'Tk-LICENSE.txt'))) {
    $source = Join-Path $pythonDir $entry[0]
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $licenseDir $entry[1]) -Force
    }
}
$pyinstallerLicense = 'build/packaging-deps/pyinstaller-6.22.3.dist-info/licenses/COPYING.txt'
if (Test-Path -LiteralPath $pyinstallerLicense) {
    Copy-Item -LiteralPath $pyinstallerLicense -Destination (Join-Path $licenseDir 'PyInstaller-COPYING.txt') -Force
}
