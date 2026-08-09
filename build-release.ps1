$ErrorActionPreference = "Stop"

function CnName($codePoints, $suffix = "") {
  return (-join ($codePoints | ForEach-Object { [char]$_ })) + $suffix
}

function Join-CnPath($base, $codePoints, $suffix = "") {
  return Join-Path $base (CnName $codePoints $suffix)
}

function Copy-Required($from, $to) {
  if (-not (Test-Path -LiteralPath $from)) {
    throw "Missing required file: $from"
  }
  Copy-Item -LiteralPath $from -Destination $to -Force
}

function Copy-Optional($from, $to) {
  if (Test-Path -LiteralPath $from) {
    Copy-Item -LiteralPath $from -Destination $to -Force
  }
}

$root = $PSScriptRoot
$stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$releaseRoot = Join-Path $root "release"
$out = Join-Path $releaseRoot "PrintSphere_Lite_$stamp"
$zip = Join-Path $releaseRoot "PrintSphere_Lite_$stamp.zip"

$nameFirmware = @(0x56FA, 0x4EF6)
$nameCompanion = @(0x540E, 0x7AEF, 0x914D, 0x7F6E, 0x5DE5, 0x5177)
$nameFlasher = @(0x5237, 0x56FA, 0x4EF6, 0x5DE5, 0x5177)
$nameReadme = @(0x4F7F, 0x7528, 0x8BF4, 0x660E)
$nameDriver = @(0x9A71, 0x52A8)
$fileOpenCompanion = CnName @(0x6253, 0x5F00, 0x914D, 0x7F6E, 0x5DE5, 0x5177) ".bat"
$fileOneClickFlash = CnName @(0x4E00, 0x952E, 0x5237, 0x5165, 0x56FA, 0x4EF6) ".bat"

$sourceFirmwareDir = Join-CnPath $root $nameFirmware
$sourceCompanionDir = Join-CnPath $root $nameCompanion
$sourceFlasherDir = Join-CnPath $root $nameFlasher

$compiledBin = Join-Path $root ".pio\build\sd2\firmware.bin"
$firmware = Join-Path $sourceFirmwareDir "printsphere-lite-esp8266.bin"

if (Test-Path -LiteralPath $compiledBin) {
  Copy-Item -LiteralPath $compiledBin -Destination $firmware -Force
  Write-Host "Updated firmware bin from compilation output."
}

if (-not (Test-Path -LiteralPath $firmware)) {
  throw "Missing firmware: $firmware"
}

New-Item -ItemType Directory -Force -Path $releaseRoot | Out-Null
if (Test-Path -LiteralPath $out) {
  Remove-Item -LiteralPath $out -Recurse -Force
}

$firmwareOut = Join-CnPath $out $nameFirmware
$companionOut = Join-CnPath $out $nameCompanion
$flasherOut = Join-CnPath $out $nameFlasher

New-Item -ItemType Directory -Force -Path `
  $firmwareOut, `
  (Join-Path $companionOut "node"), `
  (Join-Path $flasherOut "tools"), `
  (Join-CnPath $flasherOut $nameDriver) | Out-Null

Copy-Required $firmware (Join-Path $firmwareOut "printsphere-lite-esp8266.bin")
Copy-Required (Join-Path $root "README.md") (Join-CnPath $out $nameReadme ".md")

Copy-Required (Join-Path $sourceCompanionDir $fileOpenCompanion) (Join-Path $companionOut $fileOpenCompanion)
Copy-Required (Join-Path $sourceCompanionDir "server.js") (Join-Path $companionOut "server.js")
Copy-Required (Join-Path $sourceCompanionDir "package.json") (Join-Path $companionOut "package.json")
Copy-Required (Join-Path $sourceCompanionDir "README.md") (Join-Path $companionOut "README.md")
Copy-Required (Join-Path $sourceCompanionDir "VERSIONS.md") (Join-Path $companionOut "VERSIONS.md")
Copy-Optional (Join-Path $sourceCompanionDir "node\node.exe") (Join-Path $companionOut "node\node.exe")

Copy-Required (Join-Path $sourceFlasherDir $fileOneClickFlash) (Join-Path $flasherOut $fileOneClickFlash)
Copy-Required (Join-Path $sourceFlasherDir "flash-firmware.ps1") (Join-Path $flasherOut "flash-firmware.ps1")
Copy-Required (Join-Path $sourceFlasherDir "README.md") (Join-Path $flasherOut "README.md")
Copy-Optional (Join-Path $sourceFlasherDir "tools\esptool.exe") (Join-Path $flasherOut "tools\esptool.exe")
Copy-Optional (Join-Path (Join-CnPath $sourceFlasherDir $nameDriver) "CH341SER.EXE") (Join-Path (Join-CnPath $flasherOut $nameDriver) "CH341SER.EXE")

Compress-Archive -Path (Join-Path $out "*") -DestinationPath $zip -Force

$desktopZip = "C:\Users\huangyiqian\Desktop\PrintSphere_Lite_v0.4.90-ams_webcfg.zip"
Copy-Item -LiteralPath $zip -Destination $desktopZip -Force

Write-Host ""
Write-Host "Release package created successfully!"
Write-Host "Project Release Zip: $zip"
Write-Host "Desktop Release Zip: $desktopZip"
Write-Host ""
Write-Host "Runtime private data under 后端配置工具/data was not included."
