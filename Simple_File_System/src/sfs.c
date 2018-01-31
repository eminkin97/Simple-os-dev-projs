/*
  Simple File System

  This code is derived from function prototypes found /usr/include/fuse/fuse.h
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  His code is licensed under the LGPLv2.

*/

#include "params.h"
#include "block.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include "log.h"

char* diskF;
int numOccupiedInodeBytemap;     //number of bytemap nodes occupied for inodes
int numOccupiedDataBlocksBytemap;     //number of bytemap nodes occupied for Data Blocks

///////////////////////////////////////////////////////////
//
// Prototypes for all these functions, and the C-style comments,
// come indirectly from /usr/include/fuse.h
//

/**
 * Initialize filesystem
 *
 * The return value will passed in the private_data field of
 * fuse_context to all file operations and as a parameter to the
 * destroy() method.
 *
 * Introduced in version 2.3
 * Changed in version 2.6
 */
void *sfs_init(struct fuse_conn_info *conn)
{
    fprintf(stderr, "in bb-init\n");
    log_msg("\nsfs_init()\n");
    
    log_conn(conn);
    log_fuse_context(fuse_get_context());
    
    //open the flatfile that represents disk space
    log_msg("\nBEFORE OPENED FLATFILE\n");
    disk_open(diskF);
    log_msg("\nOPENED FLATFILE\n");
    
    //initialize these to 0
    numOccupiedInodeBytemap = 0;
    numOccupiedDataBlocksBytemap = 0;

    //DATA ARRANGED IN THE FOLLOWING WAY:
    //first 10 blocks are Inode bytemap
    //next 10 blocks are datablock bytemap
    //next 2764 blocks are are Inodes
    //next 5000 blocks are datablocks
    char TEMP_INODE_BLOCK[BLOCK_SIZE];
    char TEMP_BITMAP_BLOCK[BLOCK_SIZE];
    
    //struct timespec *now = (struct timespec *)malloc(sizeof(struct timespec));
    //clock_gettime(CLOCK_REALTIME, now); //gets the current system time
    time_t now;
    time(&now);
    
    //creat inode for root directory
    inode *root = (inode *)TEMP_INODE_BLOCK;
    root -> mode = S_IRUSR | S_IWUSR | S_IXUSR;     //give root read, write, execute from beginning
    root -> state = 'd';       //indicates directory
    //root -> name = "/";
    root -> uid = 0;
    root -> links_count = 1;
    root -> flag = 0;
    root -> ctime = now;
    root -> time = now;
    root -> mtime = now;
    
    log_msg("\nCREATED INODE FOR ROOT\n");

    int i;
    for (i = 0; i < 10; i++) {
        //initialize block ptrs to be -1 indicating no reference
        root -> block_ptr[i] = -1;
    }
    
    root -> block_ptr_indirect[1] = NULL;
    root -> block_ptr_indirect[2] = NULL;
    root -> block_ptr_double_indirect = NULL;
    
    char *ptr = (char *)TEMP_BITMAP_BLOCK;
    *ptr = 1;
    
    if (block_write(0, TEMP_BITMAP_BLOCK) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in INIT\n");
        exit(1);
    }
    
    if (block_write(20, TEMP_INODE_BLOCK) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in INIT\n");
        exit(1);
    }
    
    log_msg("\nPERFORMED THE DISK WRITES\n");
    
    return SFS_DATA;
}

/**
 * Clean up filesystem
 *
 * Called on filesystem exit.
 *
 * Introduced in version 2.3
 */
void sfs_destroy(void *userdata)
{
    log_msg("\nsfs_destroy(userdata=0x%08x)\n", userdata);
}

//takes inode number. Returns block number where that Inode is
static int getBlockNumbfromInodeNumb(int inumb) {
    //find block we need to read
    int block_num = 1;
    while(1) {
        if (BLOCK_SIZE * block_num > sizeof(inode) * inumb) {
            block_num--;
            break;
        }
        block_num++;
    }
    
    return block_num;
}

static char *getBlockByInodeNumb(int inumb) {
    char *TEMP =(char*)malloc(2*BLOCK_SIZE);
    
    //find block we need to read
    int block_num = getBlockNumbfromInodeNumb(inumb);
	        
    log_msg("\nBLOCK_NUM: %d\n", block_num);

    int offset_in_block = (sizeof(inode) * inumb) % BLOCK_SIZE;
    int end_offset = (sizeof(inode) * (inumb+1)) % BLOCK_SIZE;
    
    if (end_offset < offset_in_block) {
        //inode leaks into 2 blocks so read 2 blocks
        //read in first block
        if (block_read(20+block_num, TEMP) < 0) {
            log_msg("\nERROR in block_read in getattr\n");
	    log_msg(strerror(errno));
            exit(1);
        }
	            
        //read in second block
        if (block_read(20+block_num, TEMP+BLOCK_SIZE) < 0) {
            log_msg("\nERROR in block_read in getattr\n");
	    log_msg(strerror(errno));
            exit(1);
        }
    } else {
        //read in block
        if (block_read(20+block_num, TEMP) < 0) {
            log_msg("\nERROR in block_read in getattr\n");
	    log_msg(strerror(errno));
            exit(1);
        }
    }
    
    return TEMP;
}

//given a path, returns inode referencing inode for directory or file at that path
static int getInodeNumbFromPath(const char *path) {
    //start at root. Search for entries in path
	char *token;
	
	int curr_inumb = 0;    //start at root directory and search from there
	token = strtok(path, "/");
	
	while(token != NULL) {
	    //get block for curr_inumb which is directory we will look through
    	log_msg("\nToken: %s\n", token);
	    int offset_in_block1 = (sizeof(inode) * curr_inumb) % BLOCK_SIZE;
	    
	    char *temp1 = getBlockByInodeNumb(curr_inumb);
	    inode *ptr = (inode *)(temp1 + offset_in_block1);
	    
	    //check all child inodes to see if name matches token
	    int found = 0;
        int i;
	    for (i = 0; i < 10; i++) {
	        if (ptr->block_ptr[i] == -1) {
	            //no more child inodes
	            break;
	        }
	        int child_inode_num = ptr->block_ptr[i];
	        int offset_in_block = (sizeof(inode) * child_inode_num) % BLOCK_SIZE;
	        
	        char *temp = getBlockByInodeNumb(child_inode_num);
	        inode *child = 	(inode *)(temp + offset_in_block);
	        
	        if (strcmp(child -> name, token) == 0) {
	            //found what I'm looking for
	            found = 1;
    			log_msg("\nFound correct file\n");
	            curr_inumb = child_inode_num;
	            break;
            } else {
                //keep searching
                continue;
            }
            
            free(temp);
        }
        
        if (found == 0) {
            //didn't find child with that path name
            log_msg("\ngetattr(): DIDN'T FIND CHILD WITH THAT PATH\n");
            return -1;
        }
        
	    free(temp1);
	    //gives the next token
	    token = strtok(NULL, "/");
	}
	
	return curr_inumb;  //return inumb of thing we were looking for
}

/** Get file attributes.
 *
 * Similar to stat().  The 'st_dev' and 'st_blksize' fields are
 * ignored.  The 'st_ino' field is ignored except if the 'use_ino'
 * mount option is given.
 */
 
int sfs_getattr(const char *path, struct stat *statbuf)
{
    int retstat = 0;
    char fpath[PATH_MAX];
    
    log_msg("\nsfs_getattr(path=\"%s\", statbuf=0x%08x)\n",
	  path, statbuf);
	
	int correct_inumb = getInodeNumbFromPath(path);
	if (correct_inumb == -1) {
	    log_msg("\nCOULDN'T FIND INODE CORRESPONDING TO PATH\n");
	    return -1;
	}
	
	int offset_in_block = (sizeof(inode) * correct_inumb) % BLOCK_SIZE;
	char *temp2 = getBlockByInodeNumb(correct_inumb);
	inode *correct_inode = (inode *)(temp2 + offset_in_block);
    log_msg("\nCorrect inumb: %d\n", correct_inumb);
	
	//fill stat buffer with data from correct_inode
	statbuf -> st_ino = correct_inode -> uid;
	if(correct_inode->state == 'd') {
	    statbuf -> st_mode = S_IFDIR | 0755;
	    statbuf -> st_nlink = 2;
    }else if(correct_inode->state == 'f'){
        statbuf -> st_mode = S_IFREG | 0444;
	    statbuf -> st_nlink = 1;
    }
	statbuf -> st_uid = correct_inode -> ownerID;
	statbuf -> st_gid = correct_inode -> groupID;
	statbuf -> st_rdev = 0;     //device ID
	statbuf -> st_size = correct_inode -> size;
	
	//number of blocks in filesize
    int num_blocks = 1;
    while(1) {
        if (BLOCK_SIZE * num_blocks > correct_inode -> size) {
            break;
        }
        num_blocks++;
    }
    
    statbuf -> st_blocks = num_blocks;
    statbuf -> st_atime = correct_inode -> time;
    statbuf -> st_mtime = correct_inode -> mtime;
    statbuf -> st_ctime = correct_inode -> ctime;
    
    log_msg("\nST_INO %d\n", statbuf -> st_ino);
    log_msg("\nST_mode %d\n", statbuf -> st_mode);
    log_msg("\nST_nlink %d\n", statbuf -> st_nlink);
    log_msg("\nST_uid %d\n", statbuf -> st_uid);
    log_msg("\nST_gid %d\n", statbuf -> st_gid);
    log_msg("\nST_size %d\n", statbuf -> st_size);
    
    log_msg("\nEND OF getattr\n");
    free(temp2);
    log_msg("\nRETSTAT: %d\n", retstat);
    return 0;
}

/** 
 * This function creates a substrng from an original string 
 * 
 * @param string The string that will be parsed
 * @param position Starting position in the string
 * @param length The length of the desired substring
 * 
 * @return pointer A pointer to the substring that was created
 */
static char *substring(char *string, int position, int length) 
{
   char *pointer;
   int c;
 
   pointer = malloc(length+1);
 
   if (pointer == NULL)
   {
      printf("Unable to allocate memory.\n");
      exit(1);
   }
 
   for (c = 0 ; c < length ; c++)
   {
      *(pointer+c) = *(string+position-1);      
      string++;   
   }
 
   *(pointer+c) = '\0';
 
   return pointer;
}


static int getFreeBitMapEntry(int type) {
    //go into bitmap and look for open entrry
	int byteMapPosition = rand() % 5000;
	int currBlock = -1;
	char *temp = NULL;
	int offset;
	
	if (type == 0) {
	    //inode bitmap
	    offset = 0;
	} else {
	    //data block bitmap
	    offset = 10;
	}
	
	//find open ByteMap Entry
	while(1) {
	    //find block that bitmap entry is in
        int block_in = 1;
        while(1) {
            if (BLOCK_SIZE * block_in > byteMapPosition) {
                block_in--;
                break;
            }
            block_in++;
        }
        
        //check to see whether you need to read a new blok
        if (block_in != currBlock) {
            currBlock = block_in;
            
            if (temp != NULL) {
                free(temp);
            }
            temp = malloc(BLOCK_SIZE);
            
            if (block_read(offset+currBlock, temp) < 0) {
                log_msg("\nERROR in block_read in sfs_create\n");
                return -1;
            }
	    }
        
        int offset_in_block = byteMapPosition % BLOCK_SIZE;
        char *byte = (temp + offset_in_block);
        
        //check if byte is equal to 1
        if (*byte == 1) {
            //increment byteMapPosition and try again
            if (byteMapPosition == 4999) {
                byteMapPosition = 1;
            } else {
                byteMapPosition++;
            }
            
            continue;
        } else {
            //found open byte. USE IT FOR INODE. Set to 1
            *byte = 1;
            break;
        }
        
	}
	
	//write back block because we edited bitMap
	if (block_write(offset+currBlock, temp) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in create\n");
        return -1;
    }
    
    free(temp);
    
    return byteMapPosition;     //returning inode number
}


/**
 * Create and open a file
 *
 * If the file does not exist, first create it with the specified
 * mode, and then open it.
 *
 * If this method is not implemented or under Linux kernel
 * versions earlier than 2.6.15, the mknod() and open() methods
 * will be called instead.
 *
 * Introduced in version 2.5
 */
int sfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_create(path=\"%s\", mode=0%03o, fi=0x%08x)\n",
	    path, mode, fi);
    
    if (numOccupiedInodeBytemap == 5000) {
        log_msg("\nALL INODES OCCUPIED\n");
        return -1;
    }
    
    //gets a string for the path to the file and
    //the file itself
    int pathlen = strlen(path);
    char* newFile;
    char* originalPath;
    int i;
    for(i = pathlen - 1 ; path[i] != '/' ; i--);
    
    newFile = substring(path,i+1,pathlen-1);
    originalPath = substring(path,0,i);
    
    log_msg("\nNEWFILE: %s, ORIGINAL PATH %s\n", newFile, originalPath);
    
    //get inode number corresponding to directory above where file is going to be
    int dir_inumb = getInodeNumbFromPath(originalPath);
	
	//get inode corresponding to number
	int offset_in_block = (sizeof(inode) * dir_inumb) % BLOCK_SIZE;
	char *dir_block = getBlockByInodeNumb(dir_inumb);
	inode *dir_inode = (inode *)(dir_block + offset_in_block);
	
	//see if any open spaces in directory
	int open_child_space = -1;
	for (i = 0; i < 10; i++) {
        if (dir_inode -> block_ptr[i] == -1) {
            open_child_space = i;
        }
    }
    
    if (open_child_space == -1) {
        //no more spaces in the directory
        log_msg("\nNO MORE SPACES IN DIRECTORY\n");
        return -1;
    }
	
	//find open inode by examining byte map
	int new_inumb = getFreeBitMapEntry(0);
	numOccupiedInodeBytemap++;
	if (new_inumb == -1) {
	    //error
	    return -1;
	}
	
    //get block with the new inode we're allocating
    char *temp;
	int offset_in_block1 = (sizeof(inode) * new_inumb) % BLOCK_SIZE;
	temp = getBlockByInodeNumb(new_inumb);
	inode *new_inode = (inode *)(temp + offset_in_block);
	
	//initialize new inode
	time_t now;
    time(&now);
    
    new_inode -> mode = mode;     //give root read, write, execute from beginning
    new_inode -> state = 'f';       //indicates directory
    strcpy(new_inode -> name, newFile);
    new_inode -> uid = new_inumb;
    new_inode -> links_count = 1;
    new_inode -> flag = 'o';            //sets open flag
    new_inode -> ctime = now;
    new_inode -> time = now;
    new_inode -> mtime = now;

    for (i = 0; i < 10; i++) {
        //initialize block ptrs to be -1 indicating no reference
        new_inode -> block_ptr[i] = -1;
    }
    
    new_inode -> block_ptr_indirect[1] = NULL;
    new_inode -> block_ptr_indirect[2] = NULL;
    new_inode -> block_ptr_double_indirect = NULL;
    
    //go into data block bitmap and look for open data block
	int datablock_numb = getFreeBitMapEntry(1);
	numOccupiedDataBlocksBytemap++;
	if (datablock_numb == -1) {
	    //error
	    return -1;
	}
	
	//new file gets one data block to start with
	new_inode -> block_ptr[0] = datablock_numb;
	
	//directory makes inode referencing file its child
	dir_inode -> block_ptr[open_child_space] = new_inumb;
	
	//writes parent directory back
	int dir_block_numb = getBlockNumbfromInodeNumb(dir_inumb);
	
	if (block_write(20+dir_block_numb, dir_block) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in create\n");
        return -1;
    }
    
    //writes new inode in
    int new_inode_block_numb = getBlockNumbfromInodeNumb(new_inumb);
	
	if (block_write(20+new_inode_block_numb, temp) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in create\n");
        return -1;
    }
	
	free(dir_block);
	free(temp);
	
    log_msg("\nEND OF CREATE\n");
    return retstat;
}

//find block that bitmap is in
static int findBlockForBitmap(int bitmapNumb) {
    int block_in = 1;
    while(1) {
        if (BLOCK_SIZE * block_in > bitmapNumb) {
            block_in--;
            break;
        }
        block_in++;
    }
    return block_in;
}

//gets the Bitmap Block
static char* getBitmapBlock(int bitmapNumb, int type) {
    char *temp = malloc(BLOCK_SIZE);
    int block_in = findBlockForBitmap(bitmapNumb);
    int offset = 0;
    
    if (type == 1) {
        //data block
        offset = 10;
    }
    
    if (block_read(offset+block_in, temp) < 0) {
        log_msg("\nERROR in block_read in sfs_create\n");
        return NULL;
    }
    
    return temp;
}

//memserts the data block and clears its bytemap entry
static int clearDataBlock(int data_block_num) {
    //CLEARS BYTEMAP NUM
    //gets block that data_bytemap_num is in
    int block_num = findBlockForBitmap(data_block_num);
    
    //reads that block
    int offset_in_block = block_num % BLOCK_SIZE;
    char *block = getBitmapBlock(data_block_num, 1);
    
    //sets byte to 0
    char *byte = (block + offset_in_block);
    *byte = 0;
    
    //writes the block back in
	if (block_write(block_num, block) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in clearDataBlock\n");
        return -1;
    }
    
    free(block);
    
    //CLEAR DATABLOCK;
    char* temp = malloc(BLOCK_SIZE);
    temp = (char *)memset(temp, 0, BLOCK_SIZE);
    
    //writes in to datablock
	if (block_write(block_num, temp) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in clearDataBlock\n");
        return -1;
    }
    free(temp);
    
}

/** Remove a file */
int sfs_unlink(const char *path)
{
    int retstat = 0;
    log_msg("sfs_unlink(path=\"%s\")\n", path);

    //gets a string for the path to the file and
    //the file itself
    int pathlen = strlen(path);
    char* dirPath;
    int i;
    for(i = pathlen - 1 ; path[i] != '/' ; i--);
    
    dirPath = substring(path,0,i);
    
    // find directory inode and file inode
    int inumb = getInodeNumbFromPath(path);
    int dir_inumb = getInodeNumbFromPath(dirPath);
    
    //find block in
    int offset_in_block1 = (sizeof(inode) * inumb) % BLOCK_SIZE;
    char *inode_block = getBlockByInodeNumb(inumb);
    inode *inode1 = (inode *)(inode_block + offset_in_block1);
    int inode_block_numb = getBlockNumbfromInodeNumb(inumb);
    
    //find block for directory
    int offset_in_block = (sizeof(inode) * dir_inumb) % BLOCK_SIZE;
	char *dir_block = getBlockByInodeNumb(dir_inumb);
	inode *dir_inode = (inode *)(dir_block + offset_in_block);
	int dir_block_numb = getBlockNumbfromInodeNumb(dir_inumb);
	
	//remove reference in dir_inode to inode
	int found = 0;
	int j;
	
	//search direct blocks
    for (i = 0; i < 10; i++) {
        if (dir_inode -> block_ptr[i] == inumb) {
            dir_inode -> block_ptr[i] = -1;
            found = 1;
            break;
        }
    }

    //search indirection blocks
    if (found == 0) {
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 4; j++) {
                if (dir_inode -> block_ptr_indirect[i] -> block_ptr[j] == inumb) {
                    dir_inode -> block_ptr_indirect[i] -> block_ptr[j] = -1;
                    found = 1;
                    break;
                }
            }
            if (found == 1)
                break;
        }
    }
    
    //search double indirection block
    if (found == 0) {
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                if (dir_inode -> block_ptr_double_indirect -> indirect_ptr[i] -> block_ptr[j] == inumb) {
                    dir_inode -> block_ptr_double_indirect -> indirect_ptr[i] -> block_ptr[j] = -1;
                    found = 1;
                    break;
                }
            }
            if (found == 1)
                break;
        }
    }
    
    //write back then free directory inode
	if (block_write(20+dir_block_numb, dir_block) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in unlink\n");
        return -1;
    }
	
	free(dir_block);
	
	//get the bitmap block
	char *bitmap_block = getBitmapBlock(inumb, 0);
	if (bitmap_block == NULL) {
	    return -1;
	}
	
	//get bytemap entry corresponding to inumb and set to 0
	offset_in_block = inumb % BLOCK_SIZE;
    char *byte = (bitmap_block + offset_in_block);
    
    *byte = 0;
    
    int block_in = findBlockForBitmap(inumb);
    
	//write back block in bitmap
	if (block_write(block_in, bitmap_block) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in unlink\n");
        return -1;
    }
    
    free(bitmap_block);
    
    //find data blocks for the inode and clear them
    int done = 0;
    
    for (i = 0; i < 10; i++) {
        if(inode1 -> block_ptr[i] == -1){
            done = 1;
        } else {
            if(clearDataBlock(inode1 -> block_ptr[i]) == -1) {
                log_msg("\nERROR CLEARING DATABLOCK\n");
                exit(1);
            }
        }
    }

    //search indirection blocks
    if (done == 0) {
        for (i = 0; i < 2; i++) {
            for (j = 0; j < 4; j++) {
                if (inode1 -> block_ptr_indirect[i] -> block_ptr[j] == -1) {
                    done = 1;
                } else {
                    //clear data block
                    if (clearDataBlock(dir_inode -> block_ptr_indirect[i] -> block_ptr[j]) == -1) {
                        log_msg("\nERROR CLEARING DATABLOCK\n");
                        exit(1);
                    }
                }
            }
            if (done == 1)
                break;
        }
    }
    
    //search double indirection block and clear its datablocks
    if (done == 0) {
        for (i = 0; i < 4; i++) {
            for (j = 0; j < 4; j++) {
                if (inode1 -> block_ptr_double_indirect -> indirect_ptr[i] -> block_ptr[j] == inumb) {
                    done = 1;
                    break;
                } else {
                    //clear data block
                    if (clearDataBlock(inode1 -> block_ptr_double_indirect -> indirect_ptr[i] -> block_ptr[j]) == -1) {
                        log_msg("\nERROR CLEARING DATABLOCK\n");
                        exit(1);
                    }
                }
            }
            if (done == 1)
                break;
        }
    }
    
    
    //memset the inode
    memset(inode1, 0, sizeof(inode));
    
    //write back the memsetted inode
	if (block_write(20+inode_block_numb, inode_block) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in unlink\n");
        return -1;
    }
    
    free(inode1);
    
    return retstat;
}

/** File open operation
 *
 * No creation, or truncation flags (O_CREAT, O_EXCL, O_TRUNC)
 * will be passed to open().  Open should check if the operation
 * is permitted for the given flags.  Optionally open may also
 * return an arbitrary filehandle in the fuse_file_info structure,
 * which will be passed to all file operations.
 *
 * Changed in version 2.2
 */
int sfs_open(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_open(path\"%s\", fi=0x%08x)\n",
	    path, fi);

    
    return retstat;
}

/** Release an open file
 *
 * Release is called when there are no more references to an open
 * file: all file descriptors are closed and all memory mappings
 * are unmapped.
 *
 * For every open() call there will be exactly one release() call
 * with the same flags and file descriptor.  It is possible to
 * have a file opened more than once, in which case only the last
 * release will mean, that no more reads/writes will happen on the
 * file.  The return value of release is ignored.
 *
 * Changed in version 2.2
 */
int sfs_release(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_release(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    

    return retstat;
}

/** Read data from an open file
 *
 * Read should return exactly the number of bytes requested except
 * on EOF or error, otherwise the rest of the data will be
 * substituted with zeroes.  An exception to this is when the
 * 'direct_io' mount option is specified, in which case the return
 * value of the read system call will reflect the return value of
 * this operation.
 *
 * Changed in version 2.2
 */
int sfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_read(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);

   
    return retstat;
}

/** Write data to an open file
 *
 * Write should return exactly the number of bytes requested
 * except on error.  An exception to this is when the 'direct_io'
 * mount option is specified (see read operation).
 *
 * Changed in version 2.2
 */
int sfs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_write(path=\"%s\", buf=0x%08x, size=%d, offset=%lld, fi=0x%08x)\n",
	    path, buf, size, offset, fi);
    
    
    return retstat;
}


/** Create a directory */
int sfs_mkdir(const char *path, mode_t mode)
{
    int retstat = 0;
    log_msg("\nsfs_mkdir(path=\"%s\", mode=0%3o)\n",
	    path, mode);
   
    
    return retstat;
}


/** Remove a directory */
int sfs_rmdir(const char *path)
{
    int retstat = 0;
    log_msg("sfs_rmdir(path=\"%s\")\n",
	    path);
    
    
    return retstat;
}


/** Open directory
 *
 * This method should check if the open operation is permitted for
 * this  directory
 *
 * Introduced in version 2.3
 */
int sfs_opendir(const char *path, struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nsfs_opendir(path=\"%s\", fi=0x%08x)\n",
	  path, fi);
    
    //get Inode numb from path
    int correct_inumb = getInodeNumbFromPath(path);
    
    //get the Inode block
    int offset_in_block = (sizeof(inode) * correct_inumb) % BLOCK_SIZE;
	char *temp = getBlockByInodeNumb(correct_inumb);
	inode *correct_inode = (inode *)(temp + offset_in_block);
    
    int read_permission = (correct_inode -> mode) & (S_IRUSR | S_IXUSR);
    
    if ((correct_inode -> state == 'd') && (read_permission == (S_IRUSR | S_IXUSR))) {
        //we all good
        retstat = 0;
        log_msg("\nOPEN THE DIRECTORY, NO PROBLEMO\n");
        correct_inode -> flag = 'o';        //flag that indicates directory is open
    } else {
        //Uh oh
        log_msg("\nERROR IN OPENDIR. NO READ PERMISSION\n");
        errno = EACCES;
        retstat = -1;
    }
    
    int block_num = getBlockNumbfromInodeNumb(correct_inumb);
    
    //write block back because we changed flag
    if (block_write(20+block_num, temp) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in INIT\n");
        exit(1);
    }
    
    free(temp);
    log_msg("\nEND OF OPENDIR\n");
    
    return retstat;
}

/** Read directory
 *
 * This supersedes the old getdir() interface.  New applications
 * should use this.
 *
 * The filesystem may choose between two modes of operation:
 *
 * 1) The readdir implementation ignores the offset parameter, and
 * passes zero to the filler function's offset.  The filler
 * function will not return '1' (unless an error happens), so the
 * whole directory is read in a single readdir operation.  This
 * works just like the old getdir() method.
 *
 * 2) The readdir implementation keeps track of the offsets of the
 * directory entries.  It uses the offset parameter and always
 * passes non-zero offset to the filler function.  When the buffer
 * is full (or an error happens) the filler function will return
 * '1'.
 *
 * Introduced in version 2.3
 */
int sfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int retstat = 0;
    log_msg("\nREADDIR BEGINS\n");
    
    //get Inode numb from path
    int correct_inumb = getInodeNumbFromPath(path);
    
    //get the Inode block
    int offset_in_block = (sizeof(inode) * correct_inumb) % BLOCK_SIZE;
	char *temp = getBlockByInodeNumb(correct_inumb);
	inode *correct_inode = (inode *)(temp + offset_in_block);
    log_msg("\nCorrect inumb: %d\n", correct_inumb);
    
    if (correct_inode -> flag != 'o') {
        //open if not open
        if (sfs_opendir(path, NULL) == -1) {
            //BAD
            return -1;
        }
    }
    
    //go through entries in correct_inode. Fill buffer with them
    int i;
    for (i = 0; i < 10; i++) {
        log_msg("\nLOOPING IN READDIR\n");
        
        if (correct_inode -> block_ptr[i] == -1) {
            log_msg("\nBREAKING FROM LOOP\n");
            break;
        }
        
        int child_inumb = correct_inode -> block_ptr[i];
        
        //get the Inode block
        int offset_in_block1 = (sizeof(inode) * child_inumb) % BLOCK_SIZE;
	    char *temp1 = getBlockByInodeNumb(child_inumb);
    	inode *child_inode = (inode *)(temp1 + offset_in_block1);
    	
    	int next_offset;
        if ((i == 9) || (correct_inode -> block_ptr[i+1] == -1)) {
           //call filler function. No next offset
           next_offset = -1;
        } else {
            next_offset = correct_inode -> block_ptr[i+1];
        }
        
        //Call the filler function with buf, the filename, and the offset of the next directory entry.
        if (filler(buf, child_inode -> name, NULL, next_offset) != 0) {
            return 0;
        }
    }
    
    log_msg("\nEND OF READDIR\n");
    
    return retstat;
}

/** Release directory
 *
 * Introduced in version 2.3
 */
int sfs_releasedir(const char *path, struct fuse_file_info *fi)
{
    log_msg("\nRELEASE DIR START\n");
    int retstat = 0;

    //get Inode numb from path
    int correct_inumb = getInodeNumbFromPath(path);
    
    //get the Inode block
    int offset_in_block = (sizeof(inode) * correct_inumb) % BLOCK_SIZE;
	char *temp = getBlockByInodeNumb(correct_inumb);
	inode *correct_inode = (inode *)(temp + offset_in_block);

    correct_inode -> flag = 0;
    
    int block_num = getBlockNumbfromInodeNumb(correct_inumb);
    
    //write block back because we changed flag
    if (block_write(20+block_num, temp) != BLOCK_SIZE) {
        log_msg("\nERROR in block_write in INIT\n");
        exit(1);
    }
    
    free(temp);
    log_msg("\nRELEASE DIR ENDS\n");
    return retstat;
}

struct fuse_operations sfs_oper = {
  .init = sfs_init,
  .destroy = sfs_destroy,

  .getattr = sfs_getattr,
  .create = sfs_create,
  .unlink = sfs_unlink,
  .open = sfs_open,
  .release = sfs_release,
  .read = sfs_read,
  .write = sfs_write,

  .rmdir = sfs_rmdir,
  .mkdir = sfs_mkdir,

  .opendir = sfs_opendir,
  .readdir = sfs_readdir,
  .releasedir = sfs_releasedir
};

void sfs_usage()
{
    fprintf(stderr, "usage:  sfs [FUSE and mount options] diskFile mountPoint\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct sfs_state *sfs_data;
    
    // sanity checking on the command line
    if ((argc < 3) || (argv[argc-2][0] == '-') || (argv[argc-1][0] == '-'))
	sfs_usage();

    sfs_data = malloc(sizeof(struct sfs_state));
    if (sfs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the diskfile and save it in internal data
    sfs_data->diskfile = argv[argc-2];
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    diskF = sfs_data->diskfile;
    
    sfs_data->logfile = log_open();
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main, %s \n", sfs_data->diskfile);
    fuse_stat = fuse_main(argc, argv, &sfs_oper, sfs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
