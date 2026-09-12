param([switch]$PrepareAssets)
$ErrorActionPreference='Stop'
$port=(Resolve-Path "$PSScriptRoot/..").Path
$workspace=(Resolve-Path "$port/../../..").Path
if (-not $env:PSPDEV) { $env:PSPDEV="$workspace/work/toolchain/pspdev" -replace '\\','/' }
$python=if($env:PYTHON){$env:PYTHON}else{'python'}
$bash=if($env:BASH){$env:BASH}else{'C:/devkitPro/msys2/usr/bin/bash.exe'}
if($PrepareAssets){& $python "$PSScriptRoot/prepare_assets.py";if($LASTEXITCODE){throw 'PSP assets failed'}}
$posixSdk=& 'C:/devkitPro/msys2/usr/bin/cygpath.exe' -u $env:PSPDEV
$env:ELLIS_PSP_BIN="$posixSdk/bin"
Push-Location $port
try {
    # Match proven PSP/GAME homebrew such as the hardware-tested Celeste port:
    # a normal static PSP ELF in DATA.PSP, not a relocatable PRX or ISO binary.
    & $bash -c 'export PATH="$ELLIS_PSP_BIN:$PATH"; make -B -j4 BUILD_PRX=0 ENCRYPT=0'
    if($LASTEXITCODE){throw 'PSP compilation failed'}
    & $python "$PSScriptRoot/package.py"
    if($LASTEXITCODE){throw 'PSP packaging failed'}
} finally {Pop-Location}
