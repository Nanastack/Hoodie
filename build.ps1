param(
 [string]$AshitaSdk=$env:ASHITA_SDK,
 [string]$ToolchainRoot=$env:HOODIE_TOOLCHAIN_ROOT,
 [switch]$SkipTests
)
$ErrorActionPreference='Stop'
if(!$AshitaSdk -or !(Test-Path (Join-Path $AshitaSdk 'Ashita.h'))){throw 'Set ASHITA_SDK or pass -AshitaSdk pointing to the Ashita v4 plugins/sdk folder.'}
if($ToolchainRoot){
 $vc=Get-ChildItem (Join-Path $ToolchainRoot 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
 $kit=Join-Path $ToolchainRoot 'Windows Kits/10'
 $kitVersion=Get-ChildItem (Join-Path $kit 'Include') -Directory | Sort-Object Name -Descending | Select-Object -First 1
 $env:PATH="$($vc.FullName)/bin/Hostx64/x86;$env:PATH"
 $env:INCLUDE="$($vc.FullName)/include;$kit/Include/$($kitVersion.Name)/ucrt;$kit/Include/$($kitVersion.Name)/shared;$kit/Include/$($kitVersion.Name)/um"
 $env:LIB="$($vc.FullName)/lib/x86;$kit/Lib/$($kitVersion.Name)/ucrt/x86;$kit/Lib/$($kitVersion.Name)/um/x86"
}
if(!(Get-Command cl.exe -ErrorAction SilentlyContinue)){throw 'Run from an x86 Visual Studio Developer PowerShell, or supply -ToolchainRoot.'}
Push-Location $PSScriptRoot
try {
 New-Item -ItemType Directory -Force build,package/plugins | Out-Null
 $detours=Join-Path $PSScriptRoot 'third_party/Microsoft.Detours/src'
 $sources=@('detours.cpp','disasm.cpp','image.cpp','modules.cpp','creatwth.cpp') | ForEach-Object {Join-Path $detours $_}
 & cl.exe /nologo /std:c++20 /EHsc /O2 /MT /LD /DWIN32 /D_WINDOWS /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS "/I$AshitaSdk" "/I$detours" /Fobuild/ src/hoodie.cpp @sources /link /MACHINE:X86 /DEF:src/exports.def /OUT:package/plugins/hoodie.dll /OPT:REF user32.lib advapi32.lib
 if($LASTEXITCODE){throw 'DLL build failed'}
 if(!$SkipTests){
  foreach($test in @('test','test_assets','test_normal_armors','test_peer_refresh','test_hood_gesture')){
   & cl.exe /nologo /std:c++20 /EHsc /O2 /MT /DNOMINMAX "/I$AshitaSdk" "/Fobuild/$test.obj" "/Febuild/$test.exe" "src/$test.cpp"
   if($LASTEXITCODE){throw "$test build failed"}
   & "./build/$test.exe"
   if($LASTEXITCODE){throw "$test failed"}
  }
  & cl.exe /nologo /std:c++20 /EHsc /Od /MT /DNOMINMAX "/I$AshitaSdk" "/I$detours" /Fobuild/test_request.obj /Febuild/test_request.exe src/test_request.cpp build/detours.obj build/disasm.obj build/image.obj build/modules.obj build/creatwth.obj /link user32.lib advapi32.lib
  if($LASTEXITCODE){throw 'Native integration fixture build failed'}
  & ./build/test_request.exe
  if($LASTEXITCODE){throw 'Native integration fixture failed'}
 }
} finally {Pop-Location}

