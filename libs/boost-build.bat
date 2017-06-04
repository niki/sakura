REM éQçl : https://boostjp.github.io/howtobuild.html

call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"

cd boost

rem b2 --build-dir=build\x86 --with-regex --stagedir=x86 address-model=32 link=static runtime-link=static variant=release -j5
rem b2 --build-dir=build\x86 --with-regex --stagedir=x86 address-model=32 link=static runtime-link=shared variant=release -j5
b2 --build-dir=build\x86 --with-regex --stagedir=. address-model=32 link=static runtime-link=shared --layout=system variant=release -j5

