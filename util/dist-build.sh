#!/bin/bash

DIST_NAME=lowjs-`uname | tr A-Z a-z`-`uname -m`-`git show -s --format=%cd --date=format:%Y%m%d`_`git rev-parse --short HEAD`

# Cleanup
rm -rf dist $DIST_NAME $DIST_NAME.tar $DIST_NAME.tar.gz

# Build directory
mkdir $DIST_NAME

if [ `uname -s` = Linux ]
then
	cp -r LICENSE README.md $DIST_NAME

	mkdir $DIST_NAME/{bin,lib}
	cp bin/low $DIST_NAME/lib/low-exe
	gcc util/dist-loader.c -o $DIST_NAME/bin/low -static -O3
	strip $DIST_NAME/bin/low
	strip $DIST_NAME/lib/low-exe
	deps=$(ldd bin/low | awk 'BEGIN{ORS=" "}$1 ~/^\//{print $1}$3~/^\//{print $3}' | sed 's/,$/\n/')
	for dep in $deps
	do
		cp "$dep" $DIST_NAME/lib
	done

	if [ ! -f $DIST_NAME/lib/ld-musl* ]
	then
		echo "This system is not based on musl (like Alpine Linux)."
		echo
		echo "A distribution with glibc does not work everywhere, as glibc opens config files on the"
		echo "system on its own. Please build on a different system."
		echo
		echo "Creating distribution failed!"

		rm -rf $DIST_NAME
		exit 1
	fi

	mv $DIST_NAME/lib/ld-musl* lib/ld-musl.so
	chmod 755 $DIST_NAME/lib/*
	cp -r lib/* $DIST_NAME/lib
else
	cp -r bin lib LICENSE README.md $DIST_NAME
	strip $DIST_NAME/bin/low
fi

rm $DIST_NAME/lib/BUILT

# Tar and zip
tar -c $DIST_NAME > $DIST_NAME.tar
gzip $DIST_NAME.tar
mkdir dist
mv $DIST_NAME.tar.gz dist
rm -rf $DIST_NAME $DIST_NAME.tar
