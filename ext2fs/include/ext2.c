
struct ext2_superblock
{
	unsigned int inodes;
	unsigned int blocks;
	unsigned int r_blocks;
	unsigned int free_blocks;
	unsigned int free_inodes;
	unsigned int first_data_block;
	unsigned int block_size;
	unsigned int frag_size;
	unsigned int blocks_per_group;
	unsigned int frags_per_group;
	unsigned int inodes_per_group;
	unsigned int mtime;
	unsigned int wtime;
	unsigned short int mount_count;
	unsigned short int max_mounts;
	unsigned short int magic;
	unsigned short int state;
	unsigned short int errors;
	unsigned short int minor_rev;
	unsigned int lastcheck;
	unsigned int checkinterval;
	unsigned int creator_os;
	unsigned int rev;
	unsigned short int def_resuid;
	unsigned short int def_resgid;
	unsigned int first_ino;
	unsigned short int inode_size;
	unsigned short int block_group_nr;
	unsigned int feature_compat;
	unsigned int feature_incompat;
	unsigned int feature_ro_compat;
	unsigned char uuid[16];
	unsigned char volume_name[16];
	unsigned char last_mount[64];
	unsigned int algo_bitmap;
	unsigned char prealloc_blocks;
	unsigned char prealloc_dir_blocks;
	unsigned short int pad;
	unsigned char journal_uuid[16];
	unsigned int journal_ino;
	unsigned int journal_dev;
	unsigned int last_orphan;
	unsigned int hash_seed[4];
	unsigned char hash_version;
	unsigned char jnl_backup_type;
	unsigned short desc_size;
	unsigned int mount_opt;
	unsigned int first_meta_bg;
	unsigned int mkfs_time;
	unsigned int jnl_blocks[17];
	unsigned int blocks_hi;
	unsigned int r_blocks_hi;
	unsigned int free_blocks_hi;
	unsigned int rsv[6];
	unsigned char pad2;
	unsigned char csum_type;
	unsigned char pad3[2];
	unsigned int rsv2[62];
	unsigned int csum_seed;
	unsigned int rsv3[98];
	
	unsigned int checksum;
};
struct ext2_bgdt
{
	unsigned int block_bitmap;
	unsigned int inode_bitmap;
	unsigned int inode_table;
	unsigned short int free_blocks;
	unsigned short int free_inodes;
	unsigned short int used_dirs;
	unsigned short pad;
	char unused[12];
};
struct ext4_bgdt
{
	unsigned int block_bitmap;
	unsigned int inode_bitmap;
	unsigned int inode_table;
	unsigned short int free_blocks;
	unsigned short int free_inodes;
	unsigned short int used_dirs;
	unsigned short flags;
	unsigned int pad;
	unsigned short block_bitmap_csum;
	unsigned short inode_bitmap_csum;
	unsigned short unused_inodes;
	unsigned short checksum;
	unsigned int block_bitmap_hi;
	unsigned int inode_bitmap_hi;
	unsigned int inode_table_hi;
	unsigned short int free_blocks_hi;
	unsigned short int free_inodes_hi;
	unsigned short int used_dirs_hi;
	unsigned short unused_inodes_hi;
	unsigned int pad2;
	unsigned short block_bitmap_csum_hi;
	unsigned short inode_bitmap_csum_hi;
	unsigned int unused;
};
struct ext2_inode
{
	unsigned short int mode;
	unsigned short int uid;
	unsigned int size;
	unsigned int atime;
	unsigned int ctime;
	unsigned int mtime;
	unsigned int dtime;
	unsigned short int gid;
	unsigned short int links;
	unsigned int blocks;
	unsigned int flags;
	unsigned int osd1;
	unsigned int block[15];
	unsigned int generation;
	unsigned int file_acl;
	unsigned int dir_acl;
	unsigned int faddr;
	unsigned int osd2[3];
	unsigned int checksum_hi;
	unsigned int ctime_extra;
	unsigned int mtime_extra;
	unsigned int atime_extra;
	unsigned int crtime;
	unsigned int crtime_extra;
	char pad[104];
};
struct ext2_directory
{
	unsigned int inode;
	unsigned short int rec_len;
	unsigned char name_len;
	unsigned char file_type;
	char file_name[1];
};


#define FEATURE_EXTENTS 0x40
#define FEATURE_64BIT 0x80
#define FEATURE_CSUM_SEED 0x2000

#define FEATURE_METADATA_CSUM 0x400
#define FEATURE_DIR_NLINK 0x20
#define FEATURE_HUGE_FILE 0x8
#define FEATURE_EXTRA_ISIZE 0x40

#define FLAG_HUGE_FILE 0x40000
#define FLAG_EXTENTS 0x80000

struct ext4_extent_header
{
	unsigned short magic; // 0xf30a
	unsigned short entries;
	unsigned short max_entries;
	unsigned short depth;
	unsigned int generation;
};
struct ext4_extent
{
	unsigned int lblock;
	unsigned short len;
	unsigned short start_hi;
	unsigned int start_lo;
};
struct ext4_extent_index
{
	unsigned int lblock;
	unsigned int block_lo;
	unsigned int block_hi;
};
