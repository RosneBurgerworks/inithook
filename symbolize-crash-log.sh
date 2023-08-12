#!/usr/bin/env bash

set -e

DATA_DIR=${DATA_DIR:-/opt/cathook/data}

PREFIX="$DATA_DIR/logs/crashes/"
BACKTRACEFILE=$(ls -t1 $PREFIX | grep "cathook-$USER-.*-segfault.log" |  head -n 1)
CATHOOK="obj/bin/libcathook.so"

# Test whether debug information is available for a given binary
has_debug_info() {
    readelf -S "$1" | grep -q " \(.debug_info\)\|\(.gnu_debuglink\) "
}

if [ "$BACKTRACEFILE" == "" ]; then
    echo "No crash found."
    echo ""
    exit 0
fi

cathookcrash=false
while read p; do
    lib="$(echo "$p" | cut -f 1)"
    if [ "$lib" = "cathook" ]; then
        cathookcrash=true
    fi
done < <(sed 1d "$PREFIX/$BACKTRACEFILE")
echo "$PREFIX/$BACKTRACEFILE"

if [ "$cathookcrash" == "false" ]; then
    echo "Crash was not a cathook crash!"
    echo ""
    exit 0
fi

echo "(Possible) cathook crash detected!"
echo ""

SYMBOLS=true
has_debug_info "$CATHOOK" || SYMBOLS=false

if [ $SYMBOLS == false ]; then
    echo "No debug symbols detected!"
    echo ""
    exit 0
fi

BREADCRUMBS=''

append_crumb() {
    if [ "$BREADCRUMBS" != "" ]; then
        BREADCRUMBS=",$BREADCRUMBS"
    fi
    BREADCRUMBS="$1$BREADCRUMBS"
}

while read p; do
    lib="$(echo "$p" | cut -f 1)"
    addr="$(echo "$p" | cut -f 2)"
    if [ "$lib" = "cathook" ]; then
        while read function; do
            read addrandline
            echo "cathook $function $addrandline"
            FILE="$(realpath --relative-base="${PWD}" "$(echo $addrandline | cut -f 1 -d ":")")"
            LINE="$(echo $addrandline | cut -f 2 -d ":")"
            PREFIX="filename"
            IN_APP=true
            if [ "${FILE:0:1}" = "/" ]; then
                PREFIX="abs_path"
                IN_APP=false
            fi
            if [ "$function" == "critical_error_handler(int)" ]; then
                IN_APP=false
            fi
            append_crumb "$(printf '{"%s":"%s","lineno":%s,"package":"cathook","in_app":%s,"function":"%s"}' "$PREFIX" "$FILE" "$LINE" "$IN_APP" "${function//\"/\\\"}")"
        done < <(addr2line "$addr" -iCfe $CATHOOK)
    else
        echo $p
        append_crumb "$(printf '{"package":"%s","instruction_addr":"%s","in_app":false,"function":"%s"}' "$lib" "$addr" "$addr")"
    fi
done <"$PREFIX/$BACKTRACEFILE"

