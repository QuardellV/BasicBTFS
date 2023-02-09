For this thesis, the goal is to design, implement and analyse a basic filesystem and improved filesystem. For this project, a basic filesystem using simplistic datastructures, and an improved filesystem including optimizations and additional features will be designed and implemented. These systems are called BasicFTFS, and BasicBTFS. Whereas the disk organization of both filesystems is similar, the directory structure and file space management differ: While BasicFTFS uses a directory allocation table and a file block allocation table for itsdirectory structure and file space management respectively, BasicBTFS uses a B-tree directory and a file extent allocation table for these filesystem components to make an attempt to improve the performance of BasicBTFS. Unlike BasicFTFS where the focus lies on designing and implementing a simple but robust filesystem usable as a reference point.

These filesystems will be benchmarked using file I/O benchmark (fio). In order to compare the own implemented filesystems, Linux integrated filesystems will be included as well in the bencmarks. In this case, similar filesystems to BasicFTFS and BasicBTFS will be used. FAT/MSDOS and BTRFS will be used as reference points.

# Dependencies

## Hardware Dependencies
The following hardware dependencies are needed:

* Local machine capable of running a QEMU VM of at least 40GB disk space.

## Software Dependencies
The following software dependencies are needed.

### Virtual Machine
* emulator version 4.2.1
* Linux Version  5.4.0-126-generic (Ubuntu 18.04)

### Compiling Tools
* gcc 7.5.0
* GNU Make 4.1

### Benchmarking

* fio-3.32-36-g5932
* Python 3.6.9

# Installation
## Virtual Machine
Install QEMU as follows:
```
$ sudo apt install qemu
```
Once QEMU has been installed, download [Ubuntu 18.04](https://ubuntu.com/tutorials/install-ubuntu-desktop-1804#1-overview), open the virtual machine manager, and make sure it is connected to QEMU/KVM (In case it is not connected to this, try to run VMM as root).

After this, create a new virtual machine using the downloaded ISO image. Choose your desired memory and CPU settings and make sure that you have sufficient disk space available on your disk (40GB! is recommended). After creating the virtual machine successfully, finish the ubuntu installation and install the necessary software dependencies.

## Mounting of filesystems
Clone the repository:
```
$ git clone https://github.com/QuardellV/BasicBTFS
$ cd BasicBTFS
```

Each own designed and implemented filesystem can be compiled and run as follows. Take in consideration that each filesystem will run by default on DRAM using tmpfs. The default size of each filesystem will be 20GB. The compilation and the insertion of the kernel module can be done as follows:
```
$ cd <fs_dir>
$ ./compile.sh
```

After finishing this step, you should be able to use the specific filesystem where the root directory is mounted at ```<fs-dir>/test/mnt```. It is also possible to alter sizes or locations by adjusting ```compile.sh```.

# Benchmarks
The bench mark for each benchmarks have been performed using ```fs_benchmark.sh```. A separate benchmark can be benchmarked using ```<fs_dir>/benchmark_<fs>``` Take in consideration that there are two tests which are done as follows:

## Bandwidth
For the bandwidth a random sequence of reading and writing in a file will be done. In this case an iodepth of 1 and a running time of 300 seconds will be used trying to create a workload which recreates a benchmark trying to find the performance of a filesysteem in terms of bandwidth. The bandwidth will be performed over the file size varying from a size from 4K up to 2MB. The following command will be used to bencmark the bandwidth of a filesystem using a certain size and store the output in a file within the result directory:

``` sudo fio --output-format=json+  --output=../../../$tmp_dir/btfsbw${tmp}K.output --size=${tmp}<sizetype>} ../../perfbw.fio ```

## Latency
For the latency, a random sequence of reading and writing in a file will be done. In this case an iodepth of 1 and a running time of 300 seconds will be used trying to create a workload which recreates a benchmark trying to find the performance in terms of latency. The latency will be plotted over the file size varying from a file size from 4K up to 2MB. The following command will be used to benchmark the latency of a filesystem using a certain size and store the output of the file within the result directory.

``` sudo fio --output-format=json+  --output=../../../$tmp_dir/btfslat${tmp}K.output --size=${tmp}<sizetype> ../../perflatency.fio ```

## Plotting
In order to measure the benchmarks of the correctly, ``` Results/jsonparsers.py ``` have been written to retrieve the correct data to correctly plot the benchmarks. Of each benchmark of each file size, the benchmark in terms of either the bandwidth mean or the latency mean and have been added to one of the corresponding csv files. Both for reading and for reading, there is benchmark in bandwidth and the latency. In this case, the ```bw_r_mean```, ```bw_w_mean```, ```lat_r_mean```, ```lat_w_mean```, have been retrieved from the json and added to their corresponding csv files. So, there are four csv files: bw read, bw write, latency read, and latency write where the file size in B is the x-axis and the latency in ms and the bandwidth ms are the y-axis. Using this CSV file, the benchmark can be plotted using which tool you ever want. For this paper, Latex has been used.

Within each filesystem, the performance of the filesystem in terms of bandwidth and latency can be performed using the following command within each filesystem directory: ``` ./benchmark_<fs>.sh`` 

If the performance of all filesystems in terms of bandwidth and latency need to be performed, the following command can be used within the root directory of the working space: ```./fs_benchmark.sh```

We can plot the results for all filesystems using the following command: ```python3 plot_results.py```. These plots show the average bandwidth/latency over the file size, using the average based on the amount of iterations you decided to choose.

We can plot verbose results for an individual filesystem using he following command: ```python3 plot_results_box.py```. These plots shows the boxplot of the bandwidth/latency over the file size, where the boxplot are all the results of all iterations over a certain file size.
