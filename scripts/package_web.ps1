param(
    [Parameter(Mandatory = $true)] [string] $SourceRoot,
    [Parameter(Mandatory = $true)] [string] $ProjectFile,
    [Parameter(Mandatory = $true)] [string] $OutputRoot
)

$ErrorActionPreference = "Stop"

$sourcePath = (Resolve-Path -LiteralPath $SourceRoot).Path
$emsdkCandidates = @()
if ($env:EMSDK) {
    $emsdkCandidates += $env:EMSDK
}
$emsdkCandidates += (Join-Path (Split-Path -Parent $sourcePath) "emsdk")

if (-not (Get-Command emcmake -ErrorAction SilentlyContinue)) {
    foreach ($emsdkCandidate in ($emsdkCandidates | Select-Object -Unique)) {
        $emsdkEnvironment = Join-Path $emsdkCandidate "emsdk_env.ps1"
        if (Test-Path -LiteralPath $emsdkEnvironment) {
            & $emsdkEnvironment | Out-Null
            break
        }
    }
}

if (-not (Get-Command emcmake -ErrorAction SilentlyContinue)) {
    throw "Emscripten was not found. Install emsdk or activate it before packaging for web."
}

$vcpkgCommand = Get-Command vcpkg -ErrorAction SilentlyContinue
$vcpkgRoot = $env:VCPKG_ROOT
if (-not $vcpkgRoot -and $vcpkgCommand) {
    $vcpkgRoot = Split-Path -Parent $vcpkgCommand.Source
}
if (-not $vcpkgRoot) {
    $vcpkgRoot = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\vcpkg"
}
$vcpkgToolchain = Join-Path $vcpkgRoot "scripts/buildsystems/vcpkg.cmake"
if (-not (Test-Path -LiteralPath $vcpkgToolchain)) {
    throw "vcpkg was not found. Expected its toolchain at: $vcpkgToolchain"
}

$ninjaCommand = Get-Command ninja -ErrorAction SilentlyContinue
if (-not $ninjaCommand) {
    $visualStudioRoot = Join-Path ${env:ProgramFiles} "Microsoft Visual Studio"
    if (Test-Path -LiteralPath $visualStudioRoot) {
        $ninjaCommand = Get-ChildItem -LiteralPath $visualStudioRoot -Filter "ninja.exe" -File -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1
    }
}
if (-not $ninjaCommand) {
    throw "Ninja was not found. Install the Visual Studio CMake tools or Ninja, then try again."
}
$ninjaPath = if ($ninjaCommand -is [System.IO.FileInfo]) { $ninjaCommand.FullName } else { $ninjaCommand.Source }
$ninjaDirectory = Split-Path -Parent $ninjaPath
$env:PATH = "$ninjaDirectory;$env:PATH"

$projectPath = (Resolve-Path -LiteralPath $ProjectFile).Path
$outputPath = [System.IO.Path]::GetFullPath($OutputRoot)
$assetPath = Join-Path (Split-Path -Parent $projectPath) "assets"
$buildPath = Join-Path $sourcePath "out/build/web"

if (-not (Test-Path -LiteralPath $assetPath -PathType Container)) {
    throw "The project assets folder was not found: $assetPath"
}

if (Test-Path -LiteralPath $buildPath) {
    Remove-Item -LiteralPath $buildPath -Recurse -Force
}

if (Test-Path -LiteralPath $outputPath) {
    Get-ChildItem -LiteralPath $outputPath -Force | Remove-Item -Recurse -Force
}
else {
    New-Item -ItemType Directory -Force -Path $outputPath | Out-Null
}

$cmakeArguments = @(
    "-S", $sourcePath,
    "-B", $buildPath,
    "-G", "Ninja",
    "-DCMAKE_BUILD_TYPE=Release",
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgToolchain",
    "-DVCPKG_TARGET_TRIPLET=wasm32-emscripten",
    "-DVCPKG_HOST_TRIPLET=x64-windows",
    "-DVCPKG_MANIFEST_INSTALL=OFF",
    "-DAQUANACT_WEB_ASSET_ROOT=$assetPath",
    "-DAQUANACT_WEB_PROJECT_FILE=$projectPath"
)
& emcmake cmake @cmakeArguments
if ($LASTEXITCODE -ne 0) { throw "Emscripten CMake configuration failed." }

& cmake --build $buildPath --target AquanactGame --config Release
if ($LASTEXITCODE -ne 0) { throw "Emscripten web build failed." }

$generatedFiles = Get-ChildItem -LiteralPath $buildPath -File -Recurse | Where-Object {
    $_.Extension -in @(".html", ".js", ".wasm", ".data") -and $_.BaseName -like "AquanactGame*"
}
if ($generatedFiles.Count -eq 0) {
    throw "The web build completed without producing AquanactGame web files."
}

foreach ($file in $generatedFiles) {
    $destinationName = if ($file.Extension -eq ".html") { "index.html" } else { $file.Name }
    Copy-Item -LiteralPath $file.FullName -Destination (Join-Path $outputPath $destinationName) -Force
}

if (-not (Test-Path -LiteralPath (Join-Path $outputPath "index.html"))) {
    throw "The web package is missing its required index.html entry point."
}

$zipPath = Join-Path (Split-Path -Parent $outputPath) "aquanact-web.zip"
Compress-Archive -Path (Join-Path $outputPath "*") -DestinationPath $zipPath -Force

Write-Output "Web package written to $outputPath"
Write-Output "itch.io ZIP written to $zipPath"
