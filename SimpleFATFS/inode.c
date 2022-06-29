#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "basicftfs.h"
#include "bitmap.h"
#include "destroy.h"

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

    ino = get_free_inode(sbi->s_ifree_bitmap);

    if (!ino) {
        printk(KERN_ERR "Not enough free inodes available\n");
        return ERR_PTR(-ENOSPC);
    }

    inode = basicftfs_iget(sb, ino);

    if (IS_ERR(inode)) {
        printk(KERN_ERR "Could not allocate a new inode\n");
        put_inode(sbi, ino);
        return ERR_PTR(inode);
    }

    inode_info = BASICFTFS_INODE(inode);
    bno = get_free_blocks(sbi->s_bfree_bitmap, 1);

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
        
    }

    return 0;
}

static int basicftfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
    return 0;
}

const struct inode_operations basicfs_inode_ops = {
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

static void write_from_disk_to_vfs_inode(struct super_block *sb, struct inode *vfs_inode, struct basicftfs_inode *disk_inode, unsigned long ino) {
    vfs_inode->i_ino = ino;
    vfs_inode->i_sb = sb;
    vfs_inode->i_op = &basicfs_inode_ops;

    vfs_inode->i_mode = le32_to_cpu(disk_inode->i_mode);
    i_uid_write(vfs_inode, le32_to_cpu(disk_inode->i_uid));
    i_gid_write(vfs_inode, le32_to_cpu(disk_inode->i_gid));
    vfs_inode->i_size = le32_to_cpu(disk_inode->i_size);
    vfs_inode->i_ctime.tv_sec = (time64_t) le32_to_cpu(disk_inode->i_ctime);
    vfs_inode->i_ctime.tv_nsec = vfs_inode->i_atime.tv_nsec = vfs_inode->i_mtime.tv_nsec = 0;
    vfs_inode->i_atime.tv_sec = (time64_t) le32_to_cpu(disk_inode->i_atime);
    vfs_inode->i_mtime.tv_sec = (time64_t) le32_to_cpu(disk_inode->i_mtime);
    vfs_inode->i_blocks = le32_to_cpu(disk_inode->i_blocks);
    set_nlink(vfs_inode, le32_to_cpu(disk_inode->i_nlink));
}

static void init_inode_mode(struct inode *vfs_inode, struct basicftfs_inode *disk_inode, struct basicftfs_inode_info *inode_info) {
    if (S_ISDIR(vfs_inode->i_mode)) {
        inode_info->i_bno = le32_to_cpu(disk_inode->i_bno);
        vfs_inode->i_fop = &basicftfs_dir_ops;
    } else if (S_ISREG(vfs_inode->i_mode)) {
        inode_info->i_bno = le32_to_cpu(disk_inode->i_bno);
        vfs_inode->i_fop = &basicftfs_file_ops;
        vfs_inode->i_mapping->a_ops = &basicftfs_aops;
    } else if (S_ISLNK(vfs_inode->i_mode)) {
        strncpy(inode_info->i_data, disk_inode->i_data, sizeof(inode_info->i_data));
        vfs_inode->i_link = inode_info->i_data;
        vfs_inode->i_op = &symlink_inode_ops;
    }
}

static void init_mode_attributes(mode_t mode, struct inode *vfs_inode, struct basicftfs_inode_info *inode_info, uint32_t bno) {
    vfs_inode->i_blocks = 1;
    if (S_ISDIR(mode)) {
        inode_info->i_bno = bno;
        vfs_inode->i_size = BASICFTFS_BLOCKSIZE;
        vfs_inode->i_fop = &basicftfs_dir_ops;
        set_nlink(vfs_inode, 2); /* . and .. */
    } else if (S_ISREG(mode)) {
        inode_info->i_bno = bno;
        vfs_inode->i_size = 0;
        vfs_inode->i_fop = &basicftfs_file_ops;
        vfs_inode->i_mapping->a_ops = &basicftfs_aops;
        set_nlink(vfs_inode, 1);
    }
}

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