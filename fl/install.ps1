# install.ps1: puts My lil VJ Link where FL Studio finds it. The VST3 goes to Common Files\VST3 (asks for admin once),
# the controller script to your FL settings. Close FL first. Run: right-click > Run with PowerShell, or pwsh -File install.ps1
# Author: Ari Klopfer / A_Development. AGPL-3.0-or-later
param([string]$Bundle)
$ErrorActionPreference = 'Stop'
$name = 'My lil VJ Link.vst3'
if (-not $Bundle) {   # next to this script (release zip), else the local build
    $Bundle = @((Join-Path $PSScriptRoot $name), (Join-Path $PSScriptRoot "build\MyLilVJLink_artefacts\Release\VST3\$name")) |
        Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $Bundle) { throw "Cannot find '$name' next to this script or in build\ (run build.bat first)" }
$script = Join-Path $PSScriptRoot 'script\device_MyLilVJLink.py'
if (-not (Test-Path -LiteralPath $script)) { throw "Cannot find $script" }

# 1. Controller script: your Documents, no admin needed.
$hw = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'Image-Line\FL Studio\Settings\Hardware'
if (-not (Test-Path -LiteralPath $hw)) { throw "FL Studio settings not found at $hw (start FL Studio once first)" }
$dst = Join-Path $hw 'My lil VJ Link'
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Copy-Item -LiteralPath $script -Destination $dst -Force
$copied = Join-Path $dst 'device_MyLilVJLink.py'
if ((Get-FileHash -LiteralPath $copied).Hash -ne (Get-FileHash -LiteralPath $script).Hash) { throw "Script copy differs: $copied" }
"ok  controller script -> $dst"

# 2. VST3: Common Files needs admin, so one elevated copy (replaces an older My lil VJ Link.vst3 only).
$vst3 = Join-Path $env:CommonProgramFiles 'VST3'
$target = Join-Path $vst3 $name
$cmd = "if (Test-Path -LiteralPath '$target') { Remove-Item -LiteralPath '$target' -Recurse -Force }; " +
       "Copy-Item -LiteralPath '$Bundle' -Destination '$vst3' -Recurse -Force"
Start-Process pwsh -Verb RunAs -Wait -ArgumentList '-NoProfile', '-Command', $cmd
$bin = "Contents\x86_64-win\$name"
if (-not (Test-Path -LiteralPath (Join-Path $target $bin))) { throw "VST3 not installed (admin prompt declined?): $target" }
if ((Get-FileHash -LiteralPath (Join-Path $target $bin)).Hash -ne (Get-FileHash -LiteralPath (Join-Path $Bundle $bin)).Hash) { throw "VST3 copy differs: $target" }
"ok  VST3 -> $target"
'Next, in FL Studio: Options > Manage plugins > Find installed plugins.'
