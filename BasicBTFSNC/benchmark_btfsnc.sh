#!/usr/bin/env bash
#!/bin/bash

BW_DIR=Results/tmpfs/bandwidth/btfsnocachebw
LAT_DIR=Results/tmpfs/latency/btfsnocachelat
# SSD/HDD

sudo rm -rf ../$BW_DIR
mkdir ../$BW_DIR

for i in {2..9..1};
do
    tmp=$((2 ** i))
    tmp_dir=$BW_DIR/K${i}_${tmp}K
    echo $tmp
    mkdir ../$tmp_dir

    for j in {0..20..1};
    do
        mkdir ../$tmp_dir/$j

        ./clean.sh && ./test.sh
        cd test/mnt
        sudo fio --output-format=json+  --output=../../../$tmp_dir/$j/btfsnocachebw${tmp}K.output --size=${tmp}K  ../../perfbw.fio

        fio_jsonplus_clat2csv ../../../$tmp_dir/$j/btfsnocachebw${tmp}K.output ../../../$tmp_dir/$j/btfsnocachebw${tmp}K.csv
        cd ../../../$tmp_dir
        cd ../../../../../BasicBTFSNC
    done
done

for i in {0..1..1};
do
    tmp=$((2 ** i))
    echo $tmp
    tmp_dir=$BW_DIR/M${i}_${tmp}M
    mkdir ../$tmp_dir

    for j in {0..20..1};
    do
        mkdir ../$tmp_dir/$j

        ./clean.sh && ./test.sh
        cd test/mnt
        sudo fio --output-format=json+  --output=../../../$tmp_dir/$j/btfsnocachebw${tmp}M.output --size=${tmp}M  ../../perfbw.fio

        fio_jsonplus_clat2csv ../../../$tmp_dir/$j/btfsnocachebw${tmp}M.output ../../../$tmp_dir/$j/btfsnocachebw${tmp}M.csv
        cd ../../../$tmp_dir

        cd ../../../../../BasicBTFSNC
    done
done

sudo rm -rf ../$LAT_DIR
mkdir ../$LAT_DIR

for i in {2..9..1};
do
    tmp=$((2 ** i))
    tmp_dir=$LAT_DIR/K${i}_${tmp}K
    echo $tmp
    mkdir ../$tmp_dir

    for j in {0..20..1};
    do
        mkdir ../$tmp_dir/$j

        ./clean.sh && ./test.sh
        cd test/mnt
        sudo fio --output-format=json+  --output=../../../$tmp_dir/$j/btfsnocachelat${tmp}K.output --size=${tmp}K ../../perflatency.fio

        fio_jsonplus_clat2csv ../../../$tmp_dir/$j/btfsnocachelat${tmp}K.output ../../../$tmp_dir/$j/btfsnocachelat${tmp}K.csv
        cd ../../../$tmp_dir
        cd ../../../../../BasicBTFSNC
    done
done

for i in {0..1..1};
do
    tmp=$((2 ** i))
    echo $tmp
    tmp_dir=$LAT_DIR/M${i}_${tmp}M
    mkdir ../$tmp_dir

    for j in {0..20..1};
    do
        mkdir ../$tmp_dir/$j

        ./clean.sh && ./test.sh
        cd test/mnt
        sudo fio --output-format=json+  --output=../../../$tmp_dir/$j/btfsnocachelat${tmp}M.output --size=${tmp}M ../../perflatency.fio

        fio_jsonplus_clat2csv ../../../$tmp_dir/$j/btfsnocachelat${tmp}M.output ../../../$tmp_dir/$j/btfsnocachelat${tmp}M.csv
        cd ../../../$tmp_dir
        cd ../../../../../BasicBTFSNC
    done
done
