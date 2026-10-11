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

$bitFile = Get-ChildItem -Path $PSScriptRoot -Filter *.bit -Recurse -File | Select-Object -First 1

$hwhFile = Get-ChildItem -Path $PSScriptRoot -Filter *.hwh -Recurse -File | Select-Object -First 1

if (-not $bitFile -or -not $hwhFile) {
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

Write-Host "`nDeployment complete! Files were renamed to ${NewFileName}.bit and ${NewFileName}.hwh and transferred to $TargetDir" -ForegroundColor Green