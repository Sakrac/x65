@echo off
rmdir /S /Q %USERPROFILE%\.vscode\extensions\x65
mkdir %USERPROFILE%\.vscode\extensions\x65
xcopy %~dp0*.* %USERPROFILE%\.vscode\extensions\x65 /EXCLUDE:%~dp0\install.bat /Q /Y
