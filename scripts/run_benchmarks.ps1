# ============================================================================
# Phase 11 Benchmark Harness (PowerShell)
# MDE-HRA Research Project
# ============================================================================
#
# Usage:
#   Set-Location D:\TS\Seminar\Project
#   powershell -File scripts\run_benchmarks.ps1 [-Reps 10] [-OutputDir experiments\raw]
#
# Defaults:
#   Reps      = 10
#   OutputDir = experiments\raw
#
# The harness:
#   1. Runs each benchmark x each configuration x Reps times
#   2. Writes a single dated CSV file to OutputDir
#   3. Performs per-benchmark correctness gate (A, B, C all succeed)
#   4. Logs progress to experiments\logs\
#
# Correctness invariant enforced:
#   success column == 1 for A, B, and C
#
# ============================================================================
param(
    [int]$Reps = 10,
    [string]$OutputDir = "experiments\raw"
)

$ErrorActionPreference = "Stop"
$Driver      = "build\src\instr-driver.exe"
$BmDir       = "experiments\benchmarks"
$LogDir      = "experiments\logs"
$Timestamp   = Get-Date -Format "yyyy-MM-dd_HHmmss"
$RunCsv      = "$OutputDir\run_${Timestamp}.csv"
$LogFile     = "$LogDir\run_${Timestamp}.log"
$Configs     = @("A", "B", "C")
$Benchmarks  = @(
    "bm01_simple_alloc.c",
    "bm02_linked_list_build.c",
    "bm03_dead_pointers.c",
    "bm04_aliasing.c",
    "bm05_branch_alloc.c",
    "bm06_loop_list.c",
    "bm07_multi_field_graph.c",
    "bm08_genuine_loop.c"
)

# Ensure directories exist
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null
New-Item -ItemType Directory -Force -Path $LogDir    | Out-Null

function Log($msg) {
    $line = "[$(Get-Date -Format 'HH:mm:ss')] $msg"
    Write-Host $line
    Add-Content -Path $LogFile -Value $line
}

# Verify driver exists
if (-not (Test-Path $Driver)) {
    Write-Error "Cannot find driver: $Driver"
    exit 1
}

# Write CSV header
$headerLine = & $Driver HEADER 2>&1
$headerLine | Set-Content $RunCsv

Log "========================================================"
Log "Phase 11 Benchmark Campaign"
Log "Timestamp  : $Timestamp"
Log "Repetitions: $Reps"
Log "Output CSV : $RunCsv"
Log "Log file   : $LogFile"
Log "========================================================"

$PassCount = 0
$FailCount = 0

foreach ($BmFile in $Benchmarks) {
    $BmPath = "$BmDir\$BmFile"
    $BmName = [System.IO.Path]::GetFileNameWithoutExtension($BmFile)

    if (-not (Test-Path $BmPath)) {
        Log "SKIP: $BmFile not found at $BmPath"
        continue
    }

    Log "----"
    Log "Benchmark: $BmName"

    # ---- Correctness Gate ----
    $allOk = $true
    foreach ($cfg in $Configs) {
        $out = & $Driver $BmPath $cfg "check" 2>&1
        $outStr = $out -join ""
        $fields = $outStr.Split(",")
        if ($fields.Length -lt 4 -or $fields[3].Trim() -ne "1") {
            Log "  FAIL: $BmName x $cfg : driver returned non-success: $outStr"
            $allOk = $false
        }
    }

    if (-not $allOk) {
        Log "  EXCLUDED: correctness gate failed for $BmName"
        $FailCount++
        continue
    }

    Log "  Correctness gate: A OK  B OK  C OK"

    # ---- Repetition Runs ----
    for ($run = 1; $run -le $Reps; $run++) {
        foreach ($cfg in $Configs) {
            $line = & $Driver $BmPath $cfg "$run" 2>$null
            if ($line) {
                Add-Content -Path $RunCsv -Value $line
            }
        }
    }

    Log "  Runs complete: $Reps repetitions x 3 configs = $($Reps * 3) rows"
    $PassCount++
}

Log "========================================================"
Log "Campaign complete"
Log "Benchmarks accepted : $PassCount"
Log "Benchmarks failed   : $FailCount"
Log "Raw data written to : $RunCsv"
Log "========================================================"
Write-Host ""
Write-Host "CSV: $RunCsv"
Write-Host "Log: $LogFile"
