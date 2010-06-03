#!/bin/sh
sed -i

major=`head -n 1 version.h | sed 's/.*"\([0-9]*\)\.\([0-9]*\).*/\1/g'`
minor=`head -n 1 version.h | sed 's/.*"\([0-9]*\)\.\([0-9]*\).*/\2/g'`
revision=`head -n 1 revision.h | sed 's/.*"\([0-9]*\)".*/\1/g'`

sed 's/@major@/'$major'/' "$1" > "$2"
sed -i 's/@minor@/'$minor'/' "$2"
sed -i 's/@revision@/'$revision'/' "$2"
