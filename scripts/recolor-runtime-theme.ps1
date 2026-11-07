param(
    [string]$TargetColor = '#19B7A5'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$romfsRoot = Join-Path $projectRoot 'romfs'

Add-Type -AssemblyName System.Drawing

function Convert-HexColor([string]$Value) {
    if ($Value -notmatch '^#[0-9A-Fa-f]{6}$') {
        throw "TargetColor must use #RRGGBB: $Value"
    }
    return [Drawing.ColorTranslator]::FromHtml($Value)
}

function Convert-RgbToHsl([Drawing.Color]$Color) {
    $red = $Color.R / 255.0
    $green = $Color.G / 255.0
    $blue = $Color.B / 255.0
    $maximum = [Math]::Max($red, [Math]::Max($green, $blue))
    $minimum = [Math]::Min($red, [Math]::Min($green, $blue))
    $lightness = ($maximum + $minimum) / 2.0
    $delta = $maximum - $minimum
    $saturation = if ($delta -eq 0) {
        0.0
    } else {
        $delta / (1 - [Math]::Abs(2 * $lightness - 1))
    }
    return @($saturation, $lightness)
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

function Recolor-RuntimeAsset(
    [string]$Path,
    [Drawing.Color]$Color
) {
    $lockedSource = [Drawing.Bitmap]::FromFile($Path)
    $source = [Drawing.Bitmap]::new($lockedSource)
    $lockedSource.Dispose()
    $output = [Drawing.Bitmap]::new(
        $source.Width,
        $source.Height,
        [Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    $sourceAnchor = [Drawing.Color]::FromArgb(94, 136, 255)
    $targetHsl = Convert-RgbToHsl $Color
    $sourceHsl = Convert-RgbToHsl $sourceAnchor
    $changed = 0
    try {
        for ($y = 0; $y -lt $source.Height; $y++) {
            for ($x = 0; $x -lt $source.Width; $x++) {
                $pixel = $source.GetPixel($x, $y)
                if ($pixel.B -gt $pixel.R + 6 -and $pixel.B -gt $pixel.G + 6) {
                    $pixelHsl = Convert-RgbToHsl $pixel
                    if ($pixelHsl[1] -ge $sourceHsl[1]) {
                        $coverageR = (255.0 - $pixel.R) / (255.0 - $sourceAnchor.R)
                        $coverageG = (255.0 - $pixel.G) / (255.0 - $sourceAnchor.G)
                        $coverage = [Math]::Clamp(($coverageR + $coverageG) / 2.0, 0.0, 1.0)
                        $red = [Math]::Round(255 * (1 - $coverage) + $Color.R * $coverage)
                        $green = [Math]::Round(255 * (1 - $coverage) + $Color.G * $coverage)
                        $blue = [Math]::Round(255 * (1 - $coverage) + $Color.B * $coverage)
                        $pixel = [Drawing.Color]::FromArgb(
                            $pixel.A,
                            [int]$red,
                            [int]$green,
                            [int]$blue
                        )
                    }
                    else {
                        $lightness = [Math]::Clamp(
                            $pixelHsl[1] * $targetHsl[1] / $sourceHsl[1],
                            0.0,
                            1.0
                        )
                        $saturation = [Math]::Min($targetHsl[0], $pixelHsl[0] * $targetHsl[0])
                        $pixel = Convert-HslToColor 174 $saturation $lightness $pixel.A
                    }
                    $changed++
                }
                $output.SetPixel($x, $y, $pixel)
            }
        }
        if ($changed -gt 0) {
            Save-BitmapAtomically $output $Path
        }
        return $changed
    }
    finally {
        $output.Dispose()
        $source.Dispose()
    }
}

$target = Convert-HexColor $TargetColor
$changedFiles = 0
Get-ChildItem -LiteralPath $romfsRoot -Filter '*.png' -File | ForEach-Object {
    $changedPixels = Recolor-RuntimeAsset $_.FullName $target
    if ($changedPixels -gt 0) {
        $changedFiles++
        Write-Host "Recolored $($_.Name): $changedPixels pixels"
    }
}

Write-Host "Recolored runtime theme assets: $changedFiles file(s)"
