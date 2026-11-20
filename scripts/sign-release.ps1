# Signs the exact SHA256SUMS.txt from a release with a private key that never enters CI.
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [ValidateSet('Official', 'Seelen')][string]$Channel = 'Official',
    [string]$KeyFile,
    [string]$SumsFile,
    [string]$OutFile,
    [Security.SecureString]$Password,
    [switch]$Yes
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'release-key.psm1') -Force
$repo = if ($Channel -eq 'Seelen') { 'ISOByte00/Wireless-Device-Battery-Monitor-Seelen-UI' } else { 'krisgfx9/Wireless-Device-Battery-Monitor' }
if (-not $KeyFile) {
    $folder = if ($Channel -eq 'Seelen') { '.batteryhub-seelen' } else { '.wirelessbattery' }
    $KeyFile = Join-Path $env:USERPROFILE "$folder\release-key.wbk"
}
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Version must be MAJOR.MINOR.PATCH, for example 1.2.3.' }
if (-not (Test-Path -LiteralPath $KeyFile)) { throw "No key file at $KeyFile. Create one with scripts\new-release-key.ps1 -Channel $Channel." }
if (-not $OutFile) { $OutFile = Join-Path (Get-Location) 'SHA256SUMS.txt.sig' }

if (-not $SumsFile) {
    $SumsFile = Join-Path ([IO.Path]::GetTempPath()) "WirelessBattery-SHA256SUMS-$Version.txt"
    $url = "https://github.com/$repo/releases/download/v$Version/SHA256SUMS.txt"
    Write-Host "Downloading $url"
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest $url -OutFile $SumsFile -UseBasicParsing
}
$data = [IO.File]::ReadAllBytes($SumsFile)
$text = [Text.Encoding]::ASCII.GetString($data)
$setup = if ($Channel -eq 'Seelen') { "BatteryHub-Seelen-Setup-$Version.exe" } else { "BatteryHub-Setup-$Version.exe" }
if ($text -notmatch "(?m)^[0-9a-fA-F]{64} [ *]$([regex]::Escape($setup))\r?$") { throw "SHA256SUMS.txt does not list $setup." }

$root = Split-Path $PSScriptRoot -Parent
foreach ($line in $text -split "`n") {
    if ($line -match '^([0-9a-fA-F]{64}) [ *](.+?)\r?$') {
        $local = Join-Path $root "dist\$($Matches[2])"
        if (Test-Path -LiteralPath $local) {
            $hash = (Get-FileHash -LiteralPath $local -Algorithm SHA256).Hash
            $state = if ($hash -eq $Matches[1].ToUpper()) { 'matches dist\' } else { 'DIFFERENT from dist\' }
            Write-Host "  $($Matches[2]): $state"
        }
    }
}

Write-Host ''
Write-Host $text
if (-not $Yes) {
    $answer = Read-Host "Sign this list for $Channel version $Version? Only do this for your release (y/n)"
    if ($answer -ne 'y') { Write-Host 'Not signed.'; exit 1 }
}
if (-not $Password) { $Password = Read-Host 'Key password' -AsSecureString }

$private = Unprotect-ReleaseKey ([IO.File]::ReadAllBytes($KeyFile)) $Password
$key = [Security.Cryptography.CngKey]::Import($private, [Security.Cryptography.CngKeyBlobFormat]::EccPrivateBlob)
[Array]::Clear($private, 0, $private.Length)
$ecdsa = New-Object Security.Cryptography.ECDsaCng($key)
$ecdsa.HashAlgorithm = [Security.Cryptography.CngAlgorithm]::Sha256
$signature = $ecdsa.SignData($data)
$publicText = Get-PublicKeyText $key
$key.Dispose()

if (-not (Test-ReleaseSignature $data $signature $publicText)) { throw 'The new signature did not verify. Nothing was written.' }
$header = [IO.File]::ReadAllText((Join-Path $root 'src\update_public_key.h'))
$arrayName = if ($Channel -eq 'Seelen') { 'SeelenReleaseKeys' } else { 'OfficialReleaseKeys' }
$keyBlock = [regex]::Match($header, "(?s)inline constexpr const char\* $arrayName\[\] = \{(.*?)\};")
if (-not $keyBlock.Success -or $keyBlock.Groups[1].Value -notmatch [regex]::Escape($publicText)) {
    Write-Warning "This key is not in ${arrayName}: builds for this channel will not accept the signature."
}
[IO.File]::WriteAllText($OutFile, [Convert]::ToBase64String($signature) + "`n", (New-Object Text.UTF8Encoding($false)))
Write-Host ''
Write-Host "Signature written to $OutFile"
Write-Host "Upload it to the v$Version draft release as SHA256SUMS.txt.sig:"
Write-Host "  https://github.com/$repo/releases/edit/v$Version"
if (-not $Yes) { Start-Process "https://github.com/$repo/releases/edit/v$Version" }
