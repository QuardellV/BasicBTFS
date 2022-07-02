#!/usr/bin/env bash
#!/bin/bash

F_MOD="-rw-r--r--"
D_MOD="drwxr-xr-x"
ROOT_DIR="test"

test_create_root() {
    local test_count=1
    local test_passed=0

    mkfs_basicftfs

    echo "$(tput setaf 6)CREATE ROOT TESTS: $test_count$(tput setaf 7)" 

    local result=$(sudo sh -c "ls -l | grep -e 'basicftfs.ko' | wc -l")
    if [ "$result" == 1 ]; then
        ((test_passed++))
    fi

    echo "CREATE ROOT PASSED: "$test_passed"/"$test_count""

    if [ "$test_passed" == "$test_count" ]; then 
        echo "Start tests"; return 0
    else
        echo "Terminate"; return 1
    fi
}

mkfs_basicftfs() {
    make
    sudo insmod basicftfs.ko
    mkdir -p test
    dd if=/dev/zero of=test.img bs=1M count=50
    ./mkfs.basicftfs test.img
    # sudo mount -o loop -t basicftfs test.img test
}

test_create_root