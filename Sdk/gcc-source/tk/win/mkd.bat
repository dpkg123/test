@echo off
rem RCS: @(#) $Id: mkd.bat,v 1.4.6.1 2000/05/04 21:26:31 spolk Exp $

if exist %1\tag.txt goto end

if "%OS%" == "Windows_NT" goto winnt

md %1
if errorlevel 1 goto end

goto success

:winnt
md %1
if errorlevel 1 goto end

:success
echo TAG >%1\tag.txt
echo created directory %1

:end

