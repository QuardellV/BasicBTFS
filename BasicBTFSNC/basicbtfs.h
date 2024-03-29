#ifndef BASICBTFS_H
#define BASICBTFS_H

#define BASICBTFS_MAGIC_NUMBER         0x1DEADBAD
#define BASICBTFS_IOCTL_MAGIC          0x94
#define BASICBTFS_BLOCKSIZE            (1 << 12)
#define BASICBTFS_SB_BNO               0
#define BASICBTFS_NAME_LENGTH          255
#define BASICBTFS_HASH_LENGTH          32
#define BASICBTFS_SALT_LENGTH          8
#define BASICBTFS_MIN_DEGREE           80
#define BASICBTFS_MAX_BLOCKS_PER_CLUSTER 16
#define BASICBTFS_ATABLE_MAX_BLOCKS    ((BASICBTFS_BLOCKSIZE - sizeof(uint32_t)) / sizeof(uint32_t))
#define BASICBTFS_ATABLE_MAX_CLUSTERS  ((BASICBTFS_BLOCKSIZE - 3 * sizeof(uint32_t)) / sizeof(struct basicbtfs_cluster))
#define BASICBTFS_MAX_BLOCKS_PER_DIR   (BASICBTFS_ATABLE_MAX_CLUSTERS * BASICBTFS_MAX_BLOCKS_PER_CLUSTER)
#define BASICBTFS_ENTRIES_PER_BLOCK    (BASICBTFS_BLOCKSIZE / sizeof(struct basicbtfs_entry))
#define BASICBTFS_ENTRIES_PER_DIR      (BASICBTFS_ENTRIES_PER_BLOCK * BASICBTFS_ATABLE_MAX_BLOCKS)
#define BASICBTFS_FILE_BSIZE           (BASICBTFS_BLOCKSIZE * BASICBTFS_MAX_BLOCKS_PER_DIR)
#define BASICBTFS_EMPTY_NAME_TREE      ((BASICBTFS_BLOCKSIZE - BASICBTFS_NAME_ENTRY_S_OFFSET))
#define BASICBTFS_NAME_ENTRY_S_OFFSET  ((sizeof(struct basicbtfs_name_list_hdr) + sizeof(uint32_t)))
#define BASICBTFS_WORDS_PER_BLOCK      ((BASICBTFS_BLOCKSIZE / sizeof(struct basicbtfs_fileblock_info)))


#define BASICBTFS_MAX_CACHE_DIR_ENTRIES    100
#define BASICBTFS_MAX_CACHE_BLOCKS_PER_DIR 250

#define BASICBTFS_BLOCKTYPE_BTREE_NODE    0x01
#define BASICBTFS_BLOCKTYPE_NAMETREE      0x02
#define BASICBTFS_BLOCKTYPE_CLUSTER_TABLE 0x03

#define BASICBTFS_DEFRAG_PERIOD 100

struct basicbtfs_ioctl_vol_args {
    int fd;
    char name[4000 + 1];
};

#define BASICBTFS_IOC_DEFRAG _IOW(BASICBTFS_IOCTL_MAGIC, 1, struct basicbtfs_ioctl_vol_args)

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

struct basicbtfs_fileblock_info {
    uint32_t ino;
    uint32_t cluster_index;
};


#define BASICBTFS_INODES_PER_BLOCK (BASICBTFS_BLOCKSIZE / sizeof(struct basicbtfs_inode))
#define BASICBTFS_FBLOCK_INFO_PER_BLOCK (BASICBTFS_BLOCKSIZE / sizeof(struct basicbtfs_fileblock_info))

struct basicbtfs_sb_info {
    uint32_t s_magic;
    uint32_t s_nblocks;
    uint32_t s_ninodes;
    uint32_t s_imap_blocks;
    uint32_t s_bmap_blocks;
    uint32_t s_filemap_blocks;
    uint32_t s_inode_blocks;
    uint32_t s_nfree_inodes;
    uint32_t s_nfree_blocks;
    uint32_t s_cache_dir_entries;
    uint32_t s_unused_area;

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
    uint32_t hash;
    uint32_t name_bno;
    uint32_t block_index;
};

struct basicbtfs_name_entry {
    uint32_t ino;
    uint32_t name_length;
};

struct basicbtfs_name_list_hdr {
    uint32_t free_bytes;
    uint32_t start_unused_area;
    uint32_t nr_of_entries;
    uint32_t next_block;
    uint32_t prev_block;
    bool first_list;
};

struct basicbtfs_name_list_block {
    struct basicbtfs_name_list_hdr name_list_hdr;
    char data[BASICBTFS_EMPTY_NAME_TREE];
};

struct basicbtfs_cluster {
    uint32_t start_bno;
    uint32_t cluster_length;
};

struct basicbtfs_cluster_table {
    uint32_t nr_of_clusters;
    uint32_t ino;
    struct basicbtfs_cluster table[BASICBTFS_ATABLE_MAX_CLUSTERS];
};

struct basicbtfs_alloc_table {
    uint32_t nr_of_entries;
    uint32_t table[BASICBTFS_ATABLE_MAX_BLOCKS];
};

struct basicbtfs_btree_node {
    struct basicbtfs_entry entries[2 * BASICBTFS_MIN_DEGREE - 1];
    uint32_t children[2 * BASICBTFS_MIN_DEGREE];
    uint32_t parent;
    uint32_t tree_name_bno;
    uint32_t nr_of_keys;
    uint32_t nr_of_files;
    uint32_t nr_times_done;
    bool leaf;
    bool root;
};

struct basicbtfs_disk_block {
    uint32_t block_type_id;
    union block_type {
        struct basicbtfs_btree_node btree_node;
        struct basicbtfs_cluster_table cluster_table;
        struct basicbtfs_name_list_hdr name_list_hdr;
    } block_type;
};

struct basicbtfs_block {
    char block[BASICBTFS_BLOCKSIZE];
};

/* Cache functions for basic_inode_info*/
int basicbtfs_init_inode_cache(void);

void basicbtfs_destroy_inode_cache(void);

int basicbtfs_btree_free_dir(struct super_block *sb, struct inode *inode, uint32_t bno);
int basicbtfs_nametree_free_namelist_blocks(struct super_block *sb, uint32_t name_bno);
int basicbtfs_file_free_blocks(struct inode *inode);

/* Superblock functions*/
int basicbtfs_fill_super(struct super_block *sb, void *data, int silent);

/* Inode functions*/
struct inode *basicbtfs_iget(struct super_block *sb, unsigned long ino);

/* Dir functions */
int basicbtfs_add_entry_name(struct inode *dir, struct inode *inode, char *filename);
int basicbtfs_add_entry(struct inode *dir, struct inode *inode, struct dentry *dentry);
struct dentry *basicbtfs_search_entry(struct inode *dir, struct dentry *dentry);
int basicbtfs_delete_entry(struct inode *dir, struct dentry *dentry);
int basicbtfs_update_entry(struct inode *old_dir, struct inode *new_dir, struct dentry *old_dentry, struct dentry *new_dentry, unsigned int flags);
int clean_file_block(struct inode *inode);

long basicbtfs_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

int basicbtfs_file_update_root(struct inode *inode, uint32_t bno);

/* Operation structs*/
extern const struct file_operations basicbtfs_file_ops;
extern const struct file_operations basicbtfs_dir_ops;
extern const struct address_space_operations basicbtfs_aops;
extern const struct inode_operations basicbtfs_inode_ops;
extern struct list_head dir_cache_list;
extern bool should_defrag;
extern bool defrag_now;
extern uint32_t nr_of_inode_operations;



/* Getter functions for disk info*/
#define BASICBTFS_SB(sb) (sb->s_fs_info)
#define BASICBTFS_INODE(inode) (container_of(inode, struct basicbtfs_inode_info, vfs_inode))

#define BASICBTFS_GET_INODE_BLOCK(ino, inode_bmap_bsize, blocks_bmap_bsize) ((ino / BASICBTFS_INODES_PER_BLOCK) + inode_bmap_bsize + blocks_bmap_bsize + 1)
#define BASICBTFS_GET_INODE_BLOCK_IDX(ino) (ino % BASICBTFS_INODES_PER_BLOCK)
#define BASICBTFS_GET_BLOCK_IDX(pos) (pos / BASICBTFS_ENTRIES_PER_BLOCK)
#define BASICBTFS_GET_ENTRY_IDX(pos) (pos % BASICBTFS_ENTRIES_PER_BLOCK)
#define BASICBTFS_GET_FILEBLOCK(bno, inode_bmap_bsize, blocks_bmap_bsize, inode_info_blocks) ((bno / BASICBTFS_FBLOCK_INFO_PER_BLOCK) + inode_bmap_bsize + blocks_bmap_bsize + inode_info_blocks + 1)
#define BASICBTFS_GET_FILEBLOCK_IDX(bno) (bno % BASICBTFS_FBLOCK_INFO_PER_BLOCK)

#endif

#endif