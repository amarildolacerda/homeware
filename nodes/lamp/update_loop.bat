:loop
powershell .\flash.ps1 -o %s
if errorlevel 0 goto :fim
goto :loop

:fim
echo "Sucesso"