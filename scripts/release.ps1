# Bump the version, build, refresh portable\, zip it, commit and push the bump,
# then publish a GitHub release with the zip. -DryRun builds and zips at the
# current version and stops before any edit, commit, push or upload.
param(
    [ValidateSet('tweak', 'patch', 'minor', 'major')]
    [string]$Bump = 'tweak',
    [string]$Notes = '',
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'

$root       = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildDir   = Join-Path $root 'build\x64-release-mingw'
$cmakeLists = Join-Path $root 'CMakeLists.txt'
$portable   = Join-Path $root 'portable'
$outDir     = 'C:\tmp\librecad-release'
$zipName    = 'LibreCAD-portable-win64.zip'
$mingwBin   = 'C:\Qt\Tools\mingw1310_64\bin'

# Windows PowerShell turns a native command's stderr (gh, git push progress) into a
# terminating error under 'Stop', so native calls run under 'Continue' and are judged
# by exit code alone.
function Invoke-Checked([string]$what, [scriptblock]$cmd) {
    $ErrorActionPreference = 'Continue'
    & $cmd
    if ($LASTEXITCODE -ne 0) { throw "$what failed ($LASTEXITCODE)" }
}

function Get-ExitCode([scriptblock]$cmd) {
    $ErrorActionPreference = 'Continue'
    & $cmd *> $null
    $LASTEXITCODE
}

# --- Preconditions ---
Push-Location $root
try {
    $status = @(git status --porcelain --branch)
    if ($LASTEXITCODE -ne 0) { throw 'git status failed' }
    if ((Get-ExitCode { gh auth status }) -ne 0) { throw 'gh is not logged in' }
    $repo = gh repo view --json nameWithOwner -q .nameWithOwner
    if ($LASTEXITCODE -ne 0 -or -not $repo) { throw 'gh repo view failed' }
} finally { Pop-Location }
if ($status[0] -notmatch '^## ([^.\s]+)') { throw "Cannot read branch from: $($status[0])" }
$branch = $Matches[1]
$dirty  = $status | Select-Object -Skip 1
if ($dirty) {
    if (-not $DryRun) { $dirty | Write-Host; throw 'Working tree has uncommitted changes' }
    Write-Host 'DryRun: working tree has uncommitted changes'
}
if ($status[0] -match '\[behind') { throw "Branch is behind its remote: $($status[0])" }

$running = Get-Process -Name librecad -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and $_.Path.StartsWith($root, [StringComparison]::OrdinalIgnoreCase) }
if ($running) { throw "Close LibreCAD first: $($running.Path -join ', ')" }

# --- Version ---
$cmakeText = [IO.File]::ReadAllText($cmakeLists)
$verRx = 'project\(LibreCAD VERSION (\d+)\.(\d+)\.(\d+)\.(\d+)\)'
$m = [regex]::Match($cmakeText, $verRx)
if (-not $m.Success) { throw "No 'project(LibreCAD VERSION a.b.c.d)' in $cmakeLists" }
$v = 1..4 | ForEach-Object { [int]$m.Groups[$_].Value }
$current = $v -join '.'
if (-not $DryRun) {
    switch ($Bump) {
        'major' { $v = @(($v[0] + 1), 0, 0, 0) }
        'minor' { $v = @($v[0], ($v[1] + 1), 0, 0) }
        'patch' { $v = @($v[0], $v[1], ($v[2] + 1), 0) }
        'tweak' { $v = @($v[0], $v[1], $v[2], ($v[3] + 1)) }
    }
}
$version = $v -join '.'
$tag     = "v$version"
$sm = [regex]::Match($cmakeText, 'set\(LC_VERSION "\$\{PROJECT_VERSION\}([^"]*)"\)')
if (-not $sm.Success) { throw "No 'set(LC_VERSION `"`${PROJECT_VERSION}...`")' in $cmakeLists" }
$lcVersion = $version + $sm.Groups[1].Value

if (-not $DryRun) {
    if ((Get-ExitCode { gh release view $tag -R $repo }) -eq 0) { throw "Release $tag already exists on $repo" }
}
Write-Host "Version $current -> $version ($lcVersion), tag $tag, branch $branch"

$bumped = $false
try {
    if (-not $DryRun) {
        $newText = [regex]::Replace($cmakeText, $verRx, "project(LibreCAD VERSION $version)")
        [IO.File]::WriteAllText($cmakeLists, $newText)
        $bumped = $true
    }

    # --- Build ---
    $cmake = (Select-String -Path (Join-Path $buildDir 'CMakeCache.txt') -Pattern '^CMAKE_COMMAND:INTERNAL=(.+)$').Matches[0].Groups[1].Value
    $env:PATH = "$mingwBin;" + $env:PATH
    # Dropbox briefly locks .d files mid-build; each pass resumes where ninja stopped,
    # and a real compile error fails every pass.
    for ($pass = 1; $pass -le 5; $pass++) {
        & { $ErrorActionPreference = 'Continue'; & $cmake --build $buildDir }
        if ($LASTEXITCODE -eq 0) { break }
        if ($pass -eq 5) { throw "Build failed ($LASTEXITCODE) after $pass passes" }
        Write-Host "Build pass $pass failed, retrying"
    }
    $exe = Join-Path $buildDir 'librecad.exe'
    $exeText = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($exe))
    if (-not $exeText.Contains($lcVersion)) { throw "$exe does not contain version string $lcVersion" }

    # --- Portable folder and zip ---
    & (Join-Path $PSScriptRoot 'make_portable.ps1') -BuildDir $buildDir

    # Per-user state the app writes beside the exe, and leftovers that are not part of the app.
    $skipTop  = '^(.*\.ini|.*\.log|.*\.alias|.*\.lcpp|.*\.lcws(\.bak)?|rcad\..*)$'
    $skipPath = @('resources/plugins/sample.dll', 'resources/doc/rcaddoc.qhcp',
                  'resources/doc/rcaddoc.qhp', 'resources/doc/img/rcadlogo.png')
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    $zip = Join-Path $outDir $zipName
    if (Test-Path $zip) { Remove-Item $zip }
    Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
    $archive = [IO.Compression.ZipFile]::Open($zip, 'Create')
    $count = 0
    try {
        foreach ($f in Get-ChildItem -Path $portable -Recurse -File) {
            $rel = $f.FullName.Substring($portable.Length + 1).Replace('\', '/')
            if (($rel -notmatch '/' -and $rel -match $skipTop) -or $skipPath -contains $rel) { continue }
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                $archive, $f.FullName, "LibreCAD/$rel", [IO.Compression.CompressionLevel]::Optimal)
            $count++
        }
    } finally { $archive.Dispose() }
    $hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
    Write-Host ("Zip: {0} ({1} files, {2:N1} MB, sha256 {3})" -f $zip, $count, ((Get-Item $zip).Length / 1MB), $hash)

    if ($DryRun) {
        Write-Host "DryRun: stopped before commit, push and release of $tag"
        return
    }

    # --- Commit, push, release ---
    Push-Location $root
    try {
        Invoke-Checked 'git add' { git add CMakeLists.txt }
        Invoke-Checked 'git commit' { git commit -m "Release $tag" }
        $bumped = $false
        Invoke-Checked 'git push' { git push origin $branch }
    } finally { Pop-Location }

    $body = "Portable Windows x64 build. Unzip anywhere and run LibreCAD\LibreCAD.exe; no install, no Qt or MinGW needed. Settings are stored in LibreCAD.ini beside the exe."
    if ($Notes) { $body += "`n`nChanges:`n$Notes" }
    Invoke-Checked 'gh release create' {
        gh release create $tag $zip -R $repo --target $branch --latest `
            --title "LibreCAD $version portable (Windows x64)" --notes $body
    }

    $digest = gh release view $tag -R $repo --json assets -q ".assets[] | select(.name == `"$zipName`") | .digest"
    if ($digest -ne "sha256:$hash") { throw "Uploaded digest '$digest' does not match local sha256:$hash" }
    Remove-Item -Recurse $outDir
    Write-Host "Released $tag : https://github.com/$repo/releases/tag/$tag"
} finally {
    # A failed build or zip leaves CMakeLists.txt at the old version, not half-released.
    if ($bumped) { [IO.File]::WriteAllText($cmakeLists, $cmakeText); Write-Host "Restored $cmakeLists to $current" }
}
