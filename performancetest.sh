#!/usr/bin/env bash
#!/bin/bash

# cd BasicBTFS

# ./clean.sh && ./performancetest.sh && ./clean.sh

# cd ../BasicBTFSNC

# ./clean.sh && ./performancetest.sh && ./clean.sh

# cd ../BasicFATFS

# ./clean.sh && ./performancetest.sh && ./clean.sh

# cd BTRFS

# ./clean.sh && ./performancetest.sh && ./clean.sh

cd FATFS

./clean.sh && ./performancetest.sh && ./clean.sh

cd ..