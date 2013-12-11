#!/bin/bash

hostdir=/h/rapcad
build=$hostdir/build.log
error=$hostdir/error.log

pushd /c/rapcad

git pull \
  > $build 2> $error

version=$(cat VERSION)

echo Building RapCAD v$version \
  >> $build

git reset --hard master \
  >> $build 2>> $error

git clean -df \
  >> $build 2>> $error

qmake CONFIG+=official \
  >> $build 2>> $error

mingw32-make -f Makefile.Release \
  >> $build 2>> $error

mingw32-make clean \
  >> $build 2>> $error

cp ../rapcad-dlls/* release \
  >> $build 2>> $error

makensis installer.nsi \
  >> $build 2>> $error

mv rapcad_setup.exe rapcad_$version\_setup.exe \
  >> $build 2>> $error

mv release rapcad-$version  \
  >> $build 2>> $error

7z a -tzip rapcad_$version.zip rapcad-$version \
  >> $build 2>> $error

mv rapcad_$version\_setup.exe $hostdir \
  >> $build 2>> $error

mv rapcad_$version.zip $hostdir \
  >> $build 2>> $error

popd
