#!/bin/bash
rm -rf fifo
mkdir fifo/
gcc -o serverd serverd.c -pthread -L/usr/lib/mysql -lmysqlclient
gcc -o client client.c 
