#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#define IN 0
#define OUT 1
#define VALUE_MAX 400

#define LOW 0
#define HIGH 1

#define UIN1 23
#define UOUT1 18
#define UIN2 25
#define UOUT2 24
#define UIN3 16
#define UOUT3 12
#define UIN4 21
#define UOUT4 20

#define IR1 4
#define IR2 17
#define IR3 6
#define IR4 19

void error_handling(char *message){
   fputs(message,stderr);
   fputc('\n',stderr);
   exit(1);
}

char msg[5];
char prev[5]={'0','0','0','0'};
int flag;

static int
GPIOUnexport(int pin)
{
#define BUFFER_MAX 3
   char buffer[BUFFER_MAX];
   ssize_t bytes_written;
   int fd;

   fd = open("/sys/class/gpio/unexport", O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open unexport for writing!\n");
      return(-1);
   }

   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return(0);
}

static int
GPIOExport(int pin)
{
#define BUFFER_MAX 3
   char buffer[BUFFER_MAX];
   ssize_t bytes_written;
   int fd;

   fd = open("/sys/class/gpio/export", O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open export for writing!\n");
      return(-1);
   }

   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return(0);
}

static int
GPIODirection(int pin, int dir)
{
   static const char s_directions_str[]  = "in\0out";

#define DIRECTION_MAX 256
   char path[DIRECTION_MAX];
   int fd;

   snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open gpio direction for writing!\n");
      return(-1);
   }

   if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) {
      fprintf(stderr, "Failed to set direction!\n");
      return(-1);
   }

   close(fd);
   return(0);
}

static int
GPIORead(int pin)
{
   char path[VALUE_MAX];
   char value_str[3];
   int fd;

   snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_RDONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open gpio value for reading!\n");
      return(-1);
   }

   if (-1 == read(fd, value_str, 3)) {
      fprintf(stderr, "Failed to read value!\n");
      return(-1);
   }

   close(fd);

   return(atoi(value_str));
}

static int GPIOWrite(int pin, int value)
{
   static const char s_values_str[] = "01";

   char path[VALUE_MAX];
   int fd;

   snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_WRONLY);
   if (-1 == fd) 
   {
      fprintf(stderr, "Failed to open gpio value for writing!\n");
      return(-1);
   }

   if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) 
   {
      fprintf(stderr, "Failed to write value!\n");
      return(-1);
   }

   close(fd);
   return(0);
}


void *reading_1(){
    clock_t start_t, end_t;
    double time;
    double distance;
    //Enable GPIO pins
    if(-1 == GPIOExport(UOUT1) || -1 == GPIOExport(UIN1) || -1 == GPIOExport(IR1)){
        printf("gpio export err\n");
        exit(0);
    }
    //wait for writing to export file
    usleep(100000);
   
    // Set GPIO directions
    if( -1 == GPIODirection(UOUT1, OUT) || -1 == GPIODirection(UIN1, IN) || -1 == GPIODirection(IR1, IN)) {
        printf("gpio direction err \n");
        exit(0);
    }
    
    if(-1 == GPIORead(UIN1) || -1 == GPIORead(IR1)){
        printf("gpio reading err \n");
        exit(0);
    }

    //init ultrawave trigger
    GPIOWrite(UOUT1, 0);
    usleep(100000);
    
    msg[0]='0';
    while(1){
    
    if(GPIORead(IR1)==0 && prev[0]!='2' )
        continue;
        
    usleep(2000*1000);
    //start
    if ( -1 == GPIOWrite(UOUT1, 1)){
        printf("gpio write/trigger err\n");
        exit(0);
    }
    // 1sec = 1000000 ultra_sec, 1ms = 1000 ultra_sec
    usleep(10);
    GPIOWrite(UOUT1, 0);
    
    if(flag==0)
        continue;

    while(GPIORead(UIN1) == 0){
        start_t = clock();
    }
    
    while(GPIORead(UIN1) == 1){
        end_t = clock();
    }
    
    time = (double)(end_t - start_t) / CLOCKS_PER_SEC; //ms
    distance = time/2*34000;
    
    if(distance > 900)
        distance = 900;


    if((prev[0]=='2')&&(distance>30 && GPIORead(IR1)==0)){
        //printf("[1]distance : %.2lfcm \nIN\n", distance);
        msg[0] = '2';
    }
    else if((prev[0]=='2')&&(distance<30 && GPIORead(IR1)==0)){
        //printf("[1]distance : %.2lfcm \nOUT\n", distance);
        msg[0] = '1';
    }
    else if((prev[0]!='2')&&(distance>30 /*&& GPIORead(IR1)==0)*/)){\
        //printf("[1]distance : %.2lfcm \nOUT\n", distance);
        msg[0] = '0';
    }
    else if((prev[0]!='2')&&(distance<30 && GPIORead(IR1)==0)){
        //printf("[1]distance : %.2lfcm \nOUT\n", distance);
        msg[0] = '1';
    }
    prev[0] = msg[0];
    
    usleep(1000000);
}
    pthread_exit(0);
}

void *reading_2(){
    clock_t start_t, end_t;
    double distance;
    double time;
    //Enable GPIO pins
    if(-1 == GPIOExport(UOUT2) || -1 == GPIOExport(UIN2) || -1 == GPIOExport(IR2)){
        printf("gpio export err\n");
        exit(0);
    }
    //wait for writing to export file
    usleep(100000);
   
    // Set GPIO directions
    if( -1 == GPIODirection(UOUT2, OUT) || -1 == GPIODirection(UIN2, IN) || -1 == GPIODirection(IR2, IN)) {
        printf("gpio direction err \n");
        exit(0);
    }
    
    if(-1 == GPIORead(UIN2) || -1 == GPIORead(IR2)){
        printf("gpio reading err \n");
        exit(0);
    }

    //init ultrawave trigger
    GPIOWrite(UOUT2, 0);
    usleep(100000);
    
    msg[1]='0';
    
    while(1){
    
    if(GPIORead(IR2)==0 && prev[1]!='2' )
        continue;
        
    usleep(2000*1000);
    //start
    if ( -1 == GPIOWrite(UOUT2, 1)){
        printf("gpio write/trigger err\n");
        exit(0);
    }
    // 1sec = 1000000 ultra_sec, 1ms = 1000 ultra_sec
    usleep(10);
    GPIOWrite(UOUT2, 0);
    
    if(flag==0)
        pthread_exit(0);
    
    while(GPIORead(UIN2) == 0){
        start_t = clock();
    }
    
    while(GPIORead(UIN2) == 1){
        end_t = clock();
    }
    
    time = (double)(end_t - start_t) / CLOCKS_PER_SEC; //ms
    distance = time/2*34000;
    
    if(distance > 900)
        distance = 900;
        
    if((prev[1]=='2')&&(distance>10 && GPIORead(IR2)==0)){
        //printf("[2]distance : %.2lfcm \nIN\n", distance);
        msg[1] = '2';
    }
    else if((prev[1]=='2')&&(distance<10 && GPIORead(IR2)==0)){
        //printf("[2]distance : %.2lfcm \nOUT\n", distance);
        msg[1] = '1';
    }
    else if((prev[1]!='2')&&(distance>10 /*&& GPIORead(IR1)==0*/)){\
        //printf("[2]distance : %.2lfcm \nOUT\n", distance);
        msg[1] = '0';
    }
    else if((prev[1]!='2')&&(distance<10 && GPIORead(IR2)==0)){
        //printf("[2]distance : %.2lfcm \nOUT\n", distance);
        msg[1] = '1';
    }
    prev[1] = msg[1];

    usleep(1000000);
}
    pthread_exit(0);
}

void *reading_3(){
    clock_t start_t, end_t;
    double time;
    double distance;

    //Enable GPIO pins
    if(-1 == GPIOExport(UOUT3) || -1 == GPIOExport(UIN3) || -1 == GPIOExport(IR3)){
        printf("gpio export err\n");
        exit(0);
    }
    //wait for writing to export file
    usleep(100000);
   
    // Set GPIO directions
    if( -1 == GPIODirection(UOUT3, OUT) || -1 == GPIODirection(UIN3, IN) || -1 == GPIODirection(IR3, IN)) {
        printf("gpio direction err \n");
        exit(0);
    }
    
    if(-1 == GPIORead(UIN3) || -1 == GPIORead(IR3)){
        printf("gpio reading err \n");
        exit(0);
    }

    //init ultrawave trigger
    GPIOWrite(UOUT3, 0);
    usleep(100000);
    
    msg[2]='0';
    
    while(1){
        
    if(GPIORead(IR3)==0 && prev[2]!='2' )
        continue;
        
    usleep(2000*1000);

    //start
    if ( -1 == GPIOWrite(UOUT3, 1)){
        printf("gpio write/trigger err\n");
        exit(0);
    }
    // 1sec = 1000000 ultra_sec, 1ms = 1000 ultra_sec
    usleep(10);
    GPIOWrite(UOUT3, 0);
    
    if(flag==0)
        pthread_exit(0);
    
    while(GPIORead(UIN3) == 0){
        start_t = clock();
    }
    
    while(GPIORead(UIN3) == 1){
        end_t = clock();
    }
    
    time = (double)(end_t - start_t) / CLOCKS_PER_SEC; //ms
    distance = time/2*34000;
    
    if(distance > 900)
        distance = 900;
        
    
    if((prev[2]=='2')&&(distance>10 && GPIORead(IR3)==0)){
        //printf("[3]distance : %.2lfcm \nIN\n", distance);
        msg[2] = '2';
    }
    else if((prev[2]=='2')&&(distance<10 && GPIORead(IR3)==0)){
        //printf("[3]distance : %.2lfcm \nOUT\n", distance);
        msg[2] = '1';
    }
    else if((prev[2]!='2')&&(distance>10 /*&& GPIORead(IR1)==0*/)){\
        //printf("[3]distance : %.2lfcm \nOUT\n", distance);
        msg[2] = '0';
    }
    else if((prev[2]!='2')&&(distance<10 && GPIORead(IR3)==0)){
        //printf("[3]distance : %.2lfcm \nOUT\n", distance);
        msg[2] = '1';
    }
    prev[2] = msg[2];
    
    usleep(1000000);
}
    pthread_exit(0);
}

void *reading_4(){
    clock_t start_t, end_t;
    double time;
    double distance;
    msg[3]='0';
    //Enable GPIO pins
    if(-1 == GPIOExport(UOUT4) || -1 == GPIOExport(UIN4) || -1 == GPIOExport(IR4)){
        printf("gpio export err\n");
        exit(0);
    }
    //wait for writing to export file
    usleep(100000);
   
    // Set GPIO directions
    if( -1 == GPIODirection(UOUT4, OUT) || -1 == GPIODirection(UIN4, IN) || -1 == GPIODirection(IR4, IN)) {
        printf("gpio direction err \n");
        exit(0);
    }
    
    if(-1 == GPIORead(UIN4) || -1 == GPIORead(IR4)){
        printf("gpio reading err \n");
        exit(0);
    }

    //init ultrawave trigger
    GPIOWrite(UOUT4, 0);
    usleep(100000);
    
    
    while(1){
    
    if(GPIORead(IR4) == 0 && prev[3]!='2')
        continue;
        
    usleep(2000*1000);
        
    if ( -1 == GPIOWrite(UOUT4, 1)){
        printf("gpio write/trigger err\n");
        exit(0);
    }
    
    // 1sec = 1000000 ultra_sec, 1ms = 1000 ultra_sec
    usleep(10);
    GPIOWrite(UOUT4, 0);
    
    if(flag==0)
        pthread_exit(0);
        
    while(GPIORead(UIN4) == 0){
        start_t = clock();
    }
    
    while(GPIORead(UIN4) == 1){
        end_t = clock();
    }
    
    time = (double)(end_t - start_t) / CLOCKS_PER_SEC; //ms
    distance = time/2*34000;
    
    if(distance > 900)
        distance = 900;
    
    if((prev[3]=='2')&&(distance>10 && GPIORead(IR4)==0)){
        //printf("[4]distance : %.2lfcm \nIN\n", distance);
        msg[3] = '2';
    }
    else if((prev[3]=='2')&&(distance<10 && GPIORead(IR4)==0)){
        //printf("[4]distance : %.2lfcm \nOUT\n", distance);
        msg[3] = '1';
    }
    else if((prev[3]!='2')&&(distance>10 /*&& GPIORead(IR4)==0*/)){\
        //printf("[4]distance : %.2lfcm \nOUT\n", distance);
        msg[3] = '0';
    }
    else if((prev[3]!='2')&&(distance<10 && GPIORead(IR4)==0)){
        //printf("[4]distance : %.2lfcm \nOUT\n", distance);
        msg[3] = '1';
    }
    prev[3] = msg[3];
    
}
    pthread_exit(0);
}

void *reservation(){
    char input;
    char full[5] = "2222";
    
    while(1){
        printf("Do you want to make a reservation??????????????????? [Y or N]: \n");
        scanf("%c",&input);
        if(input == 'Y'){
            flag = 0;
            for(int i = 0; i<4 ; i++){
                if(!(strcmp(msg,full))){
                    printf("*****FU L L ~~~ !!!!*****\n");
                    break;
                }
                    
                if(msg[i]=='0'){
                    prev[i] = '2';
                    printf("[%d] has been reserved !!\n", i+1);
                    break;
                }
            }
            flag =1;
            continue;
        }
        else
            flag = 1;
    }
}

int main(int argc, char *argv[])
{
    int serv_sock,clnt_sock=-1;
    struct sockaddr_in serv_addr,clnt_addr;
    socklen_t clnt_addr_size;

    pthread_t p_thread[5];
    int thr_id;
    int status;
    char p1[] = "thread_1";
    char p2[] = "thread_2";
    char p3[] = "thread_3";
    char p4[] = "thread_4";
    char p5[] = "thread_5";

    if(argc!=2){
        printf("Usage : %s <port>\n",argv[0]); //서버 포트 입력 받기
    }
   
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if(serv_sock == -1)
        error_handling("socket() error");
   
    memset(&serv_addr, 0 , sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if(bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr))==-1)
        error_handling("bind() error");
   
    if(listen(serv_sock,5) == -1)
        error_handling("listen() error");

    if(clnt_sock<0){
        clnt_addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);

    if(clnt_sock == -1)
        error_handling("accept() error");
    }
    
    flag = 1;
    thr_id = pthread_create(&p_thread[4],NULL,reservation,(void*)p5);
    

        thr_id = pthread_create(&p_thread[0],NULL,reading_1,(void*)p1);
        thr_id = pthread_create(&p_thread[1],NULL,reading_2,(void*)p2);
        thr_id = pthread_create(&p_thread[2],NULL,reading_3,(void*)p3);
        thr_id = pthread_create(&p_thread[3],NULL,reading_4,(void*)p4);
        
        while(1){
            //printf("\n msg: %s\n", msg);
            write(clnt_sock, msg, strlen(msg));

            usleep(1000*1000);
        }
    pthread_join(p_thread[0],(void**)&status);
    pthread_join(p_thread[1],(void**)&status);
    pthread_join(p_thread[2],(void**)&status);
    pthread_join(p_thread[3],(void**)&status);

   //Disable GPIO pins
    if(-1 == GPIOUnexport(UIN1) || -1 == GPIOUnexport(UIN2) || -1 == GPIOUnexport(UIN3) || -1 == GPIOUnexport(UIN4))
        return(-1);
    if(-1 == GPIOUnexport(UOUT1) || -1 == GPIOUnexport(UOUT2) || -1 == GPIOUnexport(UOUT3) || -1 == GPIOUnexport(UOUT4))
        return(-1);
    if(-1 == GPIOUnexport(IR1) || -1 == GPIOUnexport(IR2) || -1 == GPIOUnexport(IR3) || -1 == GPIOUnexport(IR4))
        return(-1);

   return 0;
}
