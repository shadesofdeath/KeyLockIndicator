# make_icons.ps1 — KeyLock Indicator simge üreticisi (spec §3.5, §7).
#
# Windows PowerShell 5.1 + System.Drawing ile üretir:
#   res/icons/app.ico                          uygulama simgesi (accent plaka + beyaz glif)
#   res/icons/tray_<key>_<state>_<theme>.ico   12 tray simgesi (şeffaf zemin)
#   packaging/Assets/*.png                     MSIX görselleri (app.ico ile aynı tasarım)
#
# TEK DOĞRULUK KAYNAĞI: glif geometrisi src/IconShapes.cpp içindeki 72 birimlik
# (IconGeometry::kBoxSize) koordinatların birebir kopyasıdır. Orada bir koordinat
# değişirse buradaki karşılığı da değişmelidir; yoksa tray simgesi OSD'deki
# vektör ikondan farklı görünür. Değerler aşağıda blok blok işaretlendi.
#
# Kullanım (proje kökünden):
#   powershell.exe -NoProfile -ExecutionPolicy Bypass -File tools/make_icons.ps1

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# --- Yollar ------------------------------------------------------------------
# Betik cwd'ye değil kendi konumuna göre çalışır: CMake yapılandırma sırasında
# çağırdığında çalışma dizini farklı olabilir.
$root     = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$iconDir  = Join-Path $root 'res\icons'
$assetDir = Join-Path $root 'packaging\Assets'
New-Item -ItemType Directory -Force -Path $iconDir  | Out-Null
New-Item -ItemType Directory -Force -Path $assetDir | Out-Null

# --- Sabitler ----------------------------------------------------------------
$Box      = 72.0                 # IconGeometry::kBoxSize
$PenUnits = $Box * 0.09          # dış hat kalınlığı: boyutun ~%9'u (16 px'te ~1.4 px)
$IcoSizes = @(16, 20, 24, 32, 48, 256)
$OffAlpha = 166                  # kapalı durum: %65 opaklık

$Accent    = [Drawing.Color]::FromArgb(255,  10, 132, 255)   # #0A84FF
$White     = [Drawing.Color]::FromArgb(255, 255, 255, 255)   # koyu tema glifi
$InkGlyph  = [Drawing.Color]::FromArgb(255,  26,  26,  26)   # #1A1A1A, açık tema glifi

# ---------------------------------------------------------------------------
# Geometri yardımcıları
# ---------------------------------------------------------------------------

function Add-RoundRect {
    param(
        [Drawing.Drawing2D.GraphicsPath]$Path,
        [double]$X, [double]$Y, [double]$W, [double]$H, [double]$R
    )
    $d = $R * 2.0
    $Path.StartFigure()
    $Path.AddArc([float]$X,            [float]$Y,            [float]$d, [float]$d, 180, 90)
    $Path.AddArc([float]($X + $W - $d), [float]$Y,            [float]$d, [float]$d, 270, 90)
    $Path.AddArc([float]($X + $W - $d), [float]($Y + $H - $d), [float]$d, [float]$d,   0, 90)
    $Path.AddArc([float]$X,            [float]($Y + $H - $d), [float]$d, [float]$d,  90, 90)
    $Path.CloseFigure()
}

function Add-Dot {
    param([Drawing.Drawing2D.GraphicsPath]$Path, [double]$Cx, [double]$Cy, [double]$R)
    $Path.AddEllipse([float]($Cx - $R), [float]($Cy - $R), [float]($R * 2.0), [float]($R * 2.0))
}

function New-Poly {
    param([double[][]]$Points)
    $arr = New-Object 'Drawing.PointF[]' $Points.Count
    for ($i = 0; $i -lt $Points.Count; $i++) {
        $arr[$i] = [Drawing.PointF]::new([float]$Points[$i][0], [float]$Points[$i][1])
    }
    return $arr
}

# ---------------------------------------------------------------------------
# Glifler — her biri @{ Fill = <path|null>; Stroke = <path|null> } döner.
# IconShapes::Built ile aynı ayrım: doldurulan parçalar + konturlanan parçalar.
# ---------------------------------------------------------------------------

# Caps Lock: yukarı bakan ok + düz taban çizgisi (IconShapes::BuildCaps).
#   ok çokgeni : (36,8) (61,33) (47,33) (47,47) (25,47) (25,33) (11,33)
#   taban      : yuvarlatılmış dikdörtgen x=21 y=55 w=30 h=8 r=4
function New-CapsGlyph {
    param([bool]$On)
    $p = [Drawing.Drawing2D.GraphicsPath]::new()
    $p.AddPolygon((New-Poly @(
        @(36.0,  8.0), @(61.0, 33.0), @(47.0, 33.0), @(47.0, 47.0),
        @(25.0, 47.0), @(25.0, 33.0), @(11.0, 33.0)
    )))
    Add-RoundRect -Path $p -X 21 -Y 55 -W 30 -H 8 -R 4
    if ($On) { return @{ Fill = $p;     Stroke = $null } }
    return       @{ Fill = $null;   Stroke = $p }
}

# Num Lock: yuvarlatılmış kare çerçeve + 3x3 nokta ızgarası (IconShapes::BuildNum).
#   çerçeve : x=8 y=8 w=56 h=56 r=11
#   noktalar: x,y ∈ {21,36,51}; dış nokta r=3.6, merkez nokta r=5.6
# Kapalı hâlde noktalar hiç çizilmez — spec §3.5: "kapalıyken sadece dış çerçeve".
function New-NumGlyph {
    param([bool]$On)
    $frame = [Drawing.Drawing2D.GraphicsPath]::new()
    Add-RoundRect -Path $frame -X 8 -Y 8 -W 56 -H 56 -R 11
    if (-not $On) { return @{ Fill = $null; Stroke = $frame } }

    $dots = [Drawing.Drawing2D.GraphicsPath]::new()
    foreach ($cy in 21.0, 36.0, 51.0) {
        foreach ($cx in 21.0, 36.0, 51.0) {
            $r = if ($cx -eq 36.0 -and $cy -eq 36.0) { 5.6 } else { 3.6 }
            Add-Dot -Path $dots -Cx $cx -Cy $cy -R $r
        }
    }
    return @{ Fill = $dots; Stroke = $frame }
}

# Scroll Lock: asma kilit gövdesi + dikey çift yönlü ok (IconShapes::BuildScroll).
#   gövde        : x=15 y=33 w=42 h=32 r=7
#   kapalı kanca : (25,33)-(25,28) + yay bbox(25,17,22,22) 180°/180° + (47,28)-(47,33)
#   açık kanca   : yay bbox(29,13,22,22) 180°/180° + (51,24)-(51,33); sol uç serbest
#   çift ok      : 10 köşeli çokgen, y 38..60, şaft x 33.5..38.5, başlık x 29.5..42.5
# Açık hâlde ok çizilmez: 16 px'te dış hatlı gövde + ok birbirine karışıyor.
# Dolu hâlde ok gövdeden OYULUR — GraphicsPath varsayılan FillMode.Alternate
# (D2D karşılığı D2D1_FILL_MODE_ALTERNATE) iç figürü boşluk yapar.
function New-ScrollGlyph {
    param([bool]$On)
    if ($On) {
        $fill = [Drawing.Drawing2D.GraphicsPath]::new()
        Add-RoundRect -Path $fill -X 15 -Y 33 -W 42 -H 32 -R 7
        $fill.AddPolygon((New-Poly @(
            @(36.0, 38.0), @(42.5, 45.5), @(38.5, 45.5), @(38.5, 52.5), @(42.5, 52.5),
            @(36.0, 60.0), @(29.5, 52.5), @(33.5, 52.5), @(33.5, 45.5), @(29.5, 45.5)
        )))
        $stroke = [Drawing.Drawing2D.GraphicsPath]::new()
        $stroke.StartFigure()
        $stroke.AddLine([float]25, [float]33, [float]25, [float]28)
        $stroke.AddArc([float]25, [float]17, [float]22, [float]22, 180, 180)
        $stroke.AddLine([float]47, [float]28, [float]47, [float]33)
        return @{ Fill = $fill; Stroke = $stroke }
    }

    $stroke = [Drawing.Drawing2D.GraphicsPath]::new()
    Add-RoundRect -Path $stroke -X 15 -Y 33 -W 42 -H 32 -R 7
    $stroke.StartFigure()
    $stroke.AddArc([float]29, [float]13, [float]22, [float]22, 180, 180)
    $stroke.AddLine([float]51, [float]24, [float]51, [float]33)
    return @{ Fill = $null; Stroke = $stroke }
}

function New-Glyph {
    param([ValidateSet('caps', 'num', 'scroll')][string]$Key, [bool]$On)
    switch ($Key) {
        'caps'   { return New-CapsGlyph   -On $On }
        'num'    { return New-NumGlyph    -On $On }
        'scroll' { return New-ScrollGlyph -On $On }
    }
}

function Clear-Glyph {
    param([hashtable]$Glyph)
    if ($Glyph.Fill)   { $Glyph.Fill.Dispose() }
    if ($Glyph.Stroke) { $Glyph.Stroke.Dispose() }
}

# ---------------------------------------------------------------------------
# Çizim
# ---------------------------------------------------------------------------

# Graphics'in dönüşümü zaten 72 birimlik kutuya ayarlanmış olmalıdır; kalem
# kalınlığı da birim uzayında verilir, ölçekle birlikte büyür.
function Add-GlyphToGraphics {
    param([Drawing.Graphics]$G, [hashtable]$Glyph, [Drawing.Color]$Color)
    $brush = [Drawing.SolidBrush]::new($Color)
    $pen   = [Drawing.Pen]::new($Color, [float]$PenUnits)
    $pen.LineJoin = [Drawing.Drawing2D.LineJoin]::Round
    $pen.StartCap = [Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap   = [Drawing.Drawing2D.LineCap]::Round
    try {
        if ($Glyph.Fill)   { $G.FillPath($brush, $Glyph.Fill) }
        if ($Glyph.Stroke) { $G.DrawPath($pen, $Glyph.Stroke) }
    } finally {
        $pen.Dispose(); $brush.Dispose()
    }
}

function New-Canvas {
    param([int]$Width, [int]$Height)
    $bmp = [Drawing.Bitmap]::new($Width, $Height, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g   = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode   = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([Drawing.Color]::Transparent)
    return @{ Bitmap = $bmp; Graphics = $g }
}

# Tray simgesi: şeffaf zemin, tek renk glif.
function New-GlyphBitmap {
    param([int]$Size, [string]$Key, [bool]$On, [Drawing.Color]$Color)
    $c = New-Canvas -Width $Size -Height $Size
    try {
        $k = $Size / $Box
        $c.Graphics.ScaleTransform([float]$k, [float]$k)
        $glyph = New-Glyph -Key $Key -On $On
        try { Add-GlyphToGraphics -G $c.Graphics -Glyph $glyph -Color $Color }
        finally { Clear-Glyph -Glyph $glyph }
    } finally {
        $c.Graphics.Dispose()
    }
    return $c.Bitmap
}

# Uygulama simgesi / MSIX görseli: accent yuvarlatılmış kare + beyaz Caps glifi.
# Kare plaka kısa kenara göre ölçeklenir ve ortalanır; Wide/Splash gibi dikdörtgen
# görsellerin kalanı şeffaf kalır (MSIX'in beklediği davranış).
function New-LogoBitmap {
    param([int]$Width, [int]$Height, [double]$Scale = 1.0)
    $c = New-Canvas -Width $Width -Height $Height
    try {
        $side = [Math]::Min($Width, $Height) * $Scale
        $g = $c.Graphics
        # Prepend sırası: en son çağrılan dönüşüm koordinatlara ilk uygulanır,
        # yani önce ölçekle sonra ortala.
        $g.TranslateTransform([float](($Width - $side) / 2.0), [float](($Height - $side) / 2.0))
        $g.ScaleTransform([float]($side / $Box), [float]($side / $Box))

        $plate = [Drawing.Drawing2D.GraphicsPath]::new()
        Add-RoundRect -Path $plate -X 2 -Y 2 -W 68 -H 68 -R 15
        $brush = [Drawing.SolidBrush]::new($Accent)
        try { $g.FillPath($brush, $plate) } finally { $brush.Dispose(); $plate.Dispose() }

        # Glifi plakanın içine sığdır: merkez (36,36) etrafında %60 ölçek.
        $glyph = New-CapsGlyph -On $true
        $m = [Drawing.Drawing2D.Matrix]::new()
        $m.Translate([float]36, [float]36)
        $m.Scale([float]0.60, [float]0.60)
        $m.Translate([float](-36), [float](-36))
        $glyph.Fill.Transform($m)
        $m.Dispose()
        try { Add-GlyphToGraphics -G $g -Glyph $glyph -Color $White }
        finally { Clear-Glyph -Glyph $glyph }
    } finally {
        $c.Graphics.Dispose()
    }
    return $c.Bitmap
}

# ---------------------------------------------------------------------------
# Kodlama / dosya yazımı
# ---------------------------------------------------------------------------

function Get-PngBytes {
    param([Drawing.Bitmap]$Bitmap)
    $ms = [IO.MemoryStream]::new()
    try {
        $Bitmap.Save($ms, [Drawing.Imaging.ImageFormat]::Png)
        $bytes = $ms.ToArray()
    } finally { $ms.Dispose() }
    return , $bytes    # virgül: PowerShell'in byte[]'i tek tek elemana açmasını engeller
}

# ICO kapsayıcısını elle kur: 6 bayt ICONDIR + boyut başına 16 bayt ICONDIRENTRY
# + PNG gövdeleri. Vista+ PNG içeren ICO'yu destekler; BMP+AND maskesi kurmaya
# gerek yok, 32 bit alfa doğrudan PNG'den gelir.
function New-IcoBytes {
    param([object[]]$Entries)   # @{ Size = <int>; Bytes = <byte[]> }
    $ms = [IO.MemoryStream]::new()
    $bw = [IO.BinaryWriter]::new($ms)
    try {
        $bw.Write([UInt16]0)                  # idReserved
        $bw.Write([UInt16]1)                  # idType = ikon
        $bw.Write([UInt16]$Entries.Count)     # idCount

        $offset = 6 + 16 * $Entries.Count
        foreach ($e in $Entries) {
            $dim = if ($e.Size -ge 256) { 0 } else { $e.Size }   # 256 → 0
            $bw.Write([byte]$dim)             # bWidth
            $bw.Write([byte]$dim)             # bHeight
            $bw.Write([byte]0)                # bColorCount (32 bit'te 0)
            $bw.Write([byte]0)                # bReserved
            $bw.Write([UInt16]1)              # wPlanes
            $bw.Write([UInt16]32)             # wBitCount
            $bw.Write([UInt32]$e.Bytes.Length)
            $bw.Write([UInt32]$offset)
            $offset += $e.Bytes.Length
        }
        foreach ($e in $Entries) { $bw.Write($e.Bytes) }
        $bw.Flush()
        $out = $ms.ToArray()
    } finally { $bw.Dispose(); $ms.Dispose() }
    return , $out
}

$report = [Collections.Generic.List[object]]::new()

function Add-Report {
    param([string]$Path)
    $item = Get-Item -LiteralPath $Path
    $report.Add([PSCustomObject]@{
        File  = $item.FullName.Substring($root.Length).TrimStart('\')
        Bytes = $item.Length
    })
}

function Write-TrayIco {
    param([string]$Path, [string]$Key, [bool]$On, [Drawing.Color]$Color)
    $entries = foreach ($s in $IcoSizes) {
        $bmp = New-GlyphBitmap -Size $s -Key $Key -On $On -Color $Color
        try { @{ Size = $s; Bytes = (Get-PngBytes -Bitmap $bmp) } } finally { $bmp.Dispose() }
    }
    [IO.File]::WriteAllBytes($Path, (New-IcoBytes -Entries $entries))
    Add-Report -Path $Path
}

function Write-LogoIco {
    param([string]$Path)
    $entries = foreach ($s in $IcoSizes) {
        $bmp = New-LogoBitmap -Width $s -Height $s -Scale 1.0
        try { @{ Size = $s; Bytes = (Get-PngBytes -Bitmap $bmp) } } finally { $bmp.Dispose() }
    }
    [IO.File]::WriteAllBytes($Path, (New-IcoBytes -Entries $entries))
    Add-Report -Path $Path
}

function Write-LogoPng {
    param([string]$Path, [int]$Width, [int]$Height)
    $bmp = New-LogoBitmap -Width $Width -Height $Height -Scale 0.86
    try { [IO.File]::WriteAllBytes($Path, (Get-PngBytes -Bitmap $bmp)) } finally { $bmp.Dispose() }
    Add-Report -Path $Path
}

# ---------------------------------------------------------------------------
# Üretim
# ---------------------------------------------------------------------------

Write-Host 'KeyLock Indicator simge uretimi...'

Write-LogoIco -Path (Join-Path $iconDir 'app.ico')

foreach ($key in 'caps', 'num', 'scroll') {
    foreach ($state in 'off', 'on') {
        foreach ($theme in 'dark', 'light') {
            $base  = if ($theme -eq 'dark') { $White } else { $InkGlyph }
            $alpha = if ($state -eq 'on') { 255 } else { $OffAlpha }
            $color = [Drawing.Color]::FromArgb($alpha, $base.R, $base.G, $base.B)
            Write-TrayIco -Path (Join-Path $iconDir "tray_${key}_${state}_${theme}.ico") `
                          -Key $key -On ($state -eq 'on') -Color $color
        }
    }
}

# --- MSIX görselleri ---------------------------------------------------------
# Her görsel bir TABAN dosya (ölçeksiz ad) ve ölçek çeşitleriyle üretilir.
# Windows kutucuğu 125/150/200/400 DPI'da ".scale-N" dosyasını seçer; yoksa
# ölçek-100'ü büyütür ve kenarlar bulanıklaşır. Taban dosyanın ölçeksiz adla da
# durması bilinçlidir: AppxManifest görselleri o adla gösterir, böylece paket
# resources.pri olmadan da geçerli kalır (PRI yalnızca çeşitleri devreye sokar).
$AssetBases = @(
    @{ Name = 'Square44x44Logo';   W =  44; H =  44 },
    @{ Name = 'Square71x71Logo';   W =  71; H =  71 },
    @{ Name = 'Square150x150Logo'; W = 150; H = 150 },
    @{ Name = 'Square310x310Logo'; W = 310; H = 310 },
    @{ Name = 'Wide310x150Logo';   W = 310; H = 150 },
    @{ Name = 'StoreLogo';         W =  50; H =  50 },
    @{ Name = 'SplashScreen';      W = 620; H = 300 }
)

$AssetScales = @(125, 150, 200, 400)

# Görev çubuğu, Başlat listesi ve Alt+Tab 44x44 görselini piksel hedefine göre
# seçer. "unplated": kabuk arkaya kendi plakasını KOYMAZ, simge doğrudan çizilir
# — bizim görselimiz zaten kendi accent plakasını taşıdığı için doğru çeşit
# budur ve iki ad da aynı çizimden üretilir.
$TargetSizes = @(16, 24, 32, 48, 256)

# .NET'in varsayılan yuvarlaması bankacı yuvarlamasıdır (62,5 → 62). Windows'un
# beklediği değerler yukarı yuvarlamayla gelir: 50 → 63, 71 → 89, 150 → 188.
function Get-ScaledPx {
    param([int]$Base, [int]$Scale)
    return [int][math]::Round($Base * $Scale / 100.0, 0, [MidpointRounding]::AwayFromZero)
}

foreach ($a in $AssetBases) {
    Write-LogoPng -Path (Join-Path $assetDir "$($a.Name).png") -Width $a.W -Height $a.H

    foreach ($s in $AssetScales) {
        Write-LogoPng -Path (Join-Path $assetDir "$($a.Name).scale-$s.png") `
                      -Width  (Get-ScaledPx -Base $a.W -Scale $s) `
                      -Height (Get-ScaledPx -Base $a.H -Scale $s)
    }
}

foreach ($t in $TargetSizes) {
    Write-LogoPng -Path (Join-Path $assetDir "Square44x44Logo.targetsize-$t.png") `
                  -Width $t -Height $t
    Write-LogoPng -Path (Join-Path $assetDir "Square44x44Logo.targetsize-${t}_altform-unplated.png") `
                  -Width $t -Height $t
}

# --- Rapor -------------------------------------------------------------------
foreach ($r in $report) {
    Write-Host ('  {0,-62} {1,9:N0} bayt' -f $r.File, $r.Bytes)
}
$zero = @($report | Where-Object { $_.Bytes -eq 0 })
if ($zero.Count -gt 0) {
    throw ("Bos dosya uretildi: " + ($zero.File -join ', '))
}
Write-Host ('Tamam: {0} dosya.' -f $report.Count)
