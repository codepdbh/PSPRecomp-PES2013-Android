param(
    [string]$Iso = '',
    [string]$DecryptedElf = '',
    [string]$ToolDirectory = '',
    [switch]$SkipCodegen
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$profile = Split-Path -Parent $PSScriptRoot
$workspace = Split-Path -Parent $repo
$identity = Get-Content -Raw (Join-Path $profile 'config/ulus10597.json') | ConvertFrom-Json
if (-not $Iso) { $Iso = Join-Path $workspace 'pes2013.iso' }
if (-not $DecryptedElf) {
    $DecryptedElf = Join-Path $workspace 'PPSSPP/memstick/PSP/SYSTEM/DUMP/ULUS10597_EBOOT.BIN'
}
if (-not $ToolDirectory) { $ToolDirectory = Join-Path $repo 'out/pes2013-probe' }
$python = Get-Command python -ErrorAction SilentlyContinue
if (-not $python) { $python = Get-Command py -ErrorAction SilentlyContinue }
if (-not $python) { throw 'Python 3 is required for ISO extraction.' }

Push-Location $repo
try {
    & $python.Source tools/extract_psp_iso.py $Iso (Join-Path $profile 'game') --expected-disc-id $identity.disc_id
    if ($LASTEXITCODE -ne 0) { throw 'PES ISO extraction failed.' }
    if (-not (Test-Path -LiteralPath $DecryptedElf -PathType Leaf)) {
        throw 'Dump the decrypted executable with PPSSPP (DumpFileTypes=1) or supply -DecryptedElf.'
    }
    $hash = (Get-FileHash -LiteralPath $DecryptedElf -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($hash -ne $identity.elf_sha256) { throw "Unexpected PES executable SHA-256: $hash" }
    $elf = Join-Path $profile 'game/PSP_GAME/SYSDIR/EBOOT_DECRYPTED.ELF'
    if ([IO.Path]::GetFullPath($DecryptedElf) -ne [IO.Path]::GetFullPath($elf)) {
        Copy-Item -LiteralPath $DecryptedElf -Destination $elf
    }
    $analyzer = Join-Path $ToolDirectory 'Release/psp_analyze.exe'
    if (-not (Test-Path $analyzer)) { throw "Build psp_analyze first: $analyzer" }
    New-Item -ItemType Directory -Force (Join-Path $profile 'analysis') | Out-Null
    & $analyzer $elf (Join-Path $profile 'analysis/pes2013_report.json') $identity.load_base
    if ($LASTEXITCODE -ne 0) { throw 'PES executable analysis failed.' }
    & $python.Source (Join-Path $PSScriptRoot 'extract_overlays.py')
    if ($LASTEXITCODE -ne 0) { throw 'PES overlay extraction failed.' }
    & $python.Source (Join-Path $PSScriptRoot 'audit.py')
    if ($LASTEXITCODE -ne 0) { throw 'PES portability audit failed.' }
    if (-not $SkipCodegen) {
        $generator = Join-Path $ToolDirectory 'profiles/pes2013/Release/pes2013_recomp.exe'
        if (-not (Test-Path $generator)) { throw "Build pes2013_recomp first: $generator" }
        # The generator writes timing diagnostics to stderr. Windows PowerShell
        # treats redirected native stderr as error records even on exit code 0.
        $savedPreference = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            & $generator $elf --auto (Join-Path $profile 'generated') $identity.load_base $identity.unit_span_bytes --generic 2>&1 |
                ForEach-Object { $_.ToString() }
            $generationExit = $LASTEXITCODE
        } finally {
            $ErrorActionPreference = $savedPreference
        }
        if ($generationExit -ne 0) { throw "PES generic code generation failed (exit $generationExit)." }
    }
} finally {
    Pop-Location
}
