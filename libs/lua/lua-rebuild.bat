call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"

copy Lua.Makefile.nmake .\src\Makefile.nmake

pushd .\src

nmake /f Makefile.nmake clean

nmake /f Makefile.nmake dll x86=1
copy lua53.lib ..
copy lua53.dll ..

nmake /f Makefile.nmake lib x86=1
copy liblua53.lib ..

popd

pause
