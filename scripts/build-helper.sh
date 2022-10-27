#!/bin/sh

build_cc=${BUILD_CC:-gcc}
build_log=${BUILD_LOG:-/dev/null}

newcmd=$build_cc

for arg in $*
do
    newcmd="$newcmd $arg"
done

echo "Command:" >> $build_log
echo $newcmd >> $build_log
echo >> $build_log

exec $newcmd
