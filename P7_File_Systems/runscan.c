#include "ext2_fs.h"
#include "read_ext2.h"
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdbool.h>


int main(int argc, char **argv) {
    if (argc != 3) {
        printf("expected usage: ./runscan inputfile outputfile\n");
        exit(0);
    }

	if (opendir(argv[2])) {
		printf("Output directory already exists\n");
		exit(1);
	}

    int status = mkdir(argv[2], S_IRWXU | S_IRWXG | S_IRWXO);
    if (status == -1) {
		printf("mkdir failed\n");
		exit(1);
    }

    // open disk image
    int fd = open(argv[1], O_RDONLY);
    ext2_read_init(fd);

    struct ext2_super_block super;
    struct ext2_group_desc* groups;

    groups = malloc(sizeof(struct ext2_group_desc) * num_groups);

    read_super_block(fd, &super);
    read_group_descs(fd, groups, num_groups);

    bool inode_is_jpg[num_groups * inodes_per_group + 1];
    for (unsigned int i = 0; i < num_groups * inodes_per_group + 1; i++)
        inode_is_jpg[i] = false;

    struct ext2_dir_entry top_secret_dentry;
    bool have_top_secret = false;

    for (unsigned int ngroup = 0; ngroup < num_groups; ngroup++) {
        off_t inode_table = locate_inode_table(ngroup, groups);

        for (unsigned int i = 1; i <= inodes_per_group; i++) {
            struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
            read_inode(fd, inode_table, i, inode, super.s_inode_size);

            char buffer[block_size];
            lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
            read(fd, buffer, block_size);

            if (S_ISREG(inode->i_mode)) {
                // check if the first data block of the inode contains the jpg magic numbers
                int is_jpg = 0; 
                if (buffer[0] == (char)0xff &&
                    buffer[1] == (char)0xd8 &&
                    buffer[2] == (char)0xff &&
                    (buffer[3] == (char)0xe0 ||
                    buffer[3] == (char)0xe1 ||
                    buffer[3] == (char)0xe8)) {
                    is_jpg = 1;
                }

                if (is_jpg) {
                    // filename = <argv[2]>/file-<inode_num>.jpg
                    unsigned int inode_num = ngroup * inodes_per_group + i;
                    char inode_num_str[20];
                    inode_is_jpg[inode_num] = true;

                    // If there are 100 inodes per group, then the first inode table holds inode number 1 to 100,
                    // the second holds 101 to 200, and so on. In other words, the first inodeTable[0..99] array
                    // stores inodes numbered from 1 to 100.
                    sprintf(inode_num_str, "%u", inode_num);
                    int filename_len = strlen(argv[2]) + 6 + strlen(inode_num_str) + 5;
                    char* filename = malloc(filename_len * sizeof(char));
                    snprintf(filename, filename_len, "%s/file-%u.jpg", argv[2], inode_num);
                    int jpg_fd = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
                    free(filename);
                    if (jpg_fd == -1) {
                        printf("Failed to create jpg file\n");
                        exit(1);
                    }

                    // filename = <argv[2]>/file-<inode_num>-details.txt
                    filename_len += 8;
                    filename = malloc(filename_len * sizeof(char));
                    snprintf(filename, filename_len, "%s/file-%u-details.txt", argv[2], inode_num);
                    FILE *details_fp = fopen(filename, "w");
                    free(filename);
                    if (details_fp == NULL) {
                        printf("Failed to create details file\n");
                        exit(1);
                    }
                    fprintf(details_fp, "%hu\n%u\n%hu", inode->i_links_count, inode->i_size, inode->i_uid);
                    fclose(details_fp);

                    unsigned int bytes_copied = 0;
                    unsigned int block_idx = 0;

                    // direct pointers
                    while (bytes_copied < inode->i_size && block_idx < EXT2_NDIR_BLOCKS) {
                        unsigned int write_len = bytes_copied + block_size > inode->i_size ? inode->i_size - bytes_copied : block_size;
                        bytes_copied += write_len;
                        write(jpg_fd, buffer, write_len);
                        if (write_len == block_size) {
                            lseek(fd, BLOCK_OFFSET(inode->i_block[++block_idx]), SEEK_SET);
                            read(fd, buffer, block_size);
                        }
                    }

                    // indirect pointer
                    if (bytes_copied < inode->i_size) {
                        // get the indirect block, which contains 1024 / 4 = 256 4-byte pointers
                        unsigned int ind_block[block_size / sizeof(unsigned int)];
                        lseek(fd, BLOCK_OFFSET(inode->i_block[EXT2_IND_BLOCK]), SEEK_SET);
                        read(fd, ind_block, block_size);

                        lseek(fd, BLOCK_OFFSET(ind_block[0]), SEEK_SET);
                        read(fd, buffer, block_size);
                        block_idx = 0;

                        while (bytes_copied < inode->i_size && block_idx < block_size / 4) {
                            unsigned int write_len = bytes_copied + block_size > inode->i_size ? inode->i_size - bytes_copied : block_size;
                            bytes_copied += write_len;
                            write(jpg_fd, buffer, write_len);
                            if (write_len == block_size) {
                                lseek(fd, BLOCK_OFFSET(ind_block[++block_idx]), SEEK_SET);
                                read(fd, buffer, block_size);
                            }
                        }
                    }

                    // double indirect pointer
                    if (bytes_copied < inode->i_size) {
                        unsigned int outer_ind_block[block_size / sizeof(unsigned int)];
                        lseek(fd, BLOCK_OFFSET(inode->i_block[EXT2_DIND_BLOCK]), SEEK_SET);
                        read(fd, outer_ind_block, block_size);

                        unsigned int outer_block_idx = 0;
                        while (bytes_copied < inode->i_size && outer_block_idx < block_size / 4) {
                            unsigned int ind_block[block_size / sizeof(unsigned int)];
                            lseek(fd, BLOCK_OFFSET(outer_ind_block[outer_block_idx++]), SEEK_SET);
                            read(fd, ind_block, block_size);

                            lseek(fd, BLOCK_OFFSET(ind_block[0]), SEEK_SET);
                            read(fd, buffer, block_size);
                            block_idx = 0;

                            while (bytes_copied < inode->i_size && block_idx < block_size / 4) {
                                unsigned int write_len = bytes_copied + block_size > inode->i_size ? inode->i_size - bytes_copied : block_size;
                                bytes_copied += write_len;
                                write(jpg_fd, buffer, write_len);
                                if (write_len == block_size) {
                                    lseek(fd, BLOCK_OFFSET(ind_block[++block_idx]), SEEK_SET);
                                    read(fd, buffer, block_size);
                                }
                            }
                        }
                    }
                    close(jpg_fd);
                }
            }
            else if (S_ISDIR(inode->i_mode)) {
                // skip . and .. entries
                unsigned int offset = 24;
                while (offset < block_size) {
                    struct ext2_dir_entry* dentry = (struct ext2_dir_entry*) & (buffer[offset]);
                    int name_len = dentry->name_len & 0xFF; // convert 2 bytes to 4 bytes properly
                    char name[EXT2_NAME_LEN];
                    strncpy(name, dentry->name, name_len);
                    name[name_len] = '\0';
                    if (dentry->inode <= num_groups * inodes_per_group + 1)
                        if (strcmp(name, "top_secret") == 0) {
                            top_secret_dentry = *dentry;
                            have_top_secret = true;
                        }

                    offset += sizeof(dentry->inode) + sizeof(dentry->rec_len) + sizeof(dentry->name_len) + sizeof(dentry->file_type) + name_len;
                    offset = (offset % 4 == 0) ? offset : offset + 4 - offset % 4;
                }
            }
            free(inode);
        }
    }

    // if have top_secret dir, only recover jpgs in it
    if (have_top_secret) {
        unsigned int ngroup = top_secret_dentry.inode/inodes_per_group;
        off_t inode_table = locate_inode_table(ngroup, groups);
        struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
        read_inode(fd, inode_table, top_secret_dentry.inode%inodes_per_group, inode, super.s_inode_size);
        char buffer[block_size];
        // only need to read block[0] per project spec
        lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
        read(fd, buffer, block_size);
        // skip . and .. entries
        unsigned int offset = 24;
        while (offset < block_size) {
            struct ext2_dir_entry* dentry = (struct ext2_dir_entry*) & (buffer[offset]);
            int name_len = dentry->name_len & 0xFF; // convert 2 bytes to 4 bytes properly
            char name[EXT2_NAME_LEN];
            strncpy(name, dentry->name, name_len);
            name[name_len] = '\0';
            if (dentry->inode <= num_groups * inodes_per_group + 1) {
                if (name[0] != '\0' && inode_is_jpg[dentry->inode]) {
                    inode_is_jpg[dentry->inode] = false;
                    char inode_num_str[20];
                    sprintf(inode_num_str, "%u", dentry->inode);
                    int old_filename_len = strlen(argv[2]) + 6 + strlen(inode_num_str) + 5;
                    char* old_filename = malloc(old_filename_len * sizeof(char));
                    snprintf(old_filename, old_filename_len, "%s/file-%u.jpg", argv[2], dentry->inode);
                    FILE *old_fp = fopen(old_filename, "rb");
                    free(old_filename);

                    int new_filename_len = strlen(argv[2]) + 1 + name_len + 1;
                    char* new_filename = malloc(new_filename_len * sizeof(char));
                    snprintf(new_filename, new_filename_len, "%s/%s", argv[2], name);
                    FILE *new_fp = fopen(new_filename, "wb");
                    free(new_filename);

                    char copy_buffer[1];
                    int num_read;
                    while ((num_read = fread(copy_buffer, 1, sizeof(copy_buffer), old_fp)) > 0)
                        fwrite(copy_buffer, 1, num_read, new_fp);

                    fclose(old_fp);
                    fclose(new_fp);
                }
            }

            offset += sizeof(dentry->inode) + sizeof(dentry->rec_len) + sizeof(dentry->name_len) + sizeof(dentry->file_type) + name_len;
            offset = (offset % 4 == 0) ? offset : offset + 4 - offset % 4;
        }
        free(inode);

        // remove files not belongs to top_secret
        for (unsigned int i = 0; i < num_groups * inodes_per_group + 1; i++) {
            if(inode_is_jpg[i]) {
                char inode_num_str[20];
                sprintf(inode_num_str, "%u", i);
                int filename_len = strlen(argv[2]) + 6 + strlen(inode_num_str) + 5;
                char* filename = malloc(filename_len * sizeof(char));
                snprintf(filename, filename_len, "%s/file-%u.jpg", argv[2], i);
                remove(filename);
                filename_len += 8;
                filename = realloc(filename, filename_len * sizeof(char));
                snprintf(filename, filename_len, "%s/file-%u-details.txt", argv[2], i);
                remove(filename);
                free(filename);
            }
        }
    }
    else {        // if there is no top_secret dir, recover all jpgs
        for (unsigned int ngroup = 0; ngroup < num_groups; ngroup++) {
            off_t inode_table = locate_inode_table(ngroup, groups);

            for (unsigned int i = 1; i <= inodes_per_group; i++) {
                struct ext2_inode *inode = malloc(sizeof(struct ext2_inode));
                read_inode(fd, inode_table, i, inode, super.s_inode_size);

                char buffer[block_size];
                // only need to read block[0] per project spec
                lseek(fd, BLOCK_OFFSET(inode->i_block[0]), SEEK_SET);
                read(fd, buffer, block_size);

                if (S_ISDIR(inode->i_mode)) {
                    // skip . and .. entries
                    unsigned int offset = 24;
                    while (offset < block_size) {
                        struct ext2_dir_entry* dentry = (struct ext2_dir_entry*) & (buffer[offset]);
                        int name_len = dentry->name_len & 0xFF; // convert 2 bytes to 4 bytes properly
                        char name[EXT2_NAME_LEN];
                        strncpy(name, dentry->name, name_len);
                        name[name_len] = '\0';
                        if (dentry->inode <= num_groups * inodes_per_group + 1) {
                            if (name[0] != '\0' && inode_is_jpg[dentry->inode]) {
                                char inode_num_str[20];
                                sprintf(inode_num_str, "%u", dentry->inode);
                                int old_filename_len = strlen(argv[2]) + 6 + strlen(inode_num_str) + 5;
                                char* old_filename = malloc(old_filename_len * sizeof(char));
                                snprintf(old_filename, old_filename_len, "%s/file-%u.jpg", argv[2], dentry->inode);
                                FILE *old_fp = fopen(old_filename, "rb");
                                free(old_filename);

                                int new_filename_len = strlen(argv[2]) + 1 + name_len + 1;
                                char* new_filename = malloc(new_filename_len * sizeof(char));
                                snprintf(new_filename, new_filename_len, "%s/%s", argv[2], name);
                                FILE *new_fp = fopen(new_filename, "wb");
                                free(new_filename);

                                char copy_buffer[1];
                                int num_read;
                                while ((num_read = fread(copy_buffer, 1, sizeof(copy_buffer), old_fp)) > 0)
                                    fwrite(copy_buffer, 1, num_read, new_fp);

                                fclose(old_fp);
                                fclose(new_fp);
                            }
                        }

                        offset += sizeof(dentry->inode) + sizeof(dentry->rec_len) + sizeof(dentry->name_len) + sizeof(dentry->file_type) + name_len;
                        offset = (offset % 4 == 0) ? offset : offset + 4 - offset % 4;
                    }
                }
                free(inode);
            }
        }
    }

    close(fd);
    free(groups);

    return 0;
}