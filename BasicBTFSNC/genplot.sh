#!/usr/bin/env bash
#!/bin/bash

BW_DIR=Results/tmpfs/bandwidth/btfsbw
LAT_DIR=Results/tmpfs/latency/btfslat

for i in {2..9..1};
do  
    tmp=$((2 ** i))
    tmp_dir=$BW_DIR/${tmp}K
    echo $tmp

    
    fio_generate_plots "../$tmp_dir/${tmp}K_bw.1.log"
done

# for i in {0..9..1};
# do
#     tmp=$((2 ** i))
#     echo $tmp
#     tmp_dir=$BW_DIR/${tmp}M

#     fio_generate_plots ../$tmp_dir/${tmp}K_bw.1.log
# done

# for i in {2..9..1};
# do  
#     tmp=$((2 ** i))
#     tmp_dir=$LAT_DIR/${tmp}K
#     echo $tmp

    
# done

# for i in {0..9..1};
# do
#     tmp=$((2 ** i))
#     echo $tmp
#     tmp_dir=$LAT_DIR/${tmp}M

# done

# 2. Application benchmarks with and without defragmentation test defragmentation by regularly defragmenting the thing
#   - Mail server
#   - sql database
#   - file server