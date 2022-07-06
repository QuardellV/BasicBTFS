#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "bitmap.h"
#include "basicftfs.h"
#include "destroy.h"
#include "io.h"
#include "init.h"

static int init_vfs_inode(struct super_block *sb, struct inode *inode, unsigned long ino) {
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    struct basicftfs_inode *bftfs_inode = NULL;
    struct basicftfs_inode_info *bftfs_inode_info = BASICFTFS_INODE(inode);
    struct buffer_head *bh = NULL;
    uint32_t inode_block = BASICFTFS_GET_INODE_BLOCK(ino, sbi->s_imap_blocks, sbi->s_bmap_blocks);
    uint32_t inode_block_idx = BASICFTFS_GET_INODE_BLOCK_IDX(ino);

    bh = sb_bread(sb, inode_block);

    if (!bh) {
        iget_failed(inode);
        return -EIO;
    }

    bftfs_inode = (struct basicftfs_inode *) bh->b_data;
    bftfs_inode += inode_block_idx;
    write_from_disk_to_vfs_inode(sb, inode, bftfs_inode, ino);
    init_inode_mode(inode, bftfs_inode, bftfs_inode_info);
    return 0;
}

struct inode *basicftfs_iget(struct super_block *sb, unsigned long ino)
{
    struct inode *inode = NULL;
    struct basicftfs_inode *bfs_inode = NULL;
    struct basicftfs_inode_info *bfs_inode_info = NULL;
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    struct buffer_head *bh = NULL;
    uint32_t inode_block = BASICFTFS_GET_INODE_BLOCK(ino, sbi->s_imap_blocks, sbi->s_bmap_blocks);
    uint32_t inode_bi = BASICFTFS_GET_INODE_BLOCK_IDX(ino);

    int ret= 0;

    if (ino >= sbi->s_ninodes) {
        printk(KERN_ERR "ino is bigger than the number of inodes\n");
        return ERR_PTR(-EINVAL);
    }

    inode = iget_locked(sb, ino);
    if (!inode) {
        printk(KERN_ERR "No inode could be allocated\n");
        return ERR_PTR(-ENOMEM);
    }

    if (!(inode->i_state & I_NEW)) {
        return inode;
    }

    ret = init_vfs_inode(sb, inode, ino);

    if (ret < 0) return ERR_PTR(ret);
    unlock_new_inode(inode);

    return inode;
}

static struct dentry *basicftfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    if (dentry->d_name.len > BASICFTFS_NAME_LENGTH) {
        printk(KERN_ERR "filename is longer than %d\n", BASICFTFS_NAME_LENGTH);
        return ERR_PTR(-ENAMETOOLONG);
    }

    return basicftfs_search_entry(dir, dentry);
}

static struct inode *basicftfs_new_inode(struct inode *dir, mode_t mode) {
    struct inode *inode = NULL;
    struct basicftfs_inode_info *bfs_inode_info = NULL;
    struct super_block *sb = NULL;
    struct basicftfs_sb_info *sbi = NULL;
    uint32_t ino, bno;
    int ret;

    if (!S_ISDIR(mode) && !S_ISREG(mode) && !S_ISLNK(mode)) {
        printk(KERN_ERR "File type not supported (only directory, and regular file are supported\n");
        return ERR_PTR(-EINVAL);
    }

    sb = dir->i_sb;
    sbi = BASICFTFS_SB(sb);

    if (sbi->s_nfree_inodes == 0) {
        printk(KERN_ERR "Not enough free inodes available\n");
        return ERR_PTR(-ENOSPC);
    }
    if (sbi->s_nfree_blocks == 0) {
        printk(KERN_ERR "Not enough free blocks available\n");
        return ERR_PTR(-ENOSPC);
    }

    ino = get_free_inode(sbi);
    if (!ino) {
        printk(KERN_ERR "Not enough free inodes available\n");
        return ERR_PTR(-ENOSPC);
    }

    inode = basicftfs_iget(sb, ino);
    if (IS_ERR(inode)) {
        printk(KERN_ERR "Could not allocate a new inode\n");
        ret = PTR_ERR(inode);
        put_inode(sbi, ino);
        return ERR_PTR(ret);
    }

    bfs_inode_info = BASICFTFS_INODE(inode);
    bno = get_free_blocks(sbi, 1);

    if (!bno) {
        iput(inode);
        put_inode(sbi, ino);
        return ERR_PTR(-ENOSPC);
    }

    inode_init_owner(inode, dir, mode);
    init_mode_attributes(mode, inode, bfs_inode_info, bno);
    inode->i_ctime = inode->i_atime = inode->i_mtime = current_time(inode);

    return inode;
}

static int basicftfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    struct super_block *sb = dir->i_sb;
    struct inode *inode = NULL;
    struct basicftfs_inode_info *bfs_inode_info_dir = NULL;
    struct basicftfs_alloc_table *cblock = NULL;
    struct basicftfs_entry_list *fblock = NULL;
    struct buffer_head *bh_dir, *bh_dblock = NULL;
    uint32_t page = 0;
    int ret = 0, is_allocated = false, bno = 0;
    int bi = 0, fi = 0;

    if (strlen(dentry->d_name.name) > BASICFTFS_NAME_LENGTH) return -ENAMETOOLONG;

    bfs_inode_info_dir = BASICFTFS_INODE(dir);
    printk("basicftfs_create() sb_bread bfs_inode_info_dir->data_block: %d\n", bfs_inode_info_dir->i_bno);
    bh_dir = sb_bread(sb, bfs_inode_info_dir->i_bno);

    if (!bh_dir) {
        return -EIO;
    }

    cblock = (struct basicftfs_alloc_table *) bh_dir->b_data;

    if (cblock->nr_of_entries >= BASICFTFS_ENTRIES_PER_DIR) {
        printk(KERN_ERR "Parent directory is full\n");
        ret = -EMLINK;
        brelse(bh_dir);
        return ret;
    }

    if (cblock->nr_of_entries == 0) {
        printk(KERN_INFO "Initialize");
    }

    inode = basicftfs_new_inode(dir, mode);
    if (IS_ERR(inode)) {
        ret = PTR_ERR(inode);
        brelse(bh_dir);
        return ret;
    }

    /* clean block in case it contains garbage data. */
    ret = clean_block(sb, inode);

    if (ret == -EIO) {
        brelse(bh_dir);
        put_blocks(BASICFTFS_SB(sb), BASICFTFS_INODE(inode)->i_bno, 1);
        // dir->i_blocks--;
        put_inode(BASICFTFS_SB(sb), inode->i_ino);
        iput(inode);
        return ret;
    }

    brelse(bh_dir);

    ret = basicftfs_add_entry(dir, inode, dentry);

    if (ret < 0) {
        put_blocks(BASICFTFS_SB(sb), BASICFTFS_INODE(inode)->i_bno, 1);
        // dir->i_blocks--;
        put_inode(BASICFTFS_SB(sb), inode->i_ino);
        iput(inode);
        return ret;
    }

    /* Update stats and mark dir and new inode dirty */
    mark_inode_dirty(inode);
    dir->i_mtime = dir->i_atime = dir->i_ctime = current_time(dir);

    if (S_ISDIR(mode)) {
        inc_nlink(dir);
    }

    mark_inode_dirty(dir);
    d_instantiate(dentry, inode);

    return 0;
}

int basicftfs_makedir(struct inode *dir, struct dentry *dentry, umode_t mode) {
    return basicftfs_create(dir, dentry, mode | S_IFDIR, 0);
}


static int basicftfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
    return basicftfs_create(dir, dentry, mode | S_IFDIR, 0);
}

static int basicftfs_link(struct dentry *old_dentry,
                         struct inode *dir,
                         struct dentry *dentry)
{
    struct inode *inode = d_inode(old_dentry);
    int ret = 0;
    inode_inc_link_count(inode);
    ret = basicftfs_add_entry(dir, inode, dentry);
    d_instantiate(dentry, inode);
    return ret;
}

static const char *basicftfs_get_link(struct dentry *dentry, struct inode *inode, struct delayed_call *done) {
    return inode->i_link;
}

const struct inode_operations basicftfs_inode_ops = {
    .lookup = basicftfs_lookup,
    .create = basicftfs_create,
    // .unlink = basicftfs_unlink,
    .mkdir = basicftfs_mkdir,
    // .rmdir = basicftfs_rmdir,
    // .rename = basicftfs_rename,
    .link = basicftfs_link,
};

const struct inode_operations symlink_inode_ops = {
    .get_link = basicftfs_get_link,
};