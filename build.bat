@echo off

echo Setting PATH
set Path=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\bin\HostX64\x64;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\VC\VCPackages;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\TestWindow;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer;C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\bin\Roslyn;C:\Program Files\Microsoft Visual Studio\2022\Community\Team Tools\Performance Tools\x64;C:\Program Files\Microsoft Visual Studio\2022\Community\Team Tools\Performance Tools;C:\Program Files (x86)\Microsoft SDKs\Windows\v10.0A\bin\NETFX 4.8 Tools\x64\;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\FSharp\Tools;C:\Program Files (x86)\Windows Kits\10\bin\10.0.22000.0\\x64;C:\Program Files (x86)\Windows Kits\10\bin\\x64;C:\Program Files\Microsoft Visual Studio\2022\Community\\MSBuild\Current\Bin\amd64;C:\Windows\Microsoft.NET\Framework64\v4.0.30319;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\;C:\Program Files (x86)\VMware\VMware Workstation\bin\;C:\Program Files (x86)\Common Files\Oracle\Java\javapath;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem;C:\Windows\System32\WindowsPowerShell\v1.0\;C:\Windows\System32\OpenSSH\;C:\Program Files\Git\cmd;C:\Program Files\Git\mingw64\bin;C:\Program Files\Git\usr\bin;C:\Program Files\PuTTY\;C:\Program Files\dotnet\;C:\Program Files\Microsoft SQL Server\150\Tools\Binn\;C:\Program Files (x86)\HP\Common\HPDestPlgIn\;C:\Users\mux\AppData\Local\Microsoft\WindowsApps;C:\Program Files (x86)\HP\Common\HPDestPlgIn\;c:\oracle\instantclient_21_9\;C:\Users\mux\.dotnet\tools;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\VC\Linux\bin\ConnectionManagerExe

::set Platform=x64
echo Setting VS variables
set LIB=LIB=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\ATLMFC\lib\x64;C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\lib\x64;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8\lib\um\x64;C:\Program Files (x86)\Windows Kits\10\lib\10.0.22000.0\ucrt\x64;C:\Program Files (x86)\Windows Kits\10\\lib\10.0.22000.0\\um\x64

set INCLUDE=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\include;C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.38.33130\ATLMFC\include;C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\VS\include;C:\Program Files (x86)\Windows Kits\10\include\10.0.22000.0\ucrt;C:\Program Files (x86)\Windows Kits\10\\include\10.0.22000.0\\um;C:\Program Files (x86)\Windows Kits\10\\include\10.0.22000.0\\shared;C:\Program Files (x86)\Windows Kits\10\\include\10.0.22000.0\\winrt;C:\Program Files (x86)\Windows Kits\10\\include\10.0.22000.0\\cppwinrt;C:\Program Files (x86)\Windows Kits\NETFXSDK\4.8\include\um


set scriptpath=%~dp0
set filename=%1

echo Script: path: %scriptpath%%filename%
::/showIncludes - shows inlcude files
echo Removing old PDB files
del /q %scriptpath%build\*.obj
del /q %scriptpath%build\*.exe


echo Current directory: %CD%
rc /r %scriptpath%\resources\resources.rc

:: /GS (Buffer Security Check) - alternative to gcc -fsanitize=safe-stack
cl /W4 /wd4146 /wd4245 /RTCcsu  /GS /TC /Zi /c %scriptpath%\code\%filename%.c /Fo%scriptpath%build\ /Fd%scriptpath%build\%filename%.pdb /Fe%scriptpath%build

:: /CETCOMPAT Shadow Stack compatible executable
link -incremental:no /CETCOMPAT /DEBUG %scriptpath%\build\*.obj /SUBSYSTEM:windows /OUT:%scriptpath%\build\%filename%.exe user32.lib shell32.lib gdi32.lib Shlwapi.lib %scriptpath%\resources\resources.res

