#include "frame.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "vm/swap.h"

static struct list 			lru_list;
static struct lock			lru_lock;

static struct list_elem* gt_next_lru_clock();

void lru_list_init(void){
	list_init(&lru_list);
	lock_init(&lru_lock);
}

void add_page_to_lru_list(struct page* page){
	lock_acquire(&lru_lock);
	list_push_back(&lru_list, &page->lru);
	lock_release(&lru_lock);
}


void del_page_from_lru_list(struct page* page){
	lock_acquire(&lru_lock);
	list_remove(&page->lru);
	lock_release(&lru_lock);
}


//FIFO
struct list_elem* gt_next_lru_clock(){
	struct list_elem* 	e;

	e = list_begin(&lru_list);
	while(true)
	{
		struct page* page = list_entry(e, struct page, lru);
		struct thread* thread = page->thread;

		if(!page->vme->pinned){
			if(pagedir_is_accessed(thread->pagedir, page->vme->vaddr)){
				pagedir_set_accessed(thread->pagedir, page->vme->vaddr, false);
			}
			else{
				return page;
			}	
		}
			
		e = list_next(e);
		if(e == list_end(&lru_list)){
			e = list_begin(&lru_list);
		}
	}
	
	return NULL;
}


void try_to_free_pages(enum palloc_flags flags){
	struct page* victim = gt_next_lru_clock();
	if(victim == NULL){
		return;
	}

	free_page(victim);  
}