REM Ql : https://boostjp.github.io/howtobuild.html

cd boost

b2 --build-dir=build\x86 --with-regex --stagedir=x86 address-model=32 link=static runtime-link=static variant=release -j3

b2 --build-dir=build\x64 --with-regex --stagedir=x64 address-model=64 link=static runtime-link=static variant=release -j3

pause
