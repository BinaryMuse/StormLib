@echo off
rem This BAT file updates the ZIP file that is to be uploaded to web
rem Only use when both 32-bit and 64-bit are properly compiled

echo Creating stormlib_stream.zip ...
cd \Ladik\Appdir
zip.exe -ur9 ..\WWW\web\download\stormlib_stream.zip StormLib\doc\*
zip.exe -ur9 ..\WWW\web\download\stormlib_stream.zip StormLib\src\*
zip.exe -ur9 ..\WWW\web\download\stormlib_stream.zip StormLib\storm_dll\*
zip.exe -ur9 ..\WWW\web\download\stormlib_stream.zip StormLib\StormLib.xcodeproj\*
zip.exe -ur9 ..\WWW\web\download\stormlib_stream.zip StormLib\stormlib_dll\*
zip.exe -ur9 ..\WWW\web\download\stormlib_stream.zip StormLib\test\*
zip.exe -u9  ..\WWW\web\download\stormlib_stream.zip StormLib\makefile*
zip.exe -u9  ..\WWW\web\download\stormlib_stream.zip StormLib\*.bat
zip.exe -u9  ..\WWW\web\download\stormlib_stream.zip StormLib\*.sln
zip.exe -u9  ..\WWW\web\download\stormlib_stream.zip StormLib\*.vcproj
zip.exe -u9  ..\WWW\web\download\stormlib_stream.zip StormLib\*.plist
zip.exe -u9  ..\WWW\web\download\stormlib_stream.zip StormLib\*.txt
echo.

echo Press any key to exit ...
pause >nul
