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
  - Check where it actually would make sense. Definitely not for the bitmap for the inode/blocks. I guess bitmaps are fine to use // Next week
- Check how the filesystem can be defragmented also including garbage collection // Next week
  - remove all redundant things in cache which are marked as free. This includes name blocks, btree node cache blocks, and dir blocks.
  - Remove and clean up all redundant things on disk which are marked as free. This includes the inodes, and the btree/file/nametree blocks considered as free
  - move the inodes in such a way that those are contigous
  - move the nametree in such a way that it will be contigous. Remove the remaining blocks that will be freed. Do the same for the cache
  - move all the blocks also in such a way that it will be contigious. It is recommended to group them per directory. So, basically as follows:
    - directory group containing the btree/nametree/file blocks.
    - First defrag nametree and try to free as many blocks as possible. both on disk and cache.
    - how can we check whether we should defrag. 5 separate contigous blocks is enough I think. just keep track in root node. If more than 5, defrag and move to separate to one contigious place
    - check if there are more than five separate contigous blocks. 
    - First check if it is being used in many contigous blocks.

    - Move inode no go too much with vfs inode
    - move root btree node: references: inode_info->bno. cache needs to be updated as well
    - move btree node: references: parent, so make doubly linked list
    - move nametree node: references: if root: name_bno, else to previous nametree block, so doubly linked list as well.
    - move file allocation table: references: inode->i_bno
    - move fileblock: references: allocation table
- check how we can use cache to reduce the I/O costs. LRU which is around 10% of the total memory // This week
  - Make lazy cleanup for cache periodically or when it is being called using ioctl
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