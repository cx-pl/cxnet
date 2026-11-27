param(
    [string] $Cxc = 'cxc',
    [string] $CxCoreDir = (Join-Path $PSScriptRoot '..\..\..\..\cxcore'),
    [ValidateSet('Windows', 'Linux')]
    [string] $Platform = 'Windows',
    [ValidateSet('x86', 'x64')]
    [string] $Architecture = 'x64',
    [ValidateSet('static', 'shared', 'both')]
    [string] $LinkMode = 'static',
    [string] $Configuration = 'Release',
    [string] $Generator = '',
    [string] $CMake = 'cmake',
    [string] $Distribution = 'Ubuntu',
    [ValidateRange(1, 100)]
    [int] $TestRepeat = 1
)

$ErrorActionPreference = 'Stop'
$compilerPath = if ([IO.Path]::IsPathRooted($Cxc)) {
    (Resolve-Path -LiteralPath $Cxc).Path
} else {
    (Get-Command $Cxc -ErrorAction Stop).Source
}
$cxCorePath = (Resolve-Path -LiteralPath $CxCoreDir).Path
$cmakePath = $null
$ctestPath = $null
if ($Platform -eq 'Windows') {
    $cmakePath = if (Test-Path -LiteralPath $CMake -PathType Leaf) {
        (Resolve-Path $CMake).Path
    } else {
        (Get-Command $CMake -ErrorAction Stop).Source
    }
    $ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
    if (-not (Test-Path -LiteralPath $ctestPath -PathType Leaf)) {
        $ctestPath = (Get-Command ctest -ErrorAction Stop).Source
    }
} elseif ($Architecture -ne 'x64') {
    throw 'Linux streaming-read supports x64 only.'
} else {
    $versionOutput = & wsl.exe --distribution $Distribution -- $CMake --version 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Could not run WSL CMake '$CMake': $($versionOutput -join [Environment]::NewLine)"
    }
}
$sourceDir = $PSScriptRoot
$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..\..')).Path
$cxnetProject = Join-Path $repositoryRoot 'cxnet\src\cxnet.cxproj'
$streamingProject = Join-Path $repositoryRoot 'cxnet\examples\streaming-read\streaming-read.cxproj'
$cxnetGeneratedDir = Join-Path ([IO.Path]::GetTempPath()) `
    "cxnet-generated-$([guid]::NewGuid().ToString('N'))"
$streamingGeneratedDir = Join-Path ([IO.Path]::GetTempPath()) `
    "cx-streaming-read-generated-$([guid]::NewGuid().ToString('N'))"
$buildDir = Join-Path ([IO.Path]::GetTempPath()) "cx-streaming-read-e2e-$([guid]::NewGuid().ToString('N'))"
$modes = if ($LinkMode -eq 'both') { @('static', 'shared') } else { @($LinkMode) }
$utf8NoBom = [Text.UTF8Encoding]::new($false)
$previousLdLibraryPath = $env:LD_LIBRARY_PATH

function Convert-WindowsPathToWslPath([string] $Path) {
    if (-not [IO.Path]::IsPathRooted($Path) -or $Path.Length -lt 3 -or $Path[1] -ne ':') {
        throw "Expected a drive-qualified Windows path, found '$Path'."
    }
    $drive = $Path.Substring(0, 1).ToLowerInvariant()
    $suffix = $Path.Substring(2).Replace('\', '/')
    return "/mnt/$drive$suffix"
}

try {
    New-Item -ItemType Directory -Path $cxnetGeneratedDir -Force | Out-Null
    New-Item -ItemType Directory -Path $streamingGeneratedDir -Force | Out-Null
    $emitArguments = @(
        '--emit-only', '--cxcore-dir', $cxCorePath, '--verbosity', 'quiet',
        '--output-dir', $cxnetGeneratedDir, $cxnetProject
    )
    if ([IO.Path]::GetExtension($compilerPath) -ieq '.dll') {
        & dotnet $compilerPath @emitArguments
    } else {
        & $compilerPath @emitArguments
    }
    if ($LASTEXITCODE -ne 0) {
        throw "cxnet source generation failed (exit $LASTEXITCODE)."
    }
    $generatedCmakePath = Join-Path $cxnetGeneratedDir 'CMakeLists.txt'
    $generatedCmake = Get-Content -LiteralPath $generatedCmakePath -Raw
    $safeBinaryDirectory = if ($Platform -eq 'Linux') {
        Convert-WindowsPathToWslPath (Join-Path $cxnetGeneratedDir '.bin')
    } else {
        (Join-Path $cxnetGeneratedDir '.bin').Replace('\', '/')
    }
    $safeBinarySetting = 'set(CX_BIN_DIRECTORY "' + $safeBinaryDirectory + '")'
    $generatedCmake = [regex]::Replace(
        $generatedCmake, 'set\(CX_BIN_DIRECTORY "[^"]*"\)', $safeBinarySetting, 1)
    if (-not $generatedCmake.Contains($safeBinarySetting)) {
        throw "Could not redirect generated cxnet binaries to '$safeBinaryDirectory'."
    }
    [IO.File]::WriteAllText($generatedCmakePath, $generatedCmake, $utf8NoBom)

    $emitArguments = @(
        '--emit-only', '--cxcore-dir', $cxCorePath, '--verbosity', 'quiet',
        '--output-dir', $streamingGeneratedDir, $streamingProject
    )
    if ([IO.Path]::GetExtension($compilerPath) -ieq '.dll') {
        & dotnet $compilerPath @emitArguments
    } else {
        & $compilerPath @emitArguments
    }
    if ($LASTEXITCODE -ne 0) {
        throw "Streaming-read CX source generation failed (exit $LASTEXITCODE)."
    }
    $generatedCmakePath = Join-Path $streamingGeneratedDir 'CMakeLists.txt'
    $generatedCmake = Get-Content -LiteralPath $generatedCmakePath -Raw
    $safeBinaryDirectory = if ($Platform -eq 'Linux') {
        Convert-WindowsPathToWslPath (Join-Path $streamingGeneratedDir '.bin')
    } else {
        (Join-Path $streamingGeneratedDir '.bin').Replace('\', '/')
    }
    $safeBinarySetting = 'set(CX_BIN_DIRECTORY "' + $safeBinaryDirectory + '")'
    $generatedCmake = [regex]::Replace(
        $generatedCmake, 'set\(CX_BIN_DIRECTORY "[^"]*"\)', $safeBinarySetting, 1)
    if (-not $generatedCmake.Contains($safeBinarySetting)) {
        throw "Could not redirect generated streaming-read binaries to '$safeBinaryDirectory'."
    }
    [IO.File]::WriteAllText($generatedCmakePath, $generatedCmake, $utf8NoBom)

    foreach ($mode in $modes) {
        $modeBuildDir = Join-Path $buildDir $mode
        $staticLink = if ($mode -eq 'static') { 'ON' } else { 'OFF' }
        if ($Platform -eq 'Linux') {
            $linuxSourceDir = Convert-WindowsPathToWslPath $sourceDir
            $linuxBuildDir = Convert-WindowsPathToWslPath $modeBuildDir
            $linuxCxnetGeneratedDir = Convert-WindowsPathToWslPath $cxnetGeneratedDir
            $linuxStreamingGeneratedDir = Convert-WindowsPathToWslPath $streamingGeneratedDir
            $configureOutput = & wsl.exe --distribution $Distribution -- $CMake `
                -S $linuxSourceDir -B $linuxBuildDir `
                "-DCX_STATIC_LINK=$staticLink" `
                "-DCXNET_GENERATED_DIR=$linuxCxnetGeneratedDir" `
                "-DCX_STREAMING_READ_GENERATED_DIR=$linuxStreamingGeneratedDir" 2>&1
            if ($LASTEXITCODE -ne 0) {
                throw "WSL streaming-read configure failed: $($configureOutput -join [Environment]::NewLine)."
            }
            Write-Output $configureOutput
            $buildOutput = & wsl.exe --distribution $Distribution -- $CMake `
                --build $linuxBuildDir --parallel 2 2>&1
            if ($LASTEXITCODE -ne 0) {
                throw "WSL streaming-read build failed: $($buildOutput -join [Environment]::NewLine)."
            }
            Write-Output $buildOutput
            & wsl.exe --distribution $Distribution -- ctest `
                --test-dir $linuxBuildDir --output-on-failure --verbose `
                --repeat "until-fail:$TestRepeat"
            if ($LASTEXITCODE -ne 0) { throw "Linux streaming-read CTest failed for $mode." }
            if ($mode -eq 'shared') {
                $executable = $linuxBuildDir + '/streaming_read_e2e'
                $relocations = & wsl.exe --distribution $Distribution -- ldd -r $executable 2>&1
                if ($LASTEXITCODE -ne 0) {
                    throw "Linux shared streaming dependency check failed: $($relocations -join [Environment]::NewLine)."
                }
                Write-Output $relocations
            }
        } else {
            $configureArgs = @('-S', $sourceDir, '-B', $modeBuildDir,
                "-DCX_STATIC_LINK:BOOL=$staticLink",
                "-DCXNET_GENERATED_DIR=$cxnetGeneratedDir",
                "-DCX_STREAMING_READ_GENERATED_DIR=$streamingGeneratedDir")
            if ($Generator) { $configureArgs += @('-G', $Generator) }
            if (-not $Generator -or $Generator -match 'Visual Studio') {
                $vsPlatform = if ($Architecture -eq 'x86') { 'Win32' } else { 'x64' }
                $configureArgs += @('-A', $vsPlatform)
            } elseif ($Architecture -eq 'x86') {
                throw 'Windows x86 requires a Visual Studio generator with -A Win32.'
            }
            Write-Output "CONFIGURE streaming-read platform=Windows architecture=$Architecture link=$mode"
            & $cmakePath @configureArgs
            if ($LASTEXITCODE -ne 0) { throw "CMake configure failed for $mode (exit $LASTEXITCODE)." }

            & $cmakePath --build $modeBuildDir --config $Configuration
            if ($LASTEXITCODE -ne 0) { throw "CMake build failed for $mode (exit $LASTEXITCODE)." }

            & $ctestPath --test-dir $modeBuildDir -C $Configuration --output-on-failure --verbose `
                --repeat "until-fail:$TestRepeat"
            if ($LASTEXITCODE -ne 0) { throw "Streaming read end-to-end check failed for $mode (exit $LASTEXITCODE)." }
        }
        Write-Output "PASS streaming-read $mode $Architecture"
    }
}
finally {
    $env:LD_LIBRARY_PATH = $previousLdLibraryPath
    if (Test-Path -LiteralPath $buildDir) {
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
    if (Test-Path -LiteralPath $cxnetGeneratedDir) {
        Remove-Item -LiteralPath $cxnetGeneratedDir -Recurse -Force
    }
    if (Test-Path -LiteralPath $streamingGeneratedDir) {
        Remove-Item -LiteralPath $streamingGeneratedDir -Recurse -Force
    }
}
