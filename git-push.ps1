<#
    git-push.ps1  --  one-click commit & push for this CubeMX project

    FIRST RUN (with remote url):
        powershell -ExecutionPolicy Bypass -File .\git-push.ps1 -RemoteUrl "https://github.com/USER/REPO.git"

    LATER RUNS (remote already configured):
        powershell -ExecutionPolicy Bypass -File .\git-push.ps1

    WITH A CUSTOM MESSAGE:
        powershell -ExecutionPolicy Bypass -File .\git-push.ps1 -Message "fix: kinematics sign"

    What it does:
        1. check git
        2. init repo if needed (keeps remote history, never force-push)
        3. stage everything, but abort if Keil build artifacts sneak in
        4. commit (skipped when nothing changed)
        5. add/verify remote
        6. fetch + merge remote main with --allow-unrelated-histories
        7. push
#>

param(
    [string]$RemoteUrl = "",
    [string]$Message   = ""
)

$ErrorActionPreference = "Stop"
Set-Location -Path $PSScriptRoot

function Step($n, $t) { Write-Host ""; Write-Host "[$n] $t" -ForegroundColor Cyan }
function Ok($t)       { Write-Host "    OK  $t" -ForegroundColor Green }
function Warn($t)     { Write-Host "    !!  $t" -ForegroundColor Yellow }
function Die($t)      { Write-Host "    XX  $t" -ForegroundColor Red; Write-Host ""; Write-Host "STOP." -ForegroundColor Red; exit 1 }

# ---------------------------------------------------------------- 1
Step 1 "checking git"
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Die "git not found. Install Git for Windows: https://git-scm.com/download/win"
}
Ok (& git --version)

# ---------------------------------------------------------------- 2
Step 2 "repository"
if (-not (Test-Path ".git")) {
    & git init | Out-Null
    Ok "initialized new repo"
} else {
    Ok "already a git repo"
}

$uName = (& git config user.name)
if ([string]::IsNullOrWhiteSpace($uName)) {
    Die "user.name not configured. Run: git config --global user.name `"YourName`" ; git config --global user.email `"you@example.com`""
}
Ok ("user = " + $uName)

# ---------------------------------------------------------------- 3
Step 3 "staging files"
& git add -A

$staged = @(& git diff --cached --name-only) | Where-Object { $_ -ne "" }
Ok ("staged: " + $staged.Count + " file(s)")

$bad = $staged | Where-Object {
    ($_ -like "MDK-ARM/Objects/*") -or
    ($_ -like "MDK-ARM/Listings/*") -or
    ($_ -like "*.uvguix.*") -or
    ($_ -like "*.o") -or
    ($_ -like "*.axf")
}
if ($bad) {
    Warn "Keil build artifacts are staged:"
    $bad | Select-Object -First 10 | ForEach-Object { Warn ("    " + $_) }
    Die "run this first:  git rm -r --cached MDK-ARM/Objects MDK-ARM/Listings"
}

# ---------------------------------------------------------------- 4
Step 4 "commit"
if ($staged.Count -eq 0) {
    Ok "nothing to commit"
} else {
    if ([string]::IsNullOrWhiteSpace($Message)) {
        $Message = "update " + (Get-Date -Format "yyyy-MM-dd HH:mm")
    }
    & git commit -m $Message | Out-Null
    if ($LASTEXITCODE -ne 0) { Die "commit failed" }
    Ok ("committed: " + $Message)
}

# ---------------------------------------------------------------- 5
Step 5 "remote"
$origin = ""
try { $origin = (& git remote get-url origin) } catch { $origin = "" }

if ([string]::IsNullOrWhiteSpace($origin)) {
    if ([string]::IsNullOrWhiteSpace($RemoteUrl)) {
        Die "no remote configured. Re-run with -RemoteUrl `"https://github.com/USER/REPO.git`""
    }
    & git remote add origin $RemoteUrl
    if ($LASTEXITCODE -ne 0) { Die "failed to add remote" }
    $origin = $RemoteUrl
}
Ok ("origin = " + $origin)

& git branch -M main

# ---------------------------------------------------------------- 6
Step 6 "merge remote history (keep old versions)"

# self-heal: a previous run may have left an unfinished merge
if (Test-Path ".git/MERGE_HEAD") {
    Warn "unfinished merge detected -> aborting it first"
    & git merge --abort
    Ok "previous merge aborted, working tree restored"
}

& git fetch origin
$hasMain = ""
try { $hasMain = (& git ls-remote --heads origin main) } catch { $hasMain = "" }

if ([string]::IsNullOrWhiteSpace($hasMain)) {
    Ok "remote has no 'main' branch yet"
} else {
    # -X ours: on conflict keep LOCAL file content, but keep BOTH histories.
    # This is what stops "conflict markers inside car.uvprojx / car.ioc".
    & git merge -X ours --no-edit --allow-unrelated-histories origin/main
    if ($LASTEXITCODE -ne 0) {
        Warn "merge stopped with conflicts. Fix files (remove <<<<<<< markers), then:"
        Warn "    git add ."
        Warn "    git commit -m `"merge remote`""
        Warn "    git push -u origin main"
        Die "conflict left unresolved"
    }
    Ok "merged"
}

# ---------------------------------------------------------------- 7
Step 7 "push"
& git push -u origin main
if ($LASTEXITCODE -ne 0) {
    Die "push failed. Use a Personal Access Token (PAT) as password, or use GitHub Desktop."
}
Ok "pushed to GitHub"

Write-Host ""
Write-Host "All done." -ForegroundColor Green
Write-Host ("  repo: " + $origin) -ForegroundColor Green
