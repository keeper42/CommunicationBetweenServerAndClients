/*******************************************************************************************
 * 
 * Copyright(C) Unix Program
 * SystemName: Multi-client Instant Chat System
 * Verson: 1.0.0
 * FileName: client.c
 * Author: Keeper42
 * Date: 2018-01-01
 * Description: Client generates events such as registering, logging in, or sending messages.
 *
 *******************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include "clientinfo.h"

#define SFIFO_NUM  3
#define BUFF_SZ    128
#define NAMESIZE   BUFF_SZ
#define KBSIZE     1024
#define MSGSIZE    4096

char FIFO_NAME[SFIFO_NUM][NAMESIZE];
char MYPIPENAME[BUFF_SZ];

void handler(int sig);
void handler2(int sig);
void parseFile(const char* file, char data[SFIFO_NUM][NAMESIZE]);

int main(int argc, char* argv[]) {

	signal(SIGKILL, handler);
	signal(SIGINT, handler);
	signal(SIGTERM, handler);

	if (argc < 3) {
		printf("Usage: Illegal parameters.\n");
		exit(1);
	}

	const char* configFile = "./initial.conf";
	parseFile(configFile, FIFO_NAME);
		
	int res, fifo_fd;
	if (strcmp(argv[1], "register") == 0) {
		// open server fifo for writing
		fifo_fd = open(FIFO_NAME[0], O_WRONLY | O_NONBLOCK);
		if (fifo_fd == -1) {
			printf("Could not open %s for write access\n", FIFO_NAME[0]);
			exit(EXIT_FAILURE);
		}

		CLIENTINFO info;
		info.id = getpid();
		sprintf(info.user, "%s", argv[2]);
		sprintf(info.pswd, "%s", argv[3]);
		sprintf(info.msg, "%s", "register\0");

		write(fifo_fd, &info, sizeof(CLIENTINFO));	
		close(fifo_fd); 
	} else if (strcmp(argv[1], "login") == 0) {
		fifo_fd = open(FIFO_NAME[1], O_WRONLY | O_NONBLOCK);
		if (fifo_fd == -1) {
			printf("Could not open %s for write access\n", FIFO_NAME[1]);
			exit(EXIT_FAILURE);
		}

		CLIENTINFO info;
		info.id = getpid();
		sprintf(info.user, "%s", argv[2]);
		sprintf(info.pswd, "%s", argv[3]);
		sprintf(info.msg, "%s", "login\0");	

		write(fifo_fd, &info, sizeof(CLIENTINFO));	
		close(fifo_fd); 
	} else if (strcmp(argv[1], "logout") == 0) {
		fifo_fd = open(FIFO_NAME[1], O_WRONLY | O_NONBLOCK);
		if (fifo_fd == -1) {
			printf("Could not open %s for write access\n", FIFO_NAME[1]);
			exit(EXIT_FAILURE);
		}

		CLIENTINFO info;
		info.id = getpid();
		sprintf(info.msg, "%s", "logout\0");

		write(fifo_fd, &info, sizeof(CLIENTINFO));	
		close(fifo_fd);
		exit(1);
	} else if (strcmp(argv[1], "sendmsg") == 0) {
		CLIENTINFO info;
		info.id = getpid();
		sprintf(info.msg, "%s", argv[2]);

		write(fifo_fd, &info, sizeof(CLIENTINFO));	
		close(fifo_fd);
	} else {
		printf("Usage: Illegal parameters.\n");
		exit(1);	
	}
	
	// open client own FIFO for reading
	sprintf(MYPIPENAME, "./fifo/client%d_fifo", getpid());
	res = mkfifo(MYPIPENAME, 0777);
	if (res != 0) {
		printf("Client fifo was not created\n");
		exit(EXIT_FAILURE);
	}
	int my_fifo = open(MYPIPENAME, O_RDONLY | O_NONBLOCK);
	if (my_fifo == -1) {
		printf("Could not open %s for read only access\n", MYPIPENAME);
		exit(EXIT_FAILURE);
	}
	char buffer[MSGSIZE];
	while(1) {
		// flush buffer
		memset(buffer, '\0', MSGSIZE);
		res = read(my_fifo, buffer, MSGSIZE);
		if (res > 0) {
			printf("%s\n", buffer);
			if (strcmp(buffer, "Registration succeed!\n") == 0) {	
				close(my_fifo);
				exit(1);
			} else if (strcmp(buffer, "Client itself login failed!\n") == 0) {
				close(my_fifo);
				unlink(MYPIPENAME);
				exit(1);		
			} else if (strcmp(buffer, "Client logout!\n") == 0) {
				close(my_fifo);
				unlink(MYPIPENAME);
				exit(1);		
			} 
		}
		signal(SIGTSTP, handler2);
		signal(SIGINT,  handler2);
	}
	close(my_fifo);
	unlink(MYPIPENAME);
	return 0;
}

void handler2(int sig) {
	unlink(MYPIPENAME);
	int fifo_fd = open(FIFO_NAME[1], O_WRONLY | O_NONBLOCK);
	CLIENTINFO info;
	info.id = getpid();
	sprintf(info.msg, "%s", "logout\0");
	write(fifo_fd, &info, sizeof(CLIENTINFO));	
	close(fifo_fd);		
}

void trimString(char* str) {
	int len = strlen(str);
	int i = len - 1;
	for (; i >= 0; --i) {
		if (str[i] == '\n' || str[i] == '\r') {
			str[i] = '\0';
		}
	}
}

void parseFile(const char* file, char data[SFIFO_NUM][NAMESIZE]) {
	FILE* fp = fopen(file, "r");

	char buf[KBSIZE], str[KBSIZE];
	char* FIFO_1 = "FIFO_1";
	char* FIFO_2 = "FIFO_2";
	char* FIFO_3 = "FIFO_3";
	char* delim = "=";	
	while(!feof(fp)) {
		char* p;
		if ((p = fgets(buf, sizeof(buf), fp)) != NULL) {
			strcpy(str, p);
			p = strtok(str, delim);
			if (p) {
				if (strcmp(p, FIFO_1) == 0) {
					while((p = strtok(NULL, delim))) {
						trimString(p);
						sprintf(data[0], "%s", p);
					}
				} else if (strcmp(p, FIFO_2) == 0) {
					while((p = strtok(NULL, delim))) {
						trimString(p);
						sprintf(data[1], "%s", p);
					}
				} else if (strcmp(p, FIFO_3) == 0) {
					while((p = strtok(NULL, delim))) {
						trimString(p);
						sprintf(data[2], "%s", p);
					}
				}
			}
		}
	}
}

void handler(int sig) {
	for (int i = 0; i < SFIFO_NUM; ++i) {
		unlink(FIFO_NAME[i]);
	}
	exit(1);
}

