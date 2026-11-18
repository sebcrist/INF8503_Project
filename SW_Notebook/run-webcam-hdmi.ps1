param (
    [string]$BoardHost = "pynq",
    [string]$BoardUser = "xilinx",
    [string]$BoardPass = "xilinx",
    [string]$TargetDir = "/home/xilinx/jupyter_notebooks/PynqZ2-ORB-Accelerator",
    [int]$CamIndex = 0,
    [int]$FastThreshold = 60,
    [int]$HdmiFastThreshold = 35,
    [int]$HammingThreshold = 56,
    [int]$MinMatches = 20,
    [double]$MinRatio = 0.07,
    [double]$MinMargin = 1.45,
    [double]$ReportHz = 4.0,
    [switch]$Preview,
    [string[]]$Refs = @()
)

$pyScript = Join-Path $PSScriptRoot "webcam_hdmi_pc.py"
if (-not (Test-Path $pyScript)) {
    Write-Host "Missing webcam_hdmi_pc.py in $PSScriptRoot." -ForegroundColor Red
    exit 1
}

$argsList = @(
    $pyScript,
    "--host", $BoardHost,
    "--user", $BoardUser,
    "--password", $BoardPass,
    "--target-dir", $TargetDir,
    "--cam", $CamIndex,
    "--fast-thresh", $FastThreshold,
    "--hdmi-fast-thresh", $HdmiFastThreshold,
    "--hamming-thresh", $HammingThreshold,
    "--min-matches", $MinMatches,
    "--min-ratio", $MinRatio,
    "--min-margin", $MinMargin,
    "--report-hz", $ReportHz
)

if ($Preview) {
    $argsList += "--preview"
}

if ($Refs.Count -gt 0) {
    $argsList += "--refs"
    $argsList += $Refs
}

python @argsList
exit $LASTEXITCODE
