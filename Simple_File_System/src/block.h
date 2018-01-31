/*
  Copyright (C) 2015 CS416/CS516

  This program can be distributed under the terms of the GNU GPLv3.
  See the file COPYING.
*/

#ifndef _BLOCK_H_
#define _BLOCK_H_

#define BLOCK_SIZE 512
#include <time.h>

//indirection block
typedef struct _indirection_block {
  int block_ptr[4];
} indirection_block;

//indirection block
typedef struct _double_indirection_block {
  indirection_block* indirect_ptr[4];
} double_indirection_block;


//inode is 211 bytes
typedef struct _inode {
  int mode;             //can this file be read, written, executed (permissions)
  char state;             //directory or file
  char name[15];         //name of the file or directory
  unsigned int uid;      //id of the inode
  int size;              //size of the file this inode refers to
  short links_count;     //number of hard links
  char flag;             //flags
  time_t time;           // what time was this file last accessed?
  time_t ctime;          // what time was this file created?
  time_t mtime;          // what time was this file last modified?
  int ownerID;      //ID of owner
  int groupID;      //group ID of owner
  int block_ptr[10];    //4 numbers indicating its data blocks; if its a directory numbers reference its inodes
  indirection_block* block_ptr_indirect[2];    //2 single indirect pointer
  double_indirection_block* block_ptr_double_indirect;  //1 double indirection pointer
} inode;


void disk_open(const char* diskfile_path);
void disk_close();
int block_read(const int block_num, void *buf);
int block_write(const int block_num, const void *buf);

#endif
