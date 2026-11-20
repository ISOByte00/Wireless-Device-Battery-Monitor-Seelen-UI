param([Parameter(Mandatory = $true)][string]$Version)
$ErrorActionPreference = 'Stop'
if ($Version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$') { throw 'Version must be MAJOR.MINOR.PATCH.' }
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) { throw 'GitHub CLI (gh) is required and must be authenticated.' }
$temporary = Join-Path ([IO.Path]::GetTempPath()) "batteryhub-seelen-sign-$([guid]::NewGuid().ToString('N'))"
New-Item -ItemType Directory $temporary | Out-Null
try {
    $sums = Join-Path $temporary 'SHA256SUMS.txt'
    $signature = Join-Path $temporary 'SHA256SUMS.txt.sig'
    gh release download "v$Version" --repo ISOByte00/Wireless-Device-Battery-Monitor-Seelen-UI --pattern SHA256SUMS.txt --dir $temporary
    if ($LASTEXITCODE -ne 0) { throw 'Could not download SHA256SUMS.txt from the draft release.' }
    & (Join-Path $PSScriptRoot 'sign-release.ps1') -Version $Version -Channel Seelen -SumsFile $sums -OutFile $signature
    if ($LASTEXITCODE -ne 0) { throw 'Signing was cancelled or failed.' }
    gh release upload "v$Version" $signature --repo ISOByte00/Wireless-Device-Battery-Monitor-Seelen-UI --clobber
    if ($LASTEXITCODE -ne 0) { throw 'Could not upload SHA256SUMS.txt.sig.' }
    gh workflow run experimental-release.yml --repo ISOByte00/Wireless-Device-Battery-Monitor-Seelen-UI --ref feat/seelen-ui-integration -f "version=$Version"
    if ($LASTEXITCODE -ne 0) { throw 'Could not start the publish verification workflow.' }
    Write-Host 'The signature was uploaded and publish verification was requested.'
} finally {
    if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Recurse -Force }
}
