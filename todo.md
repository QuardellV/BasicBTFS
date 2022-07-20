# TODO List Basic Version
- Cleanup code and remove anything which is redundant
- Check if we necessarily need to remove during that specific occasion of rename
- Check what might cause the segmentation fault during clean up when unlink is being executed

# TODO list Improved version
- update directory using btree
  - Traverse
  - Search
  - Insert
  - Delete
  - Remove
  - Update
- Cleanup code and remove anything which is redundant
- fix the return types
- Fix issues where you can free A.S.A.P
- Update using clusters for at least files
- Check how we can checksums for data integrity and shorter names. this will be included for the btree
- Check how we can decrease the length using the checksums/hash and improve the usage of filenames.
  Currently we use a default length for each filename, while chances are high that a length of 255 is not necessary.
- Check for possibility to use btree for everything instead of using bitmap. See: Btrfs paper: https://dominoweb.draco.res.ibm.com/reports/rj10501.pdf
- Check how the filesystem can be defragmented also including garbage collection
- check how we can use cache to reduce the I/O costs. LRU which is around 10% of the total memory
- Check how the filesystem can truncate/shrink during write_begin/end
- Check how a snapshot can be made using reflink
- free space management using buddy slab allocator/ pre-allocation

# TODO List Measurement/Robustness
- Improve mkfs in c, including flags other options
- Create tests in c for the fsck. If necessary also let this fsck repair the filesystem in case of any wrongdoings
- Check what the durability/performance trade-off is for all these features
- Check why dot files aren't recognized by the root directory
- Use iozone, fxmark, iostat to check the scalability and performance of the different filesystems. More reliable than doing it manually. Moreover, we won't reinvent the wheel. See paper, for more information.


# TODO List Thesis
- Read more about log-structured filesystems: including lectures of storage systems
- Report about all of this in the paper