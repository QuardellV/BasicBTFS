#!/usr/bin/env bash
#!/bin/bash

cd BasicBTFS

./clean.sh && ./benchmark_basicbtfs.sh && ./clean.sh

cd ../BasicBTFSNC

./clean.sh && ./benchmark_btfsnc.sh && ./clean.sh

cd ../BasicFATFS

./clean.sh && ./benchmark_ftfs.sh && ./clean.sh

cd ../BTRFS

./clean.sh && ./benchmark_btrfs.sh && ./clean.sh

cd ../FATFS

./clean.sh && ./benchmark_fat.sh && ./clean.sh

cd ..