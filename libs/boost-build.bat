REM éQçl : https://boostjp.github.io/howtobuild.html
REM vc14.0 C:\Program Files (x86)\Microsoft Visual Studio 14.0\Common7\Tools\vsvars32.bat
REM vc14.1 

cd boost

rem b2 --build-dir=build\x86 --with-regex --stagedir=x86 address-model=32 link=static runtime-link=static variant=release -j3
b2 --build-dir=build\x86 --with-regex --stagedir=x86 address-model=32 link=static runtime-link=shared variant=release -j3

rem b2 --build-dir=build\x64 --with-regex --stagedir=x64 address-model=64 link=static runtime-link=static variant=release -j3
b2 --build-dir=build\x64 --with-regex --stagedir=x64 address-model=64 link=static runtime-link=shared variant=release -j3

pause
