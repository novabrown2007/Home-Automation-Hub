param(
    [Parameter(Mandatory = $true)][string]$EventName,
    [string]$PushInfoPath = "",
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$DiscordWebhookUrl = "https://discord.com/api/webhooks/1503406500931047567/zOWPIqDS1AZyr5MTbS9ikGVNRz2k1dmZkbg10XA97P7TcmqtsJZk_BHoIdLIQpvEYXnQ"

function Get-GitOutput {
    param([Parameter(Mandatory = $true)][string[]]$Arguments)
    $result = & git @Arguments 2>$null
    if ($LASTEXITCODE -ne 0) {
        return ""
    }
    return ($result | Out-String).Trim()
}

function Get-WebhookUrl {
    return $DiscordWebhookUrl
}

function Build-PushSummary {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return "No ref updates captured."
    }

    $lines = Get-Content $Path | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    if ($lines.Count -eq 0) {
        return "No ref updates captured."
    }

    $summary = foreach ($line in $lines) {
        $parts = $line -split '\s+'
        if ($parts.Count -lt 4) {
            continue
        }
        $localRef = $parts[0]
        $localSha = $parts[1]
        $remoteRef = $parts[2]
        $remoteSha = $parts[3]
        "$localRef ($($localSha.Substring(0, [Math]::Min(8, $localSha.Length)))) -> $remoteRef ($($remoteSha.Substring(0, [Math]::Min(8, $remoteSha.Length))))"
    }

    if (-not $summary) {
        return "No ref updates captured."
    }

    return ($summary -join "`n")
}

function Get-PushedCommitSummary {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path $Path)) {
        return "No pushed commits captured."
    }

    $summaries = New-Object System.Collections.Generic.List[string]
    $lines = Get-Content $Path | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    foreach ($line in $lines) {
        $parts = $line -split '\s+'
        if ($parts.Count -lt 4) {
            continue
        }

        $localRef = $parts[0]
        $localSha = $parts[1]
        $remoteRef = $parts[2]
        $remoteSha = $parts[3]

        if ($localSha -match '^0+$') {
            continue
        }

        $commitLines = @()
        if ($remoteSha -match '^0+$') {
            $single = Get-GitOutput -Arguments @("log", "-1", "--pretty=format:%h %s", $localSha)
            if (-not [string]::IsNullOrWhiteSpace($single)) {
                $commitLines += $single
            }
        } else {
            $rangeLines = & git log --reverse --pretty=format:"%h %s" "$remoteSha..$localSha" 2>$null
            if ($LASTEXITCODE -eq 0 -and $rangeLines) {
                $commitLines += $rangeLines
            }
        }

        if ($commitLines.Count -eq 0) {
            $fallback = Get-GitOutput -Arguments @("log", "-1", "--pretty=format:%h %s", $localSha)
            if (-not [string]::IsNullOrWhiteSpace($fallback)) {
                $commitLines += $fallback
            }
        }

        if ($commitLines.Count -gt 0) {
            $summaries.Add("$localRef -> $remoteRef")
            foreach ($commitLine in $commitLines) {
                $summaries.Add("- $commitLine")
            }
        }
    }

    if ($summaries.Count -eq 0) {
        return "No pushed commits captured."
    }

    return ($summaries -join "`n")
}

function Build-DiscordPayload {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectName,
        [Parameter(Mandatory = $true)][string]$ActionName,
        [Parameter(Mandatory = $true)][string]$ActionDetail
    )

    return @{
        content = $ProjectName
        embeds = @(
            @{
                title = "Gitl CI"
                fields = @(
                    @{
                        name = $ActionName
                        value = $ActionDetail
                    }
                )
            }
        )
    } | ConvertTo-Json -Compress -Depth 6
}

function Get-ExePath {
    param([string]$RepoRoot)

    $repoName = Split-Path -Leaf $RepoRoot
    switch ($repoName) {
        "Home Automation Bridge" {
            return Join-Path $RepoRoot "cmake-build-mingw\HomeAutomationBridge.exe"
        }
        "Home Automation Hub" {
            return Join-Path $RepoRoot "cmake-build-mingw\Home_Automation_Hub.exe"
        }
        "Home Automation UI" {
            return Join-Path $RepoRoot "dist-push\HomeAutomationUI\HomeAutomationUI.exe"
        }
        default {
            throw "Unsupported repo for exe resolution: $repoName"
        }
    }
}

function Build-ProjectExe {
    param(
        [Parameter(Mandatory = $true)][string]$RepoRoot,
        [Parameter(Mandatory = $true)][string]$RepoName
    )

    switch ($RepoName) {
        "Home Automation Bridge" {
            Push-Location $RepoRoot
            try {
                cmake --build --preset mingw-debug | Out-Host
            } finally {
                Pop-Location
            }
            return
        }
        "Home Automation Hub" {
            Push-Location $RepoRoot
            try {
                cmake --build --preset mingw-debug | Out-Host
            } finally {
                Pop-Location
            }
            return
        }
        "Home Automation UI" {
            $python = Resolve-Python
            Push-Location $RepoRoot
            try {
                & $python.FilePath @($python.Arguments + @("-m", "pip", "install", "--upgrade", "pip", "pyinstaller")) | Out-Host
                if (Test-Path "dist-push") { Remove-Item -LiteralPath "dist-push" -Recurse -Force }
                if (Test-Path "build-push") { Remove-Item -LiteralPath "build-push" -Recurse -Force }
                & $python.FilePath @(
                    $python.Arguments + @(
                        "-m", "PyInstaller",
                        "--noconfirm",
                        "--clean",
                        "--onedir",
                        "--name", "HomeAutomationUI",
                        "--distpath", "dist-push",
                        "--workpath", "build-push",
                        "--specpath", "build-push",
                        "main.py"
                    )
                ) | Out-Host
            } finally {
                Pop-Location
            }
            return
        }
        default {
            throw "Unsupported repo for build: $RepoName"
        }
    }
}

$repoRoot = Get-GitOutput -Arguments @("rev-parse", "--show-toplevel")
if ([string]::IsNullOrWhiteSpace($repoRoot)) {
    exit 0
}

$repoName = Split-Path -Leaf $repoRoot
$branch = Get-GitOutput -Arguments @("rev-parse", "--abbrev-ref", "HEAD")
$commitSha = Get-GitOutput -Arguments @("rev-parse", "--short", "HEAD")
$author = Get-GitOutput -Arguments @("log", "-1", "--pretty=format:%an <%ae>")
$title = Get-GitOutput -Arguments @("log", "-1", "--pretty=format:%s")

$actionName = $EventName
$actionDetailLines = @()

switch ($EventName) {
    "post-commit" {
        $actionName = "Commit"
        $actionDetailLines = @(
            "Commit message: $title",
            "Author: $author"
        )
    }
    "post-push" {
        $actionName = "Push"
        $actionDetailLines = @(
            "Branch: $branch",
            "Commits:",
            (Get-PushedCommitSummary -Path $PushInfoPath)
        )
    }
    default {
        $actionDetailLines = @(
            "Commit message: $title",
            "Author: $author"
        )
    }
}

$payload = Build-DiscordPayload -ProjectName $repoName -ActionName $actionName -ActionDetail ($actionDetailLines -join "`n")

if ($DryRun) {
    Write-Output $payload
    exit 0
}

$webhookUrl = Get-WebhookUrl
if ([string]::IsNullOrWhiteSpace($webhookUrl)) {
    exit 0
}

try {
    if ($EventName -eq "post-push") {
        Build-ProjectExe -RepoRoot $repoRoot -RepoName $repoName
        $exePath = Get-ExePath -RepoRoot $repoRoot
        if (-not (Test-Path $exePath)) {
            throw "Expected exe not found: $exePath"
        }

        & curl.exe `
            -sS `
            -X POST `
            -F "payload_json=$payload" `
            -F "file1=@$exePath;filename=$(Split-Path -Leaf $exePath)" `
            $webhookUrl | Out-Host
    } else {
        Invoke-RestMethod -Uri $webhookUrl -Method Post -ContentType "application/json" -Body $payload -TimeoutSec 5 | Out-Null
    }
} catch {
    [Console]::Error.WriteLine("discord git hook warning: $($_.Exception.Message)")
}
