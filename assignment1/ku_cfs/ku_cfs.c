#include "ku_cfs.h"

int main(int argc, char *argv[]){
	int n[5];
	int i, j;
	char alpha = 'A';
	PCB *pcb = NULL;
	runningPCB = NULL;
	
	//////// signal handler
	struct sigaction sa;

	if(argc != 7){
		printf("ku_cfs:Wrong number of arguments\n");
		exit(1);
	}
	for(i=0;i<5;i++){
		n[i] = atoi(argv[i+1]);
	}
	ts = atoi(argv[6]);

	// ready queue init 	
	head = ready_q_init();
	
	for(i=0;i<5;i++){
		for(j=0;j<n[i];j++){
			/// input function
			pid_t pid;
			if((pid=fork())>0) { /// parent process running 
                                pcb = pcb_init(pid, i-2, alpha);
				// insert to ready queue
				ready_q_add(pcb);
			}
			else if(pid == 0) { // children process
				execl("./ku_app", "./ku_app", alpha, NULL);
				exit(0);
			}
			else {
				printf("fork is not abvailable\n");
			}
			alpha ++;
		}
	}

	sleep(5);
	struct itimerval timer;
	sa.sa_handler = sig_h;
	sa.sa_flags = SA_NOMASK;
	sigaction(SIGALRM, &sa, NULL);

	timer.it_value.tv_sec = 1;
	timer.it_value.tv_usec = 0;

	timer.it_interval.tv_sec = 1;
	timer.it_interval.tv_usec = 0;
	setitimer(ITIMER_REAL, &timer, NULL);	

	// from ready queue make to run first one
	int tmp_num = 1;
	while(ts){
		if(tmp_num){
			runningPCB = head->front;
			ready_q_delete(runningPCB);
			kill(runningPCB->pid,SIGCONT);
			tmp_num = 0;
		}
	}
	
	
		
	// itimer finish
	
	// running process go to ready queue SIGSTOP
	ready_q_add(runningPCB);
	kill(runningPCB->pid,SIGSTOP);
	/////////////////////// loop finish /// 
	tmp_num = 1;
	int wait=0;
	int state=0;
	while(tmp_num){
		// ready queue process SIGKILL //////loop
		kill(runningPCB->pid,SIGKILL);
		ready_q_delete(runningPCB);
		wait = waitpid(runningPCB->pid,&state,0);
		// free
		free(runningPCB);
		runningPCB = head->front;
		if(runningPCB == NULL){
			tmp_num=0;
		}
	}
	return 0;
}
