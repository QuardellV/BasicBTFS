#!/usr/bin/env bash
#!/bin/bash

# SSD/HDD

# 1. Microbenchmark Analysis with and without defragmentation using iozone and iostat:
# do this 10 times and remove outliers. Make plot based on this information.
# one with defragmentation regurarly

for i in {0..1..1};
do
    iozone -e -Rab output$i.wks
    iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
done


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

for i in {0..10..1};
do
    iozone -e -n 2 -g 256 -Rab outputfilesmall$i.wks
    iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
done

for i in {0..10..1};
do
    iozone -e -n 512 -g 4096 -Rab outputfilemedium$i.wks
    iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
done

for i in {0..10..1};
do
    iozone -e -n 8192 -g 32768 -Rab outputfilelarge$i.wks
    iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
done

for i in {0..10..1};
do
    iozone -e -y 2 -q 4  -Rab outputrecordsmall$i.wks
    iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
done

for i in {0..10..1};
do
    iozone -e -y 8 -q 32  -Rab outputrecordmedium$i.wks
    iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
done

for i in {0..10..1};
do
    iozone -e -y 64 -q 512  -Rab outputrecordlarge$i.wks
    iostat -c -d -x -t -m /dev/loop22 >> iostat$i.out
done

fio --name=random-write --ioengine=posixaio --rw=randrw --bs=4K --size=1M --numjobs=1 --iodepth=1 --runtime=300 --time_based --end_fsync=1 --group_reporting

fio --name=random-write --ioengine=posixaio --rw=randrw --bs=4K --size=4K --numjobs=1 --iodepth=128 --runtime=300 --time_based --end_fsync=1 --group_reporting


# 2. Application benchmarks with and without defragmentation test defragmentation by regularly defragmenting the thing
#   - Mail server
#   - sql database
#   - file server