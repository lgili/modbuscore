param(
    [Parameter(Mandatory = $true)]
    [string] $Archive,

    [string] $Destination = "$env:ProgramFiles\modbuscore"
)

if (-not (Test-Path -Path $Archive -PathType Leaf)) {
    Write-Error "Archive '$Archive' not found."
    exit 1
}

New-Item -ItemType Directory -Path $Destination -Force | Out-Null

switch -Wildcard ($Archive) {
    "*.zip" {
        Expand-Archive -Path $Archive -DestinationPath $Destination -Force
    }
    default {
        Write-Error "Unsupported archive format '$Archive'. Only .zip is supported on Windows."
        exit 1
    }
}

Write-Host "✅ Installed archive into $Destination"
Write-Host ""
Write-Host "Next steps:"
Write-Host "  • Configure CMake with -Dmodbuscore_DIR=$Destination\lib\cmake\modbuscore"
Write-Host "  • Verify consumption:"
Write-Host "      cmake -S examples\cmake-consume -B build -Dmodbuscore_DIR=$Destination\lib\cmake\modbuscore"
Write-Host "      cmake --build build --config Release"
Write-Host "  • Update PKG_CONFIG_PATH if you rely on pkg-config: $Destination\lib\pkgconfig"
