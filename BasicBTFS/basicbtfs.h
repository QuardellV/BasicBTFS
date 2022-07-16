#ifndef BASICBTFS_H
#define BASICBTFS_H

#define BASICBTFS_MAGIC_NUMBER         0x1DEADBAD
#define BASICBTFS_BLOCKSIZE            (1 << 12)
#define BASICBTFS_SB_BNO               0
#define BASICBTFS_NAME_LENGTH          255
#define BASICBTFS_HASH_LENGTH          32
#define BASICBTFS_SALT_LENGTH          8
#define BASICBTFS_MIN_DEGREE           7
#define BASICBTFS_ATABLE_MAX_BLOCKS    ((BASICBTFS_BLOCKSIZE - sizeof(uint32_t)) / sizeof(uint32_t))
#define BASICBTFS_ENTRIES_PER_BLOCK    (BASICBTFS_BLOCKSIZE / sizeof(struct basicbtfs_entry))
#define BASICBTFS_ENTRIES_PER_DIR      (BASICBTFS_ENTRIES_PER_BLOCK * BASICBTFS_ATABLE_MAX_BLOCKS)
#define BASICBTFS_FILE_BSIZE           (BASICBTFS_BLOCKSIZE * BASICBTFS_ATABLE_MAX_BLOCKS)


struct basicbtfs_inode {
    uint32_t i_mode;
    uint32_t i_nlink;
    uint32_t i_blocks;
    uint32_t i_uid;
    uint32_t i_gid;
    uint32_t i_size;
    uint32_t i_ctime;
    uint32_t i_atime;
    uint32_t i_mtime;
    uint32_t i_bno;
    char i_data[32];
};


#define BASICBTFS_INODES_PER_BLOCK (BASICBTFS_BLOCKSIZE / sizeof(struct basicbtfs_inode))

struct basicbtfs_sb_info {
    uint32_t s_magic;
    uint32_t s_nblocks;
    uint32_t s_ninodes;
    uint32_t s_imap_blocks;
    uint32_t s_bmap_blocks;
    uint32_t s_inode_blocks;
    uint32_t s_nfree_inodes;
    uint32_t s_nfree_blocks;

#ifdef __KERNEL__
    unsigned long *s_ifree_bitmap;
    unsigned long *s_bfree_bitmap;
#endif
};

#ifdef __KERNEL__

struct basicbtfs_inode_info {
    uint32_t i_bno;
    char i_data[32];
    struct inode vfs_inode;
};

struct basicbtfs_entry {
    uint32_t ino;
    char hash_name[BASICBTFS_NAME_LENGTH];
};

struct basicbtfs_alloc_table {
    uint32_t nr_of_entries;
    uint32_t table[BASICBTFS_ATABLE_MAX_BLOCKS];
};

struct basicbtfs_btree_node {
    struct basicbtfs_entry entries[2 * BASICBTFS_MIN_DEGREE - 1];
    uint32_t children[2 * BASICBTFS_MIN_DEGREE];
    uint32_t nr_of_keys;
    uint32_t nr_of_files;
    bool leaf;
};

/* Cache functions for basic_inode_info*/
int basicbtfs_init_inode_cache(void);
void basicbtfs_destroy_inode_cache(void);

/* Superblock functions*/
int basicbtfs_fill_super(struct super_block *sb, void *data, int silent);

/* Inode functions*/
struct inode *basicbtfs_iget(struct super_block *sb, unsigned long ino);

/* Dir functions */
int basicbtfs_add_entry_name(struct inode *dir, struct inode *inode, char *filename);
int basicbtfs_add_entry(struct inode *dir, struct inode *inode, struct dentry *dentry);
struct dentry *basicbtfs_search_entry(struct inode *dir, struct dentry *dentry);
int basicbtfs_delete_entry(struct inode *dir, struct inode *inode);
int basicbtfs_update_entry(struct inode *old_dir, struct inode *new_dir, struct dentry *old_dentry, struct dentry *new_dentry, unsigned int flags);
int clean_file_block(struct inode *inode);

/* Operation structs*/
extern const struct file_operations basicbtfs_file_ops;
extern const struct file_operations basicbtfs_dir_ops;
extern const struct address_space_operations basicbtfs_aops;
extern const struct inode_operations basicbtfs_inode_ops;

/* Getter functions for disk info*/
#define BASICBTFS_SB(sb) (sb->s_fs_info)
#define BASICBTFS_INODE(inode) (container_of(inode, struct basicbtfs_inode_info, vfs_inode))

#define BASICBTFS_GET_INODE_BLOCK(ino, inode_bmap_bsize, blocks_bmap_bsize) ((ino / BASICBTFS_INODES_PER_BLOCK) + inode_bmap_bsize + blocks_bmap_bsize + 1)
#define BASICBTFS_GET_INODE_BLOCK_IDX(ino) (ino % BASICBTFS_INODES_PER_BLOCK)
#define BASICBTFS_GET_BLOCK_IDX(pos) (pos / BASICBTFS_ENTRIES_PER_BLOCK)
#define BASICBTFS_GET_ENTRY_IDX(pos) (pos % BASICBTFS_ENTRIES_PER_BLOCK)

#endif

#endif