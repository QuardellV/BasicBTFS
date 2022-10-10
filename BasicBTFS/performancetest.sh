#!/usr/bin/env bash
#!/bin/bash

BW_DIR=Results/tmpfs/bandwidth/btfsbw
LAT_DIR=Results/tmpfs/latency/btfslat
# SSD/HDD

sudo rm -rf ../$BW_DIR
mkdir ../$BW_DIR

for i in {2..9..1};
do  
    tmp=$((2 ** i))
    tmp_dir=$BW_DIR/K${i}_${tmp}K
    echo $tmp

    mkdir ../$tmp_dir

    ./clean.sh && ./test.sh
    cd test/mnt
    sudo fio --output-format=json+  --output=../../../$tmp_dir/btfsbw${tmp}K.output --size=${tmp}K ../../perfbw.fio
    # --write_bw_log=../../../$tmp_dir/${tmp}K  --write_lat_log=../../../$tmp_dir/${tmp}K \
    # --write_iops_log=../../../$tmp_dir/${tmp}K ../../perfbw.fio

    fio_jsonplus_clat2csv ../../../$tmp_dir/btfsbw${tmp}K.output ../../../$tmp_dir/btfsbw${tmp}K.csv
    cd ../../../$tmp_dir
    # fio_generate_plots ${tmp}K_bw.1.log

    # rm -rf ${tmp}K_bw.1.log
    # rm -rf ${tmp}K_clat.1.log
    # rm -rf ${tmp}K_iops.1.log
    # rm -rf ${tmp}K_lat.1.log
    # rm -rf ${tmp}K_slat.1.log

    cd ../../../../../BasicBTFS

done

for i in {0..9..1};
do
    tmp=$((2 ** i))
    echo $tmp
    tmp_dir=$BW_DIR/M${i}_${tmp}M

    mkdir ../$tmp_dir

    ./clean.sh && ./test.sh
    cd test/mnt
    sudo fio --output-format=json+  --output=../../../$tmp_dir/btfsbw${tmp}M.output --size=${tmp}M ../../perfbw.fio
    # --write_bw_log=../../../$tmp_dir/${tmp}M  --write_lat_log=../../../$tmp_dir/${tmp}M \
    # --write_iops_log=../../../$tmp_dir/${tmp}M ../../perfbw.fio

    fio_jsonplus_clat2csv ../../../$tmp_dir/btfsbw${tmp}M.output ../../../$tmp_dir/btfsbw${tmp}M.csv
    cd ../../../$tmp_dir
    # fio_generate_plots ${tmp}M_bw.1.log

    # rm -rf ${tmp}M_bw.1.log
    # rm -rf ${tmp}M_clat.1.log
    # rm -rf ${tmp}M_iops.1.log
    # rm -rf ${tmp}M_lat.1.log
    # rm -rf ${tmp}M_slat.1.log

    cd ../../../../../BasicBTFS
done

sudo rm -rf ../$LAT_DIR
mkdir ../$LAT_DIR

for i in {2..9..1};
do  
    tmp=$((2 ** i))
    tmp_dir=$LAT_DIR/K${i}_${tmp}K
    echo $tmp

    mkdir ../$tmp_dir

    ./clean.sh && ./test.sh
    cd test/mnt
    sudo fio --output-format=json+  --output=../../../$tmp_dir/btfslat${tmp}K.output --size=${tmp}K ../../perflatency.fio
    # --write_bw_log=../../../$tmp_dir/${tmp}K  --write_lat_log=../../../$tmp_dir/${tmp}K \
    # --write_iops_log=../../../$tmp_dir/${tmp}K ../../perflatency.fio

    fio_jsonplus_clat2csv ../../../$tmp_dir/btfslat${tmp}K.output ../../../$tmp_dir/btfslat${tmp}K.csv
    cd ../../../$tmp_dir
    # fio_generate_plots ${tmp}K_lat.1.log
    # rm -rf ${tmp}K_bw.1.log
    # rm -rf ${tmp}K_clat.1.log
    # rm -rf ${tmp}K_iops.1.log
    # rm -rf ${tmp}K_lat.1.log
    # rm -rf ${tmp}K_slat.1.log
    cd ../../../../../BasicBTFS
done

for i in {0..9..1};
do
    tmp=$((2 ** i))
    echo $tmp
    tmp_dir=$LAT_DIR/M${i}_${tmp}M

    mkdir ../$tmp_dir

    ./clean.sh && ./test.sh
    cd test/mnt
    sudo fio --output-format=json+  --output=../../../$tmp_dir/btfslat${tmp}M.output --size=${tmp}M ../../perflatency.fio
    # --write_bw_log=../../../$tmp_dir/${tmp}M  --write_lat_log=../../../$tmp_dir/${tmp}M \
    # --write_iops_log=../../../$tmp_dir/${tmp}M ../../perflatency.fio

    fio_jsonplus_clat2csv ../../../$tmp_dir/btfslat${tmp}M.output ../../../$tmp_dir/btfslat${tmp}M.csv
    cd ../../../$tmp_dir
    # fio_generate_plots ${tmp}M_lat.1.log
    # rm -rf ${tmp}M_bw.1.log
    # rm -rf ${tmp}M_clat.1.log
    # rm -rf ${tmp}M_iops.1.log
    # rm -rf ${tmp}M_lat.1.log
    # rm -rf ${tmp}M_slat.1.log

    cd ../../../../../BasicBTFS
done

# 2. Application benchmarks with and without defragmentation test defragmentation by regularly defragmenting the thing
#   - Mail server
#   - sql database
#   - file server