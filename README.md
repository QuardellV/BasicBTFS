For this thesis, the goal is design, implement and analyse a basic filesystem and improved filesystem. For this project, a basic filesystem using simplistic datastructures, and an improved filesystem including advanced features and improvements on top of the basic filesystem. These systems are called BasicFTFS, and BasicBTFS. These filesystems will be benchmarked using file I/O benchmark (fio). In order to compare the own implemented filesystems, linux implemented filesystems will be included as well on the bencmarks. In this case, similar filesystems to BasicFTFS and BasicBTFS will be used. FAT/MSDOS and BTRFS will be used as reference points.

# Dependencies

# Running
Each own designed and implemented filesystem can be compiled and run as follows. Take in consideration that each filesystem will run by default on DRAM using tmpfs. The default size of each filesystem will be 20GB. The compilation and the insertion of the kernel module will be done as follows:

```
    make
    sudo insmod <fs-name>.ko
    mkdir -p test
    sudo mount -t tmpfs -o size=20G tmpfs test
    mkdir $ROOT_DIR
    dd if=/dev/zero of=test/test.img bs=1 count=0 seek=15G
    ./mkfs.basicbtfs test/test.img
    sudo mount -o loop -t basicbtfs test/test.img $ROOT_DIR
```

# Benchmarks
The bench mark for each benchmarks have been performed as follows. Take in consideration that there are two tests which are done as follows:

## Bandwidth
For the bandwidth a random sequence of reading and writing in a file will be done. In this case an iodepth of 1 and a running time of 300 seconds will be used trying to create a workload which recreates a benchmark trying to find the performance of a filesysteem in terms of bandwidth. The bandwidth will be performed over the file size varying from a size from 4K up to 2MB. The following command will be used to bencmark the bandwidth of a filesystem using a certain size and store the output in a file within the result directory:

``` sudo fio --output-format=json+  --output=../../../$tmp_dir/btfsbw${tmp}K.output --size=${tmp}<sizetype>} ../../perfbw.fio ```

## Latency
For the latency, a random sequence of reading and writing in a file will be done. In this case an iodepth of 1 and a running time of 300 seconds will be used trying to create a workload which recreates a benchmark trying to find the performance in terms of latency. The latency will be plotted over the file size varying from a file size from 4K up to 2MB. The following command will be used to benchmark the latency of a filesystem using a certain size and store the output of the file within the result directory.

``` sudo fio --output-format=json+  --output=../../../$tmp_dir/btfslat${tmp}K.output --size=${tmp}<sizetype> ../../perflatency.fio ```

## Plotting
In order to measure the benchmarks of the correctly, ``` plot.py ``` have been written to retrieve the correct data to correctly plot the benchmarks. Of each benchmark of each file size, the benchmark in terms of either the bandwidth mean or the latency mean and have been added to one of the corresponding csv files. Both for reading and for reading, there is benchmark in bandwidth and the latency. In this case, the ```bw_r_mean```, ```bw_w_mean```, ```lat_r_mean```, ```lat_w_mean```, have been retrieved from the json and added to their corresponding csv files. So, there are four csv files: bw read, bw write, latency read, and latency write where the file size in B is the x-axis and the latency in ms and the bandwidth ms are the y-axis. Using this csv file, the benchmark can be plotted using which tool you ever want. For this paper, Latex has been used.

Within each filesystem, the performance of the filesystem in terms of bandwidth and latency can be performed using the following command within each filesystem directory: ``` ./performancetest.sh``` 

If the performance of all filesystems in terms of bandwidth and latency need to be performed, the following command can be used within the root directory of the working space: ```./performancetest.sh```
