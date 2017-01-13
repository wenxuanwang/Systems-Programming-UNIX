/*	
  Author: Wenxuan Wang
  Email:  wenxuan.wang@emory.edu
*/

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#define KEY (key_t)02162
#define BIT_MAP_SIZE 1048576
#define PERFECT_NUM_SIZE 20
#define PROCESSES_SIZE 20
#define SUMMARY_SIZE 3
#define SUMMARY_TESTED 0
#define SUMMARY_SKIPPED 1
#define SUMMARY_FOUND 2
#define REGISTER 1
#define UPDATE 2
#define SHUTDOWN 3
#define BIT_IN_INT (8*sizeof(int))

typedef struct process_info {
	long pid;
	long tested;
	long skipped;
	long found;
} p_info;

typedef struct shared_memory_segment{
	int bitmap[BIT_MAP_SIZE];
	long perfect[PERFECT_NUM_SIZE];
	p_info process_info[PROCESSES_SIZE];
	long summary[SUMMARY_SIZE];
} shm_seg;

typedef struct message{
	long mtype;
	pid_t pid;
	long data;
} msg;

int msqid;
int shmid;
int msgrcv_size;
int perfect_cnt;
int pinfo_index;
struct sigaction action;
msg *msg_buf;
shm_seg *seg;
sigset_t mask;

