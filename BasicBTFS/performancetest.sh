#!/usr/bin/env bash
#!/bin/bash

# SSD/HDD

# 1. Microbenchmark Analysis with and without defragmentation using iozone and iostat:
# do this 10 times and remove outliers. Make plot based on this information.
# one with defragmentation regurarly

for (( i=1 ; i<=$10 ; i++));
do
    iozone –Rab output$i.wks -e -N -O -R # see pdf for further explanation
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

for (( i=1 ; i<=$10 ; i++));
do
    iozone –Rab outputfile$i.wks -e -N -O -R -n 64 -g 128 #about file size and test different sizes corresponding to different types of applications
done

for (( i=1 ; i<=$10 ; i++));
do
    iozone –Rab outputfile$i.wks -e -N -O -R -n 512 -g 4096 #about file size and test different sizes corresponding to different types of applications
done

for (( i=1 ; i<=$10 ; i++));
do
    iozone –Rab outputfile$i.wks -e -N -O -R -n 8192 -g 16384 #about file size and test different sizes corresponding to different types of applications
done

for (( i=1 ; i<=$10 ; i++));
do
    iozone –Rab outputrecord$i.wks -e -N -O -R -y 2 -q 16  #about record size and test different sizes
done

for (( i=1 ; i<=$10 ; i++));
do
    iozone –Rab outputrecord$i.wks -e -N -O -R -y 128 -q 512  #about record size and test different sizes
done

for (( i=1 ; i<=$10 ; i++));
do
    iozone –Rab outputrecord$i.wks -e -N -O -R -y 1024 -q 16384 #about record size and test different sizes
done



# 2. Application benchmarks with and without defragmentation test defragmentation by regularly defragmenting the thing
#   - Mail server
#   - sql database
#   - file server