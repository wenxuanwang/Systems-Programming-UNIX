/*	
  Author: Wenxuan Wang
  Email:  wenxuan.wang@emory.edu
*/

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

int main(int argc,char *argv[]) {
	int i;
	int suppress_pid;
	int wstatus;
	int sort_pid[2];
	int sort_pipe[2][2];
	int suppress_pipe[2][2];

	for(i = 0; i < 2; i++) {
		if((pipe(sort_pipe[i]) == -1) || (pipe(suppress_pipe[i]) == -1)) {
			perror("pipe");
			exit(EXIT_FAILURE);
		}
	}

	for(i = 0; i < 2; i++) {
		if((sort_pid[i] = fork()) == -1) {
			perror("fork");
			exit(EXIT_FAILURE);
		}
	}


	// sorting processes
	for(i = 0; i < 2; i++) {
		if(!sort_pid[i]) {			
			int j;
			
			for(j = 0; j < 2; j++) {
				close(sort_pipe[j][1]);
				close(suppress_pipe[j][0]);
				if(j != i) {
					close(sort_pipe[j][0]);
					close(suppress_pipe[j][1]);
				}
			}
			
			dup2(sort_pipe[i][0],STDIN_FILENO);
			dup2(suppress_pipe[i][1],STDOUT_FILENO);
			close(sort_pipe[i][0]);
			close(suppress_pipe[i][1]);
			execl("/bin/sort","sort",(char*)NULL);
			exit(EXIT_FAILURE);
		}
	}

	if((suppress_pid = fork()) == -1) {
		perror("fork");
		exit(EXIT_FAILURE);
	}

	// suppress processes
	if(!suppress_pid) {		
		FILE* stream1;
		FILE* stream2;
		int multiplicity;
		int isEnd1;
		int isEnd2;
		int buffer_empty1;
		int buffer_empty2;
		int add_from;
		char old_word[50];
		char add_word[50];
		char new_word1[50];
		char new_word2[50];

		add_from = 0;
		isEnd1 = 0;
		isEnd2 = 0;
		buffer_empty1 = 0;
		buffer_empty2 = 0;

		for(i = 0 ; i < 2; i++) {
			close(sort_pipe[i][0]);
			close(sort_pipe[i][1]);
			close(suppress_pipe[i][1]);
		}

		stream1 = fdopen(suppress_pipe[0][0],"r");
		stream2 = fdopen(suppress_pipe[1][0],"r");

		// mergesort
		if(fgets(new_word1,50,stream1) == NULL)
			isEnd1 = 1;
		if(fgets(new_word2,50,stream2) == NULL)
			isEnd2 = 1;
		// both initially empty
		if(isEnd1 && isEnd2) 
			exit(EXIT_FAILURE);
		// alternate, single pipe empty
		if(isEnd2) {
			printf("%5d %s",1,new_word1);
			exit(EXIT_SUCCESS);
		}

		if(strcmp(new_word1,new_word2) < 0) {
			add_from = 1;
			strcpy(old_word,new_word1);
			memset(new_word1,0,sizeof(new_word1));
		}
		else {
			add_from = 2;
			strcpy(old_word,new_word2);
			memset(new_word2,0,sizeof(new_word2));
		}

		multiplicity = 1;

		while(1) {
			if(buffer_empty1 & buffer_empty2) 
				break;
			if(add_from == 1) {
				if(fgets(new_word1,50,stream1) == NULL) {
					isEnd1 = 1;
					buffer_empty1 = 1;
					// add from 2
					strcpy(add_word,new_word2);
					memset(new_word2,0,sizeof(new_word2));
					add_from = 2;
				}
				else{
					// have both 1 and 2, cmp
					if(!buffer_empty2) {
						if(strcmp(new_word1,new_word2) < 0) {
							strcpy(add_word,new_word1);
							memset(new_word1,0,sizeof(new_word1));
							add_from = 1;
						}
						else{
							strcpy(add_word,new_word2);
							memset(new_word2,0,sizeof(new_word2));						
							add_from = 2;
						}
					}
					else {
							strcpy(add_word,new_word1);
							memset(new_word1,0,sizeof(new_word1));
							add_from = 1;				
					}
				}
			}
			else if(add_from == 2) {
				if(fgets(new_word2,50,stream2) == NULL) {
					isEnd2 = 1;
					buffer_empty2 = 1;
					// add from 1
					strcpy(add_word,new_word1);
					memset(new_word1,0,sizeof(new_word1));
					add_from = 1;
				}
				else{
					// have both 1 and 2, cmp
					if(!buffer_empty1) {
						if(strcmp(new_word1,new_word2) < 0) {
							strcpy(add_word,new_word1);
							memset(new_word1,0,sizeof(new_word1));						
							add_from = 1;
						}
						else {
							strcpy(add_word,new_word2);
							memset(new_word2,0,sizeof(new_word2));
							add_from = 2;
						}
					}
					else{
							strcpy(add_word,new_word2);
							memset(new_word2,0,sizeof(new_word2));
							add_from = 2;
					}
				}

			}
			if(strcmp(old_word,add_word) == 0) 
				multiplicity += 1;
			else {
				printf("%5d %s", multiplicity,old_word);
				
				multiplicity = 1;
				strcpy(old_word,add_word);
			}

		}

		fclose(stream1);
		fclose(stream2);
		close(suppress_pipe[0][0]);
		close(suppress_pipe[1][0]);
		
	}

	else {
		// parser process
		int c;
		int pos;
		char buffer[50];
		int whom;

		FILE *stream1;
		FILE *stream2;

		pos = 0;
		whom = 0;

		for(i = 0; i < 2; i++) {
			close(suppress_pipe[i][0]);
			close(suppress_pipe[i][1]);
			close(sort_pipe[i][0]);
		}

		stream1 = fdopen(sort_pipe[0][1],"w");
		stream2 = fdopen(sort_pipe[1][1],"w");


		while( (c = getc(stdin)) != EOF) {
			if(isalpha(c)) {
				if(pos > 29)
					continue;
				buffer[pos++] = (char) tolower(c);
			}
			else {
				if(pos < 3) {
					pos = 0;
					memset(buffer,0,sizeof(buffer));
					continue;
				}

				buffer[pos] = '\0';
				strcat(buffer,"\n");
				if(whom % 2 == 0) {
					fputs(buffer,stream1);
				}
				if(whom % 2 == 1){
					fputs(buffer,stream2);
				}
				whom += 1;
				pos = 0;
				memset(buffer,0,sizeof(buffer));
			}
		}

		if(pos >= 3) {
			buffer[pos] = '\0';
			strcat(buffer,"\n");
			if(whom % 2 == 0) {
				fputs(buffer,stream1);
			}
			else if(whom % 2 == 1){
				fputs(buffer,stream2);
			}
		}

		fclose(stream1);
		fclose(stream2);

		close(sort_pipe[0][1]);
		close(sort_pipe[1][1]);

		for(i = 0; i < 2; i++){
			if(wait(&wstatus) == -1) {
				perror("wait");
				exit(EXIT_FAILURE);
			}
		}
		
		if(wait(&wstatus) == -1) {
			perror("wait");
			exit(EXIT_FAILURE);
		}
	}

}
