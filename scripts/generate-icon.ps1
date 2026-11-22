param(
    [Parameter(Mandatory = $true)]
    [string]$Source,
    [Parameter(Mandatory = $true)]
    [string]$IcoDestination,
    [Parameter(Mandatory = $true)]
    [string]$PngDestination
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-ResizedPng([System.Drawing.Image]$Image, [int]$Size) {
    $bitmap = [System.Drawing.Bitmap]::new(
        $Size, $Size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($Image, 0, 0, $Size, $Size)

        $stream = [System.IO.MemoryStream]::new()
        try {
            $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
            return $stream.ToArray()
        } finally {
            $stream.Dispose()
        }
    } finally {
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

$sourceImage = [System.Drawing.Image]::FromFile((Resolve-Path -LiteralPath $Source))
try {
    $entries = foreach ($size in @(16, 24, 32, 48, 64, 128, 256)) {
        [PSCustomObject]@{
            Size = $size
            Bytes = [byte[]](New-ResizedPng $sourceImage $size)
        }
    }
    [System.IO.File]::WriteAllBytes(
        $PngDestination, [byte[]](New-ResizedPng $sourceImage 256))
} finally {
    $sourceImage.Dispose()
}

$stream = [System.IO.File]::Create($IcoDestination)
$writer = [System.IO.BinaryWriter]::new($stream)
try {
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$entries.Count)
    $offset = 6 + (16 * $entries.Count)
    foreach ($entry in $entries) {
        $writer.Write([byte]$(if ($entry.Size -eq 256) { 0 } else { $entry.Size }))
        $writer.Write([byte]$(if ($entry.Size -eq 256) { 0 } else { $entry.Size }))
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$entry.Bytes.Length)
        $writer.Write([uint32]$offset)
        $offset += $entry.Bytes.Length
    }
    foreach ($entry in $entries) {
        $writer.Write($entry.Bytes)
    }
} finally {
    $writer.Dispose()
    $stream.Dispose()
}
