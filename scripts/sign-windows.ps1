param([Parameter(Mandatory=$true)][string]$Path)
$ErrorActionPreference = 'Stop'
if (-not $env:LC_WINDOWS_CERT_SHA1) { throw 'Set LC_WINDOWS_CERT_SHA1 to a signing certificate in CurrentUser\My' }
if (-not $env:LC_WINDOWS_TIMESTAMP_URL) { throw 'Set LC_WINDOWS_TIMESTAMP_URL to your RFC 3161 timestamp service' }
$signTool = (Get-Command signtool.exe -ErrorAction SilentlyContinue).Source
if (-not $signTool) {
    $sdk = ${env:ProgramFiles(x86)}
    $signTool = Get-ChildItem "$sdk\Windows Kits\10\bin\*\x64\signtool.exe" |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $signTool) { throw 'Windows SDK signtool.exe was not found' }
if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { throw 'Signing input does not exist' }
& $signTool sign /sha1 $env:LC_WINDOWS_CERT_SHA1 /fd SHA256 /tr $env:LC_WINDOWS_TIMESTAMP_URL /td SHA256 $Path
if ($LASTEXITCODE -ne 0) { throw "Authenticode signing failed: $LASTEXITCODE" }
& $signTool verify /pa /all $Path
if ($LASTEXITCODE -ne 0) { throw "Authenticode verification failed: $LASTEXITCODE" }
