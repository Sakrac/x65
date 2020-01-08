@echo off

if NOT EXIST results mkdir results

echo Unit tests for x65 assembler >results\unittest.txt
echo >>results\unittest.txt

echo 65816 Force Addressing Mode Test >>results\unittest.txt
..\bin\x64\x65 AddrMode_65816.s -cpu=65816 -lst >>results\unittest.txt
if %errorlevel% EQU 0 goto addrmode_pass
:addrmode_fail
echo Force Addressing Mode failed
goto exit
:addrmode_pass

echo 65816 Local Label Symbol Test >>results\unittest.txt
..\bin\x64\x65 local_label.s -cpu=65816 -lst >>results\unittest.txt
if %errorlevel% EQU 0 goto locallabel_pass
:locallabel_feil
echo Local Label Symbol Test failed
goto exit
:locallabel_pass

echo 65816 OpCodes Test >>results\unittest.txt
..\bin\x64\x65 Test65816_OpCodes.s -cpu=65816 -lst >>results\unittest.txt
if %errorlevel% EQU 0 goto opcodes_65816_pass
:opcodes_65816_fail
echo 65816 OpCodes failed
goto exit
:opcodes_65816_pass

echo Merlin Macro Test >>results\unittest.txt
echo ----------------- >>results\unittest.txt
..\bin\x64\x65 merlin_macro.s results\merlin_macro.bin -bin -org=$1000 -merlin -lst >>results\unittest.txt
if %errorlevel% GTR 0 goto mermac_fail
fc /B compare\merlin_macro_cmp.bin results\merlin_macro.bin >>results\unittest.txt
if %errorlevel% EQU 0 goto mermac_pass
:mermac_fail
echo Merlin macro test failed
goto exit
:mermac_pass

echo 8BitDiff Test >>results\unittest.txt
echo ------------- >>results\unittest.txt
..\bin\x64\x65 8BitDiff_6502.s results\8BitDiff_6502.prg -kickasm -sym results\8BitDiff_6502.sym -lst >>results\unittest.txt
if %errorlevel% GTR 0 goto 8BitDiff_6502_fail
fc /B compare\8BitDiff_6502_cmp.prg results\8BitDiff_6502.prg >>results\unittest.txt
if %errorlevel% EQU 0 goto 8BitDiff_6502_pass
:8BitDiff_6502_fail
echo 8BitDiff_6502 test failed
goto exit
:8BitDiff_6502_pass

echo Alias Test >>results\unittest.txt
echo ---------- >>results\unittest.txt
..\bin\x64\x65 alias_test.s results\alias_test.prg -sym results\alias_test.sym -lst >>results\unittest.txt
if %errorlevel% GTR 0 goto alias_test_fail
fc /B compare\alias_test_cmp.prg results\alias_test.prg >>results\unittest.txt
if %errorlevel% EQU 0 goto alias_test_pass
:alias_test_fail
echo Alias test failed
goto exit
:alias_test_pass

echo x65 Scope Test >>results\unittest.txt
..\bin\x64\x65 x65scope.s results\x65scope.prg -lst -sym results\x65scope.sym >>results\unittest.txt
if %errorlevel% GTR 0 goto x65scope_test_fail
fc /B compare\x65scope.prg results\x65scope.prg >>results\unittest.txt
if %errorlevel% GTR 0 goto x65scope_test_fail
fc compare\x65scope.sym results\x65scope.sym >>results\unittest.txt
if %errorlevel% EQU 0 goto x65scope_test_pass
:x65scope_test_fail
echo x65 Scope Test failed
goto exit
:x65scope_test_pass

echo Merlin LUP Test >>results\unittest.txt
echo --------------- >>results\unittest.txt
..\bin\x64\x65 merlin_lup.s results\merlin_lup.bin -bin -org=$1000 -merlin -lst >>results\unittest.txt
if %errorlevel% GTR 0 goto merlup_fail
fc /B compare\merlin_lup_cmp.bin results\merlin_lup.bin >>results\unittest.txt
if %errorlevel% EQU 0 goto merlup_pass
:merlup_fail
echo Merlin LUP test failed
goto exit
:merlup_pass

echo CA65 directives Test >>results\unittest.txt
..\bin\x64\x65.exe ca65directive.s -lst -endm >>results\unittest.txt
if %errorlevel% GTR 0 goto ca65_fail
rem check data here when relevant
if %errorlevel% EQU 0 goto ca65_pass
:ca65_fail
echo CA65 directives failed
goto exit
:ca65_pass

rem REVIEW MACROS!
rem echo x65macro.i Test >>results\unittest.txt
rem echo --------------- >>results\unittest.txt
rem ..\bin\x64\x65 x65macro_test.s results\x65macro_test.prg -org=$1000 -sym results\x65macro_test.sym -lst >>results\unittest.txt
rem if %errorlevel% GTR 0 goto x65macro_test_fail
rem fc /B compare\x65macro_test_cmp.prg results\x65macro_test.prg >>results\unittest.txt
rem if %errorlevel% EQU 0 goto x65macro_test_pass
rem :x65macro_test_fail
rem echo x65macro.i test failed
rem goto exit
rem :x65macro_test_pass


echo All Tests Passed
goto exit


:exit
