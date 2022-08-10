# TODO List Basic Version
- update directory using fat // In two weeks
  - symlink
  - hardlink
  - reflink
- Cleanup code and remove anything which is redundant // In two weeks
- Check if we necessarily need to remove during that specific occasion of rename // In two weeks
- Check what might cause the segmentation fault during clean up when unlink is being executed In two weeks
- Current bugs: // In two weeks
  - Unnecsary removing and adding entry. can also acces specific entry and update it
  - Fix issues where you can free A.S.A.P in all files

# TODO list Improved version
- update directory using btree
  - symlink
  - hardlink
  - reflink
- Current bugs:
  - fix return types
  - fix issues where you can free A.S.A.P in all files
  - Fix issues with rename where you have a double fault
  - Fix issue where we change move names in nametree. Make it lazy. Only defragment once we don't have sufficient space.
  - cleanup nametree
- Cleanup code and remove anything which is redundant
- use pseudo salt for hash: collision possibility is equal 1 / 2^32
- Check for possibility to use btree for everything instead of using bitmap. See: Btrfs paper: https://dominoweb.draco.res.ibm.com/reports/rj10501.pdf
  - Check where it actually would make sense. Definitely not for the bitmap for the inode/blocks // Next week
- Check how the filesystem can be defragmented also including garbage collection // Next week
- check how we can use cache to reduce the I/O costs. LRU which is around 10% of the total memory // This week
  - Make lazy cleanup for cache
- Check how the filesystem can truncate/shrink during write_begin/end // Next week
- Check how a snapshot can be made using reflink // Next week
<!-- - free space management using buddy slab allocator/ pre-allocation? // Next week -->

# TODO List Measurement/Robustness
- Improve mkfs in c, including flags other options // In two weeks
- Create tests in c for the fsck. If necessary also let this fsck repair the filesystem in case of any wrongdoings // In two tweeks
- Check what the durability/performance trade-off is for all these features // In two weeks
- Check why dot files aren't recognized by the root directory // In two weeks
- Use iozone, fxmark, iostat to check the scalability and performance of the different filesystems. More reliable than doing it manually. Moreover, we won't reinvent the wheel. See paper, for more information. // In two weeks


# TODO List Thesis
- Read more about log-structured filesystems: including lectures of storage systems
- make kgdb ready to use: https://www.youtube.com/watch?v=67cxIXLCfUk
- Report about all of this in the paper