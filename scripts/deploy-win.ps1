# Assemble dist\ from a Release build: the layout melo.exe looks for beside
# itself (main.cpp, SidecarProcess.cpp, SidecarService.cpp). Input for
# scripts/melo.iss and the portable zip.
param(
  [string]$Build = "build",
  [string]$Out = "dist",
  [string]$GstRoot = $env:GSTREAMER_1_0_ROOT_MSVC_X86_64,
  [string]$VcpkgBin = "$env:VCPKG_INSTALLATION_ROOT\installed\x64-windows\bin"
)
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path "$PSScriptRoot\..").Path

function Robo($from, $to, [string[]]$extra = @()) {
  # robocopy copies symlink TARGETS by default, which the plugin import dir needs
  robocopy $from $to /E /NFL /NDL /NJH /NJS /NP @extra | Out-Null
  if ($LASTEXITCODE -ge 8) { throw "robocopy $from -> $to failed ($LASTEXITCODE)" }
  $global:LASTEXITCODE = 0
}

if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
New-Item -ItemType Directory $Out | Out-Null

Copy-Item "$Build\melo.exe" $Out
& windeployqt --release --no-translations --no-system-d3d-compiler `
  --qmldir "$Root\src\qml" "$Out\melo.exe"
if ($LASTEXITCODE) { throw "windeployqt failed" }

# MSVC C++ runtime, app-local: GStreamer's bin\ carries none, and a clean PC
# may not have the VC++ redistributable installed
if (-not $env:VCToolsRedistDir) { throw "VCToolsRedistDir is not set; run from an MSVC developer shell (ilammy/msvc-dev-cmd)" }
$crt = Get-ChildItem "$env:VCToolsRedistDir\x64" -Directory -Filter 'Microsoft.VC14*.CRT' | Select-Object -First 1
if (-not $crt) { throw "no Microsoft.VC14*.CRT under $env:VCToolsRedistDir\x64" }
foreach ($pat in 'vcruntime140.dll', 'vcruntime140_1.dll', 'msvcp140*.dll', 'concrt140.dll') {
  Copy-Item -Force "$($crt.FullName)\$pat" $Out
}

# visualiser
Copy-Item -Force "$Root\vendor\projectm4\bin\*.dll" $Out
Copy-Item -Force "$VcpkgBin\glew32.dll" $Out

# GStreamer: runtime DLLs beside melo.exe, plugins and gio modules in the
# folders main.cpp points GST_PLUGIN_PATH / GIO_EXTRA_MODULES at
Copy-Item -Force "$GstRoot\bin\*.dll" $Out
New-Item -ItemType Directory "$Out\gst-plugins", "$Out\gio-modules" | Out-Null
Copy-Item "$GstRoot\lib\gstreamer-1.0\*.dll" "$Out\gst-plugins"
# the python loader needs a system python39.dll; melo uses no python elements
Remove-Item "$Out\gst-plugins\gstpython.dll" -ErrorAction SilentlyContinue
Copy-Item "$GstRoot\lib\gio\modules\*.dll" "$Out\gio-modules"
Copy-Item "$GstRoot\libexec\gstreamer-1.0\gst-plugin-scanner.exe" $Out

# Node + sidecar; jsdom reads data files beside its package, so it ships unbundled
Copy-Item (Get-Command node).Source "$Out\node.exe"
New-Item -ItemType Directory "$Out\sidecar" | Out-Null
Copy-Item "$Root\sidecar\dist\melo-sidecar.mjs", "$Root\sidecar\dist\melo-plugin-host.mjs" "$Out\sidecar"
$jsdom = (Get-Content "$Root\sidecar\node_modules\jsdom\package.json" | ConvertFrom-Json).version
& npm install --omit=dev --no-fund --no-audit --silent --prefix "$Out\sidecar" "jsdom@$jsdom"
if ($LASTEXITCODE) { throw "npm install jsdom failed" }
Remove-Item "$Out\sidecar\package.json", "$Out\sidecar\package-lock.json" -ErrorAction SilentlyContinue
$nm = "$Out\sidecar\node_modules"
Get-ChildItem $nm -Recurse -File -Include *.map, *.d.ts, *.d.mts, *.d.cts, *.md, CHANGELOG* | Remove-Item -Force
Get-ChildItem $nm -Recurse -Directory -Include test, tests, __tests__, .github, example, examples |
  Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
if (-not (Test-Path "$nm\jsdom")) { throw "jsdom missing" }

# melo's QML, the plugin engines' curated imports, fonts, presets
Robo "$Root\src\qml" "$Out\melo-qml" @('/XF', '*.frag.in', 'common.glsl', 'build-shaders.sh')
Robo "$Build\plugin-qml-imports" "$Out\plugin-qml-imports"
Robo "$Root\assets\fonts" "$Out\fonts"
Robo "$Root\vendor\presets" "$Out\presets"

# silence scan + licences
Copy-Item "$Root\vendor\ffmpeg\bin\ffmpeg.exe" $Out
Copy-Item "$Root\vendor\ffmpeg\COPYING.LGPLv2.1" "$Out\ffmpeg-COPYING.LGPLv2.1"
Copy-Item "$Root\LICENSE" $Out
Robo "$Root\LICENSES" "$Out\LICENSES"

# every DLL the shipped binaries import must be in dist\ or be a Windows
# system DLL; the loader otherwise fails with 0xC0000135 and no message
$have = @{}
Get-ChildItem $Out -File -Filter *.dll | ForEach-Object { $have[$_.Name.ToLower()] = $true }
$missing = @{}
$bins = @(Get-ChildItem $Out -File | Where-Object { $_.Extension -in '.exe', '.dll' }) +
        @(Get-ChildItem "$Out\gst-plugins", "$Out\gio-modules" -File -Filter *.dll)
foreach ($b in $bins) {
  $deps = & dumpbin /nologo /dependents $b.FullName | Where-Object { $_ -match '^\s+\S+\.dll\s*$' } |
    ForEach-Object { $_.Trim().ToLower() }
  foreach ($d in $deps) {
    if ($d -like 'api-ms-win-*' -or $d -like 'ext-ms-*' -or $have[$d]) { continue }
    if (Test-Path "$env:SystemRoot\System32\$d") { continue }
    $where = @(& where.exe $d 2>$null) + @(Get-ChildItem $GstRoot -Recurse -File -Filter $d -ErrorAction SilentlyContinue | ForEach-Object FullName)
    $global:LASTEXITCODE = 0
    $missing["$d (needed by $($b.Name); found at: $($where -join ', '))"] = $true
  }
}
if ($missing.Count) { throw "dist is missing DLLs:`n  $($missing.Keys -join "`n  ")" }

foreach ($f in 'melo.exe', 'node.exe', 'ffmpeg.exe', 'gst-plugin-scanner.exe', 'vcruntime140.dll',
               'sidecar\melo-sidecar.mjs', 'melo-qml\Main.qml',
               'plugin-qml-imports\QtQuick\qmldir', 'gst-plugins\gstwasapi2.dll') {
  if (-not (Test-Path "$Out\$f")) { throw "dist is missing $f" }
}
Write-Host "dist ready: $([math]::Round((Get-ChildItem $Out -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)) MB"
