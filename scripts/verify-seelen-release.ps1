param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$AssetDirectory
)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Version must be MAJOR.MINOR.PATCH.' }
$AssetDirectory = [IO.Path]::GetFullPath($AssetDirectory)
$required = @(
    "BatteryHub-Standard-Setup-$Version.exe",
    "BatteryHub-Seelen-Setup-$Version.exe",
    "BatteryHub-Seelen-$Version-portable.zip",
    'BatteryHub-Seelen.yml',
    'SHA256SUMS.txt',
    'SHA256SUMS.txt.sig'
)
$actual = @(Get-ChildItem -LiteralPath $AssetDirectory -File | ForEach-Object Name | Sort-Object)
if (Compare-Object ($required | Sort-Object) $actual) { throw 'The draft release does not contain exactly the required assets.' }

$sumsPath = Join-Path $AssetDirectory 'SHA256SUMS.txt'
$signaturePath = Join-Path $AssetDirectory 'SHA256SUMS.txt.sig'
$data = [IO.File]::ReadAllBytes($sumsPath)
$signatureText = [IO.File]::ReadAllText($signaturePath).Trim()
try { $signature = [Convert]::FromBase64String($signatureText) } catch { throw 'SHA256SUMS.txt.sig is not valid base64.' }
if ($signature.Length -ne 64) { throw 'SHA256SUMS.txt.sig is not an ECDSA P-256 signature.' }

Import-Module (Join-Path $PSScriptRoot 'release-key.psm1') -Force
$header = [IO.File]::ReadAllText((Join-Path (Split-Path $PSScriptRoot -Parent) 'src\update_public_key.h'))
$block = [regex]::Match($header, '(?s)inline constexpr const char\* SeelenReleaseKeys\[\] = \{(.*?)\};')
if (-not $block.Success) { throw 'SeelenReleaseKeys is missing.' }
$keys = [regex]::Matches($block.Groups[1].Value, '"([A-Za-z0-9+/=]+)"') | ForEach-Object { $_.Groups[1].Value }
$verified = $false
foreach ($key in $keys) { if (Test-ReleaseSignature $data $signature $key) { $verified = $true; break } }
if (-not $verified) { throw 'The manifest signature is not valid for the Seelen release channel.' }

$listed = @{}
$text = [Text.Encoding]::ASCII.GetString($data)
foreach ($line in $text -split "`n") {
    if (-not $line) { continue }
    if ($line -notmatch '^([0-9a-fA-F]{64}) [ *]([^\\/\r\n]+)\r?$') { throw "Malformed checksum line: $line" }
    $name = $Matches[2]
    if ($listed.ContainsKey($name)) { throw "Duplicate checksum entry: $name" }
    $listed[$name] = $Matches[1].ToLowerInvariant()
}
$payload = $required | Where-Object { $_ -notlike 'SHA256SUMS.txt*' }
if (Compare-Object ($payload | Sort-Object) ($listed.Keys | Sort-Object)) { throw 'SHA256SUMS.txt does not list exactly the release payload.' }
foreach ($name in $payload) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $AssetDirectory $name) -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -cne $listed[$name]) { throw "Checksum mismatch: $name" }
}
Write-Host "Verified signed Seelen release v$Version."
