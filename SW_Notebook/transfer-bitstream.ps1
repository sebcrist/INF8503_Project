param (
    [string]$BoardHost = "pynq",
    [string]$BoardUser = "xilinx",
    [string]$BoardPass = "xilinx",
    [string]$TargetDir = "/home/xilinx/jupyter_notebooks/PynqZ2-ORB-Accelerator",
    [string]$NewFileName = "VivadoHW"
)

$sshKeyPath = "$env:USERPROFILE\.ssh\id_ed25519"

if (-not (Test-Path $sshKeyPath)) {
    ssh-keygen -t ed25519 -f $sshKeyPath -N '""' | Out-Null
}

$pubKey = (Get-Content "$sshKeyPath.pub" -Raw).Trim() -replace "`r", "" -replace "`n", ""

Write-Host "`n[NOTE] Verifying passwordless trust. Type 'xilinx' if prompted:" -ForegroundColor Yellow

$sshSetupCmd = "mkdir -p ~/.ssh && touch ~/.ssh/authorized_keys && if ! grep -q -F -- '$pubKey' ~/.ssh/authorized_keys; then echo '$pubKey' >> ~/.ssh/authorized_keys; fi && chmod 700 ~/.ssh && chmod 600 ~/.ssh/authorized_keys"

ssh -i "$sshKeyPath" ${BoardUser}@${BoardHost} "$sshSetupCmd"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Unable to configure passwordless SSH access." -ForegroundColor Red
    exit 1
}

# --- 1. TRANSFER .bit AND .hwh ---
$bitFile = Get-ChildItem -Path "$PSScriptRoot\.." -Filter *.bit -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$hwhFile = Get-ChildItem -Path "$PSScriptRoot\.." -Filter *.hwh -Recurse -File | Sort-Object LastWriteTime -Descending | Select-Object -First 1

if (-not $bitFile -or -not $hwhFile) {
    Write-Host "Missing .bit or .hwh file." -ForegroundColor Red
    exit 1
}

Write-Host "`n[1/4] Transferring bitstream and HWH to the FPGA..." -ForegroundColor Cyan

ssh -i "$sshKeyPath" ${BoardUser}@${BoardHost} "mkdir -p '$TargetDir'"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Unable to create the target directory: $TargetDir" -ForegroundColor Red
    exit 1
}

scp -i "$sshKeyPath" $bitFile.FullName "${BoardUser}@${BoardHost}:${TargetDir}/${NewFileName}.bit"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to transfer $($bitFile.FullName)." -ForegroundColor Red
    exit 1
}

scp -i "$sshKeyPath" $hwhFile.FullName "${BoardUser}@${BoardHost}:${TargetDir}/${NewFileName}.hwh"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to transfer $($hwhFile.FullName)." -ForegroundColor Red
    exit 1
}

# --- 2. TRANSFER IMAGES ---
$localImagesDir = "$PSScriptRoot\images"
$remoteImagesDir = "$TargetDir/images"

if (Test-Path $localImagesDir) {
    Write-Host "`n[2/4] Transferring images directory to the FPGA..." -ForegroundColor Cyan
    ssh -i "$sshKeyPath" ${BoardUser}@${BoardHost} "mkdir -p '$remoteImagesDir'"
    
    Get-ChildItem -Path $localImagesDir -File | ForEach-Object {
        scp -i "$sshKeyPath" $_.FullName "${BoardUser}@${BoardHost}:${remoteImagesDir}/$($_.Name)"
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Failed to transfer image: $($_.Name)" -ForegroundColor Red
        } else {
            Write-Host "Transferred: $($_.Name)" -ForegroundColor Green
        }
    }
} else {
    Write-Host "`n[WARNING] No 'images' folder found at $localImagesDir. Skipping image transfer." -ForegroundColor Yellow
}

Write-Host "`n[3/4] Transferring source code and scripts..." -ForegroundColor Cyan

$sourceFiles = Get-ChildItem -Path $PSScriptRoot -Include "*.cpp", "*.h", "*.hpp", "*.py" -Recurse -File
$filesToCopy = [System.Collections.Generic.List[string]]::new()
foreach ($f in $sourceFiles) {
    $filesToCopy.Add($f.FullName)
}

if ($filesToCopy.Count -gt 0) {
    scp -i "$sshKeyPath" $filesToCopy "${BoardUser}@${BoardHost}:${TargetDir}/"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to transfer source files." -ForegroundColor Red
        exit 1
    }
    Write-Host "Transferred $($filesToCopy.Count) source file(s) successfully." -ForegroundColor Green
} else {
    Write-Host "No source files found to transfer." -ForegroundColor Yellow
}

# --- 4. COMPILE C++ ON PYNQ --- # changed
Write-Host "`n[4/4] Compiling C++ module on the PYNQ-Z2..." -ForegroundColor Cyan # changed

$compileScript = @"
set -e
cd '$TargetDir'

if [ -f "orb_pipeline.cpp" ]; then
    echo 'Compiling orb_pipeline.cpp -> liborb_pipeline.so...' # changed
    OCV_FLAGS=`$(pkg-config --cflags --libs opencv4 2>/dev/null || pkg-config --cflags --libs opencv)
    g++ -O3 -shared -fPIC -Wno-psabi -mcpu=cortex-a9 -mfpu=neon -fopenmp orb_pipeline.cpp `$OCV_FLAGS -o liborb_pipeline.so
fi

echo 'Compilation completed successfully!' # changed
"@

$remoteCompileScript = ($compileScript -replace "`r`n", "`n")
$remoteCompileScript | ssh -i "$sshKeyPath" "${BoardUser}@${BoardHost}" "tr -d '\r' | bash -se"

if ($LASTEXITCODE -ne 0) {
    Write-Host "C++ compilation failed on the PYNQ-Z2." -ForegroundColor Red
    exit 1
}

$bitDate = $bitFile.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
$hwhDate = $hwhFile.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")

Write-Host "`nDeployment & Compilation complete!" -ForegroundColor Green
Write-Host " - Bitstream/HWH renamed to: ${NewFileName}.bit / ${NewFileName}.hwh" -ForegroundColor Green
Write-Host " - Target directory: $TargetDir" -ForegroundColor Green