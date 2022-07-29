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

test_rmdir() {
    local op=$1
    local mode=$2
    local nlink=$3
    local filename=$4
    local expected=$5
    local test_count=2
    local test_passed=0
    
    # Check whether directory still exists
    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$filename' | wc -l")
    check_filetype $expected $result2 "RMDIR $filename BEFORE"
    ret=$?
    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    sudo sh -c "$op"

    # Check whether directory no longer exists
    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$filename' | wc -l")
    check_filetype 0 $result2 "RMDIR $filename AFTER"
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

test_rmfile() {
    local op=$1
    local mode=$2
    local nlink=$3
    local filename=$4
    local expected=$5
    local test_count=2
    local test_passed=0

    # Check whether file still exists
    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$nlink'| grep -e '$filename' | wc -l")
    check_filetype $expected $result2 "RM FILE $filename BEFORE"
    ret=$?
    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    sudo sh -c "$op"

    # Check whether file has been deleted

    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$nlink'| grep -e '$filename' | wc -l")
    check_filetype "0" $result2 "RM FILE $filename AFTER"
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

test_mvfile() {
    local op=$1
    local mode=$2
    local nlink=$3
    local old_filename=$4
    local new_filename=$5
    local expected=$6
    local new_expected=$7
    local old_expected=$8
    local abs_old_name=$9
    local abs_new_name="${10}"
    local test_count=3
    local test_passed=0

    # Check whether file still exists in old entry
    echo $old_filename
    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$abs_old_name' | wc -l")
    check_filetype $expected $result2 "MV FILE $old_filename BEFORE"
    ret=$?
    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    sudo sh -c "$op"

    # Check whether file has been created if necessary

    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$abs_new_name' | wc -l")
    check_filetype $new_expected $result2 "MV FILE $new_filename AFTER IN NEW ENTRY"
    ret=$?
    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    # check whether file has been deleted if necessary in old entry

    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$abs_old_name' | wc -l")
    check_filetype $old_expected $result2 "MV FILE $old_filename AFTER IN OLD ENTRY"
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

    local result=$(sudo sh -c "ls -l | grep -e 'basicbtfs.ko' | wc -l")
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
    local random_number=500
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

test_rmdir_empty() {
    local test_count=3
    local test_passed=0
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)RMDIR EMPTY TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi



    test_rmdir 'rm -rf test/'$dirname'' $D_MOD "2" $dirname "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmdir 'rm -rf test/'$dirname'' $D_MOD "2" $dirname "0"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "RMDIR EMPTY PASSED: "$test_passed"/"$test_count""
}

test_rmdir_nonempty() {
    local test_count=7
    local test_passed=0
    local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)RMDIR NONEMPTY TESTS: "$test_count"$(tput setaf 7)"

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

    test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmdir 'rm -rf test/'$dirname'' $D_MOD "2" $dirname "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmdir 'rm -rf test/'$dirname'' $D_MOD "2" $dirname "0"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$nlink'| grep -e '$filename1' | wc -l")

    if [ "$result2" == 0 ]
    then
        ((test_passed++))
    fi

    local result2=$(sudo sh -c "ls test -lR | grep -e '$mode' | grep -e '$nlink'| grep -e '$filename2' | wc -l")

    if [ "$result2" == 0 ]
    then
        ((test_passed++))
    fi

    echo "RMDIR NONEMPTY PASSED: "$test_passed"/"$test_count""
}

test_rm_empty() {
    local test_count=5
    local test_passed=0
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename3=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)REMOVE FILE EMPTY TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'touch test/'$filename2'' $F_MOD "1" $filename2 "" "FILE EMPTY" $filename2 "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'touch test/'$filename3'' $F_MOD "1" $filename3 "" "FILE EMPTY" $filename3 "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmfile 'rm -rf test/'$filename'' $F_MOD "1" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmfile 'rm -rf test/'$filename'' $F_MOD "1" $filename "0"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "REMOVE FILE EMPTY: "$test_passed"/"$test_count""
}

test_rm_small() {
    local test_count=3
    local test_passed=0
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 100)

    echo "$(tput setaf 6)REMOVE FILE SMALL TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'echo "'$msg'" > test/'$filename'' $F_MOD "1" $filename $msg "FILE NONEMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmfile 'rm -rf test/'$filename'' $F_MOD "1" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmfile 'rm -rf test/'$filename'' $F_MOD "1" $filename "0"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "REMOVE FILE SMALL: "$test_passed"/"$test_count""
}

test_rm_large() {
    local test_count=3
    local test_passed=0
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 100000)

    echo "$(tput setaf 6)REMOVE FILE LARGE TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'echo "'$msg'" > test/'$filename'' $F_MOD "1" $filename $msg "FILE NONEMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmfile 'rm -rf test/'$filename'' $F_MOD "1" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_rmfile 'rm -rf test/'$filename'' $F_MOD "1" $filename "0"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "REMOVE FILE LARGE: "$test_passed"/"$test_count""
}

test_rm_in_subdir() {
    local test_passed=0
    local random_number=$[RANDOM%3+2]
    local test_count=$(($random_number * 5 + 1))
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)REMOVE IN SUBDIR TESTS: "$random_number"$(tput setaf 7)"

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

        test_rmfile 'rm -rf test/'$dirname'/'$filename1'' $F_MOD "1" $filename1 "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        test_rmfile 'rm -rf test/'$dirname'/'$filename2'' $F_MOD "1" $filename2 "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi
    done

    echo "REMOVE IN SUBDIR PASSED: "$test_passed"/"$test_count""
}

test_rm_subdir() {
    local test_passed=0
    local random_number=$[RANDOM%6+2]
    local test_count=$(($random_number * 5 + 2))
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local dirname_copy=$dirname

    echo "$(tput setaf 6)REMOVE SUBDIR TESTS: "$random_number"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=1 ; i<=$random_number ; i++));
    do
        local tmp_dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local tmp_dirname2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local filename1=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local dirname2=$dirname
        local dirname2+="/"
        local dirname2+=$tmp_dirname2

        dirname+="/"
        dirname+=$tmp_dirname

        test_mkdir 'mkdir test/'$dirname'' $D_MOD "3" $tmp_dirname "1" $i
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        test_mkdir 'mkdir test/'$dirname2'' $D_MOD "3" $tmp_dirname2 "1" $i
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

        test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        test_rmdir 'rm -rf test/'$dirname2'' $D_MOD "2" $tmp_dirname2 "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

    done

    test_rmdir 'rm -rf test/'$dirname_copy'' $D_MOD "2" $dirname_copy "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

    echo "REMOVE SUBDIR PASSED: "$test_passed"/"$test_count""
}

test_move_file_simple() {
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_count=2
    local test_passed=0

    echo "$(tput setaf 6)MOVE FILE EMPTY TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_mvfile 'mv test/'$filename' test/'$filename2'' $F_MOD "1" $filename $filename2 "1" "1" "0" $filename $filename2
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "MOVE FILE EMPTY TESTS: "$test_passed"/"$test_count""
}

test_move_file_noreplace() {
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_passed=0
    local test_count=3

    echo "$(tput setaf 6)MOVE_FILE_NOREPLACE TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'touch test/'$filename2'' $F_MOD "1" $filename2 "" "FILE EMPTY" $filename2 "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_mvfile "mv -n test/'$filename' test/'$filename2'" $F_MOD "1" $filename $filename2 "1" "1" "1" $filename $filename2
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "MOVE_FILE_NOREPLACE TESTS: "$test_passed"/"$test_count""

}

test_move_file_overwrite() {
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_passed=0
    local test_count=3

    echo "$(tput setaf 6)MOVE_FILE_NOREPLACE TESTS: "$test_count"$(tput setaf 7)"

    test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'touch test/'$filename2'' $F_MOD "1" $filename2 "" "FILE EMPTY" $filename2 "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi


    test_mvfile "mv test/'$filename' test/'$filename2'" $F_MOD "1" $filename $filename2 "1" "1" "0" $filename $filename2
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "MOVE_FILE_NOREPLACE TESTS: "$test_passed"/"$test_count""
}

test_move_file_to_other_dir() {
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local dirname2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local filename2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local msg2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_passed=0
    local test_count=5

    echo "$(tput setaf 6)MOVE_FILE_TO_OTHER_DIR TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_mkdir 'mkdir test/'$dirname2'' $D_MOD "2" $dirname2 "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'touch test/'$dirname'/'$filename'' $F_MOD "1" "$dirname/$filename" "" "FILE EMPTY" "$filename" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_create_file 'touch test/'$dirname2'/'$filename2'' $F_MOD "1" "$dirname2/$filename2" "" "FILE EMPTY" "$filename2" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi


    test_mvfile "mv test/'$dirname'/'$filename' test/'$dirname2'/'$filename2'" $F_MOD "1" "$filename" "$filename2" "1" "1" "0" "$filename" "$filename2"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "MOVE_FILE_NOREPLACE TESTS: "$test_passed"/"$test_count""
}

test_move_dir_simple() {
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local dirname2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_count=2
    local test_passed=0

    echo "$(tput setaf 6)MOVE DIR EMPTY TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_mvfile 'mv test/'$dirname' test/'$dirname2'' $D_MOD "2" $dirname $dirname2 "1" "1" "0" $dirname $dirname2
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "MOVE DIR EMPTY TESTS: "$test_passed"/"$test_count""
}

test_move_dir_in_dir() {
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local dirname2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
    local test_passed=0
    local test_count=3

    echo "$(tput setaf 6)MOVE_DIR_IN_DIR TESTS: "$test_count"$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_mkdir 'mkdir test/'$dirname2'' $D_MOD "2" $dirname2 "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    test_mvfile 'mv -n test/'$dirname' test/'$dirname2'' $D_MOD "2" $dirname $dirname2 "1" "1" "1" $dirname $dirname2
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    echo "MOVE_DIR_IN_DIR TESTS: "$test_passed"/"$test_count""
}

test_move_advanced() {
    local test_passed=0
    local random_number=$[RANDOM%5+1]
    local test_count=1
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)MOVE_ADVANCED $(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "0"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=1 ; i<=6 ; i++));
    do
        echo DEPTH $i
        local tmp_dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local tmp_dirname2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local tmp_dirname3=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local tmp_dirname4=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local tmp_base=$dirname
        local dirname2=""; local dirname2+=$dirname; local dirname2+="/"; local dirname2+=$tmp_dirname2
        local dirname3=""; local dirname3+=$dirname; local dirname3+="/"; local dirname3+=$tmp_dirname3
        local dirname4=""; local dirname4+=$dirname; local dirname4+="/"; local dirname4+=$tmp_dirname4

        local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local number_file_creations=$[RANDOM%6+5]
        test_count=$(($test_count + $number_file_creations * 3 + 6))

        dirname+="/"
        dirname+=$tmp_dirname

        test_mkdir 'mkdir test/'$dirname'' $D_MOD "3" $tmp_dirname "1" $i
        ret=$?

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        test_mkdir 'mkdir test/'$dirname2'' $D_MOD "3" $tmp_dirname2 "1" $i
        ret=$?

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        test_mkdir 'mkdir test/'$dirname3'' $D_MOD "3" $tmp_dirname3 "1" $i
        ret=$?

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        test_mkdir 'mkdir test/'$dirname4'' $D_MOD "3" $tmp_dirname4 "1" $i
        ret=$?

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        for (( j=0 ; j<$number_file_creations ; j++));
        do
            echo iteration creation $j
            local name_length1=$[RANDOM%32+5]
            local name_length2=$[RANDOM%32+5]
            local msg_length=$[RANDOM%10000+5]
            local msg="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
            local filename1=$(tr -dc A-Za-z </dev/urandom | head -c $name_length1)
            echo current filename: $filename1
            local filename2=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
            local filename3=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
            local filename4=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
            local remainder2=$(($i % 2))
            local remainder3=$(($i % 3))
            local remainder4=$(($i % 4))
            local remainder5=$(($i % 5))

            if [ "$remainder2" == 0 ]
            then
                test_create_file 'echo "'$msg'" > test/'$filename1'' $F_MOD "1" $filename1 $msg "FILE NONEMPTY" $filename1 "1"
                ret1=$?

                test_create_file 'touch test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" "" "FILE EMPTY" "$filename2" "1"
                ret2=$?

                test_create_file 'echo "'$msg'" > test/'$dirname2'/'$filename3'' $F_MOD "1" "$dirname2/$filename3" $msg "FILE NONEMPTY" "$filename3" "1"
                ret3=$?

            else
                test_create_file 'touch test/'$filename1'' $F_MOD "1" $filename1 "" "FILE EMPTY" $filename1 "1"
                ret1=$?

                test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
                ret2=$?

                test_create_file 'touch test/'$dirname3'/'$filename4'' $F_MOD "1" "$dirname3/$filename4" "" "FILE EMPTY" "$filename4" "1"
                ret3=$?
            fi

            if [ "$ret1" == 0 ]
                then
                    ((test_passed++))
            fi

            if [ "$ret2" == 0 ]
                then
                    ((test_passed++))
            fi

            if [ "$ret3" == 0 ]
                then
                    ((test_passed++))
            fi

            if [ "$remainder3" == 0 ]
            then
                ((test_count=test_count+3))
                local msg_length2=$[RANDOM%10000+5]
                local msg2="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
                local filename5=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
                
                if [ "$remainder2" == 0 ]
                then
                    printf -v exp_msg "%s\n%s" "$msg $msg2"
                    test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 $msg "$exp_msg"
                    ret1=$?

                    test_write_file 'echo "'$msg'" >> test/'$dirname'/'$filename2'' $F_MOD "1" $dirname'/'$filename2 "" $msg
                    ret2=$?

                    test_mvfile "mv test/'$filename1' test/'$dirname2'/'$filename5'" $F_MOD "1" $filename1 $filename5 "1" "1" "0" $filename1 $filename5
                    ret3=$?
                else
                    test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 "" $msg2
                    ret1=$?

                    printf -v exp_msg "%s\n%s" "$msg $msg2"
                    test_write_file 'echo "'$msg2'" >> test/'$dirname3'/'$filename4'' $F_MOD "1" $dirname3'/'$filename4 $msg "$exp_msg"
                    ret2=$?

                    test_mvfile "mv test/'$filename1' test/'$dirname3'/'$filename4'" $F_MOD "1" $filename1 $filename4 "1" "1" "0" $filename1 $filename4
                    ret3=$?
                fi


                if [ "$ret1" == 0 ]
                then
                    ((test_passed++))
                fi

                if [ "$ret2" == 0 ]
                then
                    ((test_passed++))
                fi

                if [ "$ret3" == 0 ]
                then
                    ((test_passed++))
                fi
            fi

        done

        test_rmdir 'rm -rf test/'$dirname2'' $D_MOD "2" $tmp_dirname2 "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        test_rmdir 'rm -rf test/'$dirname2'' $D_MOD "2" $tmp_dirname2 "0"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        test_mvfile 'mv test/'$dirname3' test/'$dirname4'' $D_MOD "2" "$dirname3" "$dirname4" "1" "1" "1" "$tmp_dirname3" "$tmp_dirname4"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi


    done

    echo "MOVE_ADVANCED: "$test_passed"/"$test_count""
}

# make n files, pick random file and write something random in it. also delete random files
# remove, make, list, write on root
test_advanced_sequence_1() {
    local random_number=$[RANDOM%6+5]
    local test_count=$random_number
    local test_passed=0

    echo "$(tput setaf 6)ADVANCED SEQUENCE_1 TESTS: $(tput setaf 7)"

    for (( i=0 ; i<$random_number ; i++))
    do
        local name_length=$[RANDOM%32+5]
        local msg_length=$[RANDOM%100+5]
        local msg="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
        local filename=$(tr -dc A-Za-z </dev/urandom | head -c $name_length)
        local remainder2=$(($i % 2))
        local remainder3=$(($i % 3))
        local remainder5=$(($i % 5))

        if [ "$remainder2" == 0 ]
        then
            test_create_file 'echo "'$msg'" > test/'$filename'' $F_MOD "1" $filename $msg "FILE NONEMPTY" $filename "1"
            ret=$?
        else
            test_create_file 'touch test/'$filename'' $F_MOD "1" $filename "" "FILE EMPTY" $filename "1"
            ret=$?
        fi

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        if [ "$remainder3" == 0 ]
        then
            ((test_count++))
            local msg_length2=$[RANDOM%100+5]
            local msg2="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
            
            if [ "$remainder2" == 0 ]
            then
                printf -v exp_msg "%s\n%s" "$msg $msg2"
                test_write_file 'echo "'$msg2'" >> test/'$filename'' $F_MOD "1" $filename $msg "$exp_msg"
                ret=$?
            else
                test_write_file 'echo "'$msg2'" >> test/'$filename'' $F_MOD "1" $filename "" $msg2
                ret=$?
            fi


            if [ "$ret" == 0 ]
            then
                ((test_passed++))
            fi
        fi

        if [ "$remainder5" == 0 ]
        then
            ((test_count++))
            test_rmfile 'rm -rf test/'$filename'' $F_MOD "1" $filename "1"
            ret=$?

            if [ "$ret" == 0 ]
            then
                ((test_passed++))
            fi
        fi

    done

    echo "ADVANCED SEQUENCE_1: "$test_passed"/"$test_count""
}

#create random file, pick random files and write something in it. also make subdirectory, perform the same operations as mentioned before
test_advanced_sequence_2() {
    local random_number=$[RANDOM%6+5]
    local test_count=$(($random_number * 2 + 1))
    local test_passed=0
    local name_length=$
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)ADVANCED SEQUENCE_2 TESTS:$(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=0 ; i<$random_number ; i++))
    do
        local name_length1=$[RANDOM%32+5]
        local name_length2=$[RANDOM%32+5]
        local msg_length=$[RANDOM%100+5]
        local msg="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
        local filename1=$(tr -dc A-Za-z </dev/urandom | head -c $name_length1)
        local filename2=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
        local remainder2=$(($i % 2))
        local remainder3=$(($i % 3))
        local remainder4=$(($i % 4))
        local remainder5=$(($i % 5))

        if [ "$remainder2" == 0 ]
        then
            test_create_file 'echo "'$msg'" > test/'$filename1'' $F_MOD "1" $filename1 $msg "FILE NONEMPTY" $filename1 "1"
            ret1=$?

            test_create_file 'touch test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" "" "FILE EMPTY" "$filename2" "1"
            ret2=$?

        else
            test_create_file 'touch test/'$filename1'' $F_MOD "1" $filename1 "" "FILE EMPTY" $filename1 "1"
            ret1=$?

            test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
            ret2=$?
        fi

        if [ "$ret1" == 0 ]
            then
                ((test_passed++))
        fi

        if [ "$ret2" == 0 ]
            then
                ((test_passed++))
        fi

        if [ "$remainder3" == 0 ]
        then
            ((test_count=test_count+2))
            local msg_length2=$[RANDOM%100+5]
            local msg2="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
            
            if [ "$remainder2" == 0 ]
            then
                printf -v exp_msg "%s\n%s" "$msg $msg2"
                test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 $msg "$exp_msg"
                ret1=$?

                test_write_file 'echo "'$msg'" >> test/'$dirname'/'$filename2'' $F_MOD "1" $dirname'/'$filename2 "" $msg
                ret2=$?
            else
                test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 "" $msg2
                ret1=$?

                printf -v exp_msg "%s\n%s" "$msg $msg2"
                test_write_file 'echo "'$msg2'" >> test/'$dirname'/'$filename2'' $F_MOD "1" $dirname'/'$filename2 $msg "$exp_msg"
                ret2=$?
            fi


            if [ "$ret1" == 0 ]
            then
                ((test_passed++))
            fi

            if [ "$ret2" == 0 ]
            then
                ((test_passed++))
            fi
        fi

        if [ "$remainder4" == 0 ]
        then
            ((test_count++))
            test_rmfile 'rm -rf test/'$dirname'/'$filename2'' $F_MOD "1" $filename2 "1"
            ret=$?

            if [ "$ret" == 0 ]
            then
                ((test_passed++))
            fi
        fi

        if [ "$remainder5" == 0 ]
        then
            ((test_count++))
            test_rmfile 'rm -rf test/'$filename1'' $F_MOD "1" $filename1 "1"
            ret=$?

            if [ "$ret" == 0 ]
            then
                ((test_passed++))
            fi
        fi

    done

    echo "ADVANCED SEQUENCE_2: "$test_passed"/"$test_count""
}

# create random file, pick random files, and write something in it. Also make nested subdirectories, perform the same operation as before
test_advanced_sequence_3() {
    local test_passed=0
    local random_number=$[RANDOM%5+1]
    local test_count=1
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)ADVANCED_SEQUENCE_3 $(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "1"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=0 ; i<5 ; i++));
    do
        local tmp_dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local number_file_creations=$[RANDOM%6+5]
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
            local name_length1=$[RANDOM%32+5]
            local name_length2=$[RANDOM%32+5]
            local msg_length=$[RANDOM%100+5]
            local msg="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
            local filename1=$(tr -dc A-Za-z </dev/urandom | head -c $name_length1)
            local filename2=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
            local remainder2=$(($i % 2))
            local remainder3=$(($i % 3))
            local remainder4=$(($i % 4))
            local remainder5=$(($i % 5))

            if [ "$remainder2" == 0 ]
            then
                test_create_file 'echo "'$msg'" > test/'$filename1'' $F_MOD "1" $filename1 $msg "FILE NONEMPTY" $filename1 "1"
                ret1=$?

                test_create_file 'touch test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" "" "FILE EMPTY" "$filename2" "1"
                ret2=$?

            else
                test_create_file 'touch test/'$filename1'' $F_MOD "1" $filename1 "" "FILE EMPTY" $filename1 "1"
                ret1=$?

                test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
                ret2=$?
            fi

            if [ "$ret1" == 0 ]
                then
                    ((test_passed++))
            fi

            if [ "$ret2" == 0 ]
                then
                    ((test_passed++))
            fi

            if [ "$remainder3" == 0 ]
            then
                ((test_count=test_count+2))
                local msg_length2=$[RANDOM%6+5]
                local msg2="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
                
                if [ "$remainder2" == 0 ]
                then
                    printf -v exp_msg "%s\n%s" "$msg $msg2"
                    test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 $msg "$exp_msg"
                    ret1=$?

                    test_write_file 'echo "'$msg'" >> test/'$dirname'/'$filename2'' $F_MOD "1" $dirname'/'$filename2 "" $msg
                    ret2=$?
                else
                    test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 "" $msg2
                    ret1=$?

                    printf -v exp_msg "%s\n%s" "$msg $msg2"
                    test_write_file 'echo "'$msg2'" >> test/'$dirname'/'$filename2'' $F_MOD "1" $dirname'/'$filename2 $msg "$exp_msg"
                    ret2=$?
                fi


                if [ "$ret1" == 0 ]
                then
                    ((test_passed++))
                fi

                if [ "$ret2" == 0 ]
                then
                    ((test_passed++))
                fi
            fi

            if [ "$remainder4" == 0 ]
            then
                ((test_count++))
                test_rmfile 'rm -rf test/'$dirname'/'$filename2'' $F_MOD "1" $filename2 "1"
                ret=$?

                if [ "$ret" == 0 ]
                then
                    ((test_passed++))
                fi
            fi

            if [ "$remainder5" == 0 ]
            then
                ((test_count++))
                test_rmfile 'rm -rf test/'$filename1'' $F_MOD "1" $filename1 "1"
                ret=$?

                if [ "$ret" == 0 ]
                then
                    ((test_passed++))
                fi
            fi
            
        done
    done

    echo "ADVANCED_SEQUENCE_3: "$test_passed"/"$test_count""
}

# create random file, pick random files, and write something in it. also make multiple nested subdirectories, perform the same oeprations as before
test_advanced_sequence_4() {
    local test_passed=0
    local random_number=$[RANDOM%5+1]
    local test_count=1
    local dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)

    echo "$(tput setaf 6)ADVANCED_SEQUENCE_4 $(tput setaf 7)"

    test_mkdir 'mkdir test/'$dirname'' $D_MOD "2" $dirname "1" "0"
    ret=$?

    if [ "$ret" == 0 ]
    then
        ((test_passed++))
    fi

    for (( i=1 ; i<=5 ; i++));
    do
        echo DEPTH $i
        local tmp_dirname=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local tmp_dirname2=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local tmp_dirname3=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local tmp_base=$dirname
        local dirname2=""; local dirname2+=$dirname; local dirname2+="/"; local dirname2+=$tmp_dirname2
        local dirname3=""; local dirname3+=$dirname; local dirname3+="/"; local dirname3+=$tmp_dirname3

        local msg=$(tr -dc A-Za-z </dev/urandom | head -c 16)
        local number_file_creations=$[RANDOM%6+5]
        test_count=$(($test_count + 3 * 3 + 5))

        dirname+="/"
        dirname+=$tmp_dirname

        test_mkdir 'mkdir test/'$dirname'' $D_MOD "3" $tmp_dirname "1" $i
        ret=$?

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        test_mkdir 'mkdir test/'$dirname2'' $D_MOD "3" $tmp_dirname2 "1" $i
        ret=$?

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        test_mkdir 'mkdir test/'$dirname3'' $D_MOD "3" $tmp_dirname3 "1" $i
        ret=$?

        if [ "$ret" == 0 ]
            then
                ((test_passed++))
        fi

        for (( j=0 ; j<3 ; j++));
        do
            echo iteration creation $j
            local name_length1=$[RANDOM%32+5]
            local name_length2=$[RANDOM%32+5]
            local msg_length=$[RANDOM%10000+5]
            local msg="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
            local filename1=$(tr -dc A-Za-z </dev/urandom | head -c $name_length1)
            echo current filename: $filename1
            local filename2=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
            local filename3=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
            local filename4=$(tr -dc A-Za-z </dev/urandom | head -c $name_length2)
            local remainder2=$(($i % 2))
            local remainder3=$(($i % 3))
            local remainder4=$(($i % 4))
            local remainder5=$(($i % 5))

            if [ "$remainder2" == 0 ]
            then
                test_create_file 'echo "'$msg'" > test/'$filename1'' $F_MOD "1" $filename1 $msg "FILE NONEMPTY" $filename1 "1"
                ret1=$?

                test_create_file 'touch test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" "" "FILE EMPTY" "$filename2" "1"
                ret2=$?

                test_create_file 'echo "'$msg'" > test/'$dirname2'/'$filename3'' $F_MOD "1" "$dirname2/$filename3" $msg "FILE NONEMPTY" "$filename3" "1"
                ret3=$?

            else
                test_create_file 'touch test/'$filename1'' $F_MOD "1" $filename1 "" "FILE EMPTY" $filename1 "1"
                ret1=$?

                test_create_file 'echo "'$msg'" > test/'$dirname'/'$filename2'' $F_MOD "1" "$dirname/$filename2" $msg "FILE NONEMPTY" "$filename2" "1"
                ret2=$?

                test_create_file 'touch test/'$dirname3'/'$filename4'' $F_MOD "1" "$dirname3/$filename4" "" "FILE EMPTY" "$filename4" "1"
                ret3=$?
            fi

            if [ "$ret1" == 0 ]
                then
                    ((test_passed++))
            fi

            if [ "$ret2" == 0 ]
                then
                    ((test_passed++))
            fi

            if [ "$ret3" == 0 ]
                then
                    ((test_passed++))
            fi

            if [ "$remainder3" == 0 ]
            then
                ((test_count=test_count+2))
                local msg_length2=$[RANDOM%10000+5]
                local msg2="$(tr -dc A-Za-z </dev/urandom | head -c $msg_length)"
                
                if [ "$remainder2" == 0 ]
                then
                    printf -v exp_msg "%s\n%s" "$msg $msg2"
                    test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 $msg "$exp_msg"
                    ret1=$?

                    test_write_file 'echo "'$msg'" >> test/'$dirname'/'$filename2'' $F_MOD "1" $dirname'/'$filename2 "" $msg
                    ret2=$?
                else
                    test_write_file 'echo "'$msg2'" >> test/'$filename1'' $F_MOD "1" $filename1 "" $msg2
                    ret1=$?

                    printf -v exp_msg "%s\n%s" "$msg $msg2"
                    test_write_file 'echo "'$msg2'" >> test/'$dirname'/'$filename2'' $F_MOD "1" $dirname'/'$filename2 $msg "$exp_msg"
                    ret2=$?
                fi


                if [ "$ret1" == 0 ]
                then
                    ((test_passed++))
                fi

                if [ "$ret2" == 0 ]
                then
                    ((test_passed++))
                fi
            fi

            if [ "$remainder4" == 0 ]
            then
                ((test_count=test_count+2))
                test_rmfile 'rm -rf test/'$dirname'/'$filename2'' $F_MOD "1" $filename2 "1"
                ret=$?

                if [ "$ret" == 0 ]
                then
                    ((test_passed++))
                fi

                test_rmfile 'rm -rf test/'$dirname2'/'$filename3'' $F_MOD "1" $filename3 "1"
                ret=$?

                if [ "$ret" == 0 ]
                then
                    ((test_passed++))
                fi
            fi

            if [ "$remainder5" == 0 ]
            then
                echo remainder 5
                ((test_count++))
                echo current filename: $filename1
                test_rmfile 'rm -rf test/'$filename1'' $F_MOD "1" $filename1 "1"
                ret=$?

                if [ "$ret" == 0 ]
                then
                    ((test_passed++))
                fi
            fi
        done

        test_rmdir 'rm -rf test/'$dirname2'' $D_MOD "2" $tmp_dirname2 "1"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi

        test_rmdir 'rm -rf test/'$dirname2'' $D_MOD "2" $tmp_dirname2 "0"
        ret=$?

        if [ "$ret" == 0 ]
        then
            ((test_passed++))
        fi
    done

    echo "ADVANCED_SEQUENCE_4: "$test_passed"/"$test_count""
}

init() {
    make
    sudo insmod basicbtfs.ko
    mkdir -p test
    dd if=/dev/zero of=test.img bs=1M count=50
    ./mkfs.basicbtfs test.img
    sudo mount -o loop -t basicbtfs test.img test
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
test_write_advanced

test_rm_empty
test_rm_small
test_rm_large

test_rmdir_empty
test_rmdir_nonempty

test_rm_in_subdir
test_rm_subdir

# # test_move_file_simple
# # test_move_file_noreplace
# # test_move_file_overwrite
# # test_move_file_to_other_dir
# # # test_move_advanced

# # test_move_dir_simple
# # test_move_dir_in_dir

test_advanced_sequence_1
test_advanced_sequence_2
test_advanced_sequence_3
test_advanced_sequence_4