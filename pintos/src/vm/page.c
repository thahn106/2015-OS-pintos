#include "page.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/file.h"
#include <stdio.h>

static unsigned vm_hash_func (const struct hash_elem* e, void* aux UNUSED)
{
  struct vm_entry *vme = hash_entry (e, struct vm_entry, elem);
  return hash_int ((int)vme->vaddr);
}

static bool vm_less_func (const struct hash_elem* a, const struct hash_elem* b, void* aux UNUSED)
{
  struct vm_entry *vme_a, *vme_b;
  
  vme_a = hash_entry (a, struct vm_entry, elem);
  vme_b = hash_entry (b, struct vm_entry, elem);

  if (vme_a->vaddr < vme_b->vaddr)
    return true;
  else
    return false;
}

static void vm_destroy_func (struct hash_elem* e, void* aux UNUSED)
{
  struct vm_entry *vme = hash_entry (e, struct vm_entry, elem);

  if (vme)
  {
    if (vme->vaddr)
      palloc_free_page (vme->vaddr);
    free (vme);
  }
}

void vm_init (struct hash* vm)
{
  hash_init (vm, vm_hash_func, vm_less_func, NULL);
}

void vm_destroy (struct hash* vm)
{
  hash_destroy (vm, vm_destroy_func);
}

struct vm_entry* find_vme (void* vaddr)
{
  struct hash_elem* e = NULL;

  // temp vm_entry (saving addr to find element)
  struct vm_entry p;
  p.vaddr = pg_round_down (vaddr);

  // first argument: current thread's vm hash table
  e = hash_find (&thread_current()->vm, &p.elem);

  if (e != NULL)
    return hash_entry (e, struct vm_entry, elem);
  else
    return NULL;
}

bool insert_vme (struct hash* vm, struct vm_entry* vme)
{
  if (hash_insert (vm, vme) != NULL)
    return true;
  else
    return false;
}

bool delete_vme (struct hash* vm, struct vm_entry* vme)
{
  if (hash_delete (vm, vme) != NULL)
    return true;
  else
    return false;
}

void unpin_ptr (void* vaddr)
{
  struct vm_entry* vme = find_vme (vaddr);
  if (vme)
    vme->pinned = false;
}

void unpin_string (void* str)
{
  unsigned i;
  for (i = 0; i < strlen (str); ++i)
    unpin_ptr ((char*)str + i);
}

void unpin_buffer (void* buffer, unsigned size)
{
  unsigned i;
  for (i = 0; i < size; ++i)
    unpin_ptr ((char*)buffer + i);
}

bool load_file (void* kaddr, struct vm_entry *vme)
{
  file_read_at (vme->file, kaddr, vme->read_bytes, vme->offset); 
  memset (kaddr + vme->read_bytes, 0, vme->zero_bytes);
  return true;
}