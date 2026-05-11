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

$repoRoot = Get-GitOutput -Arguments @("rev-parse", "--show-toplevel")
if ([string]::IsNullOrWhiteSpace($repoRoot)) {
    exit 0
}

$repoName = Split-Path -Leaf $repoRoot
$branch = Get-GitOutput -Arguments @("rev-parse", "--abbrev-ref", "HEAD")
$commitSha = Get-GitOutput -Arguments @("rev-parse", "--short", "HEAD")
$author = Get-GitOutput -Arguments @("log", "-1", "--pretty=format:%an <%ae>")
$title = Get-GitOutput -Arguments @("log", "-1", "--pretty=format:%s")
$origin = Get-GitOutput -Arguments @("remote", "get-url", "origin")

$messageLines = @(
    "Local git activity",
    "Repo: $repoName",
    "Branch: $branch",
    "Commit: $commitSha",
    "Author: $author",
    "Title: $title",
    "Remote: $origin"
)

switch ($EventName) {
    "post-commit" {
        $messageLines += "Event: post-commit"
    }
    "post-push" {
        $messageLines += "Event: post-push"
        $messageLines += "Refs:"
        $messageLines += Build-PushSummary -Path $PushInfoPath
    }
    default {
        $messageLines += "Event: $EventName"
    }
}

$payload = @{
    username = "Home Automation Git"
    content = ($messageLines -join "`n")
} | ConvertTo-Json -Compress

if ($DryRun) {
    Write-Output $payload
    exit 0
}

$webhookUrl = Get-WebhookUrl
if ([string]::IsNullOrWhiteSpace($webhookUrl)) {
    exit 0
}

try {
    Invoke-RestMethod -Uri $webhookUrl -Method Post -ContentType "application/json" -Body $payload -TimeoutSec 5 | Out-Null
} catch {
    [Console]::Error.WriteLine("discord git hook warning: $($_.Exception.Message)")
}
