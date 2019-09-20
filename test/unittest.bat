@echo off

echo Unit tests for x65 assembler >unittest.txt
echo >>unittest.txt

echo 65816 OpCodes Test >>unittest.txt
..\bin\x64\x65 Test65816_OpCodes.s -cpu=65816 -lst >> unittest.txt
if %errorlevel% EQU 0 goto opcodes_65816_pass
:opcodes_65816_fail
echo 65816 OpCodes failed
goto exit
:opcodes_65816_pass


echo 65816 Force Addressing Mode Test >>unittest.txt
..\bin\x64\x65 AddrMode_65816.s -cpu=65816 -lst >> unittest.txt
if %errorlevel% EQU 0 goto addrmode_pass
:addrmode_fail
echo Force Addressing Mode failed
goto exit
:addrmode_pass

echo Merlin Macro Test >>unittest.txt
echo ----------------- >>unittest.txt
..\bin\x64\x65 merlin_macro.s merlin_macro.bin -bin -org=$1000 -merlin -lst >>unittest.txt
if %errorlevel% GTR 0 goto mermac_fail
fc /B compare\merlin_macro_cmp.bin merlin_macro.bin >>unittest.txt
if %errorlevel% EQU 0 goto mermac_pass
:mermac_fail
echo Merlin macro test failed
goto exit
:mermac_pass

echo 8BitDiff Test >>unittest.txt
echo ------------- >>unittest.txt
..\bin\x64\x65 8BitDiff_6502.s 8BitDiff_6502.prg -kickasm -sym 8BitDiff_6502.sym -lst >>unittest.txt
if %errorlevel% GTR 0 goto 8BitDiff_6502_fail
fc /B compare\8BitDiff_6502_cmp.prg 8BitDiff_6502.prg >>unittest.txt
if %errorlevel% EQU 0 goto 8BitDiff_6502_pass
:8BitDiff_6502_fail
echo 8BitDiff_6502 test failed
goto exit
:8BitDiff_6502_pass

echo Alias Test >>unittest.txt
echo ---------- >>unittest.txt
..\bin\x64\x65 alias_test.s alias_test.prg -sym alias_test.sym -lst >>unittest.txt
if %errorlevel% GTR 0 goto alias_test_fail
fc /B compare\alias_test_cmp.prg alias_test.prg >>unittest.txt
if %errorlevel% EQU 0 goto alias_test_pass
:alias_test_fail
echo Alias test failed
goto exit
:alias_test_pass

echo x65macro.i Test >>unittest.txt
echo --------------- >>unittest.txt
..\bin\x64\x65 x65macro_test.s x65macro_test.prg -org=$1000 -sym x65macro_test.sym -lst >>unittest.txt
if %errorlevel% GTR 0 goto x65macro_test_fail
fc /B compare\x65macro_test_cmp.prg x65macro_test.prg >>unittest.txt
if %errorlevel% EQU 0 goto x65macro_test_pass
:x65macro_test_fail
echo x65macro.i test failed
goto exit
:x65macro_test_pass

echo Merlin LUP Test >>unittest.txt
echo --------------- >>unittest.txt
..\bin\x64\x65 merlin_lup.s merlin_lup.bin -bin -org=$1000 -merlin -lst >>unittest.txt
if %errorlevel% GTR 0 goto merlup_fail
fc /B compare\merlin_lup_cmp.bin merlin_lup.bin >>unittest.txt
if %errorlevel% EQU 0 goto merlup_pass
:merlup_fail
echo Merlin LUP test failed
goto exit
:merlup_pass


echo All Tests Passed
goto exit


:exit
