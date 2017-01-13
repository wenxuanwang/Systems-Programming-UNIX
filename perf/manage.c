/*	
  Author: Wenxuan Wang
  Email:  wenxuan.wang@emory.edu
*/

#include "hw4.h"

int insert_perfectnum(long *list, long num) {
	int i;
	long tmp;
	// a single round of bubble sort
	for(i = 0; i < PERFECT_NUM_SIZE;i++) {
		// quit if it is 0
		if(!list[i])
			break;
		// insert and move forward
		if(num < list[i]) {
			tmp = list[i];
			list[i] = num;
			num = tmp;
		}
	}
	// max number
	list[i] = num;
	return 1;
}

int find_empty_process_entry(msg *msg_buf) {
	int i;
	p_info *proc;
	for(i = 0; i < PROCESSES_SIZE; i++) {
		proc = (p_info*) &seg -> process_info[i];
		// if unused
		if(proc -> pid == 0) {
			// initialize
			memset(proc,0,sizeof(p_info));
			proc -> pid = msg_buf -> pid;
			return i;
		}
	}
	return -1;
}

int register_compute(msg *msg_buf) {
	int pos =  find_empty_process_entry(msg_buf);
	seg->process_info[pos].pid = msg_buf -> pid;

	// report process index
	msg_buf -> mtype = (long) msg_buf -> pid;
	msg_buf -> pid = getpid();
	msg_buf -> data = pos;
	if(msgsnd(msqid,msg_buf,sizeof(msg),IPC_NOWAIT) == -1) {
		perror("msgsnd");
		exit(EXIT_FAILURE);
	}
	return 1;
}


int update_perfect_num(msg *msg_buf){
	// insert if valid
	//printf("updating -----> type:%ld    pid:%d   data:%ld\n", 
	//			msg_buf->mtype,(int)msg_buf->pid, msg_buf->data);
	long num_added = msg_buf -> data;
	insert_perfectnum(seg->perfect,num_added);
	return 1;
}

void handler(int signum) {
	int i;
	p_info info;
	
	// flush message queue
	while(msgrcv(msqid,msg_buf,sizeof(msg),0,IPC_NOWAIT) > 0);
	// send signals to computes	
	for(i = 0; i < PROCESSES_SIZE; i++) {
		info = seg->process_info[i];
		if(info.pid > 0) {
			if(kill(info.pid,SIGINT) == -1) {
				perror("send signal");
				exit(EXIT_FAILURE);			
			}
		}
	}

	sleep(5);
	
	// clean msq,shm
	msgctl(msqid,IPC_RMID,NULL);
	memset(seg, 0, sizeof(shm_seg));
	shmdt(seg);
	shmctl(shmid,IPC_RMID,NULL);
	exit(signum);
}

int main(int argc, char *argv[]) {
	int i;
	pid_t report_pid;

	perfect_cnt = 0;

	// build msg queue, remove existed if any
	if((msqid = msgget(KEY, IPC_CREAT|IPC_EXCL|0660)) == -1) {
		if(errno == EEXIST) {
			printf("manage existed");
			exit(EXIT_FAILURE);
		}
		else {
			perror("msgget");
			exit(EXIT_FAILURE);
		}
	}

	// get shared memory
	if((shmid = shmget(KEY,sizeof(shm_seg),IPC_CREAT|0660)) == -1) {
		msgctl(KEY,IPC_RMID,NULL);
		perror("shmget");
		exit(EXIT_FAILURE);
	}

	if((seg =(shm_seg*)shmat(shmid,0, 0)) == (shm_seg*) -1) {
		shmctl(KEY,IPC_RMID,NULL);
		msgctl(KEY,IPC_RMID,NULL);
		perror("shmat");
		exit(EXIT_FAILURE);						
	}
	
	// clean up shared memory
	memset(seg->bitmap,0,BIT_MAP_SIZE);
	memset(seg->perfect,0,PERFECT_NUM_SIZE);
	memset(seg->summary,0,SUMMARY_SIZE);
	for(i = 0; i < PROCESSES_SIZE; i++) {
		memset(&seg->process_info[i],0,sizeof(p_info));
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

	msg_buf = (msg*) malloc(sizeof(msg));

	// keep getting message
	while(msgrcv(msqid,msg_buf, sizeof(msg),-3,0) > 0) {
		if(perfect_cnt > PERFECT_NUM_SIZE)
			break;
		
		switch(msg_buf->mtype) {
			case REGISTER:
				// registering compute 
				register_compute(msg_buf);
				break;
			case UPDATE:
				// update perfect number if feasible
				if(perfect_cnt <= 19){			
					update_perfect_num(msg_buf);
					perfect_cnt += 1;
				}
				else{
					printf("perfect number full");
					handler(0);			
				}
				break;
			case SHUTDOWN:
				// report manage pid to report
				report_pid = msg_buf -> pid;
				msg_buf -> mtype = (long)report_pid;
				msg_buf -> pid = getpid();
				msg_buf -> data = 1;
				if(msgsnd(msqid,msg_buf,sizeof(msg),IPC_NOWAIT) == -1) {
					perror("report manage pid");
					exit(EXIT_FAILURE);
				}
				break;
		}

	}
	handler(0);
}
