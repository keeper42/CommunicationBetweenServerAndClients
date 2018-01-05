#ifndef _CLIENTINFO_H
#define _CLIENTINFO_H

typedef struct {
	unsigned int id;
	char user[16];
	char pswd[16];
	char msg[4096];
} CLIENTINFO, *CLIENTINFOPTR;

#endif

