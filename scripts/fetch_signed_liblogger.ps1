[CmdletBinding()]
param(
    [string]$DestinationDirectory = (Join-Path (Join-Path $PSScriptRoot '..') 'out\prebuilt-liblogger'),
    [string]$Owner = 'jojoe77777',
    [string]$Repository = 'Toolscreen',
    [string]$ReleaseTag = 'liblogger-signed-latest'
)

$ErrorActionPreference = 'Stop'

$assetNames = @(
    'liblogger_x64.dll',
    'liblogger_x64.pdb'
)

$headers = @{
    'User-Agent' = 'Toolscreen-build'
    'X-GitHub-Api-Version' = '2022-11-28'
}

$token = $env:GITHUB_TOKEN
if (-not $token) {
    $token = $env:GH_TOKEN
}

if ($token) {
    $headers['Authorization'] = "Bearer $token"
}

$releaseUrl = "https://api.github.com/repos/$Owner/$Repository/releases/tags/$ReleaseTag"

try {
    $release = Invoke-RestMethod -Uri $releaseUrl -Headers $headers
} catch {
    throw "Failed to resolve the '$ReleaseTag' release for $Owner/$Repository. Run the manual 'Build Liblogger' workflow first. $($_.Exception.Message)"
}

New-Item -ItemType Directory -Force -Path $DestinationDirectory | Out-Null

foreach ($assetName in $assetNames) {
    $asset = $release.assets | Where-Object { $_.name -eq $assetName } | Select-Object -First 1

    if (-not $asset) {
        throw "Release '$ReleaseTag' is missing required asset '$assetName'."
    }

    $destinationPath = Join-Path $DestinationDirectory $assetName
    Invoke-WebRequest -Uri $asset.browser_download_url -Headers $headers -OutFile $destinationPath
}

Write-Host "Downloaded signed liblogger assets to $DestinationDirectory"