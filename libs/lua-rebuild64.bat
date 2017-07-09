call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"

copy Lua.Makefile.nmake .\lua\src\Makefile.nmake

cd .\lua\src

nmake /f Makefile.nmake clean

rem nmake /f Makefile.nmake dll
rem copy lua53.lib ..
rem copy lua53.dll ..

nmake /f Makefile.nmake lib
copy liblua53.lib ..\lua53_64.lib
