#!/bin/bash
source config

Qt="qt-opensource-windows-x86-mingw492-5.5.1.exe"
QtSilent="QtSilentInstall.exe"
Zip="7z1509.exe"
Boost="boost_1_59_0.zip"
CMake="cmake-3.4.0-rc2-win32-x86.exe"
CGAL="CGAL-4.7-Setup.exe"

./cmd.sh mklink /d c:\\shared \\\\vboxsvr\\shared
./cmd.sh copy c:\\shared\\$Qt c:\\
./cmd.sh c:\\shared\\$QtSilent c:\\$Qt
./cmd.sh c:\\shared\\$Zip /S /D=c:\\7zip\\
./cmd.sh c:\\7zip\\7z.exe x -oc:\\ c:\\shared\\$Boost
cat << EOF > $SHAREDFOLDER/boost.bat
@echo off
call c:\\Qt\\Qt5.5.1\\5.5\\mingw492_32\\bin\\qtenv2.bat
cd c:\\boost_1_59_0\\
call bootstrap.bat mingw
.\\b2 toolset=gcc variant=release --with-thread --with-system
EOF
./cmd.sh c:\\shared\\boost.bat
./cmd.sh c:\\shared\\$CMake /S /D=c:\\cmake\\
./cmd.sh c:\\shared\\$CGAL /S /D=c:\\CGAL-4.7\\
cat << EOF > $SHAREDFOLDER/cgal.bat
@echo off
call c:\\Qt\\Qt5.5.1\\5.5\\mingw492_32\\bin\\qtenv2.bat
set PATH=%PATH%;"C:\\cmake\\bin"
set BOOST_ROOT=C:\\boost_1_59_0
cd "C:\\CGAL-4.7\\"
cmake -G"MinGW Makefiles" .
mingw32-make
EOF
./cmd.sh c:\\shared\\cgal.bat