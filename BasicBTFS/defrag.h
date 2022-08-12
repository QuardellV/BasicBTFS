#ifndef BASICBTFS_DEFRAG_H
#define BASICBTFS_DEFRAG_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/list.h>

#include "basicbtfs.h"

static inline void basicbtfs_defrag_btree_node(void) {
    /**
     * 1. If root, move btree and update inode->i_bno on disk and cache
     * 2. if non-root, move btree and update parent reference
     */
}

static inline void basicbtfs_defrag_nametree_block(void) {
    /**
     * 1. First fill up all open places and update corresponding entry in btree and update
     * 2. Use next block to fill up current block and update corresponding entries in btree
     * 3. Update corresponding block in cache and delete next block if no entries there
     * 4. swap to wanted to place. Check block type and move the block which occupies the spot to the unused area. update references
     * 4. end
     */
}

static inline void basicbtfs_defrag_nametree(void) {
    /**
     * 1. while to defrag until all blocks have been traversed
     * 
     */
}

static inline void basicbtfs_defrag_file_table_block(void) {
    /**
     * 1. defrag filetable block and place on wanted place. If necessary, move current block based on the block filetype.
     * 2. Iterate over all file extents and swap the files if necessary.
     * end
     */
}

static inline void basicbtfs_defrag_file_cluster(void) {
    /**
     * swap with the wanted blocks. Reserve one block before the root directory to keep track of the used start of clusters.
     * In this way, we know which blocks are used as fileblock with their corresponding ino
     * 
     */
}

static inline void basicbtfs_defrag_directory(void) {
    /**
     * 1. Defrag nametree
     * 2. Defrag btree
     * 3. iterate through all files and defrag based on filetype
     * 4. If directory, defrag directory add to tmp list and defrag after all files have been defragged
     * 5. if file, defrag file
     * 6. defrag directories on list
     * end
     */


}

static inline void basicbtfs_defrag_file(void) {

}

static inline void basicbtfs_defrag_disk(void) {
    /**
     * 1. 
     * 3. Defrag directory
     */
}

#endif