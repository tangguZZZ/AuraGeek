param([ValidateSet('build','run','demo','test','stocks')][string]$Action='run')
$ErrorActionPreference='Stop'
$project=Split-Path -Parent $PSScriptRoot
$source=Join-Path $project 'simulator'
$cmake='D:\LVGL-Dev\tools\cmake-4.4.0-windows-x86_64\bin\cmake.exe'
$env:PATH='D:\LVGL-Dev\tools\mingw64-15.2.0\mingw64\bin;'+$env:PATH
if($Action -eq 'stocks'){python (Join-Path $source 'update_stocks.py');exit $LASTEXITCODE}
Push-Location $source
try {
 & $cmake --preset windows-debug
 if($LASTEXITCODE){throw 'Simulator configure failed'}
 & $cmake --build --preset windows-debug
 if($LASTEXITCODE){throw 'Simulator build failed'}
 if($Action -ne 'build'){
   $arguments=@();if($Action -eq 'demo'){$arguments=@('--official')};if($Action -eq 'test'){$arguments=@('--smoke')}
   $runArgs=@{FilePath=(Join-Path $source 'build\windows-debug\aurageek_sim.exe');WorkingDirectory=(Join-Path $source 'build\windows-debug');PassThru=$true}
   if($arguments.Count){$runArgs.ArgumentList=$arguments}
   $process=Start-Process @runArgs
   if($Action -eq 'test'){$process.WaitForExit();exit $process.ExitCode}
 }
}finally{Pop-Location}
