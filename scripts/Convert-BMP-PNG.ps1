<#
.SYNOPSIS
    Converts BMP screenshots to optimized PNG format for documentation.

.DESCRIPTION
    Processes all BMP files from docs/images/bmp/ directory and converts them to PNG format
    in the docs/images/ directory. Uses System.Drawing for lossless conversion.
    
    BMP files are captured from the FluidTouch screenshot server (800×480 resolution) and
    stored in the bmp/ subdirectory. This script converts them to PNG for smaller file size
    while maintaining image quality.

.EXAMPLE
    .\Convert-BMP-PNG.ps1
    Converts all BMP files in docs/images/bmp/ to PNG in docs/images/

.NOTES
    - Script can be run from any directory
    - BMP files are preserved in bmp/ subdirectory (ignored by git)
    - PNG files overwrite existing files with same name
    - Requires .NET System.Drawing assembly (included with PowerShell)
#>

[CmdletBinding()]
param()

# Determine script location and set paths
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$bmpDir = Join-Path $scriptDir "..\docs\images\bmp"
$outputDir = Join-Path $scriptDir "..\docs\images"

Write-Host "BMP to PNG Conversion for FluidTouch Documentation" -ForegroundColor Cyan
Write-Host "Source: $bmpDir" -ForegroundColor Gray
Write-Host "Output: $outputDir`n" -ForegroundColor Gray

# Verify bmp directory exists
if (-not (Test-Path $bmpDir)) {
    Write-Host "Error: BMP directory not found: $bmpDir" -ForegroundColor Red
    Write-Host "Please create docs/images/bmp/ and place BMP screenshots there." -ForegroundColor Yellow
    exit 1
}

# Ensure output directory exists
if (-not (Test-Path $outputDir)) {
    Write-Host "Error: Output directory not found: $outputDir" -ForegroundColor Red
    exit 1
}

# Get all BMP files from bmp directory
$bmpFiles = Get-ChildItem -Path $bmpDir -Filter "*.bmp" -ErrorAction SilentlyContinue

if ($bmpFiles.Count -eq 0) {
    Write-Host "No BMP files found in $bmpDir" -ForegroundColor Yellow
    Write-Host "Place BMP screenshots in docs/images/bmp/ directory first." -ForegroundColor Gray
    exit 0
}

Write-Host "Found $($bmpFiles.Count) BMP file(s) to convert`n" -ForegroundColor Green

# Load System.Drawing assembly for image processing
try {
    Add-Type -AssemblyName System.Drawing -ErrorAction Stop
}
catch {
    Write-Host "Error: Failed to load System.Drawing assembly" -ForegroundColor Red
    Write-Host "This script requires .NET System.Drawing (included with PowerShell)" -ForegroundColor Yellow
    exit 1
}

$successCount = 0
$errorCount = 0
$skippedCount = 0

foreach ($file in $bmpFiles) {
    try {
        $baseName = [System.IO.Path]::GetFileNameWithoutExtension($file.Name)
        $outputPath = Join-Path $outputDir "$baseName.png"
        
        # Check if output file already exists and is newer
        if ((Test-Path $outputPath) -and 
            ((Get-Item $outputPath).LastWriteTime -gt $file.LastWriteTime)) {
            Write-Host "Skipping: $($file.Name) (PNG is newer)" -ForegroundColor Gray
            $skippedCount++
            continue
        }
        
        Write-Host "Converting: $($file.Name) -> $baseName.png" -ForegroundColor White
        
        # Load BMP and save as PNG with optimal compression
        $bitmap = [System.Drawing.Image]::FromFile($file.FullName)
        $bitmap.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $bitmap.Dispose()
        
        # Display file size comparison
        $bmpSize = [math]::Round($file.Length / 1KB, 1)
        $pngSize = [math]::Round((Get-Item $outputPath).Length / 1KB, 1)
        $savings = [math]::Round((1 - ($pngSize / $bmpSize)) * 100, 0)
        Write-Host "  $bmpSize KB -> $pngSize KB (${savings}% smaller)" -ForegroundColor Green
        
        $successCount++
    }
    catch {
        Write-Host "Error converting $($file.Name): $_" -ForegroundColor Red
        $errorCount++
    }
}

Write-Host "`n========================================" -ForegroundColor Cyan
Write-Host "Conversion Complete!" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Successfully converted: $successCount file(s)" -ForegroundColor Green
if ($skippedCount -gt 0) {
    Write-Host "Skipped (up to date): $skippedCount file(s)" -ForegroundColor Gray
}
if ($errorCount -gt 0) {
    Write-Host "Failed: $errorCount file(s)" -ForegroundColor Red
    exit 1
}

exit 0
