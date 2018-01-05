    #include <unistd.h>
    #include "common.h"    
        
    int main(int argc, char* argv[])    
    {    
        int msg_id = creat_msg_queue();    
        if(msg_id <0) {    
            printf("%d : %s\n", errno, strerror(errno));    
            return 1;    
        }   
        
        char buf[_SIZE_];   
        while(1) {    
            memset(buf, '\0', sizeof(buf));    
            recv_msg(msg_id, client_type,buf);    
            printf("Client inputs: %s\n",buf);     
        
            printf("Please inputs: ");    
            fflush(stdout);    
            ssize_t _s = read(0, buf, sizeof(buf) - 1);    
            if(_s > 0) {    
                buf[_s - 1] = '\0';    
            }    
        
            send_msg(msg_id, server_type,buf);
        }    
        
        destroy_queue(msg_id);    
        return 0;    
    }    
