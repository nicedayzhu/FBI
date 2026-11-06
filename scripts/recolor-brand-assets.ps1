param(
    [Parameter(Mandatory = $true)]
    [string]$TxobTool,
    [string]$TargetColor = '#19B7A5',
    [double]$BannerHueDegrees = 174
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$iconPath = Join-Path $projectRoot 'meta\icon_3ds.png'
$bannerPath = Join-Path $projectRoot 'meta\banner_3ds.cgfx'
$workRoot = Join-Path $projectRoot 'build\brand-assets'
$textureRoot = Join-Path $workRoot 'textures'
$backupIcon = Join-Path $workRoot 'icon_3ds-before.png'
$backupBanner = Join-Path $workRoot 'banner_3ds-before.cgfx'

if (-not (Test-Path -LiteralPath $TxobTool -PathType Leaf)) {
    throw "txobtool was not found: $TxobTool"
}

Add-Type -AssemblyName System.Drawing

function Convert-HexColor([string]$Value) {
    if ($Value -notmatch '^#[0-9A-Fa-f]{6}$') {
        throw "TargetColor must use #RRGGBB: $Value"
    }
    return [Drawing.ColorTranslator]::FromHtml($Value)
}

function Save-BitmapAtomically(
    [Drawing.Bitmap]$Bitmap,
    [string]$Destination
) {
    $temporary = "$Destination.new.png"
    try {
        $Bitmap.Save($temporary, [Drawing.Imaging.ImageFormat]::Png)
        Move-Item -LiteralPath $temporary -Destination $Destination -Force
    }
    finally {
        if (Test-Path -LiteralPath $temporary) {
            Remove-Item -LiteralPath $temporary -Force
        }
    }
}

function Recolor-Icon(
    [string]$Path,
    [Drawing.Color]$Color
) {
    $lockedSource = [Drawing.Bitmap]::FromFile($Path)
    $source = [Drawing.Bitmap]::new($lockedSource)
    $lockedSource.Dispose()
    $output = [Drawing.Bitmap]::new(
        $source.Width,
        $source.Height,
        [Drawing.Imaging.PixelFormat]::Format24bppRgb
    )
    try {
        $sourceAnchor = [Drawing.Color]::FromArgb(94, 136, 255)
        for ($y = 0; $y -lt $source.Height; $y++) {
            for ($x = 0; $x -lt $source.Width; $x++) {
                $pixel = $source.GetPixel($x, $y)
                if ($pixel.B -gt $pixel.R + 8 -and $pixel.B -gt $pixel.G + 8) {
                    $coverageR = (255.0 - $pixel.R) / (255.0 - $sourceAnchor.R)
                    $coverageG = (255.0 - $pixel.G) / (255.0 - $sourceAnchor.G)
                    $coverage = [Math]::Clamp(($coverageR + $coverageG) / 2.0, 0.0, 1.0)
                    $red = [Math]::Round(255 * (1 - $coverage) + $Color.R * $coverage)
                    $green = [Math]::Round(255 * (1 - $coverage) + $Color.G * $coverage)
                    $blue = [Math]::Round(255 * (1 - $coverage) + $Color.B * $coverage)
                    $pixel = [Drawing.Color]::FromArgb(
                        [int]$red,
                        [int]$green,
                        [int]$blue
                    )
                }
                $output.SetPixel($x, $y, $pixel)
            }
        }
        Save-BitmapAtomically $output $Path
    }
    finally {
        $output.Dispose()
        $source.Dispose()
    }
}

function Convert-HslToColor(
    [double]$Hue,
    [double]$Saturation,
    [double]$Lightness,
    [int]$Alpha
) {
    $chroma = (1 - [Math]::Abs(2 * $Lightness - 1)) * $Saturation
    $hueSector = ($Hue % 360) / 60.0
    $secondary = $chroma * (1 - [Math]::Abs(($hueSector % 2) - 1))
    $redPart = 0.0
    $greenPart = 0.0
    $bluePart = 0.0
    if ($hueSector -lt 1) {
        $redPart = $chroma; $greenPart = $secondary
    }
    elseif ($hueSector -lt 2) {
        $redPart = $secondary; $greenPart = $chroma
    }
    elseif ($hueSector -lt 3) {
        $greenPart = $chroma; $bluePart = $secondary
    }
    elseif ($hueSector -lt 4) {
        $greenPart = $secondary; $bluePart = $chroma
    }
    elseif ($hueSector -lt 5) {
        $redPart = $secondary; $bluePart = $chroma
    }
    else {
        $redPart = $chroma; $bluePart = $secondary
    }
    $offset = $Lightness - $chroma / 2
    return [Drawing.Color]::FromArgb(
        $Alpha,
        [int][Math]::Round(255 * ($redPart + $offset)),
        [int][Math]::Round(255 * ($greenPart + $offset)),
        [int][Math]::Round(255 * ($bluePart + $offset))
    )
}

function Recolor-BannerTexture(
    [string]$Path,
    [double]$HueDegrees
) {
    $lockedSource = [Drawing.Bitmap]::FromFile($Path)
    $source = [Drawing.Bitmap]::new($lockedSource)
    $lockedSource.Dispose()
    $output = [Drawing.Bitmap]::new(
        $source.Width,
        $source.Height,
        [Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    try {
        for ($y = 0; $y -lt $source.Height; $y++) {
            for ($x = 0; $x -lt $source.Width; $x++) {
                $pixel = $source.GetPixel($x, $y)
                if ($pixel.B -gt $pixel.R + 8 -and $pixel.B -gt $pixel.G + 8) {
                    $red = $pixel.R / 255.0
                    $green = $pixel.G / 255.0
                    $blue = $pixel.B / 255.0
                    $maximum = [Math]::Max($red, [Math]::Max($green, $blue))
                    $minimum = [Math]::Min($red, [Math]::Min($green, $blue))
                    $lightness = ($maximum + $minimum) / 2.0
                    $delta = $maximum - $minimum
                    if ($delta -gt 0) {
                        $saturation = $delta / (1 - [Math]::Abs(2 * $lightness - 1))
                        $saturation = [Math]::Min(0.78, $saturation * 0.72)
                        $lightness = [Math]::Max(0.0, $lightness - 0.025 * $saturation)
                        $pixel = Convert-HslToColor $HueDegrees $saturation $lightness $pixel.A
                    }
                }
                $output.SetPixel($x, $y, $pixel)
            }
        }
        Save-BitmapAtomically $output $Path
    }
    finally {
        $output.Dispose()
        $source.Dispose()
    }
}

$target = Convert-HexColor $TargetColor
if (Test-Path -LiteralPath $workRoot) {
    Remove-Item -LiteralPath $workRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $textureRoot -Force | Out-Null
Copy-Item -LiteralPath $iconPath -Destination $backupIcon
Copy-Item -LiteralPath $bannerPath -Destination $backupBanner

try {
    & $TxobTool --export --file $bannerPath --dir $textureRoot
    if ($LASTEXITCODE -ne 0) { throw 'Failed to extract banner textures.' }

    $frontTexture = Join-Path $textureRoot 'COMMON2.png'
    if (-not (Test-Path -LiteralPath $frontTexture -PathType Leaf)) {
        throw "Banner front texture was not extracted: $frontTexture"
    }

    Recolor-Icon $iconPath $target
    Recolor-BannerTexture $frontTexture $BannerHueDegrees

    & $TxobTool --import --file $bannerPath --dir $textureRoot
    if ($LASTEXITCODE -ne 0) { throw 'Failed to inject recolored banner textures.' }
}
catch {
    Copy-Item -LiteralPath $backupIcon -Destination $iconPath -Force
    Copy-Item -LiteralPath $backupBanner -Destination $bannerPath -Force
    throw
}

Write-Host "Recolored HOME Menu icon: $iconPath"
Write-Host "Recolored 3D banner texture: $bannerPath"
