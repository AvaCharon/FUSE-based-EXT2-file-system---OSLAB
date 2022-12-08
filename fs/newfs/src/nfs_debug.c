#include "../include/newfs.h"

extern struct nfs_super      nfs_super; 
extern struct custom_options nfs_options;

void nfs_dump_map() {
    int byte_cursor = 0;
    int bit_cursor = 0;

    // inode map
    printf("inode map:\n");
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_inode_blks); 
         byte_cursor+=4)
    {
        // printf("%x ", ((nfs_super.map_inode[byte_cursor]>>4) & (0x1111))); 
        // printf("%x ", (nfs_super.map_inode[byte_cursor] & (0x1111))); 
        // printf("\t");
        // printf("%x ", ((nfs_super.map_inode[byte_cursor+1]>>4) & (0x1111))); 
        // printf("%x ", (nfs_super.map_inode[byte_cursor+1] & (0x1111))); 
        // printf("\t");
        // printf("%x ", ((nfs_super.map_inode[byte_cursor+2]>>4) & (0x1111))); 
        // printf("%x ", (nfs_super.map_inode[byte_cursor+2] & (0x1111))); 
        // printf("\t");
        // printf("%x ", ((nfs_super.map_inode[byte_cursor+3]>>4) & (0x1111))); 
        // printf("%x ", (nfs_super.map_inode[byte_cursor+3] & (0x1111))); 
        // printf("\n");
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            printf("%x ", (nfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            printf("%x ", (nfs_super.map_inode[byte_cursor + 1] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");
        
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            printf("%x ", (nfs_super.map_inode[byte_cursor + 2] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");
        
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            printf("%x ", (nfs_super.map_inode[byte_cursor + 3] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\n");
    }

    // data map
    printf("data map:\n");
    for (byte_cursor = 0; byte_cursor < NFS_BLKS_SZ(nfs_super.map_data_blks); 
         byte_cursor+=4)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            printf("%x ", (nfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");

        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            printf("%x ", (nfs_super.map_data[byte_cursor + 1] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");
        
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            printf("%x ", (nfs_super.map_data[byte_cursor + 2] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\t");
        
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            printf("%x ", (nfs_super.map_data[byte_cursor + 3] & (0x1 << bit_cursor)) >> bit_cursor);   
        }
        printf("\n");
    }
}