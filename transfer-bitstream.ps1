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

$sshSetupCmd = "mkdir -p ~/.ssh && touch ~/.ssh/authorized_keys && grep -q -F '$pubKey' ~/.ssh/authorized_keys \vert{}\vert{} echo '$pubKey' >> ~/.ssh/authorized_keys && chmod 700 ~/.ssh && chmod 600 ~/.ssh/authorized_keys"

ssh -i "$sshKeyPath" ${BoardUser}@${BoardHost} "$sshSetupCmd"

$bitFile = Get-ChildItem -Path . -Include *.bit -Recurse -File | Select-Object -First 1

$hwhFile = Get-ChildItem -Path . -Include *.hwh -Recurse -File | Select-Object -First 1

if (-not $bitFile -or -not$hwhFile) {
    Write-Host "Missing .bit or .hwh file." -ForegroundColor Red
    exit
}

Write-Host "`nTransferring new files to the FPGA..." -ForegroundColor Cyan

scp -i "$sshKeyPath" $bitFile.FullName "${BoardUser}@${BoardHost}:${TargetDir}/${NewFileName}.bit"

scp -i "$sshKeyPath" $hwhFile.FullName "${BoardUser}@${BoardHost}:${TargetDir}/${NewFileName}.hwh"

Write-Host "`nDeployment complete! Files were renamed to ${NewFileName}.bit and ${NewFileName}.hwh and transferred to$TargetDir" -ForegroundColor Green