#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/statfs.h>

#include "basicbtfs.h"
#include "io.h"
#include "init.h"
#include "bitmap.h"

static struct kmem_cache *basicbtfs_inode_cache;

bool should_defrag = true;
bool defrag_now = false;
uint32_t nr_of_inode_operations = 0;



int basicbtfs_init_inode_cache(void) {
    basicbtfs_inode_cache = kmem_cache_create("basicbtfs_cache", sizeof(struct basicbtfs_inode_info), 0, 0, NULL);

    if (!basicbtfs_inode_cache) return -ENOMEM;
    return 0;
}

void basicbtfs_destroy_inode_cache(void) {
    kmem_cache_destroy(basicbtfs_inode_cache);
}

static void basicbtfs_put_super(struct super_block *sb) {
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    if (sbi) {
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi->s_bfree_bitmap);
        kfree(sbi);
    }
}

static struct inode *basicbtfs_alloc_inode(struct super_block *sb) {
    struct basicbtfs_inode_info *ci = kmem_cache_alloc(basicbtfs_inode_cache, GFP_KERNEL);
    if (!ci) return NULL;

    inode_init_once(&ci->vfs_inode);
    return &ci->vfs_inode;
}



static int basicbtfs_write_inode(struct inode *inode, struct writeback_control *wbc) {
    struct basicbtfs_inode *disk_inode;
    struct super_block *sb = inode->i_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct buffer_head *bh;
    uint32_t ino = inode->i_ino;
    uint32_t inode_block = BASICBTFS_GET_INODE_BLOCK(ino, sbi->s_imap_blocks, sbi->s_bmap_blocks);
    uint32_t inode_bi = BASICBTFS_GET_INODE_BLOCK_IDX(ino);

    if (ino >= sbi->s_ninodes) return 0;

    bh = sb_bread(sb, inode_block);

    if (!bh) return -EIO;

    disk_inode = (struct basicbtfs_inode *) bh->b_data;
    disk_inode += inode_bi;

    write_from_vfs_inode_to_disk(disk_inode, inode);
    mark_buffer_dirty(bh);
    sync_dirty_buffer(bh);
    brelse(bh);

    return 0;
}

static void basicbtfs_destroy_inode(struct inode *inode) {
    struct basicbtfs_inode_info *ci = BASICBTFS_INODE(inode);
    kmem_cache_free(basicbtfs_inode_cache, ci);
}

static int basicbtfs_sync_fs(struct super_block *sb, int wait)
{
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    int ret = 0;

    /* Flush superblock */
    printk("basicbtfs_sync_fs() 0: %d\n", 0);
    ret = flush_superblock(sb, wait);
    if (ret < 0) return ret;
    ret = flush_bitmap(sb, sbi->s_ifree_bitmap, sbi->s_imap_blocks, 1, wait);
    if (ret < 0) return ret;
    ret = flush_bitmap(sb, sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_imap_blocks + 1, wait);

    return ret;
}

static int basicbtfs_statfs(struct dentry *dentry, struct kstatfs *stat)
{
    struct super_block *sb = dentry->d_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);

    stat->f_type = BASICBTFS_MAGIC_NUMBER;
    stat->f_bsize = BASICBTFS_BLOCKSIZE;
    stat->f_blocks = sbi->s_nblocks;
    stat->f_bfree = sbi->s_nfree_blocks;
    stat->f_bavail = sbi->s_nfree_blocks;
    stat->f_files = sbi->s_ninodes - sbi->s_nfree_inodes;
    stat->f_ffree = sbi->s_nfree_inodes;
    stat->f_namelen = BASICBTFS_NAME_LENGTH;

    return 0;
}

static struct super_operations basicftfs_super_ops = {
    .put_super = basicbtfs_put_super,
    .alloc_inode = basicbtfs_alloc_inode,
    .destroy_inode = basicbtfs_destroy_inode,
    .write_inode = basicbtfs_write_inode,
    .sync_fs = basicbtfs_sync_fs,
    .statfs = basicbtfs_statfs,
};

struct list_head dir_cache_list = LIST_HEAD_INIT(dir_cache_list);

int init_super_block(struct super_block *sb) {
    int ret = 0;
    sb->s_magic = BASICBTFS_MAGIC_NUMBER;
    ret = sb_set_blocksize(sb, BASICBTFS_BLOCKSIZE);
    sb->s_maxbytes = BASICBTFS_FILE_BSIZE;
    sb->s_op = &basicftfs_super_ops;
    return ret;
}

int init_sbi(struct super_block *sb, struct basicbtfs_sb_info *csb, struct basicbtfs_sb_info *sbi) {
    sbi->s_nblocks = csb->s_nblocks;
    sbi->s_ninodes = csb->s_ninodes;
    sbi->s_inode_blocks = csb->s_inode_blocks;
    sbi->s_imap_blocks = csb->s_imap_blocks;
    sbi->s_bmap_blocks = csb->s_bmap_blocks;
    sbi->s_nfree_inodes = csb->s_nfree_inodes;
    sbi->s_nfree_blocks = csb->s_nfree_blocks;
    sbi->s_cache_dir_entries = 0;
    sbi->s_filemap_blocks = csb->s_filemap_blocks;
    sbi->s_unused_area = csb->s_unused_area;
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

        memcpy((void *) bitmap + i * BASICBTFS_BLOCKSIZE, bh->b_data, BASICBTFS_BLOCKSIZE);
        brelse(bh);
    }
    return 0;
}

int init_filemap(struct super_block *sb, struct basicbtfs_fileblock_info *filemap, uint32_t nr_blocks, uint32_t block_offset) {
    struct buffer_head *bh = NULL;
    int i = 0;

    for (i = 0; i < nr_blocks; i++) {
        int idx = block_offset + i;
        bh = sb_bread(sb, idx);

        if (!bh) return -EIO;

        memcpy((void *) filemap + i * BASICBTFS_BLOCKSIZE, bh->b_data, BASICBTFS_BLOCKSIZE);
        brelse(bh);
    }
    return 0;
}

/* Fill the struct superblock from partition superblock */
int basicbtfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct buffer_head *bh = NULL, *bh_name_table = NULL;
    struct basicbtfs_sb_info *csb = NULL;
    struct basicbtfs_sb_info *sbi = NULL;
    struct inode *root_inode = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    struct basicbtfs_name_list_hdr *list_hdr = NULL;
    int ret = 0;

    ret = init_super_block(sb);

    if (ret < 0) return ret;

    bh = sb_bread(sb, BASICBTFS_SB_BNO);
    if (!bh) return -EIO;

    csb = (struct basicbtfs_sb_info *) bh->b_data;

    if (csb->s_magic != sb->s_magic) {
        printk(KERN_ERR "Wrong magic number: %x\n", csb->s_magic);
        brelse(bh);
        return -EINVAL;
    }

    sbi = kzalloc(sizeof(struct basicbtfs_sb_info), GFP_KERNEL);
    if (!sbi) {
        printk(KERN_ERR "Could not allocate sufficient memory\n");
        brelse(bh);
        return -ENOMEM;
    }
    should_defrag = true;
    nr_of_inode_operations = 0;
    init_sbi(sb, csb, sbi);
    brelse(bh);

    sbi->s_ifree_bitmap = kzalloc(sbi->s_imap_blocks * BASICBTFS_BLOCKSIZE, GFP_KERNEL);
    if (!sbi->s_ifree_bitmap) {
        kfree(sbi);
        printk("not sufficient memory for ifree bitmap\n");
        return -ENOMEM;
    }

    ret = init_bitmap(sb, sbi->s_ifree_bitmap, sbi->s_imap_blocks, 1);

    if (ret < 0) return ret;

    sbi->s_bfree_bitmap = kzalloc(sbi->s_bmap_blocks * BASICBTFS_BLOCKSIZE, GFP_KERNEL);
    if (!sbi->s_bfree_bitmap) {
        printk("not sufficient memory for bfree bitmap\n");
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi);
        return -ENOMEM;
    }

    init_bitmap(sb, sbi->s_bfree_bitmap, sbi->s_bmap_blocks, sbi->s_imap_blocks + 1);

    root_inode = basicbtfs_iget(sb, 0);
    if (IS_ERR(root_inode)) {
        kfree(sbi->s_bfree_bitmap);
        kfree(sbi->s_ifree_bitmap);
        kfree(sbi);
        return PTR_ERR(root_inode);
    }

    inode_init_owner(root_inode, NULL, root_inode->i_mode);


    bh = sb_bread(sb, BASICBTFS_INODE(root_inode)->i_bno);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    disk_block->block_type_id = BASICBTFS_BLOCKTYPE_BTREE_NODE;
    node = &disk_block->block_type.btree_node;
    node->leaf = true;
    node->root = true;
    node->nr_of_keys = 0;
    node->nr_of_files = 0;
    node->parent = root_inode->i_ino;

    if (node->tree_name_bno == 0) {
        node->tree_name_bno = get_free_blocks(BASICBTFS_SB(sb), 1);

        if (node->tree_name_bno == -1) {
            return -ENOSPC;
        }
        bh_name_table = sb_bread(sb, node->tree_name_bno);

        if (!bh_name_table) {
            brelse(bh);
            put_blocks(BASICBTFS_SB(sb), node->tree_name_bno, 1);
            return -EIO;
        }
        disk_block = (struct basicbtfs_disk_block *) bh_name_table->b_data;
        memset(disk_block, 0, sizeof(struct basicbtfs_disk_block));
        disk_block->block_type_id =BASICBTFS_BLOCKTYPE_NAMETREE;
        list_hdr = &disk_block->block_type.name_list_hdr;

        list_hdr->free_bytes = BASICBTFS_EMPTY_NAME_TREE;
        list_hdr->start_unused_area = BASICBTFS_BLOCKSIZE - BASICBTFS_EMPTY_NAME_TREE;
        list_hdr->first_list = true;
        list_hdr->prev_block = root_inode->i_ino;
        list_hdr->next_block = 0;
        list_hdr->nr_of_entries = 0;
        mark_buffer_dirty(bh_name_table);
        brelse(bh_name_table);
    }

    mark_buffer_dirty(bh);
    brelse(bh);

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
