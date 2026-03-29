param(
    [Parameter(Mandatory = $true)]
    [string]$ArtifactDirectory
)

$ErrorActionPreference = 'Stop'

function Normalize-VersionString {
    param(
        [string]$Value
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        return $null
    }

    $parts = [regex]::Matches($Value, '\d+') | ForEach-Object { $_.Value }
    if ($parts.Count -eq 0) {
        return $null
    }

    while ($parts.Count -gt 3 -and $parts[-1] -eq '0') {
        $parts = $parts[0..($parts.Count - 2)]
    }

    return ($parts -join '.')
}

function Get-SingleArtifact {
    param(
        [string]$Root,
        [string]$Pattern,
        [string]$Label
    )

    $matches = Get-ChildItem -Path $Root -File -Filter $Pattern -ErrorAction Stop
    if ($matches.Count -ne 1) {
        throw "Expected exactly one $Label matching '$Pattern' in '$Root', found $($matches.Count)."
    }

    return $matches[0]
}

function Assert-Metadata {
    param(
        [System.IO.FileInfo]$File,
        [string]$ExpectedDescription,
        [string]$ExpectedOriginalFilename,
        [string]$ExpectedVersion
    )

    $info = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($File.FullName)
    $normalizedFileVersion = Normalize-VersionString $info.FileVersion
    $normalizedProductVersion = Normalize-VersionString $info.ProductVersion

    if ($info.ProductName -ne 'Toolscreen') {
        throw "Unexpected ProductName for '$($File.Name)': '$($info.ProductName)'"
    }

    if ($info.CompanyName -ne 'jojoe77777') {
        throw "Unexpected CompanyName for '$($File.Name)': '$($info.CompanyName)'"
    }

    if ($info.FileDescription -ne $ExpectedDescription) {
        throw "Unexpected FileDescription for '$($File.Name)': '$($info.FileDescription)'"
    }

    if ($info.OriginalFilename -ne $ExpectedOriginalFilename) {
        throw "Unexpected OriginalFilename for '$($File.Name)': '$($info.OriginalFilename)'"
    }

    if ($info.LegalCopyright -ne 'Copyright (c) 2026 jojoe77777') {
        throw "Unexpected LegalCopyright for '$($File.Name)': '$($info.LegalCopyright)'"
    }

    if (-not $normalizedFileVersion) {
        throw "Missing FileVersion for '$($File.Name)'"
    }

    if (-not $normalizedProductVersion) {
        throw "Missing ProductVersion for '$($File.Name)'"
    }

    if ($normalizedFileVersion -ne $normalizedProductVersion) {
        throw "FileVersion/ProductVersion mismatch for '$($File.Name)': '$normalizedFileVersion' vs '$normalizedProductVersion'"
    }

    if ($normalizedProductVersion -ne $ExpectedVersion) {
        throw "Unexpected ProductVersion for '$($File.Name)': '$normalizedProductVersion' (expected '$ExpectedVersion')"
    }
}

$artifactRoot = (Resolve-Path $ArtifactDirectory).Path
$dll = Get-SingleArtifact -Root $artifactRoot -Pattern 'Toolscreen.dll' -Label 'Toolscreen DLL'
$loggerDll = Get-SingleArtifact -Root $artifactRoot -Pattern 'liblogger_x64.dll' -Label 'liblogger DLL'
$installer = Get-SingleArtifact -Root $artifactRoot -Pattern 'Toolscreen-*-double-click-me.exe' -Label 'Toolscreen installer EXE'
$downloader = Get-SingleArtifact -Root $artifactRoot -Pattern 'toolscreen-downloader.exe' -Label 'Toolscreen downloader EXE'

$dllInfo = [System.Diagnostics.FileVersionInfo]::GetVersionInfo($dll.FullName)
$expectedVersion = Normalize-VersionString $dllInfo.ProductVersion
if (-not $expectedVersion) {
    throw "Unable to determine expected version from '$($dll.Name)'"
}

Assert-Metadata -File $dll -ExpectedDescription 'Toolscreen hook DLL' -ExpectedOriginalFilename 'Toolscreen.dll' -ExpectedVersion $expectedVersion
Assert-Metadata -File $loggerDll -ExpectedDescription 'Toolscreen liblogger' -ExpectedOriginalFilename 'liblogger_x64.dll' -ExpectedVersion $expectedVersion
Assert-Metadata -File $installer -ExpectedDescription 'Toolscreen installer' -ExpectedOriginalFilename $installer.Name -ExpectedVersion $expectedVersion
Assert-Metadata -File $downloader -ExpectedDescription 'Toolscreen downloader' -ExpectedOriginalFilename 'toolscreen-downloader.exe' -ExpectedVersion $expectedVersion

Write-Host "Verified PE metadata for Toolscreen signing artifacts in '$artifactRoot'."