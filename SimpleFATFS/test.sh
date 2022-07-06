#!/usr/bin/env bash
#!/bin/bash

F_MOD="-rw-r--r--"
D_MOD="drwxr-xr-x"
ROOT_DIR="test"

check_output() {
    local message=$1
    local result=$2
    local result_msg=$3

    if [ "$message" == "$result" ] 
    then
        echo -e "    DATA CHECK "$result_msg": $(tput setaf 2)PASSED $(tput setaf 7)"
        return 0
    else
        echo -e "    DATA CHECK "$result_msg": $(tput setaf 1)FAILED $(tput setaf 7)"
        diff <(echo "$message") <(echo "$result")
        return -1
    fi
}

check_filetype() {
    local expected=$1
    local result=$2
    local msg=$3

    if [ "$result" == "$expected" ]
    then
        echo -e "    FILETYPE "$msg" ON "$depth": $(tput setaf 2)PASSED $(tput setaf 7)"
        return 0
    else
        echo -e "    FILETYPE "$msg" ON "$depth": $(tput setaf 1)FAILED $(tput setaf 7)"
        return 1
    fi
}

test_create_file() {
    local op=$1
    local mode=$2
    local nlink=$3
    local abs_file_name=$4
    local message=$5
    local result_msg=$6
    local rel_file_name=$7
    local expected=$8
    local test_count=2
    local test_passed=0

    sudo sh -c "$op"

    # check whether data of file is correct
    local result=$(sudo sh -c 'cat test/'$abs_file_name'')
    check_output "$message" "$result" "CREATE FILE $rel_file_name DATA"
    ret=$?
    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    # Check whether file metadata is correct
    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$nlink'| grep -e '$rel_file_name' | wc -l")
    check_filetype $expected $result2 "CREATE FILE $rel_file_name METADATA"
    ret=$?
    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    if [ "$test_passed" == "$test_count" ]
    then 
        return 0
    else
        return 1
    fi
}

test_create_file_empty() {
    local test_count=1
    local test_passed=0
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)CREATE FILE EMPTY TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "CREATE FILE EMPTY: "$test_passed"/"$test_count""
}

test_create_root() {
    local mode=$1
    local nlink=$2
    local file_name=$3
    local root_dir=$4
    local test_count=1
    local test_passed=0

    init

    echo "$(tput setaf 6)CREATE ROOT TESTS: $test_count$(tput setaf 7)" 

    local result=$(sudo sh -c "ls -l | grep -e 'basicftfs.ko' | wc -l")
    if [ "$result" == 1 ]
    then
        ((test_passed++))
    fi

    echo "CREATE ROOT PASSED: "$test_passed"/"$test_count""

    if [ "$test_passed" == "$test_count" ]
    then 
        echo "Start tests"
        return 0
    else
        echo "Terminate"
        return 1
    fi
}

test_create_file_empty() {
    local test_count=1
    local test_passed=0
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)CREATE FILE EMPTY TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "CREATE FILE EMPTY: "$test_passed"/"$test_count""
}

init() {
    make
    sudo insmod basicftfs.ko
    mkdir -p test
    dd if=/dev/zero of=test.img bs=1M count=50
    ./mkfs.basicftfs test.img
    sudo mount -o loop -t basicftfs test.img test
}

test_create_root $D_MOD "2" "test" "root"
test_create_file_empty