#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "bitmap.h"
#include "basicbtfs.h"
#include "destroy.h"
#include "io.h"
#include "init.h"
#include "defrag.h"

static int init_vfs_inode(struct super_block *sb, struct inode *inode, unsigned long ino) {
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct basicbtfs_inode *bftfs_inode = NULL;
    struct basicbtfs_inode_info *bftfs_inode_info = BASICBTFS_INODE(inode);
    struct buffer_head *bh = NULL;
    uint32_t inode_block = BASICBTFS_GET_INODE_BLOCK(ino, sbi->s_imap_blocks, sbi->s_bmap_blocks);
    uint32_t inode_block_idx = BASICBTFS_GET_INODE_BLOCK_IDX(ino);

    bh = sb_bread(sb, inode_block);

    if (!bh) {
        iget_failed(inode);
        return -EIO;
    }

    bftfs_inode = (struct basicbtfs_inode *) bh->b_data;
    bftfs_inode += inode_block_idx;
    write_from_disk_to_vfs_inode(sb, inode, bftfs_inode, ino);
    init_inode_mode(inode, bftfs_inode, bftfs_inode_info);
    return 0;
}

struct inode *basicbtfs_iget(struct super_block *sb, unsigned long ino)
{
    struct inode *inode = NULL;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
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

static struct dentry *basicbtfs_lookup(struct inode *dir, struct dentry *dentry, unsigned int flags) {
    struct dentry  *ret = 0;
    if (dentry->d_name.len > BASICBTFS_NAME_LENGTH) {
        printk(KERN_ERR "filename is longer than %d\n", BASICBTFS_NAME_LENGTH);
        return ERR_PTR(-ENAMETOOLONG);
    }

    ret =  basicbtfs_search_entry(dir, dentry);

    nr_of_inode_operations = increase_counter(nr_of_inode_operations, BASICBTFS_DEFRAG_PERIOD);

    return ret;
}

static struct inode *basicbtfs_new_inode(struct inode *dir, mode_t mode) {
    struct inode *inode = NULL;
    struct basicbtfs_inode_info *bfs_inode_info = NULL;
    struct super_block *sb = NULL;
    struct basicbtfs_sb_info *sbi = NULL;
    uint32_t ino, bno;
    int ret;

    if (!S_ISDIR(mode) && !S_ISREG(mode) && !S_ISLNK(mode)) {
        printk(KERN_ERR "File type not supported (only directory, and regular file are supported\n");
        return ERR_PTR(-EINVAL);
    }

    sb = dir->i_sb;
    sbi = BASICBTFS_SB(sb);

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

    inode = basicbtfs_iget(sb, ino);
    if (IS_ERR(inode)) {
        printk(KERN_ERR "Could not allocate a new inode\n");
        ret = PTR_ERR(inode);
        put_inode(sbi, ino);
        return ERR_PTR(ret);
    }

    bfs_inode_info = BASICBTFS_INODE(inode);
    bno = get_free_blocks(sbi, 1);

    if (bno == -1) {
        iput(inode);
        put_inode(sbi, ino);
        return ERR_PTR(-ENOSPC);
    }

    inode_init_owner(inode, dir, mode);
    init_mode_attributes(mode, inode, bfs_inode_info, bno);
    inode->i_ctime = inode->i_atime = inode->i_mtime = current_time(inode);

    return inode;
}

static int basicbtfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl) {
    struct super_block *sb = dir->i_sb;
    struct inode *inode = NULL;
    struct basicbtfs_inode_info *bfs_inode_info_dir = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_name_list_hdr *name_list_hdr = NULL;
    struct buffer_head *bh_dir = NULL, *bh = NULL, *bh_name_table = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int ret = 0;

    nr_of_inode_operations = increase_counter(nr_of_inode_operations, BASICBTFS_DEFRAG_PERIOD);

    if (strlen(dentry->d_name.name) > BASICBTFS_NAME_LENGTH) return -ENAMETOOLONG;

    bfs_inode_info_dir = BASICBTFS_INODE(dir);
    bh_dir = sb_bread(sb, bfs_inode_info_dir->i_bno);

    if (!bh_dir) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh_dir->b_data;
    node = &disk_block->block_type.btree_node;

    if (node->nr_of_files >= BASICBTFS_ENTRIES_PER_DIR) {
        printk(KERN_ERR "Parent directory is full\n");
        ret = -EMLINK;
        brelse(bh_dir);
        return ret;
    }

    inode = basicbtfs_new_inode(dir, mode);
    if (IS_ERR(inode)) {
        ret = PTR_ERR(inode);
        brelse(bh_dir);
        return ret;
    }

    /* clean block in case it contains garbage data. */
    ret = clean_block(sb, inode);

    if (ret == -EIO) {
        brelse(bh_dir);
        put_blocks(BASICBTFS_SB(sb), BASICBTFS_INODE(inode)->i_bno, 1);
        put_inode(BASICBTFS_SB(sb), inode->i_ino);
        iput(inode);
        return ret;
    }

    brelse(bh_dir);
    node = NULL;

    if (S_ISDIR(inode->i_mode)) {
        bh = sb_bread(sb, BASICBTFS_INODE(inode)->i_bno);

        if (!bh) return -EIO;

        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        disk_block->block_type_id = BASICBTFS_BLOCKTYPE_BTREE_NODE;
        node = &disk_block->block_type.btree_node;
        node->leaf = true;
        node->root = true;
        node->nr_of_files = 0;
        node->nr_of_keys = 0;
        node->parent = inode->i_ino;

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
            disk_block->block_type_id = BASICBTFS_BLOCKTYPE_NAMETREE;
            name_list_hdr = &disk_block->block_type.name_list_hdr;

            name_list_hdr->free_bytes = BASICBTFS_EMPTY_NAME_TREE;
            name_list_hdr->start_unused_area = BASICBTFS_BLOCKSIZE - BASICBTFS_EMPTY_NAME_TREE;
            name_list_hdr->prev_block = inode->i_ino;
            name_list_hdr->first_list =  true;
            name_list_hdr->next_block = 0;
            name_list_hdr->nr_of_entries = 0;
            mark_buffer_dirty(bh_name_table);
            brelse(bh_name_table);
        }

        bh_name_table = sb_bread(sb, node->tree_name_bno);

        if (!bh_name_table) {
            brelse(bh);
            return -EIO;
        }

        printk("new directory with btree and nametree: %d | %d\n", BASICBTFS_INODE(inode)->i_bno, node->tree_name_bno);
        

        mark_buffer_dirty(bh);
        brelse(bh);

    } else if (S_ISREG(inode->i_mode)) {
        bh = sb_bread(sb, BASICBTFS_INODE(inode)->i_bno);
        printk("new file with file cluster entry: %d\n", BASICBTFS_INODE(inode)->i_bno);

        if (!bh) return -EIO;
        disk_block = (struct basicbtfs_disk_block *) bh->b_data;
        disk_block->block_type.cluster_table.ino = inode->i_ino;
        disk_block->block_type_id =BASICBTFS_BLOCKTYPE_CLUSTER_TABLE;
        printk("ino of file: %ld\n", inode->i_ino);
        mark_buffer_dirty(bh);
        brelse(bh);
    }

    ret = basicbtfs_add_entry(dir, inode, dentry);
    if (ret < 0) {
        put_blocks(BASICBTFS_SB(sb), BASICBTFS_INODE(inode)->i_bno, 1);
        put_inode(BASICBTFS_SB(sb), inode->i_ino);
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

int basicbtfs_makedir(struct inode *dir, struct dentry *dentry, umode_t mode) {
    return basicbtfs_create(dir, dentry, mode | S_IFDIR, 0);
}


static int basicbtfs_mkdir(struct inode *dir, struct dentry *dentry, umode_t mode) {
    return basicbtfs_create(dir, dentry, mode | S_IFDIR, 0);
}

static int basicbtfs_link(struct dentry *old_dentry,
                         struct inode *dir,
                         struct dentry *dentry)
{
    struct inode *inode = d_inode(old_dentry);
    int ret = 0;

    nr_of_inode_operations = increase_counter(nr_of_inode_operations, BASICBTFS_DEFRAG_PERIOD);

    inode_inc_link_count(inode);
    ret = basicbtfs_add_entry(dir, inode, dentry);
    d_instantiate(dentry, inode);

    return ret;
}

static int basicbtfs_unlink(struct inode *dir ,struct dentry *dentry) {
    int ret = 0;
    uint32_t bno = 0, ino = 0;
    struct super_block *sb  = dir->i_sb;
    struct basicbtfs_sb_info *sbi = BASICBTFS_SB(sb);
    struct buffer_head *bh = NULL;
    struct basicbtfs_disk_block *basicbtfs_disk_block = NULL;
    struct inode *inode = d_inode(dentry);
    ino = inode->i_ino;

    nr_of_inode_operations = increase_counter(nr_of_inode_operations, BASICBTFS_DEFRAG_PERIOD);

    ret = basicbtfs_delete_entry(dir, dentry);

    if (ret < 0) return ret;

    dir->i_mtime = dir->i_atime = dir->i_ctime = current_time(dir);

    inode_dec_link_count(inode);

    /* Currently, it just resets the inode*/
    bno = BASICBTFS_INODE(inode)->i_bno;

    bh = sb_bread(sb, bno);
    if (!bh) {
        clean_inode(inode);
        put_blocks(sbi, bno, 1);
        put_inode(sbi, inode->i_ino);
        return ret;
    }

    basicbtfs_disk_block = (struct basicbtfs_disk_block *) bh->b_data;

    if (S_ISDIR(inode->i_mode)) {
        basicbtfs_nametree_free_namelist_blocks(sb, basicbtfs_disk_block->block_type.btree_node.tree_name_bno );
        basicbtfs_btree_free_dir(sb, inode, bno);
        put_inode(sbi, ino);

        clean_inode(inode);
    } else if (S_ISREG(inode->i_mode)) {
        basicbtfs_file_free_blocks(inode);
    }

    return ret;
}

static int basicbtfs_rmdir(struct inode *dir, struct dentry *dentry) {
    struct inode *inode = d_inode(dentry);
    struct super_block *sb = dir->i_sb;
    uint32_t ino = BASICBTFS_INODE(inode)->i_bno;
    struct buffer_head *bh = NULL;
    struct basicbtfs_btree_node *node = NULL;
    struct basicbtfs_disk_block *disk_block = NULL;
    int ret = basicbtfs_unlink(dir, dentry);

    if (inode->i_nlink > 2) return -ENOTEMPTY;

    bh = sb_bread(sb, ino);

    if (!bh) return -EIO;

    disk_block = (struct basicbtfs_disk_block *) bh->b_data;
    node = &disk_block->block_type.btree_node;

    if (node->nr_of_files > 0) {
        brelse(bh);
        return -ENOTEMPTY;
    }

    brelse(bh);
    return ret;
}

static int basicbtfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry, unsigned int flags) {
    int ret = 0;

    nr_of_inode_operations = increase_counter(nr_of_inode_operations, BASICBTFS_DEFRAG_PERIOD);

    if (flags & (RENAME_EXCHANGE)) {
        return -EINVAL;
    }

    if (flags & (RENAME_EXCHANGE & RENAME_NOREPLACE)) {
        return -EINVAL;
    }

    if (strlen(new_dentry->d_name.name) > BASICBTFS_NAME_LENGTH) {
        return -ENAMETOOLONG;
    }

    ret = basicbtfs_update_entry(old_dir, new_dir, old_dentry, new_dentry, flags);

    new_dir->i_atime = new_dir->i_ctime = new_dir->i_mtime = current_time(new_dir);

    if (S_ISDIR(d_inode(old_dentry)->i_mode)) inc_nlink(new_dir);

    mark_inode_dirty(new_dir);

    old_dir->i_atime = old_dir->i_ctime = old_dir->i_mtime = current_time(old_dir);

    if (S_ISDIR(d_inode(old_dentry)->i_mode)) drop_nlink(old_dir);

    mark_inode_dirty(old_dir);
    return 0;
}

const struct inode_operations basicbtfs_inode_ops = {
    .lookup = basicbtfs_lookup,
    .create = basicbtfs_create,
    .unlink = basicbtfs_unlink,
    .mkdir = basicbtfs_mkdir,
    .rmdir = basicbtfs_rmdir,
    .rename = basicbtfs_rename,
    .link = basicbtfs_link,
};
