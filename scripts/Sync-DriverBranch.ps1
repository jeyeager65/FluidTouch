[CmdletBinding()]
param(
    [string]$RepoPath = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path,
    [string]$Remote = "origin",
    [string]$MainBranch = "main",
    [string]$DriverBranch = "waveshare-driver-stable",
    [switch]$Merge,
    [switch]$Push,
    [switch]$NoStash
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Invoke-Git {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Args,
        [switch]$Capture
    )

    if ($Capture) {
        $output = & git @Args 2>&1
        if ($LASTEXITCODE -ne 0) {
            throw "git $($Args -join ' ') failed: $output"
        }
        return ($output | Out-String).Trim()
    }

    & git @Args
    if ($LASTEXITCODE -ne 0) {
        throw "git $($Args -join ' ') failed."
    }
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "git executable was not found in PATH."
}

$repoResolved = (Resolve-Path $RepoPath).Path
Push-Location $repoResolved

$stashed = $false
$stashLabel = ""

try {
    $insideRepo = Invoke-Git -Args @("rev-parse", "--is-inside-work-tree") -Capture
    if ($insideRepo -ne "true") {
        throw "RepoPath '$repoResolved' is not a git working tree."
    }

    $currentBranch = Invoke-Git -Args @("branch", "--show-current") -Capture
    if ([string]::IsNullOrWhiteSpace($currentBranch)) {
        throw "Detached HEAD is not supported by this script. Checkout a branch first."
    }

    $statusShort = Invoke-Git -Args @("status", "--porcelain") -Capture
    if (-not $NoStash -and -not [string]::IsNullOrWhiteSpace($statusShort)) {
        $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
        $stashLabel = "auto-sync-$timestamp"
        Write-Host "Stashing local changes as '$stashLabel'..."
        Invoke-Git -Args @("stash", "push", "-u", "-m", $stashLabel)
        $stashed = $true
    }

    Write-Host "Fetching $Remote..."
    Invoke-Git -Args @("fetch", $Remote, "--prune")

    Write-Host "Switching to $MainBranch and fast-forwarding from $Remote/$MainBranch..."
    Invoke-Git -Args @("switch", $MainBranch)
    Invoke-Git -Args @("pull", "--ff-only", $Remote, $MainBranch)

    Write-Host "Switching to driver branch $DriverBranch..."
    Invoke-Git -Args @("switch", $DriverBranch)

    $backupName = "backup/$DriverBranch-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
    Write-Host "Creating safety backup branch: $backupName"
    Invoke-Git -Args @("branch", $backupName)

    if ($Merge) {
        Write-Host "Merging $MainBranch into $DriverBranch..."
        Invoke-Git -Args @("merge", "--no-ff", "--no-edit", $MainBranch)
    }
    else {
        Write-Host "Rebasing $DriverBranch onto $MainBranch..."
        Invoke-Git -Args @("rebase", $MainBranch)
    }

    if ($Push) {
        Write-Host "Pushing $MainBranch to $Remote..."
        Invoke-Git -Args @("push", $Remote, $MainBranch)

        Write-Host "Pushing $DriverBranch to $Remote..."
        if ($Merge) {
            Invoke-Git -Args @("push", $Remote, $DriverBranch)
        }
        else {
            Invoke-Git -Args @("push", "--force-with-lease", $Remote, $DriverBranch)
        }
    }

    Write-Host "Sync complete. Driver branch '$DriverBranch' now includes latest '$MainBranch'."
}
catch {
    Write-Error $_
    Write-Host "If you hit conflicts during rebase/merge, resolve them then continue manually."
    throw
}
finally {
    try {
        if (-not [string]::IsNullOrWhiteSpace($currentBranch)) {
            Invoke-Git -Args @("switch", $currentBranch)
        }

        if ($stashed) {
            Write-Host "Re-applying stashed changes ($stashLabel)..."
            Invoke-Git -Args @("stash", "pop")
        }
    }
    finally {
        Pop-Location
    }
}
