@echo off
if not exist "%USERPROFILE%\.vscode\extensions\x65" mkdir %USERPROFILE%\.vscode\extensions\x65
xcopy %~dp0*.* %USERPROFILE%\.vscode\extensions\x65 /EXCLUDE:%~dp0\install.bat /Q /Y

