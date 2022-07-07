#!/usr/bin/env bash
#!/bin/bash

F_MOD="-rw-r--r--"
D_MOD="drwxr-xr-x"
ROOT_DIR="test"

BLUE_TXT="$(tput setaf 6)"
WHITE_TXT="$(tput setaf 7)"
GREEN_TXT="$(tput setaf 2)"
RED_TXT="$(tput setaf 1)"

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

test_mkdir() {
    local op=$1
    local mode=$2
    local nlink=$3
    local filename=$4
    local expected=$5
    local depth=$6
    local test_count=1
    local test_passed=0

    sudo sh -c "$op"

    # Check whether file metadata is correct
    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$filename' | wc -l")
    check_filetype $expected $result2 "MKDIR $filename"
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

test_write_file() {
    local op=$1
    local mode=$2
    local nlink=$3
    local file_name=$4
    local old_msg=$5
    local new_msg=$6
    local test_count=2
    local test_passed=0

    # check whether data of file is correct
    local result=$(sudo sh -c 'cat test/'$file_name'')

    check_output "$old_msg" "$result" "WRITE FILE $file_name BEFORE"
    ret=$?
    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    sudo sh -c "$op"

    # check whether data of file is correct
    local result=$(sudo sh -c 'cat test/'$file_name'')

    check_output "$new_msg" "$result" "WRITE FILE $file_name AFTER"
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

test_mkdir_depth_1() {
    local test_count=1
    local test_passed=0
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)MKDIR DEPTH_1 TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "MKDIR DEPTH 1 PASSED: "$test_passed"/"$test_count""
}

test_mkdir_depth_n() {
    local test_passed=0
    local test_count=$[RANDOM%5+2]
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)MKDIR DEPTH N TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=2 ; i<=$test_count ; i++));
    do
        local tmp_dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        dirname+="/"
        dirname+=$tmp_dirname



        test_mkdir 'mkdir test/'$dirname'' $D_MOD "3" $tmp_dirname "1" $i
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi
    done

    echo "MKDIR DEPTH N PASSED: "$test_passed"/"$test_count""
}

test_mkdir_toolong() {
    local test_count=1
    local test_passed=0
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 256)

    echo "$(tput setaf 6)MKDIR TOOLONGNAME TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "0" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "MKDIR TOOLONGNAME PASSED: "$test_passed"/"$test_count""
}


test_create_file_empty() {
    local test_count=1
    local test_passed=0
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$BLUE_TXT CREATE FILE EMPTY TESTS: "$test_count" $WHITE_TXT"

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "CREATE FILE EMPTY: "$test_passed"/"$test_count""
}

test_create_file_nonempty() {
    local test_count=1
    local test_passed=0
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)CREATE FILE NONEMPTY TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'echo "'$msg'" > test/'$filename'' $F_MOD "1" $filename $msg "FILE NONEMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "CREATE FILE NONEMPTY PASSED: "$test_passed"/"$test_count""
}

test_create_file_toolong() {
    local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 256)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 256)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_passed=0
    local test_count=1

    echo "$(tput setaf 6)CREATE FILE TOOLONG TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename1'' $F_MOD "1" $filename1 "" "FILE EMPTY" $filename1 "0"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "CREATE FILE TOOLONG PASSED: "$test_passed"/"$test_count""
}

test_create_already_exist() {
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_passed=0
    local test_count=2

    echo "$(tput setaf 6)CREATE FILE ALREADYEXISTS TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "CREATE FILE ALREADYEXISTS: "$test_passed "/" $test_count""
}

test_create_file_subdir_1() {
    local test_count=3
    local test_passed=0
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename3=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)CREATE FILE SUBDIR 1 TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'touch test/'$dirname'/'$filename1'' $F_MOD "1" "$dirname/$filename1" "" "FILE EMPTY" "$filename1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "CREATE FILE SUBDIR 1 PASSED: "$test_passed"/"$test_count""
}

test_create_nfiles_subdir_1() {

    local test_passed=0
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local random_number=$[RANDOM%15+14]
    local test_count=$((1 + 2 * $random_number))

    echo "$(tput setaf 6)CREATE FILES SUBDIR DEPTH 1 TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=1 ; i<=$random_number ; i++));
    do
        local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        test_create_file 'touch test/'$dirname'/'$filename1'' $F_MOD "1" "$dirname/$filename1" "" "FILE EMPTY" "$filename1" "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi
    done
    echo "CREATE FILE SUBDIR 1 PASSED: "$test_passed"/"$test_count""
}

test_create_file_subdir_n() {
    local test_passed=0
    local random_number=$[RANDOM%6+5]
    local test_count=$(($random_number * 3 + 1))
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)CREATE FILES SUBDIR DEPTH N TESTS: "$random_number"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=1 ; i<=$random_number ; i++));
    do
        local tmp_dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        dirname+="/"
        dirname+=$tmp_dirname

        test_mkdir 'mkdir test/'$dirname'' $D_MOD "3" $tmp_dirname "1" $i
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        test_create_file 'touch test/'$dirname'/'$filename1'' $F_MOD "1" "$dirname/$filename1" "" "FILE EMPTY" "$filename1" "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)

        test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi
    done

    echo "MKDIR DEPTH N PASSED: "$test_passed"/"$test_count""
}

test_write_small() {
    local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_passed=0
    local test_count=4

    echo "$(tput setaf 6)WRITESMALL TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename1'' $F_MOD "1" $filename1 "" "FILE EMPTY" $filename1 "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'echo "'$msg'" > test/'$filename2'' $F_MOD "1" $filename2 $msg "FILE NONEMPTY" $filename2 "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 "" $msg2
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi
    printf -v exp_msg "%s\n%s" "$msg $msg2"
    test_write_file 'echo "'$msg2'" >> test/'$filename2'' $F_MOD "1" $filename2 $msg "$exp_msg"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "WRITE SMALL: "$test_passed "/" $test_count""
}

test_write_advanced() {
    local test_passed=0
    local random_number=$[RANDOM%5+1]
    local test_count=1
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)WRITE ADVANCED TESTS: "$random_number"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=0 ; i<5 ; i++));
    do
        local tmp_dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local number_file_creations=$[RANDOM%100+10]
        test_count=$(($test_count + $number_file_creations * 2 + 1))

        dirname+="/"
        dirname+=$tmp_dirname

        test_mkdir 'mkdir test/'$dirname'' $D_MOD "3" $tmp_dirname "1" $i
        ret=$?

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        for (( j=0 ; j<$number_file_creations ; j++));
        do
            local name_length=$[RANDOM%20+3]
            local msg_length=$[RANDOM%4000+1]
            local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 16)
            local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)

            test_create_file 'touch test/'$dirname'/'$filename1'' $F_MOD "1" "$dirname/$filename1" "" "FILE EMPTY" "$filename1" "1"
            ret=$?

            if [ "$ret" == 0 ]
            then
                ((test_passed++))
            fi

            test_write_file 'echo "'$msg'" >> test/'$dirname'/'$filename1'' $F_MOD "1" $dirname'/'$filename1 "" $msg
            ret=$?

            if [ "$ret" == 0 ]
            then
                ((test_passed++))
            fi
            
        done
    done

    echo "WRITE ADVANCED PASSED: "$test_passed"/"$test_count""
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
test_create_file_nonempty
test_create_file_toolong
test_create_already_exist

test_mkdir_depth_1
test_mkdir_depth_n
test_mkdir_toolong

test_create_file_subdir_1
test_create_file_subdir_n
test_create_nfiles_subdir_1

test_write_small