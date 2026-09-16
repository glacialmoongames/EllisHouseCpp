param([string]$Version = 'v0.1.0-port-preview')

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$dist = Join-Path $root 'dist'
$stage = Join-Path $dist 'stage'

if (Test-Path $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

function New-ReleaseZip([string]$Name, [scriptblock]$Populate) {
    $folder = Join-Path $stage $Name
    New-Item -ItemType Directory -Force -Path $folder | Out-Null
    & $Populate $folder
    $zip = Join-Path $dist "$Name.zip"
    if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
    Compress-Archive -Path (Join-Path $folder '*') -DestinationPath $zip -CompressionLevel Optimal
}

New-ReleaseZip 'EllisHouse-Windows' {
    param($folder)
    Copy-Item (Join-Path $root 'ellis_house.exe') $folder
    Copy-Item (Join-Path $root 'assets') $folder -Recurse
    Copy-Item (Join-Path $root 'ATTRIBUTION.md') $folder
    Copy-Item (Join-Path $root 'docs\INSTALL_WINDOWS.md') $folder
}

New-ReleaseZip 'EllisHouse-3DSX' {
    param($folder)
    $target = Join-Path $folder '3ds\EllisHouse3DS'
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item (Join-Path $root 'EllisHouse3DS.3dsx') (Join-Path $target 'EllisHouse3DS.3dsx')
    Copy-Item (Join-Path $root '3ds\meta\icon.smdh') (Join-Path $target 'EllisHouse3DS.smdh')
    Copy-Item (Join-Path $root 'docs\INSTALL_3DS.md') $folder
}

New-ReleaseZip 'EllisHouse-PSP' {
    param($folder)
    $target = Join-Path $folder 'PSP\GAME'
    New-Item -ItemType Directory -Force -Path $target | Out-Null
    Copy-Item (Join-Path $root 'EllisHousePSP') $target -Recurse
    Copy-Item (Join-Path $root 'docs\INSTALL_PSP.md') $folder
}

New-ReleaseZip 'EllisHouse-Covers' {
    param($folder)
    Copy-Item (Join-Path $root 'artwork\*') $folder -Recurse
    Copy-Item (Join-Path $root 'docs\INSTALL_GBA.md') $folder
    Copy-Item (Join-Path $root 'docs\INSTALL_PSP.md') $folder
}

Copy-Item (Join-Path $root 'ellis_house_gba.gba') (Join-Path $dist 'EllisHouse-GBA.gba') -Force
Copy-Item (Join-Path $root 'EllisHouse.cia') (Join-Path $dist 'EllisHouse-3DS.cia') -Force

[IO.File]::WriteAllText((Join-Path $dist 'VERSION.txt'), "$Version`n", [Text.UTF8Encoding]::new($false))
$files = Get-ChildItem -LiteralPath $dist -File |
    Where-Object Name -ne 'SHA256SUMS.txt' |
    Sort-Object Name
$lines = foreach ($file in $files) {
    $hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $file.FullName).Hash.ToLowerInvariant()
    "$hash  $($file.Name)"
}
[IO.File]::WriteAllLines((Join-Path $dist 'SHA256SUMS.txt'), $lines, [Text.UTF8Encoding]::new($false))
Write-Output "Release files created in $dist"
