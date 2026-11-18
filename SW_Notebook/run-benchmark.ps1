param (
    [string]$BoardHost = "pynq",
    [string]$BoardUser = "xilinx",
    [string]$BoardPass = "xilinx",
    [string]$TargetDir = "/home/xilinx/jupyter_notebooks/PynqZ2-ORB-Accelerator"
)

$sshKeyPath = "$env:USERPROFILE\.ssh\id_ed25519"

if (-not (Test-Path $sshKeyPath)) {
    ssh-keygen -t ed25519 -f $sshKeyPath -N '""' | Out-Null
}

# Verify passwordless SSH (only prompt if not yet configured)
ssh -i "$sshKeyPath" -o BatchMode=yes -o ConnectTimeout=5 "${BoardUser}@${BoardHost}" "exit" 2>$null
if ($LASTEXITCODE -ne 0) {
    $pubKey = (Get-Content "$sshKeyPath.pub" -Raw).Trim() -replace "`r", "" -replace "`n", ""
    Write-Host "`n[NOTE] Configuring passwordless trust. Type '$BoardPass' if prompted:" -ForegroundColor Yellow

    $sshSetupCmd = "mkdir -p ~/.ssh && touch ~/.ssh/authorized_keys && if ! grep -q -F -- '$pubKey' ~/.ssh/authorized_keys; then echo '$pubKey' >> ~/.ssh/authorized_keys; fi && chmod 700 ~/.ssh && chmod 600 ~/.ssh/authorized_keys"
    ssh -i "$sshKeyPath" "${BoardUser}@${BoardHost}" "$sshSetupCmd"
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Unable to configure passwordless SSH access." -ForegroundColor Red
        exit 1
    }
}

# --- 1. VERIFY LOCAL FILES ---
$pyFile = Join-Path $PSScriptRoot "benchmark.py"
if (-not (Test-Path $pyFile)) {
    Write-Host "Missing benchmark.py in $PSScriptRoot." -ForegroundColor Red
    exit 1
}

$cppFiles = Get-ChildItem -Path $PSScriptRoot -Filter "*.cpp" -File
if ($cppFiles.Count -eq 0) {
    Write-Host "No .cpp files found in $PSScriptRoot." -ForegroundColor Red
    exit 1
}
$hdrFiles = Get-ChildItem -Path "$PSScriptRoot\*" -Include "*.h", "*.hpp" -File

# --- 2. TRANSFER FILES TO PYNQ ---
Write-Host "`n[1/3] Transferring benchmark.py and C++ source files to ${BoardHost}:${TargetDir}..." -ForegroundColor Cyan

ssh -i "$sshKeyPath" "${BoardUser}@${BoardHost}" "mkdir -p '$TargetDir'"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Unable to create target directory: $TargetDir" -ForegroundColor Red
    exit 1
}

$filesToCopy = [System.Collections.Generic.List[string]]::new()
$filesToCopy.Add($pyFile)
foreach ($f in $cppFiles) { $filesToCopy.Add($f.FullName) }
foreach ($f in $hdrFiles) { $filesToCopy.Add($f.FullName) }

scp -i "$sshKeyPath" $filesToCopy "${BoardUser}@${BoardHost}:${TargetDir}/"
if ($LASTEXITCODE -ne 0) {
    Write-Host "Failed to transfer files to the board." -ForegroundColor Red
    exit 1
}
Write-Host "Transferred $($filesToCopy.Count) file(s) successfully." -ForegroundColor Green

# --- 3. COMPILE C++ ON PYNQ ---
Write-Host "`n[2/3] Compiling C++ files on the PYNQ-Z2..." -ForegroundColor Cyan

$compileScript = @"
set -e
cd '$TargetDir'

echo '1/3 Compiling fpga_bench.cpp -> libfpga_bench.so...'
g++ -O3 -shared -fPIC -Wno-psabi -mcpu=cortex-a9 -mfpu=neon -fopenmp fpga_bench.cpp -o libfpga_bench.so

if [ -f "opencv_bench.cpp" ]; then
    echo '2/3 Compiling opencv_bench.cpp -> opencv_bench...'
    OCV_FLAGS=`$(pkg-config --cflags --libs opencv4 2>/dev/null || pkg-config --cflags --libs opencv)
    g++ -O3 -Wno-psabi -mcpu=cortex-a9 -mfpu=neon -fopenmp opencv_bench.cpp $OCV_FLAGS -o opencv_bench
fi

if [ -f "orb_pipeline.cpp" ]; then
    echo '3/3 Compiling orb_pipeline.cpp -> liborb_pipeline.so...'
    OCV_FLAGS=`$(pkg-config --cflags --libs opencv4 2>/dev/null || pkg-config --cflags --libs opencv)
    g++ -O3 -shared -fPIC -Wno-psabi -mcpu=cortex-a9 -mfpu=neon -fopenmp orb_pipeline.cpp `$OCV_FLAGS -o liborb_pipeline.so
fi

echo 'All compilations completed successfully!'
"@

$remoteCompileScript = ($compileScript -replace "`r`n", "`n")
$remoteCompileScript | ssh -i "$sshKeyPath" "${BoardUser}@${BoardHost}" "tr -d '\r' | bash -se"
if ($LASTEXITCODE -ne 0) {
    Write-Host "C++ compilation failed on the PYNQ-Z2." -ForegroundColor Red
    exit 1
}
Write-Host "Compilation succeeded!" -ForegroundColor Green

# --- 4. RUN BENCHMARK ---
Write-Host "`n[3/3] Running benchmark.py on the PYNQ-Z2..." -ForegroundColor Cyan

$runCmd = "bash -lc 'cd $TargetDir && PY=`$(command -v python3) && (sudo -n true 2>/dev/null || echo $BoardPass | sudo -S -v) && sudo -E `$PY -u benchmark.py && if [ -x ./opencv_bench ]; then ./opencv_bench images/coke_ref.jpg images/park_and_coke.jpg 50 5; fi'"
ssh -t -i "$sshKeyPath" "${BoardUser}@${BoardHost}" "$runCmd"
if ($LASTEXITCODE -ne 0) {
    Write-Host "`nBenchmark exited with error code $LASTEXITCODE." -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "`nBenchmark completed successfully!" -ForegroundColor Green