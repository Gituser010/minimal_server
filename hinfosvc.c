#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int transfer_to_int(char *buff,size_t buff_size)
{
    size_t final_number=0;
    size_t j=1;
    for(int i=buff_size-1;i>=0;i--)
    {
        final_number=final_number+((int)(buff[i]-'0'))*j;
        j=j*10;
    }
    return final_number;
}



void make_array(size_t size,char *buffer,size_t **values)
{
    char local_buff[50];
    size_t local_index=0;
    size_t array_index=0;
    size_t number;
    for(size_t i=5;i<size;i++)
    {
        if(buffer[i]==' ')
        {
            number=transfer_to_int(local_buff,local_index);
            *values=realloc(*values,sizeof(size_t)*(array_index+1));
            (*values)[array_index]=number;
            local_index=0;
            array_index++;
        }
        else
        {
            local_buff[local_index]=buffer[i];
            local_index++;
        }
    }
}

float cpu_percentage(size_t prev_values[],size_t values[]) //values represent cullomns of proc/stat
{
    size_t prev_idle=prev_values[3]+prev_values[4];
    size_t idle=values[3]+values[4];
    
    size_t prev_non_idle=prev_values[0] + prev_values[1] + prev_values[2] + prev_values[5] + prev_values[6] + prev_values[7];
    size_t non_idle= values[0] +  values[1] +  values[2] +  values[5] +  values[6] +  values[7];

    size_t prev_total=prev_idle+prev_non_idle;
    size_t total=idle+non_idle;

    size_t total_diff=total-prev_total;    
    size_t idle_diff=idle-prev_idle;

    float a=((float)total_diff-(float)idle_diff)/(float)total_diff;
    return a*100;
 
    
}



void write_cpu_per(float *value)
{
    FILE* file;
    file = fopen("/proc/stat", "r");
 
    char *line=NULL;
    size_t lineSize=0;
    size_t buffSize=150;
    size_t *cpu_values_first;
    size_t *cpu_values_sec;

    cpu_values_first=malloc(sizeof(size_t));
    cpu_values_sec=malloc(sizeof(size_t));

    lineSize=getline(&line,&buffSize,file);
    make_array(lineSize,line,&cpu_values_first);  //creates array of values from cpu/stat skippin cpu 
    fclose(file);

    sleep(1);

    file=fopen("/proc/stat","r");
    lineSize=getline(&line,&buffSize,file);
    make_array(lineSize,line,&cpu_values_sec);  //creates array of values from cpu/stat skippin cpu 
    fclose(file);
    *value=cpu_percentage(cpu_values_first,cpu_values_sec); //write out cpu percentage
}

char* get_hostname(int *lineSize)
{
    FILE* file;
    file = fopen("/proc/sys/kernel/hostname", "r");
 
    char *line=NULL;
    size_t buffSize=150;  

    *lineSize=getline(&line,&buffSize,file);

    return line;
}

char* get_cpu(int *len)
{
    FILE* file;
    file =popen("cat /proc/cpuinfo | grep 'model name' | head -n 1 | awk 'BEGIN {FS =\": \"}  END {print $2}'","r");

    char *line=NULL;
    size_t lineSize=0;
    size_t buffSize=150;  

    *len=getline(&line,&buffSize,file);

    return line;
}

int main(int argc,char **argv)
{
    int accpt;
    int server_socket;
    int optval=1;
    char buff[1024];
    unsigned short port=1020;
 
    if(argc>1)
    {
        port=(unsigned short)atoi(argv[1]);
    }
    printf("%d",port);
    server_socket=socket(AF_INET,SOCK_STREAM,0);
    setsockopt(server_socket,SO_REUSEADDR,SO_REUSEPORT,(const void*)&(optval),sizeof(int));
    struct sockaddr_in server_address;
    server_address.sin_family=AF_INET;
    server_address.sin_port=htons(port);
    server_address.sin_addr.s_addr=htonl(INADDR_ANY);
    int rc;
    if(rc=bind(server_socket,(struct sockaddr*) &server_address,sizeof(server_address))<0)
    {
        perror("ERROr: bind");
        exit(EXIT_FAILURE);
    }
    if(listen(server_socket,1)<0)
    {
        perror("ERROR: listen");
        exit(EXIT_FAILURE);
    }
    socklen_t addressSize = sizeof(server_address);
    int soc;
    while (1)
    {
        soc=accept(server_socket,(struct sockaddr*)&server_address,&addressSize);
        if(soc >=0)
        {
            int res=0;
            res=recv(soc,buff,1024,0);
            if(res<=0)
                break;
            char http[100];
            strcpy(http,"HTTP/1.1 200 OK\r\nContent-Type: text/plain;\r\n\r\n\0");
            if(strncmp(buff,"GET /hostname",13)==0)
            {
                int hostname_len;
                send(soc,http,strlen(http),0);
                send(soc,get_hostname(&hostname_len) ,strlen(buff),0);
             
            }
            else if(strncmp(buff,"GET /cpu-name",12)==0)
            {
                int cpu_len;
                send(soc,http ,strlen(http),0);
                strcpy(buff,get_cpu(&cpu_len));
                send(soc,buff,cpu_len,0);

            }
            else if(strncmp(buff,"GET /load",8)==0)
            {
                send(soc,http,strlen(http),0);
                float value=0;
                write_cpu_per(&value);
                sprintf(buff,"%d%%\n",(int)value);
                send(soc,buff,strlen(buff),0);
            }
            else
            {
                send(soc,http,strlen(http),0);
                send(soc,"400 Bad Request\n",6,0);
            }
        }
    }
    close(server_socket);

}