$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$gbaCover = Join-Path $root 'ellis house advance.png'
$pspCover = Join-Path $root 'ellis House psp.png'

function Save-CroppedImage {
    param(
        [string]$Source,
        [string]$Destination,
        [int]$Width,
        [int]$Height,
        [ValidateSet('Png','Bmp')][string]$Format = 'Png'
    )
    $sourceImage = [System.Drawing.Image]::FromFile($Source)
    try {
        $targetRatio = $Width / [double]$Height
        $sourceRatio = $sourceImage.Width / [double]$sourceImage.Height
        if ($sourceRatio -gt $targetRatio) {
            $cropHeight = $sourceImage.Height
            $cropWidth = [int][Math]::Round($cropHeight * $targetRatio)
            $cropX = [int](($sourceImage.Width - $cropWidth) / 2)
            $cropY = 0
        } else {
            $cropWidth = $sourceImage.Width
            $cropHeight = [int][Math]::Round($cropWidth / $targetRatio)
            $cropX = 0
            $cropY = [int](($sourceImage.Height - $cropHeight) / 2)
        }
        $bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
                $graphics.DrawImage($sourceImage,
                    (New-Object System.Drawing.Rectangle(0, 0, $Width, $Height)),
                    (New-Object System.Drawing.Rectangle($cropX, $cropY, $cropWidth, $cropHeight)),
                    [System.Drawing.GraphicsUnit]::Pixel)
            } finally { $graphics.Dispose() }
            $parent = Split-Path -Parent $Destination
            New-Item -ItemType Directory -Force -Path $parent | Out-Null
            $imageFormat = if ($Format -eq 'Bmp') { [System.Drawing.Imaging.ImageFormat]::Bmp } else { [System.Drawing.Imaging.ImageFormat]::Png }
            $bitmap.Save($Destination, $imageFormat)
        } finally { $bitmap.Dispose() }
    } finally { $sourceImage.Dispose() }
}

function Save-FittedImage {
    param([string]$Source, [string]$Destination, [int]$Width, [int]$Height)
    $sourceImage = [System.Drawing.Image]::FromFile($Source)
    try {
        $scale = [Math]::Min($Width / [double]$sourceImage.Width, $Height / [double]$sourceImage.Height)
        $drawWidth = [int][Math]::Round($sourceImage.Width * $scale)
        $drawHeight = [int][Math]::Round($sourceImage.Height * $scale)
        $drawX = [int](($Width - $drawWidth) / 2)
        $drawY = [int](($Height - $drawHeight) / 2)
        $bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::FromArgb(25, 14, 35))
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($sourceImage, $drawX, $drawY, $drawWidth, $drawHeight)
            } finally { $graphics.Dispose() }
            New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Destination) | Out-Null
            $bitmap.Save($Destination, [System.Drawing.Imaging.ImageFormat]::Png)
        } finally { $bitmap.Dispose() }
    } finally { $sourceImage.Dispose() }
}

function Save-GbaBmp {
    param([string]$Source, [string]$Destination, [int]$Width, [int]$Height)
    $sourceImage = [System.Drawing.Image]::FromFile($Source)
    try {
        $targetRatio = $Width / [double]$Height
        $sourceRatio = $sourceImage.Width / [double]$sourceImage.Height
        if ($sourceRatio -gt $targetRatio) {
            $cropHeight = $sourceImage.Height
            $cropWidth = [int][Math]::Round($cropHeight * $targetRatio)
            $cropX = [int](($sourceImage.Width - $cropWidth) / 2)
            $cropY = 0
        } else {
            $cropWidth = $sourceImage.Width
            $cropHeight = [int][Math]::Round($cropWidth / $targetRatio)
            $cropX = 0
            $cropY = [int](($sourceImage.Height - $cropHeight) / 2)
        }
        $bitmap = New-Object System.Drawing.Bitmap($Width, $Height, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $graphics.DrawImage($sourceImage,
                    (New-Object System.Drawing.Rectangle(0, 0, $Width, $Height)),
                    (New-Object System.Drawing.Rectangle($cropX, $cropY, $cropWidth, $cropHeight)),
                    [System.Drawing.GraphicsUnit]::Pixel)
            } finally { $graphics.Dispose() }

            # DS Style's own Manager writes 16-bit, top-down BGR555 BMP files.
            # A conventional 24-bit Windows BMP can decode as corrupted colour
            # data on the cart even though desktop image viewers accept it.
            $parent = Split-Path -Parent $Destination
            New-Item -ItemType Directory -Force -Path $parent | Out-Null
            $stride = (($Width * 2 + 3) -band -4)
            $pixelBytes = $stride * $Height
            $stream = [System.IO.File]::Create($Destination)
            $writer = New-Object System.IO.BinaryWriter($stream)
            try {
                $writer.Write([byte]0x42); $writer.Write([byte]0x4D)
                $writer.Write([int](54 + $pixelBytes)); $writer.Write([int]0); $writer.Write([int]54)
                $writer.Write([int]40); $writer.Write([int]$Width); $writer.Write([int](-$Height))
                $writer.Write([short]1); $writer.Write([short]16); $writer.Write([int]0)
                $writer.Write([int]$pixelBytes); $writer.Write([int]2834); $writer.Write([int]2834)
                $writer.Write([int]0); $writer.Write([int]0)
                for ($y = 0; $y -lt $Height; $y++) {
                    for ($x = 0; $x -lt $Width; $x++) {
                        $pixel = $bitmap.GetPixel($x, $y)
                        $bgr555 = (($pixel.B -shr 3) -shl 10) -bor (($pixel.G -shr 3) -shl 5) -bor ($pixel.R -shr 3)
                        $writer.Write([uint16]$bgr555)
                    }
                    for ($padding = $Width * 2; $padding -lt $stride; $padding++) { $writer.Write([byte]0) }
                }
            } finally { $writer.Dispose(); $stream.Dispose() }
        } finally { $bitmap.Dispose() }
    } finally { $sourceImage.Dispose() }
}

$originalGba = Join-Path $root 'artwork\original\ellis-house-gba-cover.png'
$originalPsp = Join-Path $root 'artwork\original\ellis-house-psp-cover.png'
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $originalGba) | Out-Null
Copy-Item -LiteralPath $gbaCover -Destination $originalGba -Force
Copy-Item -LiteralPath $pspCover -Destination $originalPsp -Force

$retroflow = Join-Path $root 'artwork\psp\retroflow\EllisHousePSP.png'
Save-FittedImage -Source $pspCover -Destination $retroflow -Width 250 -Height 320
Copy-Item -LiteralPath $retroflow -Destination (Join-Path $root "artwork\psp\retroflow\Elli's House.png") -Force
New-Item -ItemType Directory -Force -Path (Join-Path $root 'artwork\psp\generic') | Out-Null
Copy-Item -LiteralPath $retroflow -Destination (Join-Path $root 'artwork\psp\generic\cover.png') -Force

$standard = Join-Path $root 'artwork\gba\standard\IMGS\E\L\ELLI.bmp'
$dsWide = Join-Path $root 'artwork\gba\ds-style\SYSTEM\IMGS\E\L\ELLI.bmp'
$dsSquare = Join-Path $root 'artwork\gba\ds-style\SYSTEM\IMGS2\E\L\ELLI.bmp'
Save-CroppedImage -Source $gbaCover -Destination $standard -Width 120 -Height 80 -Format Bmp
Save-GbaBmp -Source $gbaCover -Destination $dsWide -Width 120 -Height 80
Save-GbaBmp -Source $gbaCover -Destination $dsSquare -Width 80 -Height 80

Write-Output 'Converted the supplied cover files for RetroFlow, EZ-Flash and DS Style.'
