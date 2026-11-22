param(
    [Parameter(Mandatory = $true)]
    [string]$BuildDir,
    [Parameter(Mandatory = $true)]
    [string]$WindeployQt,
    [Parameter(Mandatory = $true)]
    [string]$QtLicense
)

$ErrorActionPreference = 'Stop'

$resolvedBuild = (Resolve-Path -LiteralPath $BuildDir).Path
$executable = Join-Path $resolvedBuild 'diffpick.exe'
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "diffpick.exe was not found at $executable"
}
if (-not (Test-Path -LiteralPath $WindeployQt -PathType Leaf)) {
    throw "windeployqt was not found at $WindeployQt"
}
if (-not (Test-Path -LiteralPath $QtLicense -PathType Leaf)) {
    throw "The Qt LGPL license was not found at $QtLicense"
}

$packageRoot = [IO.Path]::GetFullPath((Join-Path $resolvedBuild 'package'))
$stageDir = [IO.Path]::GetFullPath((Join-Path $packageRoot 'diffpick'))
$zipPath = [IO.Path]::GetFullPath((Join-Path $packageRoot 'diffpick-windows-x64.zip'))
$expectedPrefix = $packageRoot.TrimEnd([IO.Path]::DirectorySeparatorChar) + [IO.Path]::DirectorySeparatorChar
if (-not $stageDir.StartsWith($expectedPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to stage outside the build/package directory.'
}

New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null
if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir | Out-Null

$stagedExecutable = Join-Path $stageDir 'diffpick.exe'
Copy-Item -LiteralPath $executable -Destination $stagedExecutable

& $WindeployQt --release --no-translations --compiler-runtime `
    --skip-plugin-types generic,imageformats,networkinformation,tls `
    --no-system-dxc-compiler $stagedExecutable
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

# The MSYS2 build of windeployqt does not copy every MinGW and third-party DLL.
# Resolve non-system imports recursively from the same toolchain bin directory.
$toolchainBin = Split-Path -Parent $WindeployQt
$objdump = Join-Path $toolchainBin 'objdump.exe'
$dumpbin = Get-Command dumpbin.exe -ErrorAction SilentlyContinue
if (-not (Test-Path -LiteralPath $objdump -PathType Leaf) -and -not $dumpbin) {
    throw 'Neither objdump nor dumpbin was found for dependency verification.'
}

function Get-ImportedDlls([string]$Binary) {
    if (Test-Path -LiteralPath $objdump -PathType Leaf) {
        $imports = & $objdump -p $Binary 2>$null
        if ($LASTEXITCODE -ne 0) {
            throw "objdump could not inspect $Binary (exit code $LASTEXITCODE)."
        }
        foreach ($line in $imports) {
            if ($line -match 'DLL Name:\s+(.+)$') {
                $Matches[1].Trim()
            }
        }
        return
    }

    $imports = & $dumpbin.Source /nologo /dependents $Binary 2>$null
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin could not inspect $Binary (exit code $LASTEXITCODE)."
    }
    foreach ($line in $imports) {
        if ($line -match '^\s+([^\s]+\.dll)\s*$') {
            $Matches[1]
        }
    }
}

$processed = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
do {
    $addedDependency = $false
    $binaries = Get-ChildItem -LiteralPath $stageDir -Recurse -File |
        Where-Object { $_.Extension -in '.exe', '.dll' }
    foreach ($binary in $binaries) {
        if (-not $processed.Add($binary.FullName)) {
            continue
        }
        foreach ($dependency in (Get-ImportedDlls $binary.FullName)) {
            $destination = Join-Path $stageDir $dependency
            if (Test-Path -LiteralPath $destination -PathType Leaf) {
                continue
            }
            $source = Join-Path $toolchainBin $dependency
            if (Test-Path -LiteralPath $source -PathType Leaf) {
                Copy-Item -LiteralPath $source -Destination $destination
                $addedDependency = $true
            }
        }
    }
} while ($addedDependency)

# Fail packaging if a dependency is neither bundled nor supplied by Windows.
$systemDir = [Environment]::SystemDirectory
$unresolved = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$binaries = Get-ChildItem -LiteralPath $stageDir -Recurse -File |
    Where-Object { $_.Extension -in '.exe', '.dll' }
foreach ($binary in $binaries) {
    foreach ($dependency in (Get-ImportedDlls $binary.FullName)) {
        if ($dependency -match '^(api-ms-|ext-ms-)') {
            continue
        }
        if (-not (Test-Path -LiteralPath (Join-Path $stageDir $dependency) -PathType Leaf) -and
            -not (Test-Path -LiteralPath (Join-Path $systemDir $dependency) -PathType Leaf)) {
            [void]$unresolved.Add($dependency)
        }
    }
}
if ($unresolved.Count -gt 0) {
    throw "Unresolved runtime dependencies: $([string]::Join(', ', $unresolved))"
}

$sourceRoot = Split-Path -Parent $PSScriptRoot
Copy-Item -LiteralPath (Join-Path $sourceRoot 'LICENSE') -Destination $stageDir
Copy-Item -LiteralPath (Join-Path $sourceRoot 'README.md') -Destination $stageDir
Copy-Item -LiteralPath $QtLicense -Destination (Join-Path $stageDir 'QT-LICENSE-LGPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/QT-SOURCES.txt') -Destination $stageDir
Copy-Item -LiteralPath (Join-Path $sourceRoot 'docs/THIRD-PARTY.txt') -Destination $stageDir

$stagedLicenses = Join-Path $stageDir 'licenses'
New-Item -ItemType Directory -Path $stagedLicenses | Out-Null
$qtLicenseDir = Split-Path -Parent $QtLicense
if ((Split-Path -Leaf $qtLicenseDir) -eq 'qt6-base') {
    $licensesRoot = Split-Path -Parent $qtLicenseDir
    foreach ($package in @('qt6-base', 'gcc', 'libgcc', 'double-conversion', 'freetype',
                            'glib2', 'harfbuzz', 'libb2', 'libpng', 'md4c', 'pcre2',
                            'zlib', 'zstd', 'brotli', 'bzip2', 'gettext-runtime',
                            'graphite2', 'libiconv')) {
        $packageLicenses = Join-Path $licensesRoot $package
        if (Test-Path -LiteralPath $packageLicenses -PathType Container) {
            Copy-Item -LiteralPath $packageLicenses -Destination $stagedLicenses -Recurse
        }
    }
} else {
    Copy-Item -LiteralPath $qtLicenseDir -Destination $stagedLicenses -Recurse
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $stageDir '*') -DestinationPath $zipPath -CompressionLevel Optimal
Write-Output "Created $zipPath"
