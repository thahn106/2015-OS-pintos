#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/synch.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;

struct lock lock;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();

  thread_current()->dir = dir_open_root();

  lock_init(&lock);
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  char filename[NAME_MAX+1];
  struct dir *dir = parse_path(name, filename);
  // printf("open : filename(%s), sector(%u)\n", filename, inode_get_inumber(dir_get_inode(dir)));
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, 0)
                  && dir_add (dir, filename, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

bool filesys_create_dir (const char *name){
  char filename[NAME_MAX+1];
  block_sector_t inode_sector = 0;
  struct dir *dir = parse_path(name, filename);
  struct inode* inode;

  if(dir_lookup(dir, filename, &inode)==true){
    return false;
  }

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create(inode_sector, 16)
                  && dir_add (dir, filename, inode_sector));

  if(success){
    struct dir *newdir = dir_open(inode_open(inode_sector));
    dir_add(newdir, ".", inode_get_inumber(dir_get_inode(newdir)));
    dir_add(newdir, "..", inode_get_inumber(dir_get_inode(dir)));
  }

  return true;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char filename[NAME_MAX+1];
  struct dir *dir = parse_path(name, filename);

  struct inode *inode = NULL;
  struct file* file = NULL;

  lock_acquire(&lock);

  if (dir != NULL)
    dir_lookup (dir, filename, &inode);
  dir_close (dir);

  file = file_open (inode);

  lock_release(&lock);

  return file;
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char filename[NAME_MAX+1];
  struct dir *dir = parse_path(name, filename);
  struct inode* inode;
  bool success = false;
  if(dir && dir_lookup(dir, filename, &inode)){
    if(!inode_is_dir(inode)){
      success = dir != NULL && dir_remove (dir, filename);
    }
    else{
      struct dir* dir2 = dir_open(inode);
      if(!dir_haschild (dir2)){
        success = dir != NULL && dir_remove (dir, filename);
      }
    }
    dir_close (dir); 
  }

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  struct dir* root;
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  root = dir_open(inode_open(ROOT_DIR_SECTOR));
  dir_add(root, ".", ROOT_DIR_SECTOR);
  dir_add(root, "..", ROOT_DIR_SECTOR);
  free_map_close ();
  dir_close(root);
  printf ("done.\n");
}

struct dir* parse_path(char* path_name, char* file_name){
  struct dir* dir = NULL;
  struct inode* inode;
  char token[NAME_MAX+1];
  char *nextToken = NULL, *savePtr;
  char* path_name_cp;
  int length = 0;
  path_name_cp = (char*)malloc(strlen(path_name)+1);
  if(path_name == NULL || path_name_cp == NULL || strlen(path_name) == 0){
    free(path_name_cp);
    return NULL;
  }
  memcpy(path_name_cp, path_name, strlen(path_name)+1);

  // printf("parse_path : %s\n", path_name);
  // debug_backtrace();

  nextToken = strtok_r(path_name_cp, "/", &savePtr);
  if(nextToken == NULL){
    dir = dir_open_root();
    strlcpy(token, ".", NAME_MAX+1);
    length = 1;
  }
  else{
    strlcpy(token, nextToken, NAME_MAX+1);
    length = strlen(nextToken);
    dir = dir_reopen(thread_current()->dir);
    nextToken = strtok_r(NULL, "/", &savePtr);
  }
  
  while(nextToken!=NULL || file_name==NULL){
    dir_lookup (dir, token, &inode);

    if(inode == NULL || !inode_is_dir(inode)){
      dir_close (dir);
      free(path_name_cp);
      return NULL;
    }
    else{
      dir_close (dir);
      dir = dir_open(inode);
    }

    if(nextToken){
      strlcpy(token, nextToken, NAME_MAX+1);
      length = strlen(nextToken);
      nextToken = strtok_r(NULL, "/", &savePtr);
    }
    else{
      break;
    }
  }

  if(file_name){
    strlcpy(file_name, token, NAME_MAX+1);
  }

  ASSERT(dir);
  free(path_name_cp);

  if(length>NAME_MAX){
    dir_close(dir);
    dir = NULL;
  }

  return dir;
}