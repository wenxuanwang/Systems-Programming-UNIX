/*	
  Author: Wenxuan Wang
  Email:  wenxuan.wang@emory.edu
*/

/* 
	Includes:
	q - quick append;
	x - extract;
	t - concise table;
	A - quick append files modified within last one hour;
	v - verbose table;
*/

#include <stdlib.h>
#include <stdio.h>
#include <ar.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <dirent.h>
#include <time.h>

char **get_files(char** argv, int file_num) {
	int i;
	char **files = (char**) malloc(sizeof(char *) * file_num);
	for(i = 0; i < file_num; i++) {
		files[i] = argv[i + 3];
	}
	return files;
}

char get_key(char *key) {
	char key_value;
	key_value = key[0] == '-' ? key[1] : key[0];
	if(strlen(key) > 2)
		return 0;
	if( (strlen(key) == 2) && (key[0] != '-'))
		return 0;
	return key_value;
}

int check_file(char* file) {
	struct stat file_stat;
	if(stat(file,&file_stat) == -1)
		return 0;
	return 1;
}

// t - concise table;
char *get_name(char *file_name, int name_length) {
	int i;
	for(i = 0; i < name_length; i++) {
		if(file_name[i] == '/') {
			file_name[i] = '\0';
			break;
		}
	}
	return file_name;
}

int print_concise_table(char *arch) {
	int arch_exist = check_file(arch);
	if(!arch_exist) {
		printf("archive not exist\n");
		exit(EXIT_FAILURE);
	}

	int ar_fd = open(arch,O_RDWR);

	char* magic_num = (char *) malloc(SARMAG);
	if(read(ar_fd,magic_num,SARMAG) == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	if(strcmp(magic_num,ARMAG)) {
		printf("not an archive\n");
		exit(EXIT_FAILURE);
	}

	struct ar_hdr *my_hdr = (struct ar_hdr*) malloc(sizeof(struct ar_hdr));

	int rd_size;

	while(rd_size = read(ar_fd, my_hdr,sizeof(struct ar_hdr))) {
		if(rd_size == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}

		char *file_name = get_name(my_hdr -> ar_name, 16);
		printf("%s\n",file_name);

		int content_size = atol(my_hdr -> ar_size);
		content_size += content_size % 2;
		lseek(ar_fd,content_size,SEEK_CUR);
	}
	free(my_hdr);
	close(ar_fd);
}



// q - quick append;
int write_content(int ar_fd, char *file, int optimal_size) {
	char* buffer = (char*) malloc(optimal_size);
	int file_fd = open(file,O_RDONLY);	
	int read_size = 1;

	while(read_size) {
		read_size = read(file_fd, buffer, optimal_size);
		if(read_size == -1) {
			perror("read");
			exit(EXIT_FAILURE);
		}
		if(read_size == 0) 
			break;
		if(read_size % 2 == 1) {
			buffer[read_size] = '\n';
			read_size += 1;
		}	
		if(write(ar_fd,buffer,read_size) == -1) {
			perror("write");
			break;
		}
	}
	return 0;
}

int append_separate_file(int ar_fd, char* file) {
	struct ar_hdr *my_hdr = (struct ar_hdr *) malloc(sizeof(struct ar_hdr));
	
	struct stat file_stat;
	if(stat(file,&file_stat) == -1) {
		perror("stat");
		return -1;
	}
	
	char name[16];
	strcpy(name,file);
	strcat(name,"/");
	
	sprintf(my_hdr -> ar_name, "%-16s", name);
	sprintf(my_hdr -> ar_date, "%-12lld", (long long int)file_stat.st_mtime);
	sprintf(my_hdr -> ar_uid, "%-6ld", (long int)file_stat.st_uid);
	sprintf(my_hdr -> ar_gid, "%-6ld", (long int)file_stat.st_gid);
	sprintf(my_hdr -> ar_mode, "%-8o", file_stat.st_mode);
	sprintf(my_hdr -> ar_size, "%-10ld", (long int)file_stat.st_size);
	strcpy(my_hdr -> ar_fmag, ARFMAG);
		
	if(write(ar_fd, my_hdr, sizeof(struct ar_hdr)) == -1) {
		perror("writte");
		exit(EXIT_FAILURE);	
	}

	write_content(ar_fd, file, file_stat.st_blksize);
	free(my_hdr);

	return 0;
}

int append_files(char *arch, char** files, int file_num) {
	int ar_fd;
	int arch_exist = check_file(arch);
	if(arch_exist) {
		ar_fd = open(arch, O_WRONLY|O_APPEND);
	}
	else {
		ar_fd = open(arch, O_WRONLY|O_CREAT|O_TRUNC, 0666);
		if(write(ar_fd, ARMAG,SARMAG) == -1) {
			perror("write");
			exit(EXIT_FAILURE);		
		}
		printf("myar: creating %s\n",arch);
	}
	
	if(ar_fd == -1) {
			perror("open");
			exit(EXIT_FAILURE);
	}
	
	if(lseek(ar_fd,0,SEEK_END) == -1) {
			perror("lseek");
			exit(EXIT_FAILURE);
	}

	int i;
	for(i = 0; i < file_num; i++) {
		append_separate_file(ar_fd,files[i]);
	}

	close(ar_fd);
	return 0;
}



// x - extract;
int extract_files(char *arch, char** files, int file_num) {	
	int arch_exist = check_file(arch);
	if(!arch_exist) {
		perror("archive not exist");
		exit(EXIT_FAILURE);
	}
	struct utimbuf *times;	
	int mark[file_num];
	int n;
	for(n = 0; n < file_num; n++) {
		mark[n] = 0;
	}
	
	int ar_fd = open(arch,O_RDONLY);
	if(ar_fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	struct stat ar_stat;
	if(stat(arch,&ar_stat) == -1) {
		perror("stat");
		exit(EXIT_FAILURE);	
	}

	char* magic_num = (char *) malloc(SARMAG);
	if(read(ar_fd,magic_num,SARMAG) == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	if(strcmp(magic_num,ARMAG)) {
		printf("not an archive\n");
		exit(EXIT_FAILURE);
	}

	char *buffer = (char*) malloc(sizeof(ar_stat.st_blksize));
	struct ar_hdr *my_hdr = (struct ar_hdr *) malloc(sizeof(struct ar_hdr));
	//printf("archive size %d\n",(int) ar_stat.st_size);

	int rd_sz;
	while((rd_sz = read(ar_fd,my_hdr,sizeof(struct ar_hdr))) != 0) {
			int extracted = 0;
			if(rd_sz != sizeof(struct ar_hdr)) {
				perror("read");
				exit(EXIT_FAILURE);
			}
			char *file_name = get_name(my_hdr -> ar_name, 16);
			long long int file_sz = atoll(my_hdr -> ar_size);
			int odd = file_sz % 2;
			
			int i;
			for(i = 0; i < file_num; i++) {
				if( (!strcmp(files[i],file_name)) && (!mark[i])) {
					// mark extracted   				
					// printf("file name:%s\n",my_hdr);
					extracted = 1;
					mark[i] = 1;

					int file_fd;
					if( (file_fd = open(file_name, O_WRONLY|O_CREAT|O_TRUNC, strtol(my_hdr->ar_mode,NULL,8))) == -1) {
							exit(EXIT_FAILURE);
							perror("open");
					}

					int read_sz;					
					while(file_sz > 0) {
						int size = (sizeof(buffer) > file_sz ? file_sz : sizeof(buffer));

						if( (read_sz = read(ar_fd, buffer, size)) == -1) {
							perror("read");
							exit(EXIT_FAILURE);	
						}	

						if(write(file_fd,buffer, read_sz) == -1) {
							perror("write");
							exit(EXIT_FAILURE);
						}
						file_sz -= read_sz;
					}	

					//restore
					times = (struct utimbuf*) malloc(sizeof(struct utimbuf));
					time_t atime = (time_t) atoll(my_hdr -> ar_date);
					time_t mtime = (time_t) atoll(my_hdr -> ar_date);
					times -> actime = atime;
					times -> modtime = mtime;
					if(utime(file_name, times) == -1) {
						perror("utime");
						exit(EXIT_FAILURE);	
					}
					
					uid_t uid = (uid_t) atol (my_hdr -> ar_uid);
					gid_t gid = (gid_t) atol (my_hdr -> ar_gid);
					chown(file_name,uid,gid);
				
					close(file_fd);
					free(times);
				}
			}
			
		if(!extracted) {
			if(lseek(ar_fd, atoll(my_hdr -> ar_size),SEEK_CUR) == -1) {
				perror("lseek");
				exit(EXIT_FAILURE);
			}
		}
		
		if(odd) {
			lseek(ar_fd,1,SEEK_CUR);					
		}

	}
	close(ar_fd);
	return 0;
}



// A - quick append files modified within last one hour;
int check_valid(struct dirent *dir_dirent, char *arch) {
	//modification time
	struct stat file_stat;	
	if(stat(dir_dirent -> d_name, &file_stat) == -1) {
		perror("read");
		return 0;
	}

	int current_time;
	current_time =(unsigned long) time(NULL);	
	
	if( (current_time - file_stat.st_mtime) > 3600) {
		return 0;
	}

	if(!S_ISREG(file_stat.st_mode)) {
		return 0;	
	}
	
	if(!strcmp(dir_dirent -> d_name,arch)) {
		return 0;	
	}
	if(strlen(dir_dirent -> d_name) > 15) {
		return 0;
	}
	//printf("appending file: %16s  epoch time: %lld\n",dir_dirent -> d_name,(long long int)file_stat.st_mtime);
	return 1;
}

int append_all_ord(char *arch) {
	int arch_exist = check_file(arch);
	if(!arch_exist) {
		perror("archive not exist\n");
		exit(EXIT_FAILURE);
	}
	
	int ar_fd = open(arch, O_RDWR|O_APPEND);
	
	if(ar_fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}

	char* magic_num = (char *) malloc(SARMAG);
	if(read(ar_fd,magic_num,SARMAG) == -1) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	if(strcmp(magic_num,ARMAG)) {
		printf("not an archive\n");
		exit(EXIT_FAILURE);
	}
	
	if(lseek(ar_fd,0,SEEK_END) == -1) {
		perror("lseek");
		exit(EXIT_FAILURE);
	}

	DIR *current_dir;	
	
	if(!(current_dir = opendir("."))) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	
	struct dirent *dir_dirent;
	while(dir_dirent = readdir(current_dir)) {
		if(check_valid(dir_dirent, arch)) {
			append_separate_file(ar_fd, dir_dirent -> d_name);
		}
	}
	close(ar_fd);

	return 0;
}

// v - verbose;
char* get_perm(char* ar_mode) {
    	mode_t mode = (mode_t)strtol(ar_mode,NULL,8);
    	char *perm = (char*) malloc(9);
    	sprintf(perm++,"%c",mode & S_IRUSR ? 'r' : '-');
    	sprintf(perm++,"%c",mode & S_IWUSR ? 'w' : '-');
    	sprintf(perm++,"%c",mode & S_IXUSR ? 'x' : '-');
    	sprintf(perm++,"%c",mode & S_IRGRP ? 'r' : '-');
    	sprintf(perm++,"%c",mode & S_IWGRP ? 'w' : '-');
    	sprintf(perm++,"%c",mode & S_IXGRP ? 'x' : '-');
    	sprintf(perm++,"%c",mode & S_IROTH ? 'r' : '-');
    	sprintf(perm++,"%c",mode & S_IWOTH ? 'w' : '-');
    	sprintf(perm++,"%c",mode & S_IXOTH ? 'x' : '-');
    	perm -= 9;
    	return perm;
}

char* get_time(char* date) {
    	time_t file_mtime = (time_t) atoll(date);
    	char *buffer = (char*)malloc(20);
    	struct tm *time = localtime(&file_mtime);
    	strftime(buffer,20,"%b %d %H:%M %Y",time);
    	return buffer;
}
int print_verbose(struct ar_hdr *my_hdr,char* name) {
    	char* perm = get_perm(my_hdr -> ar_mode);
    	char* time = get_time(my_hdr -> ar_date);
    	printf("%s %ld/%ld %6lld %s %s\n",perm,atol(my_hdr -> ar_uid),atol(my_hdr -> ar_gid),atoll(my_hdr -> ar_size),time,name);
    	return 0;
}



int verbose(char *arch) {
	int arch_exist = check_file(arch);
	if(!arch_exist) {
		perror("archive not exist\n");
		exit(EXIT_FAILURE);
	}
    	int ar_fd = open(arch,O_RDWR);

	char* magic_num = (char *) malloc(SARMAG);
	if(read(ar_fd,magic_num,SARMAG) == -1) {
		perror("not an archive");
		exit(EXIT_FAILURE);
	}
	if(strcmp(magic_num,ARMAG)) {
		printf("not an archive\n");
		exit(EXIT_FAILURE);
	}

	struct ar_hdr *my_hdr = (struct ar_hdr*) malloc(sizeof(struct ar_hdr));
	int rd_size;

	while(rd_size = read(ar_fd, my_hdr,sizeof(struct ar_hdr))) {
		if(rd_size == -1) {
		    perror("read");
		    exit(EXIT_FAILURE);
		}

		char *file_name = get_name(my_hdr -> ar_name, 16);
		print_verbose(my_hdr,file_name);

		int content_size = atol(my_hdr -> ar_size);
		content_size += content_size % 2;
		lseek(ar_fd,content_size,SEEK_CUR);
	}
	free(my_hdr);
	close(ar_fd);

	return 0;
}



int main(int argc, char** argv) {
	char key;
	char **files;
	char *arch;
	if(argc < 3) {
		printf("not having enough arguments\n");
		exit(EXIT_FAILURE);
	}
	key = get_key(argv[1]);
	arch = argv[2];
	files = get_files(argv,argc-3);
	
	switch(key) {
		case 'q' :
			append_files(arch, files, argc-3);
			break;
		case 'x' :
			extract_files(arch, files, argc-3);
			break;
		case 't' :
			print_concise_table(arch);
			break;
		case 'v' :
			verbose(arch);
			break;
		case 'd' :
			break;
		case 'A' :
			append_all_ord(arch);
			break;
		default:
			printf("option not supportted\n");
			break;			
	}
	free(files);
	return 0;
}
