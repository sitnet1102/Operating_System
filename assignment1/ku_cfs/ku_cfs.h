#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <pthread.h>

///////// struct ////////
typedef struct _PCB {
        pid_t pid;
        float vruntime;
        int nice;
        char alpha;
        struct _PCB *next;
} PCB;

typedef struct _Ready_Queue {
	PCB *front;
	PCB *rear;
	int size;
} Ready_Queue;

///////// static variable /////////
static float nicevalue[5] = {0.64, 0.8, 1, 1.25, 1.5625};

static PCB* runningPCB;
static Ready_Queue* head;
static int ts=0;
////////function pre declare///////
Ready_Queue* ready_q_init();
PCB* pcb_init(pid_t pid, int nice_num, char alpha);
int ready_q_add(PCB *pcb);
int ready_q_delete(PCB* pcb);
int print_f();
void vt_c(PCB* pcb);
void sig_h();


//////// function start///////
Ready_Queue* ready_q_init(){
	Ready_Queue* rq = (Ready_Queue *)malloc(sizeof(Ready_Queue));
	if(rq == NULL) {
		printf("Queue is not available\n");
	}
	rq->size = 0;
	return rq;
}
PCB* pcb_init(pid_t pid, int nice_num, char alpha){
	PCB* pcb = (PCB *)malloc(sizeof(PCB));
	if(pcb == NULL) {
		printf("pcb is not available\n");
	}
	pcb->pid = pid;
	pcb->vruntime = 0;
	pcb->nice = nice_num;
	pcb->alpha = alpha;
	pcb->next = NULL;
	return pcb;
}
int ready_q_add(PCB *pcb){
	int option = 0;
	PCB *prepcb = NULL;
	PCB *thispcb = NULL;
	
	if((head->size) == 0){
		option = 4;
	}else{
		thispcb = head->front;
		while(1){
			if((thispcb->vruntime)>=(pcb->vruntime)){
				if(prepcb == NULL){
					option = 1;
					break;
				}else{
					if((thispcb->next) == NULL){
						option = 3;
						break;
					}else{
						option = 2;
						break;
					}
				}
			}
			else{
				if((thispcb->next) == NULL){
					option = 3;
					break;
				}
				prepcb = thispcb;
                                thispcb = thispcb->next;
			}
		}
	}
	switch (option) {
		case 1 :
			head->front = pcb;
			pcb->next = thispcb;
			break;
		case 2 :
			pcb->next = thispcb;
			prepcb->next = pcb;
			break;
		case 3 :
			prepcb->next = pcb;
			head->rear = pcb;
			break;
		case 4 :
			head->front = pcb;
	                head->rear = pcb;
			break;
		default : 
			printf("Ready_Queue_add Error!\n");
	}
	head->size ++;

	return 0;
}
int ready_q_delete(PCB* pcb){
	int option = 0;
        PCB *prepcb = NULL;
        PCB *thispcb = NULL;

        if((head->size) == 0){
                option = 4;
        }else{
                thispcb = head->front;
                while(1){
                        if(pcb == thispcb){
                                if((head->size) == 1){
					option = 5;
					break;
				}else{
					if((head->front) == pcb){
						option = 1;
						break;
					}else{
						if((head->rear) == pcb){
							option = 3;
							break;
						}else{
							option = 2;
							break;
						}
					}
				}					
			}
                        else{
				prepcb = thispcb;
                                thispcb = thispcb->next;
				if((thispcb->next) == NULL){
					break;
				}
                        }
                }
        }
        switch (option) {
                case 1 :
			//front node
                        head->front = pcb->next;
			pcb->next = NULL;
                        break;
                case 2 :
			//middle node
			prepcb->next = pcb->next;
			pcb->next = NULL;
                        break;
                case 3 :
			//rear node
                        head->rear = prepcb;
			pcb->next = NULL;
                        break;
                case 4 :
			// nothing to delete 
                        printf("There is nothing to delete in Ready_Queue\n");
			break;
		case 5 :
			// node just one
			head->front = NULL;
                        head->rear = NULL;
			pcb->next = NULL;
			break;
                default :
			// my node is not inside
                        printf("Ready_Queue_delete Error!\n");
        }
        head->size --;


	return 0;
}

int print_f(){
	PCB* pcb = head->front;
	if(pcb == NULL){
		return 0;
	}
	int i=0;
	while(1){
		printf("%d\n",i);
		printf("pid = [%d]\n", pcb->pid);
		printf("vruntime = [%f]\n", pcb->vruntime);
		printf("nice = [%d]\n", pcb->nice);
		printf("alpha = [%c]\n\n", pcb->alpha);
		pcb = pcb->next;

		
		if(pcb == NULL){
			break;
		}
		i++;
		
	}
	printf("size = [%d]\n", head->size);

	return 0;
}
void vt_c(PCB* pcb){// vruntime calculation
	float vt_tmp = pcb->vruntime;
	int nice_tmp = pcb->nice;
	float deltaexec = 1.0;

	vt_tmp = vt_tmp + deltaexec * nicevalue[nice_tmp + 2];

	pcb->vruntime = vt_tmp;
}
void sig_h(){
	kill(runningPCB->pid,SIGSTOP);
	vt_c(runningPCB);
	ready_q_add(runningPCB);
	runningPCB = head->front;
	ready_q_delete(runningPCB);
	kill(runningPCB->pid,SIGCONT);
        ts--;
}

