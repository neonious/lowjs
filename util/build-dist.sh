#!/bin/bash

DIST_NAME=lowjs-`uname | tr A-Z a-z`-`uname -m`-`git show -s --format=%cd --date=format:%Y%m%d`_`git rev-parse --short HEAD`



echo $DIST_NAME
    rm -rf dist $DIST_NAME $DIST_NAME.tar $DIST_NAME.tar.gz
        mkdir $DIST_NAME
        cp -r LICENSE README.md $DIST_NAME


mkdir $DIST_NAME/{bin,lib}
cp bin/low $DIST_NAME/lib/low-exe
gcc util/loader.c -o $DIST_NAME/bin/low -static -O3
        strip $DIST_NAME/bin/low
        strip $DIST_NAME/lib/low-exe

    deps=$(ldd bin/low | awk 'BEGIN{ORS=" "}$1 ~/^\//{print $1}$3~/^\//{print $3}' | sed 's/,$/\n/')
    for dep in $deps
    do
        cp "$dep" $DIST_NAME/lib
    done
    mv $DIST_NAME/lib/ld-musl* lib/low
    chmod 755 $DIST_NAME/lib/*
    cp -r lib/* $DIST_NAME/lib




exit
        rm $DIST_NAME/lib/BUILT
        tar -c $DIST_NAME > $DIST_NAME.tar
        gzip $DIST_NAME.tar
        mkdir dist
        mv $DIST_NAME.tar.gz dist
        rm -rf $DIST_NAME $DIST_NAME.tar

