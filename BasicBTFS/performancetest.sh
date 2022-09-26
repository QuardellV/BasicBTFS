#!/usr/bin/env bash
#!/bin/bash

BW_DIR=Results/tmpfs/bandwidth/btfsbw
LAT_DIR=Results/tmpfs/latency/btfslat
# SSD/HDD

# 1. Microbenchmark Analysis with and without defragmentation using iozone and iostat:
# do this 10 times and remove outliers. Make plot based on this information.
# one with defragmentation regurarly

# for i in {0..1..1};
# do
#     iozone -e -Rab output$i.wks
#     iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
# done


#one without regularly defragmentation
#   - block read
#   - block overwrite
#   - truncating not implemented: so not applicable
#   - file sync operation
#   - Path name resolution
#   - Directory read
#   - file creation and deletion
#   - file rename
#   - file sync
#   - combined
#   - specific amount of files/ and sizes

# for i in {0..10..1};
# do
#     iozone -e -n 2 -g 256 -Rab outputfilesmall$i.wks
#     iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
# done

# for i in {0..10..1};
# do
#     iozone -e -n 512 -g 4096 -Rab outputfilemedium$i.wks
#     iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
# done

# for i in {0..10..1};
# do
#     iozone -e -n 8192 -g 32768 -Rab outputfilelarge$i.wks
#     iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
# done

# for i in {0..10..1};
# do
#     iozone -e -y 2 -q 4  -Rab outputrecordsmall$i.wks
#     iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
# done

# for i in {0..10..1};
# do
#     iozone -e -y 8 -q 32  -Rab outputrecordmedium$i.wks
#     iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
# done

# for i in {0..10..1};
# do
#     iozone -e -y 64 -q 512  -Rab outputrecordlarge$i.wks
#     iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
# done

# fio --name=random-write --ioengine=posixaio --rw=randrw --bs=4K --size=1M --numjobs=1 --iodepth=1 --runtime=300 --time_based --end_fsync=1 --group_reporting

# fio --name=random-write --ioengine=posixaio --rw=randrw --bs=4K --size=4K --numjobs=1 --iodepth=128 --runtime=300 --time_based --end_fsync=1 --group_reporting

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