@echo off

set defn=-D_CRT_SECURE_NO_WARNINGS
set warn=-wd4530 -wd4820 -wd4996 -wd4577 -wd5045 -wd4710 -wd4711 -wd4774 -wd4514
set expm=-experimental:external /external:anglebrackets -external:W0
set incl=-I..\
set opts=-std:c++17 -FC -GR- -EHa- -nologo -Zi -Wall -WX %warn% %defn% %expm% %incl%
set code=%cd%

if not exist "bin" mkdir bin
pushd bin
cl %opts% %code%\src\codegen_unity.cpp -Fecodegen.exe -Fdcodegen.pdb
popd
