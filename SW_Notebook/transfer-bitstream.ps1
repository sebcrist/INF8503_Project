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


#TRANSFER .bit AND .hwh
$bitFile = Get-ChildItem -Path "$PSScriptRoot\.." -Filter *.bit -Recurse -File | Select-Object -First 1

$hwhFile = Get-ChildItem -Path "$PSScriptRoot\.." -Filter *.hwh -Recurse -File | Select-Object -First 1

if (-not $bitFile -or -not$hwhFile) {
    Write-Host "Missing .bit or .hwh file." -ForegroundColor Red
    exit 1
}

Write-Host "`nTransferring new files to the FPGA..." -ForegroundColor Cyan

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


#TRANSFER IMAGES

$localImagesDir = "$PSScriptRoot\images"
$remoteImagesDir = "$TargetDir/images"

if (Test-Path $localImagesDir) {
    Write-Host "`nTransferring images directory to the FPGA..." -ForegroundColor Cyan
    
    # Create remote images directory
    ssh -i "$sshKeyPath" ${BoardUser}@${BoardHost} "mkdir -p '$remoteImagesDir'"
    
    # Copy all images over
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



$bitDate = $bitFile.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")
$hwhDate = $hwhFile.LastWriteTime.ToString("yyyy-MM-dd HH:mm:ss")

Write-Host "`nDeployment complete! Files were renamed to ${NewFileName}.bit ($bitDate) and${NewFileName}.hwh ($hwhDate) and transferred to$TargetDir" -ForegroundColor Green