#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/statfs.h>

#include "basicftfs.h"
#include "io.h"
#include "init.h"

static struct kmem_cache *basicftfs_inode_cache;

int basicftfs_init_inode_cache(void) {
    basicftfs_inode_cache = kmem_cache_create("basicftfs_cache", sizeof(struct basicftfs_inode_info), 0, 0, NULL);

    if (!basicftfs_inode_cache) return -ENOMEM;
    return 0;
}

void basicftfs_destroy_inode_cache(void) {
    kmem_cache_destroy(basicftfs_inode_cache);
}

static void basicftfs_put_super(struct super_block *sb) {
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    if (sbi) {
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi->s_bfree_bitmap);
        kfree(sbi);
    }
}

static struct inode *basicftfs_alloc_inode(struct super_block *sb) {
    struct basicftfs_inode_info *ci = kmem_cache_alloc(basicftfs_inode_cache, GFP_KERNEL);
    if (!ci) return NULL;

    inode_init_once(&ci->vfs_inode);
    return &ci->vfs_inode;
}

static int basicftfs_write_inode(struct inode *inode, struct writeback_control *wbc) {
    struct basicftfs_inode *disk_inode;
    struct super_block *sb = inode->i_sb;
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    struct buffer_head *bh;
    uint32_t ino = inode->i_ino;
    uint32_t inode_block = BASICFTFS_GET_INODE_BLOCK(ino, sbi->s_imap_blocks, sbi->s_bmap_blocks);
    uint32_t inode_bi = BASICFTFS_GET_INODE_BLOCK_IDX(ino);

    if (ino >= sbi->s_ninodes) return 0;

    printk("basicftfs_write_inode() inode_block: %d\n", inode_block);
    bh = sb_bread(sb, inode_block);

    if (!bh) return -EIO;

    disk_inode = (struct basicftfs_inode *) bh->b_data;
    disk_inode += inode_bi;

    write_from_vfs_inode_to_disk(disk_inode, inode);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    return 0;
}

static void basicftfs_destroy_inode(struct inode *inode) {
    struct basicftfs_inode_info *ci = BASICFTFS_INODE(inode);
    kmem_cache_free(basicftfs_inode_cache, ci);
}

static int basicftfs_sync_fs(struct super_block *sb, int wait)
{
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    int ret = 0;

    /* Flush superblock */
    printk("basicftfs_sync_fs() 0: %d\n", 0);
    ret = flush_superblock(sb, wait);
    if (ret < 0) return ret;
    ret = flush_bitmap(sb, sbi->s_ifree_bitmap, sbi->s_imap_blocks, 1, wait);
    if (ret < 0) return ret;
    ret = flush_bitmap(sb, sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_imap_blocks + 1, wait);

    return ret;
}

static int basicftfs_statfs(struct dentry *dentry, struct kstatfs *stat)
{
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

static struct super_operations basicftfs_super_ops = {
    .put_super = basicftfs_put_super,
    .alloc_inode = basicftfs_alloc_inode,
    .destroy_inode = basicftfs_destroy_inode,
    .write_inode = basicftfs_write_inode,
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

int init_sbi(struct super_block *sb, struct basicftfs_sb_info *csb, struct basicftfs_sb_info *sbi) {
    sbi->s_nblocks = csb->s_nblocks;
    sbi->s_ninodes = csb->s_ninodes;
    sbi->s_inode_blocks = csb->s_inode_blocks;
    sbi->s_imap_blocks = csb->s_imap_blocks;
    sbi->s_bmap_blocks = csb->s_bmap_blocks;
    sbi->s_nfree_inodes = csb->s_nfree_inodes;
    sbi->s_nfree_blocks = csb->s_nfree_blocks;
    sb->s_fs_info = sbi;
    return 0;
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

/* Fill the struct superblock from partition superblock */
int basicftfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct buffer_head *bh = NULL;
    struct basicftfs_sb_info *csb = NULL;
    struct basicftfs_sb_info *sbi = NULL;
    struct inode *root_inode = NULL;
    int ret = 0;

    ret = init_super_block(sb);

    if (ret < 0) return ret;

    bh = sb_bread(sb, BASICFTFS_SB_BNO);
    if (!bh) return -EIO;

    csb = (struct basicftfs_sb_info *) bh->b_data;

    if (csb->s_magic != sb->s_magic) {
        printk(KERN_ERR "Wrong magic number: %x\n", csb->s_magic);
        brelse(bh);
        return -EINVAL;
    }

    sbi = kzalloc(sizeof(struct basicftfs_sb_info), GFP_KERNEL);
    if (!sbi) {
        printk(KERN_ERR "Could not allocate sufficient memory\n");
        brelse(bh);
        return -ENOMEM;
    }

    init_sbi(sb, csb, sbi);
    brelse(bh);

    sbi->s_ifree_bitmap = kzalloc(sbi->s_imap_blocks * BASICFTFS_BLOCKSIZE, GFP_KERNEL);
    if (!sbi->s_ifree_bitmap) {
        kfree(sbi);
        return -ENOMEM;
    }

    ret = init_bitmap(sb, sbi->s_ifree_bitmap, sbi->s_imap_blocks, 1);

    if (ret < 0) return ret;

    sbi->s_bfree_bitmap = kzalloc(sbi->s_bmap_blocks * BASICFTFS_BLOCKSIZE, GFP_KERNEL);
    if (!sbi->s_bfree_bitmap) {
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi);
        return -ENOMEM;
    }

    init_bitmap(sb, sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_bmap_blocks + 1);

    root_inode = basicftfs_iget(sb, 0);
    if (IS_ERR(root_inode)) {
        kfree(sbi->s_bfree_bitmap);
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi);
        return PTR_ERR(root_inode);
    }

    inode_init_owner(root_inode, NULL, root_inode->i_mode);

    init_empty_dir(sb, root_inode, root_inode);

    sb->s_root = d_make_root(root_inode);

    if (!sb->s_root) {
        iput(root_inode);
        kfree(sbi->s_bfree_bitmap);
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi);
        return -ENOMEM;
    }

    return 0;
}