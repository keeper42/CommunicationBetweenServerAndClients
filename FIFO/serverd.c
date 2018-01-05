/****************************************************************
 * 
 * Copyright(C) System Program
 * SystemName: Multi-client Instant Chat System
 * Verson: 1.0.0
 * FileName: serverd.c
 * Author: Keeper42
 * Date: 2018-01-01
 * Description: Server as a daemon monitor clients' events.
 *
 ***************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <assert.h>
#include <pthread.h>
#include <semaphore.h>
#include <mysql/mysql.h>
#include <dirent.h>
#include "clientinfo.h"

#define SFIFO_NUM  3
#define THREAD_NUM SFIFO_NUM
#define NAMESIZE   128
#define KBSIZE     1024
#define MSGSIZE    4096

struct linux_dirent {
	unsigned long  d_ino;
	unsigned long  d_off;
	unsigned short d_reclen;
	char           d_name[];
};

int   MAXCONNS;
int   FIFO_FD[SFIFO_NUM];
char  FIFO_NAME[SFIFO_NUM][NAMESIZE];

void  handler(int sig);
int   daemon (int nochdir, int noclose);
void  parseFile(const char* file, char data1[SFIFO_NUM][NAMESIZE], int* data2);
void* threadFunction(void* arg);

sem_t MUTEX;

int main(int argc, char* argv[]) {

	// daemon(0, 0);
	// sleep(10);

	MAXCONNS = 0;
	const char* configFile = "./initial.conf";
	parseFile(configFile, FIFO_NAME, &MAXCONNS);

	int i, res;
	for (i = 0; i < SFIFO_NUM; ++i) {
		// create FIFO, if necessary
		if (access(FIFO_NAME[i], F_OK) == -1) {
			res = mkfifo(FIFO_NAME[i], 0777);
			if (res != 0) {
				printf("FIFO %s was not created\n", FIFO_NAME[i]);
				exit(EXIT_FAILURE);
			}
		}
		// open FIFO for reading
		if ((FIFO_FD[i] = open(FIFO_NAME[i], O_RDONLY | O_NONBLOCK)) == -1) {
			printf("Could not open %s for read only access\n", FIFO_NAME[i]);
			exit(EXIT_FAILURE);
		}	
	}	
	
	sem_init(&MUTEX, 0, 1);

	pthread_t threadId[THREAD_NUM];
	int index[THREAD_NUM];
	for (i = 0; i < THREAD_NUM; ++i) {
		index[i] = i;
		pthread_create(&threadId[i], NULL, &threadFunction, &index[i]);
	}
	sleep(10);
	for (i = 0; i < THREAD_NUM; ++i) {
		pthread_join(threadId[i], NULL);	
	}	
	
	for (i = 0; i < SFIFO_NUM; ++i) {
		unlink(FIFO_NAME[i]);
	}

	sem_destroy(&MUTEX);
	return 0;
}

//CLIENTINFO online_client[MAXCONNS];

void* threadFunction(void* arg) {
	int index = *((int*)arg);
	if (index == 0) {		// register	
		CLIENTINFO client_info;
		unsigned int client_id = 0;
		char username[16];
		char password[16];
		char pipename[NAMESIZE];	
		char msgbuf[MSGSIZE];	
		printf("\nServer is open %s for reading!\n", FIFO_NAME[0]);

		while(1){
			sem_wait(&MUTEX);
			int res = read(FIFO_FD[0], &client_info, sizeof(CLIENTINFO));
			sem_post(&MUTEX);
			if(res != 0) {
				client_id = client_info.id;
				sprintf(username, "%s", client_info.user);
				sprintf(password, "%s", client_info.pswd);
				sprintf(pipename, "./fifo/client%d_fifo", client_id);
				int fd = open(pipename, O_WRONLY | O_NONBLOCK);

				// Save client info to database.
				if ((client_id != 0) && (strlen(username) != 0) && (strlen(password) != 0)) {
					MYSQL mysql_conn;
					if (mysql_init(&mysql_conn) != NULL) {
						if (mysql_real_connect(&mysql_conn, "localhost", "root", "******", "client_db", MYSQL_PORT, NULL, 0) != NULL) {
							// query							
							char select[128] = "SELECT `username` from client_info";
							sem_wait(&MUTEX);
							mysql_query(&mysql_conn, select);									
							MYSQL_RES* resptr = mysql_store_result(&mysql_conn);

							int same = 0;							
							if (resptr) {
								int row = mysql_num_rows(resptr);	
								for (int i = 0; i < row; ++i) {
									MYSQL_ROW rowres = mysql_fetch_row(resptr);	
									if (strcmp(rowres[0], username) == 0) {
										same = 1;
										break;
									}						
								}						
							}
							mysql_free_result(resptr);

							if (!same) {
								char sql[256];
								// insert
								char insert[128] = "INSERT INTO client_info(client_id, username, password) VALUES (";
								snprintf(sql, sizeof(sql), "%s%u, '%s', '%s')", insert, client_id, username, password);
								int inres;
							
								inres = mysql_query(&mysql_conn, sql);
								if (inres == 0){	
									sprintf(msgbuf, "%s", "Registration succeed!\n");
									write(fd, msgbuf, strlen(msgbuf));							
								} else {
									printf("Insert error.\n");
								}
								close(fd);
							} else {
								sprintf(msgbuf, "%s", "Registration failed!\n");
								write(fd, msgbuf, strlen(msgbuf));
								close(fd);
							}
							sem_post(&MUTEX);
						}
					} else {
						printf("Connect database failed\n");
					}
					mysql_close(&mysql_conn);				
				}			
			}
		}
 	} 
	else if (index == 1) {	// login
		CLIENTINFO online_client[MAXCONNS];
		int client_index = 0;
		CLIENTINFO client_info;		
		unsigned int client_id = 0;
		char username[16];
		char password[16];
		char messages[MSGSIZE];
		char pipename[NAMESIZE];	
		char msgbuf[MSGSIZE];
		int fd;	
		printf("Server is open %s for reading!\n", FIFO_NAME[1]);
		while(1){
			// sem_wait(&MUTEX);
			int res = read(FIFO_FD[1], &client_info, sizeof(CLIENTINFO));
			// sem_post(&MUTEX);
			if(res != 0) {
				client_id = client_info.id;
				sprintf(username, "%s", client_info.user);
				sprintf(password, "%s", client_info.pswd);
				sprintf(messages, "%s", client_info.msg);

				if ((client_id != 0) && (strlen(username) != 0) && (strlen(password) != 0)) {
					MYSQL mysql_conn;
					if (mysql_init(&mysql_conn) != NULL) {
						if (mysql_real_connect(&mysql_conn, "localhost", "root", "******", "client_db", MYSQL_PORT, NULL, 0) != NULL) {
							char select[128] = "SELECT `username`,`password` from client_info";
							sem_wait(&MUTEX);
							mysql_query(&mysql_conn, select);									
							MYSQL_RES* resptr = mysql_store_result(&mysql_conn);
							sem_post(&MUTEX);
							int same_db = 0, same_on = 0;							
							if (resptr) {
								int row = mysql_num_rows(resptr);	
								for (int i = 0; i < row; ++i) {
									MYSQL_ROW rowres = mysql_fetch_row(resptr);	
									if ((strcmp(rowres[0], username) == 0) && (strcmp(rowres[1], password) == 0)) {
										sem_wait(&MUTEX);
										same_db = 1;
										sem_post(&MUTEX);
										break;
									}						
								}						
							}
							mysql_free_result(resptr);
							
							if (same_db) { 
								for (int i = 0; i < client_index; ++i) {
									if ((strlen(online_client[i].user) != 0) && (strcmp(client_info.user, online_client[i].user)==0)) {
										sem_wait(&MUTEX);
										same_on = 1;
										sem_post(&MUTEX);
										break;
									}	
								}
								if (!same_on) {
									sem_wait(&MUTEX);
									// copy client_info to online_client list
									online_client[client_index].id = client_info.id;
									sprintf(online_client[client_index].user, "%s", client_info.user);
									sprintf(online_client[client_index].pswd, "%s", client_info.pswd);
									sprintf(online_client[client_index].msg, "%s", client_info.msg);
									client_index = client_index + 1;
									
									if (client_index <= MAXCONNS) {
										for (int i = 0; i < client_index; ++i) {
											sprintf(pipename, "./fifo/client%d_fifo", online_client[i].id);
											fd = open(pipename, O_WRONLY | O_NONBLOCK);
											sprintf(msgbuf, "Client %d login succeed!\n", client_info.id);
											write(fd, msgbuf, strlen(msgbuf));
											close(fd);	
										}	
									} 
									else {
										sprintf(pipename, "./fifo/client%d_fifo", client_info.id);
										fd = open(pipename, O_WRONLY | O_NONBLOCK);
										sprintf(msgbuf, "%s", "The connection is full, please wait!\n");
										write(fd, msgbuf, strlen(msgbuf));
										close(fd);
									}
									sem_post(&MUTEX);									
								} 
							} else {
								sprintf(pipename, "./fifo/client%d_fifo", client_info.id);
								fd = open(pipename, O_WRONLY | O_NONBLOCK);
								sprintf(msgbuf, "%s", "Client itself login failed!\n");
								write(fd, msgbuf, strlen(msgbuf));
								close(fd);
							}	
						}
					} else {
						printf("Connect database failed\n");
					}
					mysql_close(&mysql_conn);		
				}

				if ((client_info.id != 0) && (strcmp(client_info.msg, "logout\0") == 0)) {
					// poll folder fifo/
					DIR* dir;
					struct dirent* ptr;
					char clientfifo_list[128][128];
					dir = opendir("./fifo/");
					int cfifo_index = 0;
					while((ptr = readdir(dir)) != NULL) {
						int same_cfifo = 0;
						for (int i = 0; i < cfifo_index; ++i) {
							if (strcmp(clientfifo_list[i], ptr->d_name) == 0) {
								sem_wait(&MUTEX);
								same_cfifo = 1;	
								sem_post(&MUTEX);
								break;					
							}					
						}

						if (strstr(ptr->d_name, "client") && !same_cfifo) {
							sem_wait(&MUTEX);
							sprintf(clientfifo_list[cfifo_index++], "%s", ptr->d_name);
							sem_post(&MUTEX);					
						}			
					}
					closedir(dir);

					// find logout client
					int logout_index = -1, exist = 0;
					for (int i = 0; i < client_index; ++i) {
						char clientfifo_name[128];					
						sprintf(clientfifo_name, "client%u_fifo", online_client[i].id);
						exist = 0;
						for (int j = 0; j < cfifo_index; ++j) {
							if ((strcmp(clientfifo_list[j], clientfifo_name)) == 0) {
								exist = 1;
								break;
							}
						}
						if (!exist) {
							logout_index = i;
							break;
						}
					}

					// notify other clients that has client exited
					if (!exist && logout_index > -1) {
						int logout_id = online_client[logout_index].id;
						for (int i = logout_index; i < client_index-1; ++i) {
							online_client[i] = online_client[i+1];					
						}
						client_index = client_index - 1;				
					
						char remain[KBSIZE] = "";
						for (int i = 0; i < client_index; ++i) {
							char clientid_str[10]; 
							// itoa(online_client[i].id, clientid_str, 10);
							sprintf(clientid_str, "%u", online_client[i].id);
							strcat(strcat(strcat(strcat(strcat(remain, "Remain client: "), clientid_str), ", "), online_client[i].user), "\n");
						}
						for (int i = 0; i < client_index; ++i) {
							sprintf(pipename, "./fifo/client%d_fifo", online_client[i].id);
							fd = open(pipename, O_WRONLY | O_NONBLOCK);
							snprintf(msgbuf, sizeof(msgbuf), "Client %u logout!\n%s", logout_id, remain);
							write(fd, msgbuf, strlen(msgbuf));
							
							close(fd);	
						}
					}		
				}
			}
		}
	} else if (index == 2) {	// send messages
		printf("Server is open %s for reading!\n", FIFO_NAME[2]);

		// poll folder fifo/
		DIR* dir;
		struct dirent* ptr;
		char clientfifo_list[128][128];
		dir = opendir("./fifo/");
		int cfifo_index = 0;
		while((ptr = readdir(dir)) != NULL) {
			int same_cfifo = 0;
			for (int i = 0; i < cfifo_index; ++i) {
				if (strcmp(clientfifo_list[i], ptr->d_name) == 0) {
					sem_wait(&MUTEX);
					same_cfifo = 1;	
					sem_post(&MUTEX);
					break;					
				}					
			}

			if (strstr(ptr->d_name, "client") && !same_cfifo) {
				sem_wait(&MUTEX);
				sprintf(clientfifo_list[cfifo_index++], "%s", ptr->d_name);
				sem_post(&MUTEX);					
			}			
		}
		closedir(dir);
		
		// allow two client communication
		if (cfifo_index == 2) {
			CLIENTINFO client_info;
			unsigned int client_id;
			char msgbuf[MSGSIZE];
			char fifoname[NAMESIZE];
			int fd;	
			while(1){
				int res = read(FIFO_FD[2], &client_info, sizeof(CLIENTINFO));
				if(res != 0) {
					client_id = client_info.id;
					sprintf(fifoname, "./fifo/client%u_fifo", client_id);
					if ((strcmp(fifoname, clientfifo_list[0])) != 0) {
						fd = open(clientfifo_list[1], O_WRONLY | O_NONBLOCK);
						sprintf(msgbuf, "%s", client_info.msg);	
						write(fd, msgbuf, strlen(msgbuf));
						close(fd);
					} else if ((strcmp(fifoname, clientfifo_list[1])) != 0) {
						fd = open(clientfifo_list[0], O_WRONLY | O_NONBLOCK);
						sprintf(msgbuf, "%s", client_info.msg);	
						write(fd, msgbuf, strlen(msgbuf));
						close(fd);
					}
				}
			}			
		}
	}
}

int daemon(int nochdir, int noclose) {
        // Ignore terminal signal.
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
        signal(SIGTSTP, SIG_IGN);
        signal(SIGHUP,  SIG_IGN);
	signal(SIGKILL, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

        pid_t pid = fork();
        if (pid < 0) {
                perror("fork");
                return -1; 
        } else if (pid > 0) {
                exit(0);
        }
    
        // Create a new session.
        pid = setsid();
        if (pid < -1) {
                perror("setsid");
                return -1; 
        }
    
        // Build a child process again, exit the parent process 
        // to ensure that the child process is not the process leader.
        pid = fork();
        if (pid < 0) {
                perror("fork");
                return -1; 
        } else if (pid > 0) {
                exit(0);
        }

        if (!nochdir) {
                chdir("/");
        }
        if(!noclose) {
                int fd = open("/dev/null", O_RDWR, 0); 
                if (fd != -1) {
                        dup2(fd, STDIN_FILENO);
                        dup2(fd, STDOUT_FILENO);
                        dup2(fd, STDERR_FILENO);
                        if(fd > 2) {
                                close(fd);
                        }
                }
        }

        umask(0);
        return 0;
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

void parseFile(const char* file, char data1[SFIFO_NUM][NAMESIZE], int* data2) {
	FILE* fp = fopen(file, "r");

	char buf[KBSIZE], str[KBSIZE];
	char* FIFO_1 = "FIFO_1";
	char* FIFO_2 = "FIFO_2";
	char* FIFO_3 = "FIFO_3";
	char* MAXCLIENTCONNS = "MAX_CLIENT_CONNS";
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
						sprintf(data1[0], "%s", p);
					}
				} else if (strcmp(p, FIFO_2) == 0) {
					while((p = strtok(NULL, delim))) {
						trimString(p);
						sprintf(data1[1], "%s", p);
					}
				} else if (strcmp(p, FIFO_3) == 0) {
					while((p = strtok(NULL, delim))) {
						trimString(p);
						sprintf(data1[2], "%s", p);
					}
				} else if (strcmp(p, MAXCLIENTCONNS) == 0) {
					while((p = strtok(NULL, delim))) {
						trimString(p);
						*(data2) = atoi(p);				
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

