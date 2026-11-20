# Creates a release signing key once. Run it on your own PC, never in CI:
#   powershell -ExecutionPolicy Bypass -File scripts\new-release-key.ps1 -Channel Seelen
param(
    [ValidateSet('Official', 'Seelen')][string]$Channel = 'Official',
    [string]$KeyFile,
    [string]$PublicKeyHeader = (Join-Path (Split-Path $PSScriptRoot -Parent) 'src\update_public_key.h'),
    [Security.SecureString]$Password
)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'release-key.psm1') -Force

if (-not $KeyFile) {
    $folder = if ($Channel -eq 'Seelen') { '.batteryhub-seelen' } else { '.wirelessbattery' }
    $KeyFile = Join-Path $env:USERPROFILE "$folder\release-key.wbk"
}
if (Test-Path -LiteralPath $KeyFile) { throw "$KeyFile already exists. Refusing to overwrite a release key." }
$repo = Split-Path $PSScriptRoot -Parent
if ([IO.Path]::GetFullPath($KeyFile).StartsWith([IO.Path]::GetFullPath($repo), [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Keep the key file outside the repository, so it can never be committed.'
}
if (-not $Password) {
    $Password = Read-Host 'New password for the key (at least 12 characters)' -AsSecureString
    $again = Read-Host 'Same password again' -AsSecureString
    if ((ConvertTo-PlainText $Password) -cne (ConvertTo-PlainText $again)) { throw 'The passwords do not match.' }
}
if ((ConvertTo-PlainText $Password).Length -lt 12) { throw 'Use a password of at least 12 characters.' }

$params = New-Object Security.Cryptography.CngKeyCreationParameters
$params.ExportPolicy = [Security.Cryptography.CngExportPolicies]::AllowPlaintextExport
$params.KeyCreationOptions = [Security.Cryptography.CngKeyCreationOptions]::None
$key = [Security.Cryptography.CngKey]::Create([Security.Cryptography.CngAlgorithm]::ECDsaP256, [NullString]::Value, $params)
$private = $key.Export([Security.Cryptography.CngKeyBlobFormat]::EccPrivateBlob)
$publicText = Get-PublicKeyText $key
$key.Dispose()

New-Item -ItemType Directory -Force (Split-Path $KeyFile -Parent) | Out-Null
[IO.File]::WriteAllBytes($KeyFile, (Protect-ReleaseKey $private $Password))
[Array]::Clear($private, 0, $private.Length)

$header = [IO.File]::ReadAllText($PublicKeyHeader)
$arrayName = if ($Channel -eq 'Seelen') { 'SeelenReleaseKeys' } else { 'OfficialReleaseKeys' }
$pattern = "(?s)(inline constexpr const char\* $arrayName\[\] = \{\s*)(.*?)(\s*\};)"
$match = [regex]::Match($header, $pattern)
if (-not $match.Success) { throw "Could not find $arrayName in $PublicKeyHeader." }
$body = $match.Groups[2].Value
if ($body -match '(?m)^(\s*)"",\s*$') { $body = $body -replace '(?m)^(\s*)"",\s*$', "`$1`"$publicText`"," }
else {
    $indent = if ($body -match '(?m)^(\s*)"') { $Matches[1] } else { '    ' }
    $body = $body.TrimEnd() + "`r`n$indent`"$publicText`","
}
$header = $header.Substring(0, $match.Index) + $match.Groups[1].Value + $body + $match.Groups[3].Value +
          $header.Substring($match.Index + $match.Length)
if ($body -notmatch [regex]::Escape($publicText)) { throw "Could not add the key to $arrayName." }
[IO.File]::WriteAllText($PublicKeyHeader, $header, (New-Object Text.UTF8Encoding($false)))

Write-Host ''
Write-Host "Private key: $KeyFile (encrypted with your password)"
Write-Host "Public key:  $publicText"
Write-Host "             added to $arrayName in $PublicKeyHeader - commit that file."
Write-Host ''
Write-Host 'Back up the key file AND remember the password (for example in a password manager).' -ForegroundColor Yellow
Write-Host 'Without them, later automatic updates cannot be signed.' -ForegroundColor Yellow
