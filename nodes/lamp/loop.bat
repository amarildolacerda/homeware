:loop

if %1 == "" goto :fim
  py ..\..\telnet.py %1


goto :loop

:fim