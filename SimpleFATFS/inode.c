#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"
#include "bitmap.h"
#include "init.h"
#include "destroy.h"
#include "io.h"

struct inode *basicftfs_new_inode(struct inode *dir, mode_t mode) {
    struct super_block *sb = dir->i_sb;
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    struct inode *inode = NULL;
    struct basicftfs_inode_info *inode_info = NULL;
    uint32_t ino = 0, bno = 0;

    if (!S_ISDIR(mode) && !S_ISREG(mode)) {
        printk(KERN_ERR "File type not supported\n");
        return ERR_PTR(-EINVAL);
    }

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
        put_inode(sbi, ino);
        return ERR_PTR(PTR_ERR(inode));
    }

    inode_info = BASICFTFS_INODE(inode);
    bno = get_free_blocks(sbi, 1);

    if (!bno) {
        iput(inode);
        put_inode(sbi, ino);
        return ERR_PTR(-ENOSPC);
    }

    inode_init_owner(inode, dir, mode);
    init_mode_attributes(mode, inode, inode_info, bno);
    inode->i_ctime = inode->i_atime = inode->i_mtime = current_time(inode);

    return inode;
}

static int basicftfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    struct super_block *sb = dir->i_sb;
    struct basicftfs_inode_info *par_inode_info = NULL;
    struct basicftfs_alloc_table *alloc_table = NULL;
    struct buffer_head *bh = NULL;
    struct inode *inode = NULL;
    int ret = 0;

    if (strlen(dentry->d_name.name) > BASICFTFS_NAME_LENGTH) return -ENAMETOOLONG;

    par_inode_info = BASICFTFS_INODE(dir);
    bh = sb_bread(sb, par_inode_info->i_bno);

    if (!bh) return -EIO;

    alloc_table = (struct basicftfs_alloc_table *) bh->b_data;

    if (alloc_table->nr_of_entries >= BASICFTFS_ATABLE_MAX_BLOCKS) {
        printk(KERN_ERR "Full parent directory\n");
        brelse(bh);
        return -EMLINK;
    }

    brelse(bh);

    inode = basicftfs_new_inode(dir, mode);

    if (IS_ERR(inode)) {
        return PTR_ERR(inode);
    }

    ret = clean_block(sb, inode);

    if (ret < 0) {
        put_blocks(BASICFTFS_SB(sb), BASICFTFS_INODE(inode)->i_bno, 1);
        // dir->i_blocks--;
        put_inode(BASICFTFS_SB(sb), inode->i_ino);
        iput(inode);
        return ret;
    }

    ret = basicftfs_add_entry(dir, inode, dentry);

    if (ret < 0) {
        put_blocks(BASICFTFS_SB(sb), BASICFTFS_INODE(inode)->i_bno, 1);
        // dir->i_blocks--;
        put_inode(BASICFTFS_SB(sb), inode->i_ino);
        iput(inode);
        return ret;
    }

    mark_inode_dirty(inode);
    d_instantiate(dentry, inode);
    return 0;
}

static int basicftfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
    return 0;
}

const struct inode_operations basicftfs_inode_ops = {
    // .lookup = basicftfs_lookup,
    .create = basicftfs_create,
    // .unlink = basicftfs_unlink,
    .mkdir = basicftfs_mkdir,
    // .rmdir = basicftfs_rmdir,
    // .rename = basicftfs_rename,
    // .link = basicftfs_link,
};

const struct inode_operations symlink_inode_ops = {
    // .get_link = basicfs_get_link,
};

struct inode *basicftfs_iget(struct super_block *sb, unsigned long ino) {
    struct basicftfs_sb_info *sbi = BASICFTFS_SB(sb);
    struct inode *inode = NULL;
    int ret = 0;

    if (ino > sbi->s_ninodes) {
        printk(KERN_ERR "Not enough inodes available\n");
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