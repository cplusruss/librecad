# No-wipe refresh of portable\ from a build tree, then a dependency audit that
# fails if any shipped PE file imports a DLL that is neither in portable\ nor
# part of every Windows install.
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build\x64-release-mingw')
)
$ErrorActionPreference = 'Stop'

$root     = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$BuildDir = (Resolve-Path $BuildDir).Path
$portable = Join-Path $root 'portable'
$qtPrefix = 'C:\Qt\6.9.0\mingw_64'
$mingwBin = 'C:\Qt\Tools\mingw1310_64\bin'
$objdump  = Join-Path $mingwBin 'objdump.exe'

$exeSrc = Join-Path $BuildDir 'librecad.exe'
if (-not (Test-Path $exeSrc)) { throw "Missing $exeSrc" }
$exe = Join-Path $portable 'LibreCAD.exe'
New-Item -ItemType Directory -Force -Path $portable | Out-Null
Copy-Item $exeSrc $exe -Force

Set-Content -Path (Join-Path $portable 'qt.conf') -Encoding ASCII -Value @"
[Paths]
Prefix = .
Plugins = plugins
Translations = translations
"@

$qtPlugins = Join-Path $portable 'plugins'
$env:PATH  = "$qtPrefix\bin;$mingwBin;" + $env:PATH
$windeployqt = Join-Path $qtPrefix 'bin\windeployqt6.exe'

# opengl32sw.dll and d3dcompiler_47.dll are kept so machines without a GPU
# driver (VMs, RDP sessions, bare installs) can still render.
& $windeployqt --release --no-translations --compiler-runtime `
    --plugindir $qtPlugins --dir $portable $exe
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed ($LASTEXITCODE)" }

# The SVG icon engine is a Qt iconengines plugin: it must sit beside qsvgicon.dll.
$iconEngine = Join-Path $BuildDir 'libraries\lciconengine\lc_svgiconengine.dll'
if (-not (Test-Path $iconEngine)) { throw "Missing $iconEngine" }
Copy-Item $iconEngine (Join-Path $qtPlugins 'iconengines') -Force

# LC_PluginInvoker scans <appDir>/resources/plugins non-recursively.
$lcPlugins = Get-ChildItem -Path (Join-Path $BuildDir 'plugins') -Recurse -File -Filter '*.dll'
if (-not $lcPlugins) { throw "No LibreCAD plugin DLLs under $BuildDir\plugins" }
$lcPluginDir = Join-Path $portable 'resources\plugins'
New-Item -ItemType Directory -Force -Path $lcPluginDir | Out-Null
$lcPlugins | ForEach-Object { Copy-Item $_.FullName $lcPluginDir -Force }

foreach ($sub in 'fonts', 'patterns', 'library', 'doc') {
    $dst = Join-Path $portable "resources\$sub"
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Copy-Item (Join-Path $root "librecad\support\$sub\*") $dst -Recurse -Force
}

# --- Dependency audit ---
$shipped = @{}
Get-ChildItem -Path $portable -Recurse -File -Include '*.dll' |
    ForEach-Object { $shipped[$_.Name.ToLower()] = $true }
$sys32 = Join-Path $env:WINDIR 'System32'
$missing = @()
$pe = Get-ChildItem -Path $portable -Recurse -File -Include '*.exe', '*.dll'
foreach ($f in $pe) {
    $imports = & $objdump -p $f.FullName | Select-String 'DLL Name:\s*(\S+)' |
               ForEach-Object { $_.Matches[0].Groups[1].Value.ToLower() }
    foreach ($d in $imports) {
        if ($shipped.ContainsKey($d)) { continue }
        # api-ms-win-*/ext-ms-* are API-set contracts resolved by the loader.
        if ($d -like 'api-ms-win-*' -or $d -like 'ext-ms-*') { continue }
        # Redistributables can sit in this machine's System32 yet be absent elsewhere.
        $redist = $d -match '^(vcruntime|msvcp|concrt|vcomp|libssl|libcrypto|libgcc|libstdc|libwinpthread|qt6)'
        if (-not $redist -and (Test-Path (Join-Path $sys32 $d))) { continue }
        $missing += "$($f.FullName.Substring($portable.Length + 1)) -> $d"
    }
}
if ($missing) {
    $missing | ForEach-Object { Write-Host "MISSING $_" }
    throw "$($missing.Count) unresolved imports"
}
Write-Host ("Audit: {0} PE files, all imports resolved" -f $pe.Count)

$size = (Get-ChildItem -Recurse -File -Path $portable | Measure-Object -Property Length -Sum).Sum
Write-Host ("Portable: {0} ({1:N1} MB)" -f $portable, ($size / 1MB))
