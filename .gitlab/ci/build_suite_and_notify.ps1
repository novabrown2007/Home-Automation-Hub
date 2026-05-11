Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$DiscordWebhookUrl = "https://discord.com/api/webhooks/1503406500931047567/zOWPIqDS1AZyr5MTbS9ikGVNRz2k1dmZkbg10XA97P7TcmqtsJZk_BHoIdLIQpvEYXnQ"

function Resolve-Python {
    $candidates = @(
        @{ FilePath = "py"; Arguments = @("-3.13") },
        @{ FilePath = "py"; Arguments = @("-3") },
        @{ FilePath = "python"; Arguments = @() }
    )

    foreach ($candidate in $candidates) {
        try {
            & $candidate.FilePath @($candidate.Arguments + @("--version")) *> $null
            return $candidate
        } catch {
        }
    }

    throw "Python 3 was not found on PATH."
}

function Ensure-RepoCheckout {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectPath,
        [Parameter(Mandatory = $true)][string]$RepoUrl,
        [Parameter(Mandatory = $true)][string]$Branch,
        [Parameter(Mandatory = $true)][string]$DirectoryName,
        [Parameter(Mandatory = $true)][string]$WorkspaceRoot,
        [Parameter(Mandatory = $true)][string]$CurrentProjectPath,
        [Parameter(Mandatory = $true)][string]$CurrentProjectDir
    )

    if ($CurrentProjectPath -eq $ProjectPath) {
        return $CurrentProjectDir
    }

    $targetPath = Join-Path $WorkspaceRoot $DirectoryName
    if (Test-Path $targetPath) {
        Remove-Item -LiteralPath $targetPath -Recurse -Force
    }

    git clone --depth 1 --branch $Branch $RepoUrl $targetPath | Out-Host
    return $targetPath
}

function New-DiscordPayload {
    param(
        [Parameter(Mandatory = $true)][string]$BridgeExe,
        [Parameter(Mandatory = $true)][string]$HubExe,
        [Parameter(Mandatory = $true)][string]$UiExe
    )

    $lines = @(
        "Home Automation Suite build",
        "Repo: $env:CI_PROJECT_PATH",
        "Branch: $env:CI_COMMIT_REF_NAME",
        "Commit: $env:CI_COMMIT_SHORT_SHA",
        "Author: $env:CI_COMMIT_AUTHOR",
        "Source: $env:CI_PIPELINE_SOURCE",
        "Title: $env:CI_COMMIT_TITLE",
        "Pipeline: $env:CI_PIPELINE_URL",
        "Bridge: $(Split-Path -Leaf $BridgeExe)",
        "Hub: $(Split-Path -Leaf $HubExe)",
        "UI: $(Split-Path -Leaf $UiExe)"
    )

    return @{
        username = "Home Automation CI"
        content = ($lines -join "`n")
    } | ConvertTo-Json -Compress
}

$workspaceRoot = Split-Path -Parent $env:CI_PROJECT_DIR
$currentProjectPath = $env:CI_PROJECT_PATH
$currentProjectDir = $env:CI_PROJECT_DIR

$bridgePath = Ensure-RepoCheckout `
    -ProjectPath $env:BRIDGE_PROJECT_PATH `
    -RepoUrl $env:BRIDGE_REPO_URL `
    -Branch $env:BRIDGE_REPO_BRANCH `
    -DirectoryName $env:BRIDGE_REPO_DIR `
    -WorkspaceRoot $workspaceRoot `
    -CurrentProjectPath $currentProjectPath `
    -CurrentProjectDir $currentProjectDir

$hubPath = Ensure-RepoCheckout `
    -ProjectPath $env:HUB_PROJECT_PATH `
    -RepoUrl $env:HUB_REPO_URL `
    -Branch $env:HUB_REPO_BRANCH `
    -DirectoryName $env:HUB_REPO_DIR `
    -WorkspaceRoot $workspaceRoot `
    -CurrentProjectPath $currentProjectPath `
    -CurrentProjectDir $currentProjectDir

$uiPath = Ensure-RepoCheckout `
    -ProjectPath $env:UI_PROJECT_PATH `
    -RepoUrl $env:UI_REPO_URL `
    -Branch $env:UI_REPO_BRANCH `
    -DirectoryName $env:UI_REPO_DIR `
    -WorkspaceRoot $workspaceRoot `
    -CurrentProjectPath $currentProjectPath `
    -CurrentProjectDir $currentProjectDir

Push-Location $bridgePath
try {
    powershell -ExecutionPolicy Bypass -File ".\tools\integration_smoketest.ps1" | Out-Host
} finally {
    Pop-Location
}

$python = Resolve-Python
Push-Location $uiPath
try {
    & $python.FilePath @($python.Arguments + @("-m", "pip", "install", "--upgrade", "pip", "pyinstaller")) | Out-Host
    if (Test-Path "dist-ci") { Remove-Item -LiteralPath "dist-ci" -Recurse -Force }
    if (Test-Path "build-ci") { Remove-Item -LiteralPath "build-ci" -Recurse -Force }
    & $python.FilePath @(
        $python.Arguments + @(
            "-m", "PyInstaller",
            "--noconfirm",
            "--clean",
            "--onedir",
            "--name", "HomeAutomationUI",
            "--distpath", "dist-ci",
            "--workpath", "build-ci",
            "--specpath", "build-ci",
            "main.py"
        )
    ) | Out-Host
} finally {
    Pop-Location
}

$bridgeExe = Join-Path $bridgePath "cmake-build-mingw\HomeAutomationBridge.exe"
$hubExe = Join-Path $hubPath "cmake-build-mingw\Home_Automation_Hub.exe"
$uiExe = Join-Path $uiPath "dist-ci\HomeAutomationUI\HomeAutomationUI.exe"

foreach ($path in @($bridgeExe, $hubExe, $uiExe)) {
    if (-not (Test-Path $path)) {
        throw "Expected build output not found: $path"
    }
}

$artifactRoot = Join-Path $env:CI_PROJECT_DIR "suite-artifacts"
New-Item -ItemType Directory -Force -Path $artifactRoot | Out-Null
Copy-Item -LiteralPath $bridgeExe -Destination (Join-Path $artifactRoot "HomeAutomationBridge.exe") -Force
Copy-Item -LiteralPath $hubExe -Destination (Join-Path $artifactRoot "Home_Automation_Hub.exe") -Force
Copy-Item -LiteralPath $uiExe -Destination (Join-Path $artifactRoot "HomeAutomationUI.exe") -Force

$payload = New-DiscordPayload -BridgeExe $bridgeExe -HubExe $hubExe -UiExe $uiExe

& curl.exe `
    -sS `
    -X POST `
    -F "payload_json=$payload" `
    -F "file1=@$bridgeExe;filename=HomeAutomationBridge.exe" `
    -F "file2=@$hubExe;filename=Home_Automation_Hub.exe" `
    -F "file3=@$uiExe;filename=HomeAutomationUI.exe" `
    $DiscordWebhookUrl | Out-Host
