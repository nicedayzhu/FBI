[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $ArchivePath
)

$ErrorActionPreference = 'Stop'

$outputDirectory = (Get-Location).Path
$destination = [System.IO.Path]::GetFullPath((Join-Path $outputDirectory $ArchivePath))

if (Test-Path -LiteralPath $destination) {
    Remove-Item -LiteralPath $destination -Force
}

$files = @(
    Get-ChildItem -LiteralPath $outputDirectory -File -Recurse |
        Where-Object { $_.FullName -ne $destination -and $_.Extension -ne '.elf' }
)

if ($files.Count -eq 0) {
    throw "Cannot create an empty release archive from '$outputDirectory'."
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::Open($destination, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in $files) {
        $relativePath = $file.FullName.Substring($outputDirectory.Length).TrimStart('\', '/').Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $archive,
            $file.FullName,
            $relativePath,
            [System.IO.Compression.CompressionLevel]::Optimal
        ) | Out-Null
    }
}
finally {
    $archive.Dispose()
}
