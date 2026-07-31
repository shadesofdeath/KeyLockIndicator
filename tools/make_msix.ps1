#requires -Version 5.1
<#
.SYNOPSIS
    KeyLock Indicator MSIX paketleyicisi (Microsoft Store).

.DESCRIPTION
    Paketli ikiliyi derler, hazırlık dizinini kurar, resources.pri üretir ve
    makeappx ile .msix paketini oluşturur. Üç kullanım biçimi vardır:

      1) Geliştirme döngüsü   -Register
         Hazırlık dizinini "loose file" olarak kaydeder. Sertifika, imza ve
         yönetici hakkı GEREKMEZ; yalnızca Geliştirici Modu açık olmalıdır.
         Paketli kod yolunu (windows.startupTask) denemenin en hızlı yolu budur.

      2) Yerel dağıtım        -Sign
         Kendinden imzalı sertifikayla imzalar. Kuran makinenin sertifikaya
         güvenmesi gerekir (Güvenilen Kişiler deposu, yönetici hakkı ister) —
         bu yol yalnızca kendi makinende veya test ekibinde işe yarar.

      3) Store gönderimi      -Store
         İMZASIZ .msix üretir ve kimlik alanlarını Partner Center değerleriyle
         yazar. Store paketi kendi sertifikasıyla yeniden imzalar; imzalı paket
         yüklersen "publisher uyuşmuyor" hatası alırsın.

    Windows PowerShell 5.1 ile uyumludur: `&&`, ternary (`?:`) ve
    null-coalescing (`??`) operatörleri kullanılmaz.

.PARAMETER Config
    Release (varsayılan) | MinSizeRel | Debug. Store gönderiminde Debug
    REDDEDİLİR: sertifikasyon hata ayıklama yapılandırmasını eler.

.PARAMETER Arch
    x64 (varsayılan) | arm64. arm64 için VS'in "MSVC ARM64 build tools"
    bileşeni kurulu olmalıdır.

.PARAMETER Version
    Dört parçalı paket sürümü. Verilmezse CMakeLists.txt içindeki
    project(VERSION ...) değerine ".0" eklenerek üretilir. Store SON parçanın
    0 olmasını şart koşar; onu kendisi kullanır.

.PARAMETER IdentityName
    Identity/@Name. Store gönderiminde Partner Center > Ürün kimliği >
    "Paket/Kimlik/Adı" değeri (ör. 12345ShadesOfDeath.KeyLockIndicator).

.PARAMETER Publisher
    Identity/@Publisher. Store gönderiminde Partner Center'daki
    "Paket/Kimlik/Yayımcı" değeri (CN=... biçiminde, birebir).

.PARAMETER PublisherDisplayName
    Properties/PublisherDisplayName. Store'da görünen yayımcı adı.

.PARAMETER Register
    Paketi üretmek yerine hazırlık dizinini geliştirme kaydıyla kurar.

.PARAMETER Sign
    Paketi kendinden imzalı sertifikayla imzalar (yoksa üretir).

.PARAMETER Store
    Store'a yüklenecek imzasız paketi üretir. -Sign ile birlikte kullanılamaz.

.PARAMETER Wack
    Paketi ürettikten sonra Windows App Certification Kit'i çalıştırır.

.PARAMETER SkipBuild
    Derlemeyi atlar, mevcut build çıktısını kullanır.

.EXAMPLE
    tools\make_msix.ps1 -Register
    Geliştirici Modunda hızlı deneme; imza gerekmez.

.EXAMPLE
    tools\make_msix.ps1 -Sign -Wack
    İmzalı yerel paket ve sertifikasyon denetimi.

.EXAMPLE
    tools\make_msix.ps1 -Store -IdentityName '12345ShadesOfDeath.KeyLockIndicator' -Publisher 'CN=A1B2C3D4-...'
    Partner Center'a yüklenecek imzasız paket.
#>
[CmdletBinding(DefaultParameterSetName = 'Pack')]
param(
    [ValidateSet('Release', 'MinSizeRel', 'Debug')]
    [string] $Config = 'Release',

    [ValidateSet('x64', 'arm64')]
    [string] $Arch = 'x64',

    [ValidatePattern('^\d+\.\d+\.\d+\.\d+$')]
    [string] $Version,

    [string] $IdentityName,
    [string] $Publisher,
    [string] $PublisherDisplayName,

    [Parameter(ParameterSetName = 'Register')]
    [switch] $Register,

    [Parameter(ParameterSetName = 'Pack')]
    [switch] $Sign,

    [Parameter(ParameterSetName = 'Pack')]
    [switch] $Store,

    [switch] $Wack,
    [switch] $SkipBuild
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

# Windows SDK'nın en yeni sürümündeki x64 araç dizinini bulur. Araçlar her
# zaman x64 konağında çalışır; hedef mimari (-Arch) paketin içeriğini belirler,
# aracın kendisini değil.
function Get-SdkToolDir {
    $roots = @(
        (Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'),
        (Join-Path $env:ProgramFiles 'Windows Kits\10\bin')
    )

    $best = $null
    foreach ($root in $roots) {
        if (-not (Test-Path -LiteralPath $root)) { continue }

        $candidates = Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -match '^10\.\d+\.\d+\.\d+$' } |
            Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'x64\makeappx.exe') }

        foreach ($c in $candidates) {
            $ver = [version]$c.Name
            if ($null -eq $best -or $ver -gt $best.Version) {
                $best = [PSCustomObject]@{ Version = $ver; Path = (Join-Path $c.FullName 'x64') }
            }
        }
    }

    if ($null -eq $best) {
        throw 'Windows SDK araçları (makeappx.exe) bulunamadı. Visual Studio Installer''dan "Windows 10/11 SDK" bileşenini kurun.'
    }

    Write-Host "  SDK       : $($best.Version)"
    return $best.Path
}

# CMakeLists.txt tek sürüm kaynağıdır; paket sürümünü orada tutulan değerden
# türetmek, manifest ile ikilinin sürümünün ayrışmasını imkânsız kılar.
function Get-ProjectVersion {
    param([Parameter(Mandatory = $true)][string] $RepoRoot)

    $text = Get-Content -LiteralPath (Join-Path $RepoRoot 'CMakeLists.txt') -Raw
    $m = [regex]::Match($text, 'project\s*\([^)]*?VERSION\s+(\d+)\.(\d+)\.(\d+)', 'Singleline')
    if (-not $m.Success) {
        throw 'CMakeLists.txt içinde project(... VERSION x.y.z ...) bulunamadı.'
    }
    # Dördüncü parça DAİMA 0: Store revizyon alanını kendisi kullanır ve
    # sıfırdan farklı bir değerle gönderim reddedilir.
    return '{0}.{1}.{2}.0' -f $m.Groups[1].Value, $m.Groups[2].Value, $m.Groups[3].Value
}

# Manifest, XML olarak yüklenip kimlik alanları yazılır. Metin değiştirme
# (-replace) yerine XML DOM kullanılır: öznitelik sırası ya da boşluk değişirse
# düz metin kalıbı sessizce ıskalar, DOM ıskalamaz.
function Write-StagedManifest {
    param(
        [Parameter(Mandatory = $true)][string] $SourcePath,
        [Parameter(Mandatory = $true)][string] $DestPath,
        [Parameter(Mandatory = $true)][string] $PackageVersion,
        [Parameter(Mandatory = $true)][string] $Architecture,
        [string] $Name,
        [string] $PublisherId,
        [string] $PublisherDisplay
    )

    $xml = New-Object System.Xml.XmlDocument
    $xml.PreserveWhitespace = $true
    $xml.Load($SourcePath)

    $identity = $xml.Package.Identity
    $identity.SetAttribute('Version', $PackageVersion)
    $identity.SetAttribute('ProcessorArchitecture', $Architecture)
    if (-not [string]::IsNullOrWhiteSpace($Name)) {
        $identity.SetAttribute('Name', $Name)
    }
    if (-not [string]::IsNullOrWhiteSpace($PublisherId)) {
        $identity.SetAttribute('Publisher', $PublisherId)
    }
    if (-not [string]::IsNullOrWhiteSpace($PublisherDisplay)) {
        $xml.Package.Properties.PublisherDisplayName = $PublisherDisplay
    }

    $xml.Save($DestPath)

    Write-Host "  Kimlik    : $($identity.GetAttribute('Name'))"
    Write-Host "  Yayımcı   : $($identity.GetAttribute('Publisher'))"
    Write-Host "  Sürüm     : $($identity.GetAttribute('Version')) ($Architecture)"
}

# Kendinden imzalı kod imzalama sertifikası. Subject, manifestteki Publisher ile
# BİREBİR aynı olmalıdır; farklıysa signtool imzalar ama Windows paketi
# "publisher uyuşmuyor" diye reddeder.
function Get-TestCertificate {
    param(
        [Parameter(Mandatory = $true)][string] $Subject,
        [Parameter(Mandatory = $true)][string] $PfxPath,
        [Parameter(Mandatory = $true)][string] $Password
    )

    if (Test-Path -LiteralPath $PfxPath) {
        Write-Host "  Sertifika : mevcut ($PfxPath)"
        return
    }

    Write-Host "  Sertifika : üretiliyor ($Subject)"
    $cert = New-SelfSignedCertificate `
        -Type Custom `
        -Subject $Subject `
        -KeyUsage DigitalSignature `
        -KeyAlgorithm RSA `
        -KeyLength 2048 `
        -CertStoreLocation 'Cert:\CurrentUser\My' `
        -FriendlyName 'KeyLock Indicator MSIX test' `
        -NotAfter (Get-Date).AddYears(3) `
        -TextExtension @(
            '2.5.29.37={text}1.3.6.1.5.5.7.3.3',   # Extended Key Usage: kod imzalama
            '2.5.29.19={text}'                     # Basic Constraints: CA değil
        )

    $secure = ConvertTo-SecureString -String $Password -Force -AsPlainText
    Export-PfxCertificate -Cert $cert -FilePath $PfxPath -Password $secure | Out-Null
    Write-Host "  PFX       : $PfxPath"
}

# ---------------------------------------------------------------------------
# Hazırlık
# ---------------------------------------------------------------------------

if ($Store -and $Config -eq 'Debug') {
    throw 'Store gönderimi Debug yapılandırmasını reddeder; -Config Release kullanın.'
}

$repoRoot   = Split-Path -Parent $PSScriptRoot
$sourceMan  = Join-Path $repoRoot 'packaging\AppxManifest.xml'
$assetDir   = Join-Path $repoRoot 'packaging\Assets'
$buildDir   = Join-Path (Join-Path $repoRoot 'build') "$Config-msix"
$outRoot    = Join-Path (Join-Path $repoRoot 'build') 'msix'
$stageDir   = Join-Path $outRoot "stage-$Arch"
$exeName    = 'KeyLockIndicator.exe'

foreach ($required in @($sourceMan, $assetDir)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Paketleme girdisi eksik: $required"
    }
}

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = Get-ProjectVersion -RepoRoot $repoRoot
}

Write-Host 'KeyLock Indicator — MSIX paketleme' -ForegroundColor Green
Write-Host "  Kök       : $repoRoot"
Write-Host "  Yapılandırma: $Config"
Write-Host "  Mimari    : $Arch"
if ($Register) { $mode = 'Register (geliştirme kaydı)' }
elseif ($Store) { $mode = 'Store (imzasız)' }
elseif ($Sign)  { $mode = 'İmzalı paket' }
else            { $mode = 'İmzasız paket' }
Write-Host "  Kip       : $mode"

$sdkDir = Get-SdkToolDir
$makeappx = Join-Path $sdkDir 'makeappx.exe'
$makepri  = Join-Path $sdkDir 'makepri.exe'
$signtool = Join-Path $sdkDir 'signtool.exe'

# ---------------------------------------------------------------------------
# 1. Derleme
# ---------------------------------------------------------------------------

if (-not $SkipBuild) {
    Write-Step 'Derleme (KLI_PACKAGED=ON)'
    if ($Arch -ne 'x64') {
        throw "tools\build.ps1 şu an yalnızca x64 üretir; -Arch $Arch için ARM64 araç takımı ve vcvarsamd64_arm64 desteği eklenmelidir."
    }
    & (Join-Path $PSScriptRoot 'build.ps1') -Config $Config -Packaged
    if ($LASTEXITCODE -ne 0 -and $null -ne $LASTEXITCODE) {
        throw 'Derleme başarısız oldu.'
    }
}

$exePath = Join-Path $buildDir $exeName
if (-not (Test-Path -LiteralPath $exePath)) {
    throw "Paketli ikili bulunamadı: $exePath (önce -SkipBuild olmadan çalıştırın)"
}

# ---------------------------------------------------------------------------
# 2. Hazırlık dizini
# ---------------------------------------------------------------------------
# Paket İÇERİĞİ yalnızca buradan gelir: taşınabilir ZIP'e giren README ya da
# .ini paketin içinde işe yaramaz — MSIX kurulum dizini salt okunurdur, oraya
# konan bir .ini taşınabilir kipi tetikleyip ayarların kaydedilmesini engeller.

Write-Step 'Hazırlık dizini'
if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Copy-Item -LiteralPath $exePath -Destination (Join-Path $stageDir $exeName)
Copy-Item -LiteralPath $assetDir -Destination (Join-Path $stageDir 'Assets') -Recurse

Write-StagedManifest `
    -SourcePath $sourceMan `
    -DestPath (Join-Path $stageDir 'AppxManifest.xml') `
    -PackageVersion $Version `
    -Architecture $Arch `
    -Name $IdentityName `
    -PublisherId $Publisher `
    -PublisherDisplay $PublisherDisplayName

$assetCount = @(Get-ChildItem -LiteralPath (Join-Path $stageDir 'Assets') -File).Count
Write-Host "  Görsel    : $assetCount dosya"

# ---------------------------------------------------------------------------
# 3. resources.pri
# ---------------------------------------------------------------------------
# Ölçek ve hedef-boyut çeşitlerini (".scale-200", ".targetsize-32") kabuğa
# bağlayan indeks budur. PRI olmadan paket yine kurulur ama Windows yalnızca
# manifestte adı geçen ölçeksiz dosyayı kullanır ve çeşitler ölü ağırlık olur.

Write-Step 'Kaynak indeksi (resources.pri)'
$priConfig = Join-Path $outRoot "priconfig-$Arch.xml"
& $makepri createconfig /ConfigXml $priConfig /Default en-US /Overwrite | Out-Null
Assert-ExitCode 'makepri createconfig'

# İndeks adı manifestten okunur (/Manifest), böylece PRI'nin kök adı paket
# kimliğiyle eşleşir; eşleşmezse kabuk kaynakları çözemez.
& $makepri new `
    /ProjectRoot $stageDir `
    /ConfigXml $priConfig `
    /OutputFile (Join-Path $stageDir 'resources.pri') `
    /Manifest (Join-Path $stageDir 'AppxManifest.xml') `
    /Overwrite | Out-Null
Assert-ExitCode 'makepri new'

$priSize = [math]::Round((Get-Item -LiteralPath (Join-Path $stageDir 'resources.pri')).Length / 1KB, 1)
Write-Host "  PRI       : $priSize KB"

# ---------------------------------------------------------------------------
# 4a. Geliştirme kaydı
# ---------------------------------------------------------------------------

if ($Register) {
    Write-Step 'Geliştirme kaydı'
    $devMode = $null
    $unlockKey = 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\AppModelUnlock'
    if (Test-Path -LiteralPath $unlockKey) {
        $devMode = (Get-ItemProperty -LiteralPath $unlockKey).AllowDevelopmentWithoutDevLicense
    }
    if ($devMode -ne 1) {
        throw 'Geliştirici Modu kapalı. Ayarlar > Gizlilik ve güvenlik > Geliştiriciler için > Geliştirici Modu açın.'
    }

    Add-AppxPackage -Register (Join-Path $stageDir 'AppxManifest.xml')

    $pkg = Get-AppxPackage -Name (([xml](Get-Content -LiteralPath (Join-Path $stageDir 'AppxManifest.xml') -Raw)).Package.Identity.Name)
    Write-Step 'Tamamlandı'
    Write-Host "  Paket     : $($pkg.PackageFullName)"
    Write-Host "  Konum     : $($pkg.InstallLocation)"
    Write-Host ''
    Write-Host '  Kaldırmak için:' -ForegroundColor DarkGray
    Write-Host "    Get-AppxPackage -Name $($pkg.Name) | Remove-AppxPackage" -ForegroundColor DarkGray
    return
}

# ---------------------------------------------------------------------------
# 4b. Paketleme
# ---------------------------------------------------------------------------

Write-Step 'Paketleme (makeappx)'
$msixName = "KeyLockIndicator-$Version-$Arch.msix"
$msixPath = Join-Path $outRoot $msixName

& $makeappx pack /d $stageDir /p $msixPath /o | Out-Null
Assert-ExitCode 'makeappx pack'

$msixSize = [math]::Round((Get-Item -LiteralPath $msixPath).Length / 1KB, 1)
Write-Host "  Paket     : $msixPath"
Write-Host "  Boyut     : $msixSize KB"

# ---------------------------------------------------------------------------
# 5. İmzalama
# ---------------------------------------------------------------------------

if ($Sign) {
    Write-Step 'İmzalama'
    $manifestPublisher = ([xml](Get-Content -LiteralPath (Join-Path $stageDir 'AppxManifest.xml') -Raw)).Package.Identity.Publisher
    $pfxPath  = Join-Path $outRoot 'KeyLockIndicator-test.pfx'
    $password = 'keylock-test'

    Get-TestCertificate -Subject $manifestPublisher -PfxPath $pfxPath -Password $password

    & $signtool sign /fd SHA256 /a /f $pfxPath /p $password $msixPath | Out-Null
    Assert-ExitCode 'signtool sign'
    Write-Host '  İmza      : SHA256, kendinden imzalı'
    Write-Host ''
    Write-Host '  Bu paketi kurabilmek için sertifikanın güvenilmesi gerekir' -ForegroundColor DarkGray
    Write-Host '  (yönetici olarak, bir kez):' -ForegroundColor DarkGray
    Write-Host "    Import-Certificate -FilePath '$($pfxPath -replace '\.pfx$', '.cer')' -CertStoreLocation Cert:\LocalMachine\TrustedPeople" -ForegroundColor DarkGray
}
elseif ($Store) {
    Write-Host ''
    Write-Host '  Paket BİLEREK imzalanmadı: Store yüklemeyi kendi sertifikasıyla' -ForegroundColor DarkGray
    Write-Host '  yeniden imzalar. Partner Center > Paketler ekranına bu dosyayı yükleyin.' -ForegroundColor DarkGray
}

# ---------------------------------------------------------------------------
# 6. Sertifikasyon denetimi
# ---------------------------------------------------------------------------

if ($Wack) {
    Write-Step 'Windows App Certification Kit'
    $appcert = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\App Certification Kit\appcert.exe'
    if (-not (Test-Path -LiteralPath $appcert)) {
        Write-Warning "appcert.exe bulunamadı ($appcert). Windows SDK'nın 'App Certification Kit' bileşenini kurun."
    }
    else {
        $report = Join-Path $outRoot "wack-$Arch.xml"
        Write-Host '  Denetim birkaç dakika sürer ve yönetici hakkı ister...'
        & $appcert reset
        & $appcert test -appxpackagepath $msixPath -reportoutputpath $report
        if ($LASTEXITCODE -ne 0) {
            Write-Warning "WACK hatayla döndü (çıkış kodu $LASTEXITCODE). Rapor: $report"
        }
        else {
            Write-Host "  Rapor     : $report"
        }
    }
}

Write-Step 'Tamamlandı'
Write-Host "  Çıktı     : $msixPath"
