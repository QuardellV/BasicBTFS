#!/usr/bin/env bash
#!/bin/bash

F_MOD="-rw-r--r--"
D_MOD="drwxr-xr-x"
ROOT_DIR="test/mnt"

mkdir -p test
sudo mount -t tmpfs -o size=20G tmpfs test
mkdir $ROOT_DIR
dd if=/dev/zero of=test/test.img bs=1 count=0 seek=15G
mkfs.btrfs test/test.img
sudo mount -o loop -t btrfs test/test.img $ROOT_DIR