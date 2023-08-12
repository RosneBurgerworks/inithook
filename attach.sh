#!/usr/bin/env bash

line=$(pidof hl2_linux)
arr=($line)
inst=$1
if [ $# == 0 ]; then
    inst=0
fi

if [ ${#arr[@]} == 0 ]; then
    echo TF2 isn\'t running!
    exit
fi

if [ $inst -gt ${#arr[@]} ] || [ $inst == ${#arr[@]} ]; then
    echo wrong index!
    exit
fi

proc=${arr[$inst]}

echo Running instances: "${arr[@]}"
echo Attaching to "$proc"

FILENAME="/tmp/.gl$(cat /dev/urandom | tr -dc 'a-zA-Z0-9' | fold -w 6 | head -n 1)"

cp "obj/bin/libcathook.so" "$FILENAME"

gdbbin="gdb"
if [ -x "gdb-arch-2021-02" ]; then
  gdbbin="./gdb-arch-2021-02"
fi

echo loading "$FILENAME" to "$proc"

$gdbbin -n -q -batch                                                        \
    -ex "attach $proc"                                                  \
    -ex "echo \033[1mCalling dlopen\033[0m\n"                           \
    -ex "call ((void*(*)(const char*, int))dlopen)(\"$FILENAME\", 1)"   \
    -ex "echo \033[1mCalling dlerror\033[0m\n"                          \
    -ex "call ((char*(*)(void))dlerror)()"                              \
    -ex "detach"                                                        \
    -ex "quit"

rm $FILENAME
