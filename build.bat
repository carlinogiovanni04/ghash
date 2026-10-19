@echo off
setlocal
pushd "%~dp0"

if not defined CC set CC=clang-cl

set OUT=bin\ghash_test.exe
set EXAMPLE_OUT=bin\ghash_example.exe
set FLAGS=/nologo /std:c11 /W4 /Zi /Od /I "include"

if not exist bin mkdir bin

%CC% %FLAGS% "tests\test.c" /Fe:"%OUT%" /Fo:"bin\\"
if errorlevel 1 (
    echo Build failed.
    popd
    exit /b 1
)

%CC% %FLAGS% "examples\example.c" /Fe:"%EXAMPLE_OUT%" /Fo:"bin\\"
if errorlevel 1 (
    echo Build failed.
    popd
    exit /b 1
)

echo Built %OUT%
echo Built %EXAMPLE_OUT%
popd
endlocal
