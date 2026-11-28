param (
    [string]$BoardHost = "pynq",
    [string]$BoardUser = "xilinx",
    [string]$BoardPass = "xilinx",
    [string]$TargetDir = "/home/xilinx/jupyter_notebooks/PynqZ2-ORB-Accelerator"
)

$sshKeyPath = "$env:USERPROFILE\.ssh\id_ed25519"

Write-Host "[1/4] Preparation des images 720p..." -ForegroundColor Cyan
python (Join-Path $PSScriptRoot "plot_eval.py") --prep
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[2/4] Transfert de eval_accuracy.py et eval_720p vers la PYNQ-Z2..." -ForegroundColor Cyan
ssh -i "$sshKeyPath" "${BoardUser}@${BoardHost}" "rm -rf '$TargetDir/eval_720p' '$TargetDir/eval_results' && mkdir -p '$TargetDir'"
scp -i "$sshKeyPath" (Join-Path $PSScriptRoot "eval_accuracy.py") "${BoardUser}@${BoardHost}:${TargetDir}/"
scp -i "$sshKeyPath" -r (Join-Path $PSScriptRoot "eval_720p") "${BoardUser}@${BoardHost}:${TargetDir}/"
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[3/4] Execution de eval_accuracy.py sur la PYNQ-Z2..." -ForegroundColor Cyan
$runCmd = "bash -lc 'cd $TargetDir && PY=`$(command -v python3) && (sudo -n true 2>/dev/null || echo $BoardPass | sudo -S -v) && sudo -E `$PY -u eval_accuracy.py'"
ssh -t -i "$sshKeyPath" "${BoardUser}@${BoardHost}" "$runCmd"
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "[4/4] Rapatriement des resultats et generation des graphiques..." -ForegroundColor Cyan
$localOut = Join-Path $PSScriptRoot "eval_results"
if (Test-Path $localOut) { Remove-Item -Recurse -Force $localOut }
scp -i "$sshKeyPath" -r "${BoardUser}@${BoardHost}:${TargetDir}/eval_results" "$PSScriptRoot\"
if ($LASTEXITCODE -ne 0) { exit 1 }

python (Join-Path $PSScriptRoot "plot_eval.py")
if ($LASTEXITCODE -ne 0) { exit 1 }

Write-Host "Evaluation terminee ! Resultats et figures dans $localOut" -ForegroundColor Green
