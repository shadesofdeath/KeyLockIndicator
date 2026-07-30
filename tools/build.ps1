#requires -Version 5.1
<#
.SYNOPSIS
    KeyLock Indicator derleme betiği.

.DESCRIPTION
    Visual Studio kurulumunu vswhere ile bulur, vcvars64.bat ortamını bu
    oturuma aktarır ve VS ile gelen CMake + Ninja ikilileriyle derler.
    Ayrı bir CMake/Ninja PATH'te varsa onlar tercih edilir.

    Windows PowerShell 5.1 ile uyumludur: `&&`, ternary (`?:`) ve null-coalescing
    (`??`) operatörleri kullanılmaz.

.PARAMETER Config
    Release (varsayılan) | Debug | MinSizeRel

.PARAMETER Packaged
    -DKLI_PACKAGED=ON ile derler (MSIX; windows.startupTask yolu).

.PARAMETER Clean
    Yapılandırmadan önce build\<Config> dizinini siler.

.PARAMETER Package
    Derleme sonrası cpack ile portable ZIP üretir.

.EXAMPLE
    tools\build.ps1 -Config Release

.EXAMPLE
    tools\build.ps1 -Config Release -Packaged -Clean -Package
#>
[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug', 'MinSizeRel')]
    [string] $Config = 'Release',

    [switch] $Packaged,
    [switch] $Clean,
    [switch] $Package
)

$ErrorActionPreference = 'Stop'

# ---------------------------------------------------------------------------
# Yardımcılar
# ---------------------------------------------------------------------------

# Native araçların çıkış kodu kontrolü. $ErrorActionPreference exe hatalarını
# yakalamaz; bu yüzden her adımdan sonra açıkça çağrılır.
function Assert-ExitCode {
    param([Parameter(Mandatory = $true)][string] $What)

    if ($LASTEXITCODE -ne 0) {
        throw "$What başarısız oldu (çıkış kodu $LASTEXITCODE)."
    }
}

function Write-Step {
    param([Parameter(Mandatory = $true)][string] $Text)
    Write-Host ''
    Write-Host "==> $Text" -ForegroundColor Cyan
}

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere.exe bulunamadı ($vswhere). Visual Studio 2022 veya 2026 kurulu olmalı."
    }

    $query = @(
        '-latest'
        '-products', '*'
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64'
        '-property', 'installationPath'
    )

    # Önce Preview/Insider sürümleri de dâhil ara; eski vswhere -prerelease'i
    # tanımıyorsa sessizce ikinci denemeye düşülür.
    $withPrerelease = $query + '-prerelease'
    $path = & $vswhere @withPrerelease | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($path)) {
        $path = & $vswhere @query | Select-Object -First 1
    }
    if ([string]::IsNullOrWhiteSpace($path)) {
        throw 'C++ araç takımı (v143/v145) olan bir Visual Studio kurulumu bulunamadı.'
    }
    return $path.Trim()
}

# vcvars64.bat'ı cmd altında çalıştırıp `set` çıktısını bu prosesin ortamına
# yazar. Ortam değişkenleri Set-Item yerine .NET API ile atanır: adında
# parantez geçen değişkenler (ProgramFiles(x86)) provider yolunda sorun çıkarır.
function Import-VcVars {
    param([Parameter(Mandatory = $true)][string] $VsPath)

    $vcvars = Join-Path $VsPath 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path -LiteralPath $vcvars)) {
        throw "vcvars64.bat bulunamadı: $vcvars"
    }

    $lines = cmd.exe /c "call `"$vcvars`" >nul 2>&1 && set"
    Assert-ExitCode 'vcvars64.bat'

    foreach ($line in $lines) {
        $split = $line.IndexOf('=')
        # $split -gt 0: cmd'nin `=C:=...` biçimli sözde değişkenleri atlanır.
        if ($split -gt 0) {
            $name = $line.Substring(0, $split)
            $value = $line.Substring($split + 1)
            [System.Environment]::SetEnvironmentVariable($name, $value)
        }
    }
}

# PATH'teki araç varsa onu, yoksa VS'in bundled kopyasını döndürür.
function Resolve-Tool {
    param(
        [Parameter(Mandatory = $true)][string] $Name,
        [Parameter(Mandatory = $true)][string] $Fallback
    )

    $onPath = Get-Command $Name -CommandType Application -ErrorAction SilentlyContinue
    if ($null -ne $onPath) {
        return $onPath.Source
    }
    if (Test-Path -LiteralPath $Fallback) {
        return $Fallback
    }
    throw "$Name bulunamadı. PATH'te yok ve VS ile gelen kopya da eksik: $Fallback"
}

# ---------------------------------------------------------------------------
# Ana akış
# ---------------------------------------------------------------------------

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path (Join-Path $repoRoot 'build') $Config

Write-Host 'KeyLock Indicator — derleme' -ForegroundColor Green
Write-Host "  Kök       : $repoRoot"
Write-Host "  Yapılandırma: $Config"
Write-Host "  Paketli   : $($Packaged.IsPresent)"

Write-Step 'Visual Studio ortamı'
$vsPath = Find-VisualStudio
Write-Host "  VS        : $vsPath"
Import-VcVars -VsPath $vsPath
Write-Host "  MSVC      : $env:VCToolsVersion"
Write-Host "  SDK       : $env:WindowsSDKVersion"

Write-Step 'Araçlar'
$vsCMakeRoot = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$cmakeExe = Resolve-Tool -Name 'cmake.exe' -Fallback (Join-Path $vsCMakeRoot 'CMake\bin\cmake.exe')
$ninjaExe = Resolve-Tool -Name 'ninja.exe' -Fallback (Join-Path $vsCMakeRoot 'Ninja\ninja.exe')
Write-Host "  cmake     : $cmakeExe"
Write-Host "  ninja     : $ninjaExe"

if ($Clean) {
    if (Test-Path -LiteralPath $buildDir) {
        Write-Step "Temizlik: $buildDir"
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
}

# CMake yol argümanlarında ters bölü kaçış sorunu yaşanmaması için ileri bölü.
$ninjaForCMake = $ninjaExe.Replace('\', '/')
if ($Packaged) { $packagedValue = 'ON' } else { $packagedValue = 'OFF' }

Write-Step 'Yapılandırma'
& $cmakeExe `
    '-S' $repoRoot `
    '-B' $buildDir `
    '-G' 'Ninja' `
    "-DCMAKE_BUILD_TYPE=$Config" `
    "-DCMAKE_MAKE_PROGRAM=$ninjaForCMake" `
    "-DKLI_PACKAGED=$packagedValue"
Assert-ExitCode 'CMake yapılandırma'

Write-Step 'Derleme'
& $cmakeExe '--build' $buildDir
Assert-ExitCode 'CMake derleme'

if ($Package) {
    Write-Step 'Paketleme (cpack → ZIP)'
    $cpackExe = Join-Path (Split-Path -Parent $cmakeExe) 'cpack.exe'
    if (-not (Test-Path -LiteralPath $cpackExe)) {
        throw "cpack.exe bulunamadı: $cpackExe"
    }
    & $cpackExe `
        '--config' (Join-Path $buildDir 'CPackConfig.cmake') `
        '-G' 'ZIP' `
        '-C' $Config `
        '-B' $buildDir
    Assert-ExitCode 'cpack'

    $zip = Get-ChildItem -LiteralPath $buildDir -Filter 'KeyLockIndicator-*.zip' |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if ($null -ne $zip) {
        Write-Host "  ZIP       : $($zip.FullName)"
    }
}

# ---------------------------------------------------------------------------
# Sonuç — spec §0 hedefi: tek dosya .exe < 500 KB
# ---------------------------------------------------------------------------
$exePath = Join-Path $buildDir 'KeyLockIndicator.exe'
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Beklenen çıktı üretilmedi: $exePath"
}

$sizeKb = [math]::Round((Get-Item -LiteralPath $exePath).Length / 1KB, 1)

Write-Step 'Tamamlandı'
Write-Host "  Çıktı     : $exePath"
Write-Host "  Boyut     : $sizeKb KB"

if ($sizeKb -gt 500) {
    Write-Warning "Çıktı 500 KB hedefini aşıyor ($sizeKb KB). Spec §0: tek dosya .exe < 500 KB."
}
else {
    Write-Host '  Boyut hedefi (< 500 KB) sağlandı.' -ForegroundColor Green
}
