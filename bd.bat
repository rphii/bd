@ECHO OFF
ROBOCOPY %~dp0 %cd% bd.conf /xn /xc /xo /NFL /NDL /NJH /NJS /nc /ns /np > NUL
SET "GCC_CMD=gcc -Wall -O2 -D CONFIG="^<%cd%\bd.conf^>^" -o bd ^"%~dp0bd.c^""
ECHO %GCC_CMD%
%GCC_CMD%
IF %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
bd.exe %*
DEL /q bd.exe REM optional
