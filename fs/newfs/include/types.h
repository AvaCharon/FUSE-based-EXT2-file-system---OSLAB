#ifndef _TYPES_H_
#define _TYPES_H_

/******************************************************************************
 * SECTION: Type def
 *******************************************************************************/
#define MAX_NAME_LEN 128

typedef int boolean;
typedef uint16_t flag16;

typedef enum nfs_file_type
{
    NFS_REG_FILE,
    NFS_DIR,
    // NFS_SYM_LINK
} NFS_FILE_TYPE;
/******************************************************************************
 * SECTION: Macro
 *******************************************************************************/
#define TRUE 1
#define FALSE 0
#define UINT32_BITS 32
#define UINT8_BITS 8

#define NFS_MAGIC_NUM 0x20111427
#define NFS_SUPER_OFS 0
#define NFS_ROOT_INO 0

#define NFS_ERROR_NONE 0
#define NFS_ERROR_ACCESS EACCES
#define NFS_ERROR_SEEK ESPIPE
#define NFS_ERROR_ISDIR EISDIR
#define NFS_ERROR_NOSPACE ENOSPC
#define NFS_ERROR_EXISTS EEXIST
#define NFS_ERROR_NOTFOUND ENOENT
#define NFS_ERROR_UNSUPPORTED ENXIO
#define NFS_ERROR_IO EIO       /* Error Input/Output */
#define NFS_ERROR_INVAL EINVAL /* Invalid Args */

#define NFS_MAX_FILE_NAME 128
#define SFS_INODE_PER_FILE 1
#define NFS_DATA_PER_FILE 6   // 文件最大为6*1024KB
#define NFS_DEFAULT_PERM 0777 /* 全权限打开 */

#define NFS_IOC_MAGIC 'S'
#define NFS_IOC_SEEK _IO(NFS_IOC_MAGIC, 0)

#define NFS_FLAG_BUF_DIRTY 0x1
#define NFS_FLAG_BUF_OCCUPY 0x2

#define NFS_SUPER_BLOCKS 1
#define NFS_MAP_INODE_BLOCKS 1
#define NFS_MAP_DATA_BLOCKS 1
#define NFS_INODE_BLOCKS 512 
#define NFS_DATA_BLOCKS 2048 // 3072 
/******************************************************************************
 * SECTION: Macro Function
 *******************************************************************************/
#define NFS_IO_SZ() (nfs_super.sz_io)     // 512B
#define NFS_BLK_SZ() (nfs_super.sz_blk)   // 1024B
#define NFS_DISK_SZ() (nfs_super.sz_disk) // 4MB
#define NFS_DRIVER() (nfs_super.fd)

#define NFS_ROUND_DOWN(value, round) ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define NFS_ROUND_UP(value, round) ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))

#define NFS_BLKS_SZ(blks) ((blks)*NFS_BLK_SZ())
#define NFS_ASSIGN_FNAME(pnfs_dentry, _fname) memcpy(pnfs_dentry->fname, _fname, strlen(_fname))
#define NFS_INO_OFS(ino) (nfs_super.inode_offset + (ino)*NFS_BLK_SZ())
#define NFS_DATA_OFS(bno) (nfs_super.data_offset + (bno)*NFS_BLK_SZ())

#define NFS_IS_DIR(pinode) (pinode->dentry->ftype == NFS_DIR)
#define NFS_IS_REG(pinode) (pinode->dentry->ftype == NFS_REG_FILE)
/******************************************************************************
 * SECTION: FS Specific Structure - In memory structure
 *******************************************************************************/
struct nfs_dentry;
struct nfs_inode;
struct nfs_super;

struct custom_options
{
    const char *device; // 驱动路径
};

struct nfs_super
{
    uint32_t magic;
    int fd;
    /* TODO: Define yourself */
    int sz_io;    // 一次io操作的字节数 512B
    int sz_disk;  // 磁盘大小
    int sz_usage; // 当前磁盘已经使用的大小
    int sz_blk;   // 1024B

    int max_ino;          // 最多支持的文件数
    uint8_t *map_inode;   // inode位图的内存起点
    int map_inode_blks;   // inode位图占用的块数
    int map_inode_offset; // inode位图在磁盘上的偏移

    int max_data;
    uint8_t *map_data;   // data位图的内存起点
    int map_data_blks;   // data位图占用的块数
    int map_data_offset; // data位图在磁盘上的偏移

    int inode_offset; // 索引结点的偏移
    int data_offset;  // 数据块的偏移

    boolean is_mounted;
    struct nfs_dentry *root_dentry; // 根目录项
};

struct nfs_inode
{
    uint32_t ino; // 在inode位图中的下标
    /* TODO: Define yourself */
    uint32_t size;             // 已占用空间
    uint32_t link;             // 连接数
    struct nfs_dentry *dentry; // 指向该inode的父dentry

    NFS_FILE_TYPE ftype;        // 文件类型：普通/目录
    uint32_t dir_cnt;           // 目录下目录项个数
    struct nfs_dentry *dentrys; // 指向目录下所有子项文件

    uint32_t block_pointer[NFS_DATA_PER_FILE]; // 数据块指针
    int bno[NFS_DATA_PER_FILE];                // 数据块在磁盘中的块号
};

struct nfs_dentry
{
    char fname[MAX_NAME_LEN]; // 指向的ino文件名
    uint32_t ino;             // 指向的ino号
    /* TODO: Define yourself */
    NFS_FILE_TYPE ftype; // 指向的ino类型
    int valid;           // 指向的ino是否有效

    struct nfs_inode *inode;    // 这个目录项指向的inode节点
    struct nfs_dentry *parent;  // 父inode的dentry
    struct nfs_dentry *brother; // 下一个兄弟inode的dentry
    struct nfs_dentry *child;   // 目录下子inode的dentry
};

static inline struct nfs_dentry *new_dentry(char *fname, NFS_FILE_TYPE ftype)
{
    struct nfs_dentry *dentry = (struct nfs_dentry *)malloc(sizeof(struct nfs_dentry));
    memset(dentry, 0, sizeof(struct nfs_dentry));
    NFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype = ftype;
    dentry->ino = -1;
    dentry->valid = 0;
    dentry->inode = NULL;
    dentry->parent = NULL;
    dentry->brother = NULL;
    dentry->child = NULL;
    return dentry;
}

/******************************************************************************
 * SECTION: FS Specific Structure - Disk structure
 *******************************************************************************/
struct nfs_super_d
{
    uint32_t magic;
    int sz_usage; // 当前磁盘已经使用的大小

    int map_inode_blks;   // inode位图占用的块数
    int map_inode_offset; // inode位图在磁盘上的偏移

    int map_data_blks;   // data位图占用的块数
    int map_data_offset; // data位图在磁盘上的偏移

    int inode_offset; // 索引结点的偏移
    int data_offset;  // 数据块的偏移
};

struct nfs_inode_d
{
    uint32_t ino;               // 在inode位图中的下标
    uint32_t size;              // 已占用空间
    NFS_FILE_TYPE ftype;        // 文件类型：普通/目录
    uint32_t dir_cnt;           // 目录下目录项个数
    int bno[NFS_DATA_PER_FILE]; // 数据块在磁盘中的块号
};

struct nfs_dentry_d
{
    char fname[MAX_NAME_LEN]; // 指向的ino文件名
    uint32_t ino;             // 指向的ino号
    NFS_FILE_TYPE ftype;      // 指向的ino类型
    int valid;                // 指向的ino是否有效
};
#endif /* _TYPES_H_ */