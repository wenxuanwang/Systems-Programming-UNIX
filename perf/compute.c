/*	
  Author: Wenxuan Wang
  Email:  wenxuan.wang@emory.edu
*/

#include "hw4.h"

int perfect(long num) {
	long i,sum;
	sum = 1;
	for(i = 2; i < num;i++) 
		if(!(num%i)) 
			sum += i;
	return sum == num;
}

void handler(int signum) {
	// clean up entry and detach
	if(pinfo_index >= 0) 
		memset(&seg->process_info[pinfo_index],0,sizeof(p_info));
	shmdt(seg);
	exit(signum);
}

int main(int argc, char *argv[]) {
	long start;
	long i;
	pid_t ppid;
	p_info *info;
	msg *msg_buf;

	ppid = getpid();

	if(argc != 2) {
		printf("error: please specify the first number to test\n");
		exit(EXIT_FAILURE);
	}

	// get start pos
	start = atol(argv[1]);
	if(start < (long)2 || start > (long)33554432) {
		printf("number out of range");
		exit(EXIT_FAILURE);
	}

	if((msqid = msgget(KEY,0660)) == -1) {
		perror("manage missing");
		msgctl(msqid,IPC_RMID,NULL);
		exit(EXIT_FAILURE);
	}

	// register process
	msg *register_msg = (msg *)malloc(sizeof(msg));
	register_msg->mtype = REGISTER;
	register_msg->pid = ppid;
	register_msg->data = (long)1;

	if(msgsnd(msqid,register_msg,sizeof(msg),IPC_NOWAIT) == -1) {
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}

	// get process index
	if(msgrcv(msqid,register_msg,sizeof(msg),(long)ppid,0) == -1) {
		perror("msgrcv");
		exit(EXIT_FAILURE);
	}
	
	if((pinfo_index = register_msg->data) == -1) {
		printf("process table full\n");
		exit(EXIT_FAILURE);
	}

	if((shmid = shmget(KEY,sizeof(shm_seg),0660)) == -1) {
		perror("shmget");
		exit(EXIT_FAILURE);
	}

	// handle signals
	void handler();
	memset(&action,0,sizeof(struct sigaction));
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGQUIT);
	sigaddset(&mask, SIGHUP);

	action.sa_flags = 0;
	action.sa_mask = mask;
	action.sa_handler = handler;

	sigaction(SIGINT, &action, NULL);
	sigaction(SIGQUIT, &action, NULL);
	sigaction(SIGHUP, &action, NULL);

	if((seg = (shm_seg*)shmat(shmid,0,0)) == (shm_seg*)-1) {
		perror("shmat");
		exit(EXIT_FAILURE);
	}

	info = &seg->process_info[pinfo_index];
	msg_buf = (msg*) malloc(sizeof(msg));

	for(i = start; i < (long)33554433; i++) {
		// check in the bitmap
		long pos = i - 2;
		long bit_pos = pos % BIT_IN_INT;
		long num_pos = pos / BIT_IN_INT;

		if(seg->bitmap[num_pos] & (1 << bit_pos)) {
			info->skipped += 1;
			seg->summary[SUMMARY_SKIPPED] += 1;
			continue;
		}

		// set bit
		seg->bitmap[num_pos] |= (1 << bit_pos);		

		if(perfect(i)) {
			// modify proc
			msg_buf -> mtype = UPDATE;
			msg_buf -> pid = ppid;
			msg_buf -> data = i;
			
			if(msgsnd(msqid,msg_buf,sizeof(msg),IPC_NOWAIT) == -1) {
				perror("msgsnd");
				exit(EXIT_FAILURE);
			}
			
			info->found += 1;
			seg->summary[SUMMARY_FOUND] += 1;
		}

		info->tested += 1;
		seg->summary[SUMMARY_TESTED] += 1;
	}	
	handler(0);
}
