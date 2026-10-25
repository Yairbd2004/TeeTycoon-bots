param(
    [Parameter(Mandatory = $true)]
    [string] $ServerDir
)

$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$destination = (Resolve-Path -LiteralPath $ServerDir).Path
if (-not (Test-Path -LiteralPath (Join-Path $destination 'gamecontext.cpp'))) {
    throw "Expected a TeeTycoon src/game/server directory: $destination"
}

foreach ($file in @('bot.cpp', 'bot_navigation.cpp', 'bot.h', 'botengine.cpp', 'botengine.h', 'pet_skill_storage.h')) {
    Copy-Item -LiteralPath (Join-Path $repoRoot "adapter/teetycoon/$file") -Destination (Join-Path $destination $file) -Force
}

foreach ($folder in @('ai', 'bot_ai')) {
    $target = Join-Path $destination $folder
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    $source = if ($folder -eq 'ai') { Join-Path $repoRoot 'adapter/teetycoon/ai' } else { Join-Path $repoRoot 'include/bot_ai' }
    Get-ChildItem -LiteralPath $source -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $target $_.Name) -Force
    }
}

Write-Host "Bot sources copied to $destination. Review the TeeTycoon diff before building."
