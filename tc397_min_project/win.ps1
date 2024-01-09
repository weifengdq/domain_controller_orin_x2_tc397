function Build {
  $startTime = Get-Date

  $exePath = (Get-Command cctc.exe).Source
  $env:TOOLCHAIN_ROOT = (Split-Path (Split-Path $exePath -Parent) -Parent).Replace("\", "/")
  Write-Host "TOOLCHAIN_ROOT: $env:TOOLCHAIN_ROOT"

  if (!(Test-Path -Path "build")) {
    New-Item -Path "build" -ItemType "directory" -ErrorAction SilentlyContinue
  }

  # cross-compiling toolchain
  $env:CMAKE_TOOLCHAIN_FILE = "$PSScriptRoot\tasking_tricore.cmake"
  Write-Host "CMAKE_TOOLCHAIN_FILE: $env:CMAKE_TOOLCHAIN_FILE"
  
  cmake -B build -G "MinGW Makefiles" .
  $totalLogicalCores = ( `
    (Get-CimInstance –ClassName Win32_Processor).NumberOfLogicalProcessors | `
      Measure-Object -Sum `
  ).Sum
  cmake --build build -- -j $totalLogicalCores

  # if cmake or make failed, exit with the same error code
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }

  $endTime = Get-Date
  $timeSpan = New-TimeSpan -Start $startTime -End $endTime
  Write-Host "Build time taken: $($timeSpan.Minutes) minutes $($timeSpan.Seconds) seconds"
}

function Clean {
  if (Test-Path -Path "build") {
    Remove-Item -Path "build" -Recurse -Force
  }
}

function Help {
  Write-Host "Usage: win.ps1 [command]"
  Write-Host "Commands:"
  Write-Host "  build: build the project"
  Write-Host "  clean: clean the project"
  Write-Host "  help: show this help message"
}

if ($args.Length -eq 0) {
  Help
  exit 0
}

switch ($args[0]) {
  "build" {
    Build
  }
  "clean" {
    Clean
  }
  "help" {
    Help
  }
  default {
    Write-Host "Unknown command: $args[0]"
    Help
  }
}
