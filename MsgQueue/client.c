    #include <unistd.h>
    #include "common.h"    
        
    int main(int argc, char* argv[])    
    {    
        int msg_id = get_msg_queue();    
        
        char buf[_SIZE_];    
        while(1) {    
            printf("Please input: ");    
            fflush(stdout);    
            ssize_t _s = read(0, buf, sizeof(buf) - 1);    
            if(_s >0 ) {     
                buf[_s - 1] = '\0';    
            }    
            send_msg(msg_id,client_type,buf); 
        
            memset(buf,'\0',sizeof(buf));    
            recv_msg(msg_id,server_type,buf);    
            printf("Server inputs: %s\n",buf);    
        }    
        
        return 0;    
    }    
