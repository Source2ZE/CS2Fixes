param(
    [Parameter(Mandatory = $true)]
    [string]$Cs2Root
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$content = Join-Path $Cs2Root 'content\csgo_addons\cs2fixes_hud'
$output = Join-Path $Cs2Root 'game\csgo_addons\cs2fixes_hud'
$compiler = Join-Path $Cs2Root 'game\bin\win64\resourcecompiler.exe'
if (-not (Test-Path -LiteralPath $compiler)) { throw 'Install CS2 Workshop Tools to obtain resourcecompiler.exe.' }
$layout = Join-Path $content 'panorama\layout\custom_game\hud.xml'
$style = Join-Path $content 'panorama\styles\entwatch\hud.css'
New-Item -ItemType Directory -Force (Split-Path $layout), (Split-Path $style) | Out-Null
Copy-Item -LiteralPath (Join-Path $repo 'assets_source\panorama\layout\custom_game\hud.xml') -Destination $layout -Force
Copy-Item -LiteralPath (Join-Path $repo 'assets_source\panorama\styles\entwatch\hud.css') -Destination $style -Force
Set-Content -LiteralPath (Join-Path $content 'panorama\preprocessor_config.txt') -Encoding ascii -Value '"PanzipCfg" { "BlockDefs" {} }'
Set-Content -LiteralPath (Join-Path $content 'addoninfo.txt') -Encoding utf8 -Value @'
<!-- kv3 encoding:text:version{e21c7f3c-8a33-41c5-9977-a76d3a32aa0d} format:generic:version{7412167c-06e9-4698-aff2-e63eb59037e7} -->
{ IsPlayable = false }
'@
& $compiler -game (Join-Path $Cs2Root 'game\csgo') -i $style -i $layout -f -nop4
if ($LASTEXITCODE -ne 0) { throw "Panorama compiler failed: $LASTEXITCODE" }
foreach ($relative in @('panorama\layout\custom_game\hud.vxml_c', 'panorama\styles\entwatch\hud.vcss_c')) {
    $compiled = Join-Path $output $relative
    if (-not (Test-Path -LiteralPath $compiled)) { throw "Missing compiled resource: $compiled" }
    $destination = Join-Path $repo ('assets\' + $relative)
    New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
    Copy-Item -LiteralPath $compiled -Destination $destination -Force
}
Copy-Item -LiteralPath (Join-Path $content 'addoninfo.txt') -Destination (Join-Path $output 'addoninfo.txt') -Force
Write-Host 'Compiled Panorama resources copied into assets/panorama.'
