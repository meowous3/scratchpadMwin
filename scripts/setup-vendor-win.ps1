# Build patched projectM 4 for Windows into vendor/projectm4 (mirrors
# setup-vendor.sh). projectM's deps (glew, glm) come from vcpkg via the
# toolchain file. Run from CI with VCPKG_ROOT set.
param(
  [string]$Prefix = "$PSScriptRoot/../vendor/projectm4"
)
$ErrorActionPreference = "Stop"
$Root = Resolve-Path "$PSScriptRoot/.."
$Vendor = "$Root/vendor"
$Src = "$Vendor/projectm-src"
$PmVer = "v4.1.7"

if (Test-Path "$Prefix/lib/cmake/projectM4/projectM4Config.cmake") {
  Write-Host "projectM already built at $Prefix — skipping."
  exit 0
}

New-Item -ItemType Directory -Force -Path $Vendor | Out-Null
if (-not (Test-Path $Src)) {
  # LF checkout: the patch is applied with LF line endings, and a runner's
  # core.autocrlf=true would leave CRLF sources it cannot match
  git -c core.autocrlf=false clone --recursive --depth 1 --branch $PmVer `
    https://github.com/projectM-visualizer/projectm.git $Src
  if ($LASTEXITCODE) { throw "git clone failed" }
}

Push-Location $Src
# caller-bound FBO patch — required for QQuickFramebufferObject embedding.
# melo's own checkout may have CRLF line endings, so apply an LF copy.
$Patch = Join-Path ([IO.Path]::GetTempPath()) "projectm-caller-fbo.patch"
$text = [IO.File]::ReadAllText("$Root/patches/projectm-caller-fbo.patch") -replace "`r`n", "`n"
[IO.File]::WriteAllText($Patch, $text)
& git apply --check $Patch 2>$null
if ($LASTEXITCODE -eq 0) {
  & git apply $Patch
  if ($LASTEXITCODE) { throw "git apply failed" }
  Write-Host "applied projectm-caller-fbo.patch"
} else {
  Write-Host "patch did not apply cleanly — checking whether it is already in"
}
# same guard as setup-vendor.sh: an unpatched projectM renders black
if (-not (Select-String -Quiet -SimpleMatch callerDrawFbo src/libprojectM/ProjectM.cpp)) {
  throw "projectm-caller-fbo.patch is NOT in ProjectM.cpp"
}
Pop-Location

cmake -S $Src -B "$Src/build" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_INSTALL_PREFIX="$Prefix" `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DENABLE_PLAYLIST=ON -DBUILD_TESTING=OFF
if ($LASTEXITCODE) { throw "projectM configure failed" }
cmake --build "$Src/build" --config Release
if ($LASTEXITCODE) { throw "projectM build failed" }
cmake --install "$Src/build" --config Release
if ($LASTEXITCODE) { throw "projectM install failed" }
Write-Host "projectM $PmVer ready at $Prefix"
