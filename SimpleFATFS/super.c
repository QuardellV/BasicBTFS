#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/statfs.h>

#include "basicftfs.h"
#include "io.h"

static struct kmem_cache *basicftfs_cache;

int basicftfs_init_inode_cache(void) {
    basicftfs_cache = kmem_cache_create("basicftfs_cache", sizeof(struct  basicftfs_inode), 0, 0, NULL);

    if (!basicftfs_cache) {
        return -ENOMEM;
    }

    return 0;
}

void basicftfs_destroy_inode_cache(void) {
    kmem_cache_destroy(basicftfs_cache);
}

/**
 * @brief Free all allocated kernel memory
 * 
 * @param sb Superblock
 */
static void basicftfs_put_super(struct super_block *sb) {
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);

    if (sbi) {
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi);
    }
}

static struct inode *basicftfs_alloc_inode(struct super_block *sb) {
    struct basicftfs_inode_info *inode_info = kmem_cache_alloc(basicftfs_cache, GFP_KERNEL);

    if (!inode_info) return NULL;

    inode_init_once(&inode_info->vfs_inode);
    return &inode_info->vfs_inode;
    return NULL;
}

static int basicftfs_write_inode(struct inode *inode, struct writeback_control *wbc){
    struct super_block *sb = inode->i_sb;
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    struct basicftfs_inode *disk_inode = NULL;
    uint32_t ino = inode->i_ino;
    uint32_t inode_block = BASICFTFS_GET_INODE_BLOCK(ino, sbi->s_imap_blocks, sbi->s_bmap_blocks);
    uint32_t inode_bidx = BASICFTFS_GET_INODE_BLOCK_IDX(ino);
    struct buffer_head *bh = NULL;

    if (ino >= sbi->s_nfree_inodes) return 0;

    bh = sb_bread(sb, inode_block);

    if (!bh) return -EIO;

    disk_inode = (struct basicftfs_inode *)bh->b_data;
    disk_inode += inode_bidx;

    write_from_vfs_inode_to_disk(disk_inode, inode);

    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);
    return 0;
}

static void basicftfs_evict_inode(struct inode *inode) {
}

static void basicftfs_destroy_inode(struct inode *inode) {
    kmem_cache_destroy(basicftfs_cache);
}

static int basicftfs_sync_fs(struct super_block *sb, int wait) {
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    int ret = 0;

    ret = flush_superblock(sb, wait);

    if (ret < 0) return ret;

    ret = flush_bitmap(sb, sbi->s_ifree_bitmap, sbi->s_imap_blocks, 1, wait);

    if (ret < 0) return ret;

    ret = flush_bitmap(sb, sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_imap_blocks + 1, wait);

    return ret;
}

static int basicftfs_statfs(struct dentry *dentry, struct kstatfs *stat) {
    struct super_block *sb = dentry->d_sb;
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);

    stat->f_type = BASICFTFS_MAGIC_NUMBER;
    stat->f_bsize = BASICFTFS_BLOCKSIZE;
    stat->f_blocks = sbi->s_nblocks;
    stat->f_bfree = sbi->s_nfree_blocks;
    stat->f_bavail = sbi->s_nfree_blocks;
    stat->f_files = sbi->s_ninodes - sbi->s_nfree_inodes;
    stat->f_ffree = sbi->s_nfree_inodes;
    stat->f_namelen = BASICFTFS_NAME_LENGTH;

    return 0;
}

//TODO: Implement functions
static struct super_operations basicftfs_super_ops = {
    .put_super = basicftfs_put_super,
    .alloc_inode = basicftfs_alloc_inode,
    .write_inode = basicftfs_write_inode,
    .evict_inode = basicftfs_evict_inode,
    .destroy_inode = basicftfs_destroy_inode,
    .sync_fs = basicftfs_sync_fs,
    .statfs = basicftfs_statfs,
};

int init_super_block(struct super_block *sb) {
    int ret = 0;
    sb->s_magic = BASICFTFS_MAGIC_NUMBER;
    ret = sb_set_blocksize(sb, BASICFTFS_BLOCKSIZE);
    sb->s_maxbytes = BASICFTFS_FILE_BSIZE;
    sb->s_op = &basicftfs_super_ops;
    return ret;
}

int init_bitmap(struct super_block *sb, unsigned long *bitmap, uint32_t map_nr_blocks, uint32_t block_offset) {
    struct buffer_head *bh = NULL;
    int i = 0;

    for (i = 0; i < map_nr_blocks; i++) {
        int idx = block_offset + i;
        bh = sb_bread(sb, idx);

        if (!bh) return -EIO;

        memcpy((void *) bitmap + i * BASICFTFS_BLOCKSIZE, bh->b_data, BASICFTFS_BLOCKSIZE);
        brelse(bh);
    }
    return 0;
}

struct basicftfs_sb_info *init_super_block_info(struct super_block *sb, int *res) {
    struct buffer_head *bh = NULL;
    struct basicftfs_sb_info *new_sbi = NULL;
    struct basicftfs_sb_info *disk_sbi = NULL;
    int ret = 0;

    bh = sb_bread(sb, BASICFTFS_SB_BNO);

    if (!bh) {
        *res = -EIO;
        return NULL;
    }

    disk_sbi = (struct basicftfs_sb_info *) bh->b_data;

    if (sb->s_magic != disk_sbi->s_magic) {
        printk(KERN_ERR "Incorrect magic number. Wrong filesystem\n");
        brelse(bh);
        *res = -EINVAL;
        return NULL;
    }

    new_sbi = kzalloc(sizeof(struct basicftfs_sb_info), GFP_KERNEL);

    if (!new_sbi) {
        brelse(bh);
        *res = -ENOMEM;
        return NULL;
    }

    new_sbi->s_nblocks = disk_sbi->s_nblocks;
    new_sbi->s_ninodes = disk_sbi->s_ninodes;
    new_sbi->s_inode_blocks = disk_sbi->s_inode_blocks;
    new_sbi->s_imap_blocks = disk_sbi->s_imap_blocks;
    new_sbi->s_bmap_blocks = disk_sbi->s_bmap_blocks;
    new_sbi->s_nfree_inodes = disk_sbi->s_nfree_inodes;
    new_sbi->s_nfree_blocks = disk_sbi->s_nfree_blocks;
    sb->s_fs_info = new_sbi;

    brelse(bh);

    new_sbi->s_ifree_bitmap = kzalloc(new_sbi->s_imap_blocks * BASICFTFS_BLOCKSIZE, GFP_KERNEL);

    if (!new_sbi->s_ifree_bitmap) {
        kfree(new_sbi);
        *res = -ENOMEM;
        return NULL;
    }

    ret = init_bitmap(sb, new_sbi->s_ifree_bitmap, new_sbi->s_imap_blocks, 1);

    if (ret < 0) {
        kfree(new_sbi->s_ifree_bitmap);
        kfree(new_sbi);
        *res = -EIO;
        return NULL;
    }

    ret = init_bitmap(sb, new_sbi->s_bfree_bitmap, new_sbi->s_bmap_blocks, new_sbi->s_imap_blocks + 1);

    if (ret < 0) {
        kfree(new_sbi->s_ifree_bitmap);
        kfree(new_sbi->s_bfree_bitmap);
        kfree(new_sbi);
        *res = -EIO;
        return NULL;
    }
    return new_sbi;
}

int init_root_inode(struct super_block *sb) {
    struct inode *root_inode = basicftfs_iget(sb, 0); // TODO: implement iget

    if (IS_ERR(root_inode)) return PTR_ERR(root_inode);

    inode_init_owner(root_inode, NULL, root_inode->i_mode);

    sb->s_root = d_make_root(root_inode);

    if (!sb->s_root) {
        iput(root_inode);
        return -ENOMEM;
    }
    return 0;
}



int basicftfs_fill_super(struct super_block *sb, void *data, int silent) {
    struct basicftfs_sb_info *new_sbi = NULL;
    int ret = 0;

    ret = init_super_block(sb);

    if (ret < 0) return ret;

    new_sbi = init_super_block_info(sb, &ret);

    if (!new_sbi) return ret;

    ret = init_root_inode(sb);

    if (ret < 0) {
        kfree(new_sbi->s_bfree_bitmap);
        kfree(new_sbi->s_ifree_bitmap);
        kfree(new_sbi);
        return ret;
    }

    return 0;
}
