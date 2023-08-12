#!/usr/bin/env bash

# Thank you LWSS
# https://github.com/LWSS/Fuzion/commit/a53b6c634cde0ed47b08dd587ba40a3806adf3fe

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

# Get a Random name from the build_names file.
FILENAME=$(shuf -n 1 build_names)

# Create directory if it doesn't exist
if [ ! -d "/lib/i386-linux-gnu/" ]; then
    sudo mkdir /lib/i386-linux-gnu/
fi

# In case this file exists, get another one. ( checked it works )
while [ -f "/lib/i386-linux-gnu/${FILENAME}" ]; do
    FILENAME=$(shuf -n 1 build_names)
done

# echo $FILENAME > build_id # For detaching

sudo cp "bin/libcathook.so" "/lib/i386-linux-gnu/${FILENAME}"

gdbbin="gdb"
if [ -x "gdb-arch-2021-02" ]; then
  gdbbin="./gdb-arch-2021-02"
fi

echo loading "$FILENAME" to "$proc"

$gdbbin -n -q -batch                                                                                \
    -ex "attach $proc"                                                                          \
    -ex "echo \033[1mCalling dlopen\033[0m\n"                                                   \
    -ex "call ((void*(*)(const char*, int))dlopen)(\"/lib/i386-linux-gnu/$FILENAME\", 1)"       \
    -ex "echo \033[1mCalling dlerror\033[0m\n"                                                  \
    -ex "call ((char*(*)(void))dlerror)()"                                                      \
    -ex "detach"                                                                                \
    -ex "quit"

sudo rm "/lib/i386-linux-gnu/${FILENAME}"
