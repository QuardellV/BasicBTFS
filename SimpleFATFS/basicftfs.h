#ifndef BASICFTFS_H
#define BASICFTFS_H

#define BASICFTFS_MAGIC_NUMBER         0x1DEADFAD
#define BASICFTFS_BLOCKSIZE            (1 << 12)
#define BASICFTFS_MAX_BLOCKS_PER_ENTRY 0 // TODO: define
#define BASICFTFS_FILE_BSIZE           0 // TODO: Define
#define BASICFTFS_SB_BNO               0
#define BASICFTFS_NAME_LENGTH          255
#define BASICFTFS_HASH_LENGTH          32
#define BASICFTFS_ATABLE_MAX_BLOCKS    ((BASICFTFS_BLOCKSIZE - sizeof(uint32_t)) / sizeof(uint32_t))
#define BASICFTFS_ENTRIES_PER_BLOCK    (BASICFTFS_BLOCKSIZE / sizeof(struct basicftfs_entry))
#define BASICFTFS_ENTRIES_PER_DIR      (BASICFTFS_ENTRIES_PER_BLOCK * BASICFTFS_ATABLE_MAX_BLOCKS)

struct basicftfs_inode {
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


#define BASICFTFS_INODES_PER_BLOCK (BASICFTFS_BLOCKSIZE / sizeof(struct basicftfs_inode))

struct basicftfs_sb_info {
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

struct basicftfs_inode_info {
    uint32_t i_bno;
    char i_data[32];
    struct inode vfs_inode;
};

struct basicftfs_entry {
    uint32_t ino;
    char hash_name[BASICFTFS_NAME_LENGTH];
};

struct basicftfs_entry_list {
    struct basicftfs_entry entries[BASICFTFS_ENTRIES_PER_BLOCK];
};

struct basicftfs_alloc_table {
    uint32_t nr_of_entries;
    uint32_t table[BASICFTFS_ATABLE_MAX_BLOCKS];
};

/* Cache functions for basic_inode_info*/
int basicftfs_init_inode_cache(void);
void basicftfs_destroy_inode_cache(void);

/* Superblock functions*/
int basicftfs_fill_super(struct super_block *sb, void *data, int silent);

/* Inode functions*/
struct inode *basicftfs_iget(struct super_block *sb, unsigned long ino);

/* Dir functions */
int basicftfs_add_entry(struct inode *dir, struct inode *inode, const unsigned char *name);
struct dentry *basicfs_search_entry(struct inode *dir, struct dentry *dentry);

/* Operation structs*/
extern const struct file_operations basicftfs_file_ops;
extern const struct file_operations basicftfs_dir_ops;
extern const struct address_space_operations basicftfs_aops;
extern const struct inode_operations basicftfs_inode_ops;
extern const struct inode_operations symlink_inode_ops;

/* Getter functions for disk info*/
#define BASICFTFS_SB(sb) (sb->s_fs_info)
#define BASICFTFS_INODE(inode) (container_of(inode, struct basicftfs_inode_info, vfs_inode))

#define BASICFTFS_GET_INODE_BLOCK(ino, inode_bmap_bsize, blocks_bmap_bsize) ((ino / BASICFTFS_INODES_PER_BLOCK) + inode_bmap_bsize + blocks_bmap_bsize + 1)
#define BASICFTFS_GET_INODE_BLOCK_IDX(ino) (ino % BASICFTFS_INODES_PER_BLOCK)
#define BASICFTFS_GET_BLOCK_IDX(pos) (pos / BASICFTFS_ENTRIES_PER_BLOCK)
#define BASICFTFS_GET_ENTRY_IDX(pos) (pos % BASICFTFS_ENTRIES_PER_BLOCK)

#endif


#endif