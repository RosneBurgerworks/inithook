#!/usr/bin/env bash

[[ ! -z "$SUDO_USER" ]] && user=$SUDO_USER || user=$LOGNAME

DATA_DIR=${DATA_DIR:-/opt/cathook/data}

if ! [ -d "$DATA_DIR" ]; then
    echo "Creating cathook data directory at $DATA_DIR"
    mkdir -p "$DATA_DIR"
    chown -R $user "$DATA_DIR"
    chmod -R 777 "$DATA_DIR"
fi

echo "Installing cathook data to $DATA_DIR"
rsync -avh "data/" "$DATA_DIR"
rsync -avh --ignore-existing "config_data/" "$DATA_DIR"
chown -R $user "$DATA_DIR"
chmod -R 777 "$DATA_DIR"
