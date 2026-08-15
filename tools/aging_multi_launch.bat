@echo off
REM エージング用: 別アプリとして複数のsakura.exeを多重起動する
REM 各インスタンスは -PROF= に別名を渡すことで、mutex/共有メモリ/iniが分離され
REM 完全に独立したプロセスとして起動する（同名を指定すると既存プロセスに
REM ウィンドウが追加されるだけになるので注意）。
REM
REM 使い方: aging_multi_launch.bat [起動数] [sakura.exeのパス]
setlocal enabledelayedexpansion

set COUNT=%1
if "%COUNT%"=="" set COUNT=5

set EXE=%2
if "%EXE%"=="" set EXE=%~dp0Publish\sakura.exe

for /L %%i in (1,1,%COUNT%) do (
    start "" "%EXE%" -PROF=age%%i
)

endlocal
