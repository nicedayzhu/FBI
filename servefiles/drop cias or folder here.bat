@ECHO OFF
chcp 65001 >nul
set /p DSIP="3DS IP: "
for %%a in (%*) do (
	if "%%~xa"==".cia" (
		python "%~dp0servefiles.py" %DSIP% "%%~a"
	)
	if "%%~xa"=="" (
		python "%~dp0servefiles.py" %DSIP% "%%~a"
	)
)
pause
