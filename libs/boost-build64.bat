REM éQçl : https://boostjp.github.io/howtobuild.html

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

cd boost

rem b2 --build-dir=build\x64 --with-regex --stagedir=x64 address-model=64 link=static runtime-link=static variant=release -j5
rem b2 --build-dir=build\x64 --with-regex --stagedir=x64 address-model=64 link=static runtime-link=shared variant=release -j5
b2 --build-dir=build\x64 --with-regex --stagedir=. address-model=64 link=static runtime-link=shared --layout=system variant=release -j5

pause
