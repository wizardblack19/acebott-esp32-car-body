param(
  [string]$EmscriptenRoot = 'C:\Program Files\Unity\Hub\Editor\6000.4.5f1\Editor\Data\PlaybackEngines\WebGLSupport\BuildTools\Emscripten'
)
$ErrorActionPreference = 'Stop'
$compiler = Join-Path $EmscriptenRoot 'llvm\clang++.exe'
$sysroot = Join-Path $EmscriptenRoot 'emscripten\cache\sysroot'
Push-Location (Join-Path $PSScriptRoot '..')
try {
  & $compiler '--target=wasm32-unknown-emscripten' "--sysroot=$sysroot" "-I$sysroot\include\c++\v1" '-Itests/stubs' '-I.' '-std=c++11' '-nostdlib' '-fno-exceptions' '-fno-rtti' '-Wl,--no-entry' '-Wl,--export=runTests' 'tests/body_test.cpp' '-o' 'tests/body_test.wasm'
  if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
  node tests/run.mjs
  if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
} finally { Pop-Location }
