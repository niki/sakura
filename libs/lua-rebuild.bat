call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"

copy Lua.Makefile.nmake .\lua\src\Makefile.nmake

cd .\lua\src

nmake /f Makefile.nmake clean
nmake /f Makefile.nmake dll

copy lua53.lib ..
copy lua53.dll ..

