/*	
  Author: Wenxuan Wang
  Email:  wenxuan.wang@emory.edu
*/

#include "hw4.h"

int main(int argc, char *argv[]) {
	int i;
	pid_t ppid;
	p_info tmp;

	// get shared memory
	if((shmid = shmget(KEY,sizeof(shm_seg),0660)) == -1) {
		perror("no manage");
		exit(EXIT_FAILURE);
	}

	if((seg =(shm_seg*)shmat(shmid,0, 0)) == (shm_seg*) -1) {
		perror("shmat");
		exit(EXIT_FAILURE);						
	}


	// print perfect numbers if any
	printf("Pefect numbers:\n");
	for(i = 0; i < PERFECT_NUM_SIZE; i++) {
		if(!seg->perfect[i])	
			break;
		printf("%-10ld",seg->perfect[i]);
	}

	// print process table if any
	printf("\n\nProcess table:\n");
	for(i = 0; i < PROCESSES_SIZE; i++) {
		tmp = seg -> process_info[i];
		if(tmp.pid != 0) 
			printf("pid:%-10ld tested:%-10ld skipped:%-10ld found:%-10ld\n",tmp.pid,tmp.tested,tmp.skipped,tmp.found);
	}

	// print summary info
	printf("\nSummary:\n");
	printf("TESTED:%-10ld SKIPPED:%-10ld FOUND:%-10ld\n\n",seg->summary[SUMMARY_TESTED],seg->summary[SUMMARY_SKIPPED],seg->summary[SUMMARY_FOUND]);

	if((msqid = msgget(KEY, 0660)) == -1) {
		perror("msgget");
		exit(EXIT_FAILURE);
	}

	ppid = getpid();

	if(argc == 2) {
		if((argv[1][0] == '-') && (argv[1][1] == 'k')) {
			// request for manage pid			
			msg *msg_buf = (msg*) malloc(sizeof(msg));
			msg_buf -> mtype = (long) SHUTDOWN;
			msg_buf -> pid = ppid;
			msg_buf -> data = 1;

			if(msgsnd(msqid,msg_buf,sizeof(msg),IPC_NOWAIT) == -1) {
				perror("getting manage pid");
				exit(EXIT_FAILURE);
			}
			// get manage pid
			if(msgrcv(msqid,msg_buf,sizeof(msg),(long)ppid,0) == -1) {
				perror("manage pid");
				exit(EXIT_FAILURE);
			}
			// send signal
			kill(msg_buf -> pid,SIGINT);
		}
	}
	return 0;
}
