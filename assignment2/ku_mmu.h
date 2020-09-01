#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// define
#define KU_H_PDMASK 	192 	// 11000000
#define KU_H_PDSHIFT 	6
#define	KU_H_PMDMASK 	48 	// 00110000
#define	KU_H_PMDSHIFT 	4
#define	KU_H_PTMASK 	12 	// 00001100
#define KU_H_PTSHIFT	2
#define KU_H_PTEMASK	252	// 11111100
#define KU_H_PTESHIFT	2	
#define KU_H_SWAPSHIFT 	1
#define KU_H_UNUSEMASK	254	// 11111110
#define KU_H_PRESENT	1	// 00000001

// structure
struct ku_pte{	// page table entry
	char pte;
};

struct ku_page{ // page unit
	struct ku_pte pte[4];
};

struct ku_free_node{	// free list node
	int first_num;	// first free pfn
	int last_num;	// last free pfn
	struct ku_free_node* next;
};

struct ku_free_list{	// free list
	unsigned int size;	// number of free list page
 	struct ku_free_node* head;      	
};

struct ku_pdbr{	// page directory base register
	char pid;
	struct ku_pte* pdbr;
	struct ku_pdbr* next;
};

struct ku_node{	// node
	char pid;
	char va;
	struct ku_node* next;
};

struct ku_queue{ // queue
 	struct ku_node* head;
	struct ku_node* last;
	int num;	
};




// static variable
static unsigned int ku_mmu_mem_size;
static unsigned int ku_mmu_swap_size;
static struct ku_pte* ku_mmu_physical_memory;
static struct ku_page* ku_mmu_swap_space;
static struct ku_free_list* ku_mmu_free_list;
static struct ku_free_list* ku_mmu_swap_free_list;
static struct ku_pdbr* ku_mmu_pdbr_list_head;
static struct ku_queue* ku_mmu_queue_head;

// function
struct ku_node* ku_mmu_queue_init(void);
void ku_mmu_enqueue(char pid, char va);
int ku_mmu_dequeue(char* pid, char* va);

struct ku_free_node* ku_mmu_free_node_init(int first, int last);
int ku_mmu_unfree_free_list(struct ku_free_list* free_list);
void ku_mmu_organize_free_list(struct ku_free_list* free_list);
int ku_mmu_refree_free_list(struct ku_free_list* free_list, int num);

struct ku_pte* ku_mmu_get_pdbr(char pid);
int ku_mmu_alloc_page(void);

void *ku_mmu_init(unsigned int mem_size, unsigned int swap_size);
int ku_page_fault(char pid, char va);
int ku_run_proc(char pid, struct ku_pte **ku_cr3);
// 	queue functions
struct ku_node* ku_mmu_queue_init(void){
	struct ku_node* ret = (struct ku_node*) malloc(sizeof(struct ku_node));
	ret->pid = 0;
	ret->va = 0;
	ret->next = NULL;
	return ret;
}

void ku_mmu_enqueue(char pid, char va){
	struct ku_node* tmp = ku_mmu_queue_init();
	tmp->pid = pid;
	tmp->va = va;
	ku_mmu_queue_head->last->next = tmp;
	ku_mmu_queue_head->num ++;
}

int ku_mmu_dequeue(char* pid, char* va){
	struct ku_node* tmp = ku_mmu_queue_head->head->next;
	if(ku_mmu_queue_head->num == 0) {
		printf("queue is empty\n");
		return -1;
	} 
	*pid = tmp->pid;
        *va = tmp->va;
	ku_mmu_queue_head->head = tmp;
	tmp->va = 0;
	ku_mmu_queue_head->num --;

	return 0;
}

//	free list functions
struct ku_free_node* ku_mmu_free_node_init(int first, int last){
        struct ku_free_node* ret = (struct ku_free_node*) malloc(sizeof(struct ku_free_node));
        ret->first_num = first;
        ret->last_num = last;
        ret->next = NULL;
        return ret;
}


int ku_mmu_unfree_free_list(struct ku_free_list* free_list){
        struct ku_free_node* tmp = free_list->head;
	int ret = 0;
	if(free_list->size == 0) {
		printf("free list is empty\n");
		return ret;	// fail return 0
	}
	ret = tmp->first_num;
	tmp->first_num ++;
	free_list->size --;
	if(tmp->first_num > tmp->last_num){
		free_list->head = tmp->next;
		free(tmp);
	}
	return ret;	// success return index
}

void ku_mmu_organize_free_list(struct ku_free_list* free_list){
        struct ku_free_node* tmp = free_list->head;
	struct ku_free_node* pos;
	while(tmp != NULL){
		pos = tmp->next;
		if(pos == NULL){
			break;
		}
		if((tmp->last_num + 1) == pos->first_num){
			tmp->last_num = pos->first_num;
			tmp->next = pos->next;
			free(pos);
		} else {
			tmp = pos;
		}
	}
}

int ku_mmu_refree_free_list(struct ku_free_list* free_list, int num){
	struct ku_free_node* tmp = free_list->head;
	struct ku_free_node* pos;
	struct ku_free_node* ret = ku_mmu_free_node_init(num, num);
	int check = 0;
	if((free_list->size == 0)||(tmp==NULL)){
		check = 1;
	}
	while(!check){
		pos = tmp->next;
		if(pos == NULL){
			break;
		}
		if(pos->first_num > num){
			break;
		}
		tmp = pos;
	}
	if(tmp == free_list->head){
		check = 1;
	}
	if(check){
		free_list->head = ret;
                free_list->size ++;
		ret->next = tmp;
                return 0;
	}
	tmp->next = ret;
	ret->next = pos;

	return 0;
}

//	another functions
struct ku_pte* ku_mmu_get_pdbr(char pid){
	struct ku_pdbr* tmp = ku_mmu_pdbr_list_head;
	struct ku_pte* ret = NULL;
	while(tmp != NULL){
		if(tmp->pid == pid){
			ret = tmp->pdbr;
			break;
		}
		tmp = tmp->next;
	}

	return ret;
}

int ku_mmu_alloc_page(void){
	int index = ku_mmu_unfree_free_list(ku_mmu_free_list);
	char tmp_pid, tmp_va;
	char tmp_char = 0;
	int tmp_int = 0;
	struct ku_pte* tmp = NULL;
	struct ku_pte* tmp_pdbr = NULL;
	struct ku_pte* tmp_pd = NULL;
	struct ku_pte* tmp_pmd = NULL;
	struct ku_pte* tmp_pt = NULL;
	if(!index){
		if(ku_mmu_dequeue(&tmp_pid, &tmp_va) < 0){
			return -1;	
			// fail		
			// queue is empty
			// because in this case has no free list and queue also
			// it means there is no page on the physical memory 
			// all space is filled with pd, pmd, pt, not page
			// so we cannot use swap
		}
		tmp_pdbr = ku_mmu_get_pdbr(tmp_pid);
		// calculate 
		// pd
		tmp_char = (tmp_va & KU_H_PDMASK) >> KU_H_PDSHIFT;
		tmp_int = (int)tmp_char;
		tmp_pd = tmp_pdbr + (sizeof(struct ku_pte) * tmp_int);
		// pmd
		tmp_char = (tmp_pd->pte & KU_H_PTEMASK) >> KU_H_PTESHIFT;
		tmp_int = (int)tmp_char;
		tmp_pmd = ku_mmu_physical_memory + (sizeof(struct ku_page) * tmp_int);
		tmp_char = (tmp_va & KU_H_PMDMASK) >> KU_H_PMDSHIFT;
		tmp_int = (int)tmp_char;
		tmp_pmd = tmp_pmd + (sizeof(struct ku_pte) * tmp_int);
		// pt
                tmp_char = (tmp_pmd->pte & KU_H_PTEMASK) >> KU_H_PTESHIFT;
                tmp_int = (int)tmp_char;
                tmp_pt = ku_mmu_physical_memory + (sizeof(struct ku_page) * tmp_int);
                tmp_char = (tmp_va & KU_H_PTMASK) >> KU_H_PTSHIFT;
                tmp_int = (int)tmp_char;
                tmp_pt = tmp_pt + (sizeof(struct ku_pte) * tmp_int);
		// page
                tmp_char = (tmp_pt->pte & KU_H_PTEMASK) >> KU_H_PTESHIFT;
                tmp_int = (int)tmp_char;
                tmp = ku_mmu_physical_memory + (sizeof(struct ku_page) * tmp_int);
               	
		// checking that swap space is available
		if(!ku_mmu_unfree_free_list(ku_mmu_swap_free_list)){
			// fail
			// swap space is full
			return -1;
		}

		// change pte
		tmp_int = ku_mmu_unfree_free_list(ku_mmu_swap_free_list);
		tmp_pt->pte = tmp_int << KU_H_SWAPSHIFT;
		tmp_pmd->pte = tmp_pmd->pte & KU_H_UNUSEMASK;
		tmp_pd->pte = tmp_pd->pte & KU_H_UNUSEMASK;
		
		// realloc to free list
		ku_mmu_refree_free_list(ku_mmu_free_list, tmp_int);
		ku_mmu_organize_free_list(ku_mmu_free_list);

		index = tmp_int;
	}
	

	return index;

}


//	main functions
void *ku_mmu_init(unsigned int mem_size, unsigned int swap_size) {
	// allocate memory
	ku_mmu_mem_size = mem_size;
	ku_mmu_swap_size = swap_size;
	//ku_mmu_swap_current_index = 1;	// 0 is unused
	ku_mmu_physical_memory = (struct ku_pte*) malloc(sizeof(struct ku_pte)*mem_size);
	ku_mmu_swap_space = (struct ku_page*) malloc(sizeof(struct ku_pte)*swap_size);	
	ku_mmu_free_list = (struct ku_free_list*) malloc(sizeof(struct ku_free_list));
	ku_mmu_swap_free_list = (struct ku_free_list*) malloc(sizeof(struct ku_free_list));
	ku_mmu_pdbr_list_head = (struct ku_pdbr*) malloc(sizeof(struct ku_pdbr));
	ku_mmu_queue_head = (struct ku_queue*) malloc(sizeof(struct ku_queue));
	
	// queue initialization
	// input node to the queue x queue to use for init node 
	// x node is not to be used for a queue 
	struct ku_node* tmp = ku_mmu_queue_init();
	ku_mmu_queue_head->head = tmp;
	ku_mmu_queue_head->last = tmp;
	ku_mmu_queue_head->num = 0;
	
	// pdbr_list initialization
	ku_mmu_pdbr_list_head->pid = 0;
	ku_mmu_pdbr_list_head->pdbr = NULL;
	ku_mmu_pdbr_list_head->next = NULL;

	// free list initialization
	// first pfn is used for os
	if(mem_size > 8) {
		ku_mmu_free_list->size = (mem_size/4) - 1;
		ku_mmu_free_list->head = ku_mmu_free_node_init(1, (mem_size/4) - 1);
	} else {
		printf("mem_size is too small\n");
		return 0;
	}	
	// swap free list initialization
	//
	if(swap_size > 8) {
		ku_mmu_swap_free_list->size = (swap_size/4) - 1;
                ku_mmu_swap_free_list->head = ku_mmu_free_node_init(1, (swap_size/4) - 1);
        } else {
                printf("swap_space is too small\n");
                return 0;
        }

	// swap space and physical memory initialization
	for(unsigned int i = 0; i<mem_size; i++){
		memset(&ku_mmu_physical_memory[i], 0, sizeof(struct ku_pte));
	}
	for(unsigned int i = 0; i<swap_size; i++){
		memset(&ku_mmu_swap_space[i], 0, sizeof(struct ku_page));
	}
	// pa start address
	return ku_mmu_physical_memory;
}

int ku_page_fault(char pid, char va){
        char tmp_char = 0;
        int tmp_int = 0;
	int index;
        struct ku_pte* tmp = NULL;
        struct ku_pte* tmp_pdbr = NULL;
        struct ku_pte* tmp_pd = NULL;
        struct ku_pte* tmp_pmd = NULL;
        struct ku_pte* tmp_pt = NULL;
	
	tmp_pdbr = ku_mmu_get_pdbr(pid);
        // calculate
        // pd
        tmp_char = (va & KU_H_PDMASK) >> KU_H_PDSHIFT;
        tmp_int = (int)tmp_char;
        tmp_pd = tmp_pdbr + (sizeof(struct ku_pte) * tmp_int);
	if(tmp_pd->pte == 00000000){
                index = ku_mmu_alloc_page();
                if(index > 0){
                        tmp_int = index << KU_H_PTESHIFT;
                        tmp_int = + 1;
                        tmp_pd->pte = (char)tmp_int;
                }else{
                        return -1;      // fail
                }
        }


        // pmd
        tmp_char = (tmp_pd->pte & KU_H_PTEMASK) >> KU_H_PTESHIFT;
        tmp_int = (int)tmp_char;
        tmp_pmd = ku_mmu_physical_memory + (sizeof(struct ku_page) * tmp_int);
        tmp_char = (va & KU_H_PMDMASK) >> KU_H_PMDSHIFT;
        tmp_int = (int)tmp_char;
	tmp_pmd = tmp_pmd + (sizeof(struct ku_pte) * tmp_int);
	if(tmp_pmd->pte == 00000000){
                index = ku_mmu_alloc_page();
		if(index > 0){
			tmp_int = index << KU_H_PTESHIFT;
			tmp_int = + 1;
			tmp_pmd->pte = (char)tmp_int;
		}else{
			return -1;	// fail
		}
        }

        // pt
        tmp_char = (tmp_pmd->pte & KU_H_PTEMASK) >> KU_H_PTESHIFT;
        tmp_int = (int)tmp_char;
        tmp_pt = ku_mmu_physical_memory + (sizeof(struct ku_page) * tmp_int);
        tmp_char = (va & KU_H_PTMASK) >> KU_H_PTSHIFT;
        tmp_int = (int)tmp_char;
        tmp_pt = tmp_pt + (sizeof(struct ku_pte) * tmp_int);
	if(tmp_pt->pte == 00000000){
                index = ku_mmu_alloc_page();
                if(index > 0){
                        tmp_int = index << KU_H_PTESHIFT;
                        tmp_int = + 1;
                        tmp_pt->pte = (char)tmp_int;
			ku_mmu_enqueue(pid, va);
			return 0;	// success
                }else{
                        return -1;      // fail
                }
        }else{
		tmp_char = tmp_pt->pte & KU_H_PRESENT;
		tmp_int = (int)tmp_char;
		if(!tmp_int){
			tmp_char = (tmp_pt->pte & KU_H_UNUSEMASK) >> KU_H_SWAPSHIFT;
			tmp_int = (int)tmp_char;
			ku_mmu_refree_free_list(ku_mmu_swap_free_list, tmp_int);
			ku_mmu_organize_free_list(ku_mmu_swap_free_list);
			return 0;	// success
		}else {
			return -1;	// fail
		}
	}
}

int ku_run_proc(char pid, struct ku_pte **ku_cr3){
	struct ku_pte* tmp;
	struct ku_pdbr* tmp_pdbr = NULL;
	int ret = 0, pfn = 0;
	
	tmp = ku_mmu_get_pdbr(pid);
	if(tmp == NULL){
		tmp_pdbr = (struct ku_pdbr*) malloc(sizeof(struct ku_pdbr));
		tmp_pdbr->pid = pid;
		tmp_pdbr->next = ku_mmu_pdbr_list_head;
		ku_mmu_pdbr_list_head = tmp_pdbr;
		pfn = ku_mmu_alloc_page();
		if(pfn > 0){
			tmp_pdbr->pdbr = ku_mmu_physical_memory + (sizeof(struct ku_page)*pfn);
			tmp = tmp_pdbr->pdbr;
		}else{
			ret = -1;
		}
	}
	
	
	*ku_cr3 = tmp;
	
	return ret;
}


