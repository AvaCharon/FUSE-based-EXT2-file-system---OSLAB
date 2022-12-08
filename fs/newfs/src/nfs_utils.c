#include "../include/newfs.h"

extern struct nfs_super nfs_super;
extern struct custom_options nfs_options;

/**
 * @brief 获取文件名
 *
 * @param path
 * @return char*
 */
char *nfs_get_fname(const char *path)
{
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path
 * @return int
 */
int nfs_calc_lvl(const char *path)
{
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char *str = path;
    int lvl = 0;
    if (strcmp(path, "/") == 0)
    {
        return lvl;
    }
    while (*str != NULL)
    {
        if (*str == '/')
        {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * @brief 驱动读(按 1024B 读取)
 *
 * @param offset
 * @param out_content
 * @param size
 * @return int
 */
int nfs_driver_read(int offset, uint8_t *out_content, int size)
{
    int offset_aligned = NFS_ROUND_DOWN(offset, NFS_BLK_SZ());
    int bias = offset - offset_aligned;
    int size_aligned = NFS_ROUND_UP((size + bias), NFS_BLK_SZ());
    uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
    uint8_t *cur = temp_content;
    // lseek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(NFS_DRIVER(), cur, NFS_IO_SZ());
        ddriver_read(NFS_DRIVER(), cur, NFS_IO_SZ());
        cur += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return NFS_ERROR_NONE;
}

/**
 * @brief 驱动写
 *
 * @param offset
 * @param in_content
 * @param size
 * @return int
 */
int nfs_driver_write(int offset, uint8_t *in_content, int size)
{
    int offset_aligned = NFS_ROUND_DOWN(offset, NFS_BLK_SZ());
    int bias = offset - offset_aligned;
    int size_aligned = NFS_ROUND_UP((size + bias), NFS_BLK_SZ());
    uint8_t *temp_content = (uint8_t *)malloc(size_aligned);
    uint8_t *cur = temp_content;
    nfs_driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);

    // lseek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(NFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(NFS_DRIVER(), cur, NFS_IO_SZ());
        ddriver_write(NFS_DRIVER(), cur, NFS_IO_SZ());
        cur += NFS_IO_SZ();
        size_aligned -= NFS_IO_SZ();
    }

    free(temp_content);
    return NFS_ERROR_NONE;
}

/**
 * @brief 为一个inode分配dentry的bro，采用头插法
 *
 * @param inode
 * @param dentry
 * @return int
 */
int nfs_alloc_dentry(struct nfs_inode *inode, struct nfs_dentry *dentry)
{
    if (inode->dentrys == NULL)
    {
        inode->dentrys = dentry;
    }
    else
    {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}

/**
 * @brief 将dentry从inode的dentrys中取出
 *
 * @param inode
 * @param dentry
 * @return int
 */
int nfs_drop_dentry(struct nfs_inode *inode, struct nfs_dentry *dentry)
{
    boolean is_find = FALSE;
    struct nfs_dentry *dentry_cursor;
    dentry_cursor = inode->dentrys;

    if (dentry_cursor == dentry)
    {
        inode->dentrys = dentry->brother;
        is_find = TRUE;
    }
    else
    {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry)
            {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find)
    {
        return -NFS_ERROR_NOTFOUND;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}

/**
 * @brief 分配一个inode，占用位图
 *
 * @param dentry 该dentry指向分配的inode
 * @return nfs_inode
 */
struct nfs_inode *nfs_alloc_inode(struct nfs_dentry *dentry)
{
    struct nfs_inode *inode;
    inode = (struct nfs_inode *)malloc(sizeof(struct nfs_inode));
    int byte_cursor = 0;
    int bit_cursor = 0;
    int ino_cursor = 0; // 记录索引位图空闲位置下标

    int bno_cursor = 0;   // 记录数据位图空闲位置下标
    int data_blk_cnt = 0; // 记录已从数据位图找到多少个空闲位置

    boolean is_find_free_entry = FALSE;
    boolean is_find_enough_free_data_blk = FALSE;

    // 从索引位图中取空闲
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks);
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            if ((nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0)
            {
                /* 当前ino_cursor位置空闲 */
                nfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry)
        {
            break;
        }
    }

    inode->ino = ino_cursor;

    inode->size = 0;

    // 从数据位图中取空闲，要取出NFS_DATA_PER_FILE个
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_data_blks);
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
        {
            if ((nfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0)
            {
                /* 当前bno_cursor位置空闲 */
                nfs_super.map_data[byte_cursor] |= (0x1 << bit_cursor);

                inode->bno[data_blk_cnt++] = bno_cursor;
                // 判断是否已经取够
                if (data_blk_cnt == NFS_DATA_PER_FILE)
                {
                    is_find_enough_free_data_blk = TRUE;
                    break;
                }
            }
            bno_cursor++;
        }
        if (is_find_enough_free_data_blk)
        {
            break;
        }
    }

    if (!is_find_free_entry || ino_cursor == nfs_super.max_ino)
        return -NFS_ERROR_NOSPACE;

    if (!is_find_enough_free_data_blk || bno_cursor == nfs_super.max_data)
        return -NFS_ERROR_NOSPACE;

    dentry->inode = inode;
    dentry->ino = inode->ino;

    dentry->valid = 1;

    inode->dentry = dentry;

    inode->dir_cnt = 0;
    inode->dentrys = NULL;

    // 为文件中的数据块分配内存
    if (NFS_IS_REG(inode))
    {
        int p_count = 0;
        for (p_count = 0; p_count < NFS_DATA_PER_FILE; p_count++)
        {
            inode->block_pointer[p_count] = (uint8_t *)malloc(NFS_BLK_SZ());
        }
    }

    return inode;
}

/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 *
 * @param inode
 * @return int
 */
int nfs_sync_inode(struct nfs_inode *inode)
{
    struct nfs_inode_d inode_d;
    struct nfs_dentry *dentry_cursor;
    struct nfs_dentry_d dentry_d;
    int ino = inode->ino;
    inode_d.ino = ino;
    inode_d.size = inode->size;

    inode_d.ftype = inode->dentry->ftype;
    inode_d.dir_cnt = inode->dir_cnt;

    int offset;

    for (int blk_cnt = 0; blk_cnt < NFS_DATA_PER_FILE; blk_cnt++)
        inode_d.bno[blk_cnt] = inode->bno[blk_cnt];

    if (nfs_driver_write(NFS_INO_OFS(ino), (uint8_t *)&inode_d,
                         sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE)
    {
        NFS_DBG("[%s] io error\n", __func__);
        return -NFS_ERROR_IO;
    }
    /* Cycle 1: 写 INODE */
    /* Cycle 2: 写 数据 */
    if (NFS_IS_DIR(inode))
    {
        int blk_cnt = 0;
        dentry_cursor = inode->dentrys;

        // 共写NFS_DATA_PER_FILE个数据块
        while (dentry_cursor != NULL && blk_cnt < NFS_DATA_PER_FILE)
        {
            // 将目录下dentry存入第一个数据块
            offset = NFS_DATA_OFS(inode->bno[blk_cnt]);
            // 写一个blk
            while (dentry_cursor != NULL)
            {
                memcpy(dentry_d.fname, dentry_cursor->fname, NFS_MAX_FILE_NAME);
                dentry_d.ftype = dentry_cursor->ftype;
                dentry_d.ino = dentry_cursor->ino;
                dentry_d.valid = dentry_cursor->valid;
                if (nfs_driver_write(offset, (uint8_t *)&dentry_d,
                                     sizeof(struct nfs_dentry_d)) != NFS_ERROR_NONE)
                {
                    NFS_DBG("[%s] io error\n", __func__);
                    return -NFS_ERROR_IO;
                }

                if (dentry_cursor->inode != NULL)
                {
                    nfs_sync_inode(dentry_cursor->inode);
                }

                dentry_cursor = dentry_cursor->brother;
                offset += sizeof(struct nfs_dentry_d);
                if (offset + sizeof(struct nfs_dentry_d) > NFS_DATA_OFS(inode->bno[blk_cnt] + 1))
                    break;
            }
            blk_cnt++;
        }
    }
    else if (NFS_IS_REG(inode))
    {
        for (int blk_cnt = 0; blk_cnt < NFS_DATA_PER_FILE; blk_cnt++)
        {
            if (nfs_driver_write(NFS_DATA_OFS(inode->bno[blk_cnt]),
                                 inode->block_pointer[blk_cnt], NFS_BLK_SZ()) != NFS_ERROR_NONE)
            {
                NFS_DBG("[%s] io error\n", __func__);
                return -NFS_ERROR_IO;
            }
        }
    }
    return NFS_ERROR_NONE;
}

/**
 * @brief 删除内存中的一个inode， 暂时不释放
 * Case 1: Reg File
 *
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Reg Dentry)
 *                       |
 *                      Inode  (Reg File)
 *
 *  1) Step 1. Erase Bitmap
 *  2) Step 2. Free Inode                      (Function of sfs_drop_inode)
 * ------------------------------------------------------------------------
 *  3) *Setp 3. Free Dentry belonging to Inode (Outsider)
 * ========================================================================
 * Case 2: Dir
 *                  Inode
 *                /      \
 *            Dentry -> Dentry (Dir Dentry)
 *                       |
 *                      Inode  (Dir)
 *                    /     \
 *                Dentry -> Dentry
 *
 *   Recursive
 * @param inode
 * @return int
 */
int nfs_drop_inode(struct nfs_inode *inode)
{
    struct nfs_dentry *dentry_cursor;
    struct nfs_dentry *dentry_to_free;
    struct nfs_inode *inode_cursor;

    int byte_cursor = 0;
    int bit_cursor = 0;
    int ino_cursor = 0;
    boolean is_find = FALSE;

    if (inode == nfs_super.root_dentry->inode)
    {
        return NFS_ERROR_INVAL;
    }

    if (NFS_IS_DIR(inode))
    {
        dentry_cursor = inode->dentrys;
        /* 递归向下drop */
        while (dentry_cursor)
        {
            inode_cursor = dentry_cursor->inode;
            nfs_drop_inode(inode_cursor);
            nfs_drop_dentry(inode, dentry_cursor);
            dentry_to_free = dentry_cursor;
            dentry_cursor = dentry_cursor->brother;
            free(dentry_to_free);
        }
    }
    else if (NFS_IS_REG(inode))
    {
        for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks);
             byte_cursor++) /* 调整inodemap */
        {
            for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++)
            {
                if (ino_cursor == inode->ino)
                {
                    nfs_super.map_inode[byte_cursor] &= (uint8_t)(~(0x1 << bit_cursor));
                    is_find = TRUE;
                    break;
                }
                ino_cursor++;
            }
            if (is_find == TRUE)
            {
                break;
            }
        }

        for (int p_count = 0; p_count < NFS_DATA_PER_FILE; p_count++)
        {
            free(inode->block_pointer[p_count]);
        }

        free(inode);
    }
    return NFS_ERROR_NONE;
}

/**
 * @brief
 *
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct nfs_inode*
 */
struct nfs_inode *nfs_read_inode(struct nfs_dentry *dentry, int ino)
{
    struct nfs_inode *inode = (struct nfs_inode *)malloc(sizeof(struct nfs_inode));
    struct nfs_inode_d inode_d;
    struct nfs_dentry *sub_dentry;
    struct nfs_dentry_d dentry_d;
    int blk_cnt = 0;
    int dir_cnt = 0, offset;

    if (nfs_driver_read(NFS_INO_OFS(ino), (uint8_t *)&inode_d,
                        sizeof(struct nfs_inode_d)) != NFS_ERROR_NONE)
    {
        NFS_DBG("[%s] io error\n", __func__);
        return NULL;
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;

    for (blk_cnt = 0; blk_cnt < NFS_DATA_PER_FILE; blk_cnt++)
        inode->bno[blk_cnt] = inode_d.bno[blk_cnt];

    inode->dentry = dentry;
    inode->dentrys = NULL;

    if (NFS_IS_DIR(inode))
    {
        dir_cnt = inode_d.dir_cnt;
        blk_cnt = 0;

        while (dir_cnt != 0)
        {
            offset = NFS_DATA_OFS(inode->bno[blk_cnt]);

            while (offset + sizeof(struct nfs_dentry_d) < NFS_DATA_OFS(inode->bno[blk_cnt] + 1))
            {
                if (nfs_driver_read(offset, (uint8_t *)&dentry_d,
                                    sizeof(struct nfs_dentry_d)) != NFS_ERROR_NONE)
                {
                    NFS_DBG("[%s] io error\n", __func__);
                    return NULL;
                }

                sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
                sub_dentry->parent = inode->dentry;
                sub_dentry->ino = dentry_d.ino;
                nfs_alloc_dentry(inode, sub_dentry);

                offset += sizeof(struct nfs_dentry_d);
                dir_cnt--;
                if (dir_cnt == 0)
                    break;
            }
            blk_cnt++;
        }
    }
    else if (NFS_IS_REG(inode))
    {
        for (blk_cnt = 0; blk_cnt < NFS_DATA_PER_FILE; blk_cnt++)
        {
            inode->block_pointer[blk_cnt] = (uint8_t *)malloc(NFS_BLK_SZ());
            if (nfs_driver_read(NFS_DATA_OFS(inode->bno[blk_cnt]), inode->block_pointer[blk_cnt],
                                NFS_BLK_SZ()) != NFS_ERROR_NONE)
            {
                NFS_DBG("[%s] io error\n", __func__);
                return NULL;
            }
        }
    }
    return inode;
}

/**
 * @brief
 *
 * @param inode
 * @param dir [0...]
 * @return struct nfs_dentry*
 */
struct nfs_dentry *nfs_get_dentry(struct nfs_inode *inode, int dir)
{
    struct nfs_dentry *dentry_cursor = inode->dentrys;
    int cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt)
        {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}

/**
 * @brief
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 *
 * @param path
 * @return struct nfs_inode*
 */
struct nfs_dentry *nfs_lookup(const char *path, boolean *is_find, boolean *is_root)
{
    struct nfs_dentry *dentry_cursor = nfs_super.root_dentry;
    struct nfs_dentry *dentry_ret = NULL;
    struct nfs_inode *inode;
    int total_lvl = nfs_calc_lvl(path);
    int lvl = 0;
    boolean is_hit;
    char *fname = NULL;
    char *path_cpy = (char *)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);

    if (total_lvl == 0)
    { /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = nfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");
    while (fname)
    {
        lvl++;
        if (dentry_cursor->inode == NULL)
        { /* Cache机制 */
            nfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;

        if (NFS_IS_REG(inode) && lvl < total_lvl)
        {
            NFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        if (NFS_IS_DIR(inode))
        {
            dentry_cursor = inode->dentrys;
            is_hit = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0)
                {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;
            }

            if (!is_hit)
            {
                *is_find = FALSE;
                NFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }

            if (is_hit && lvl == total_lvl)
            {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/");
    }

    if (dentry_ret->inode == NULL)
    {
        dentry_ret->inode = nfs_read_inode(dentry_ret, dentry_ret->ino);
    }

    return dentry_ret;
}

/**
 * @brief 挂载nfs, Layout 如下
 *
 * Layout
 * | Super | Inode Map | Data Map | Data |
 *
 *  BLK_SZ = 2 * IO_SZ
 *
 * 每个Inode占用一个Blk
 * @param options
 * @return int
 */
int nfs_mount(struct custom_options options)
{
    int ret = NFS_ERROR_NONE;
    int driver_fd;
    struct nfs_super_d nfs_super_d;
    struct nfs_dentry *root_dentry;
    struct nfs_inode *root_inode;

    int inode_num;
    int map_inode_blks;
    int data_num;
    int map_data_blks;

    int super_blks;
    boolean is_init = FALSE;

    nfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);

    if (driver_fd < 0)
    {
        return driver_fd;
    }

    nfs_super.fd = driver_fd;
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_SIZE, &nfs_super.sz_disk);
    ddriver_ioctl(NFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &nfs_super.sz_io);
    nfs_super.sz_blk = nfs_super.sz_io * 2;

    root_dentry = new_dentry("/", NFS_DIR);

    // 读取super
    if (nfs_driver_read(NFS_SUPER_OFS, (uint8_t *)(&nfs_super_d),
                        sizeof(struct nfs_super_d)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }
    printf("%d\n", nfs_super_d.magic);
    /* 读取super */
    if (nfs_super_d.magic != NFS_MAGIC_NUM)
    { 
        /* 幻数无则重建磁盘 */
        /* 规定各部分大小 */
        super_blks = NFS_SUPER_BLOCKS;
        map_inode_blks = NFS_MAP_INODE_BLOCKS;
        map_data_blks = NFS_MAP_DATA_BLOCKS;
        inode_num = NFS_INODE_BLOCKS;
        data_num = NFS_DATA_BLOCKS;

        /* 布局layout */
        nfs_super.max_ino = inode_num;
        nfs_super.max_data = data_num;

        nfs_super_d.magic = NFS_MAGIC_NUM;
        nfs_super_d.map_inode_offset = NFS_SUPER_OFS + NFS_BLKS_SZ(super_blks);
        nfs_super_d.map_data_offset = nfs_super_d.map_inode_offset + NFS_BLKS_SZ(map_inode_blks);

        nfs_super_d.inode_offset = nfs_super_d.map_data_offset + NFS_BLKS_SZ(map_data_blks);
        nfs_super_d.data_offset = nfs_super_d.inode_offset + NFS_BLKS_SZ(inode_num);

        nfs_super_d.map_inode_blks = map_inode_blks;
        nfs_super_d.map_data_blks = map_data_blks;

        nfs_super_d.sz_usage = 0;
        NFS_DBG("inode map blocks: %d\n", map_inode_blks);
        is_init = TRUE;
    }
    nfs_super.sz_usage = nfs_super_d.sz_usage; /* 建立 in-memory 结构 */

    nfs_super.map_inode = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_inode_blks));
    nfs_super.map_inode_blks = nfs_super_d.map_inode_blks;
    nfs_super.map_inode_offset = nfs_super_d.map_inode_offset;

    nfs_super.map_data = (uint8_t *)malloc(NFS_BLKS_SZ(nfs_super_d.map_data_blks));
    nfs_super.map_data_blks = nfs_super_d.map_data_blks;
    nfs_super.map_data_offset = nfs_super_d.map_data_offset;

    nfs_super.inode_offset = nfs_super_d.inode_offset;
    nfs_super.data_offset = nfs_super_d.data_offset;

    // 读取两个位图
    if (nfs_driver_read(nfs_super_d.map_inode_offset, (uint8_t *)(nfs_super.map_inode),
                        NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }
    if (nfs_driver_read(nfs_super_d.map_data_offset, (uint8_t *)(nfs_super.map_data),
                        NFS_BLKS_SZ(nfs_super_d.map_data_blks)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }



    if (is_init)
    { /* 分配根节点 */
        root_inode = nfs_alloc_inode(root_dentry);
        nfs_sync_inode(root_inode);
    }

    root_inode = nfs_read_inode(root_dentry, NFS_ROOT_INO);
    root_dentry->inode = root_inode;
    nfs_super.root_dentry = root_dentry;
    nfs_super.is_mounted = TRUE;

    nfs_dump_map();
    return ret;
}

/**
 * @brief
 *
 * @return int
 */
int nfs_umount()
{
    struct nfs_super_d nfs_super_d;

    if (!nfs_super.is_mounted)
    {
        return NFS_ERROR_NONE;
    }

    nfs_sync_inode(nfs_super.root_dentry->inode); /* 从根节点向下刷写节点 */

    nfs_super_d.magic = NFS_MAGIC_NUM;
    nfs_super_d.sz_usage = nfs_super.sz_usage;

    nfs_super_d.map_inode_blks = nfs_super.map_inode_blks;
    nfs_super_d.map_inode_offset = nfs_super.map_inode_offset;
    nfs_super_d.map_data_blks = nfs_super.map_data_blks;
    nfs_super_d.map_data_offset = nfs_super.map_data_offset;

    nfs_super_d.inode_offset = nfs_super.inode_offset;
    nfs_super_d.data_offset = nfs_super.data_offset;

    if (nfs_driver_write(NFS_SUPER_OFS, (uint8_t *)&nfs_super_d,
                         sizeof(struct nfs_super_d)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }

    if (nfs_driver_write(nfs_super_d.map_inode_offset, (uint8_t *)(nfs_super.map_inode),
                         NFS_BLKS_SZ(nfs_super_d.map_inode_blks)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }

    if (nfs_driver_write(nfs_super_d.map_data_offset, (uint8_t *)(nfs_super.map_data),
                         NFS_BLKS_SZ(nfs_super_d.map_data_blks)) != NFS_ERROR_NONE)
    {
        return -NFS_ERROR_IO;
    }

    free(nfs_super.map_inode);
    free(nfs_super.map_data);
    ddriver_close(NFS_DRIVER());

    return NFS_ERROR_NONE;
}