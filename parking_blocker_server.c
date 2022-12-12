#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <time.h>
#include <pthread.h>

#include <arpa/inet.h>
#include <sys/socket.h>


#define BUFFER_MAX 3
#define DIRECTION_MAX 45

#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1
#define VALUE_MAX 256

#define PIN    24
#define POUT   23

#define PIN2   27
#define POUT2  17

#define B_PIN  20    // 버튼 1
#define B_PIN2 21    // 버튼 2
#define F_PIN  16    // 불꽃 센서
#define I_PIN  26     // 적외선 센서

double distance[2] = {0,};
char msg[10];

static int GPIOExport(int pin) {
   
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

static int GPIOUnexport(int pin){
   char buffer[BUFFER_MAX];
   ssize_t bytes_written;
   int fd;
   
   fd = open("/sys/class/gpio/unexport", O_WRONLY);
   if (-1 == fd){
      fprintf(stderr, "Failed to open unexport for writing! \n");
      return(-1);
   } bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
   write(fd, buffer, bytes_written);
   close(fd);
   return(0);
}

static int GPIODirection(int pin, int dir){
   static const char s_directions_str[] = "in\0out";
   char path[DIRECTION_MAX] = "/sys/class/gpio/gpio%d/direction";
   int fd;
   
   snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
   fd = open(path, O_WRONLY);
   if(-1 == fd){
      fprintf(stderr, "Failed to open gpio direction for writing! \n");
      return(-1);
   }
   if(-1 == write(fd, &s_directions_str[IN == dir? 0 : 3], IN == dir ? 2 : 3)) {
      fprintf(stderr, "Failed to set direction! \n");
      return(-1);
   }
   close(fd);
   return(0);
}

static int GPIORead(int pin) {
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

static int GPIOWrite(int pin, int value) {
   static const char s_values_str[] = "01";
   char path[VALUE_MAX];
   int fd;
   
   snprintf(path, VALUE_MAX, "/sys/class/gpio/gpio%d/value", pin);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open gpio value for writing!\n");
      return(-1);
   }
   
   if (1 != write(fd, &s_values_str[LOW == value ? 0 : 1], 1)) {
      fprintf(stderr, "Failed to write value!\n");
      return(-1);
      
   close(fd);
   return(0);
   }
}
void *ultrawave_thd(void *in_out){
   clock_t start_t, end_t;
   double time;
   int pin,pout;
   
   if(!in_out){
      pin = PIN;
      pout = POUT;
   }
   else{
      pin = PIN2;
      pout = POUT2;
   }
   
   usleep(100000);
   
   // Set GPIO directions
   if( -1 == GPIODirection(pout, OUT) || -1 == GPIODirection(pin, IN)) {
      printf("gpio direction err \n");
      exit(0);
   }
   //init ultrawave trigger
   GPIOWrite(pout, 0);
   usleep(100000);
   //start
   while(1){
      if ( -1 == GPIOWrite(pout, 1)){
         printf("gpio write/trigger err\n");
         exit(0);
      }
      // 1sec = 1000000 ultra_sec, 1ms = 1000 ultra_sec
      usleep(10);
      GPIOWrite(pout, 0);
      
      while(GPIORead(pin) == 0){
         start_t = clock();
      }
      
      while(GPIORead(pin) == 1){
         end_t = clock();
      }
      
      time = (double)(end_t - start_t) / CLOCKS_PER_SEC; //ms
      distance[(int)in_out] = time/2*34000;
      
      if(distance[(int)in_out] > 900)
         distance[(int)in_out] = 900;
         
     // printf("tiem : %.4lf \n", time);
     // printf("distance : %.2lfcm \n", distance);
      
      usleep(500000);
   }
   pthread_exit(0);

}

void *distance_thd(void* arg){
   int clnt_sock =(int)arg;
   char buf[BUFFER_MAX];
   
   while(1){
      snprintf(msg,10,"%d %d",(int)distance[0],(int)distance[1]);
      write(clnt_sock,msg,sizeof(msg));
      printf("msg = %s\n",msg);
      usleep(500*100);
   }
   close(clnt_sock);
   pthread_exit(0);
}

void *button_thd(void* arg){
   int clnt_sock =(int)arg;
   char buf[BUFFER_MAX];
   
   if(-1 == GPIODirection(B_PIN,IN) || GPIODirection(B_PIN2,IN))
      pthread_exit(0);
   
   while(1){
      
         
         snprintf(msg,4,"%d %d",GPIORead(B_PIN),GPIORead(B_PIN2));
         write(clnt_sock,msg,sizeof(msg));

         printf("msg = %s\n",msg);
         usleep(1000*1000);
      }  
      
   close(clnt_sock);
   pthread_exit(0);
}

void *fire_thd(void* arg){
   int clnt_sock =(int)arg;
   
   if(-1 == GPIODirection(F_PIN,IN))
      pthread_exit(0);
   
   while(1){
         snprintf(msg,4,"%d %d",GPIORead(F_PIN)==0?4:0,GPIORead(F_PIN)==0?4:0);
         write(clnt_sock,msg,sizeof(msg));
         usleep(1000*1000);
      }  
      
   close(clnt_sock);
   pthread_exit(0);
}

void *infraRed_thd(void* arg){
   int clnt_sock =(int)arg;
   
   if(-1 == GPIODirection(I_PIN,IN))
      pthread_exit(0);
   
   while(1){
         snprintf(msg,4,"%d %d",GPIORead(I_PIN)==1?3:0,GPIORead(I_PIN)==1?3:0);
         write(clnt_sock,msg,sizeof(msg));
         usleep(1000*1000);
      }  
      
   close(clnt_sock);
   pthread_exit(0);
}

void *camera_thd(void* arg){
   //int clnt_sock =(int)arg;
   //char buf[BUFFER_MAX];
   //char msg[2];
   
   //system("sudo raspistill -vf -t 600000 -tl 1000 -o images/img%04d.jpg");
   system("sudo raspistill -vf -t 20000 -tl 1000 -o images/img%04d.jpg");
   //system("raspivid --segment 3000 -o video/vid%04d.h264");

   // system("sudo raspistill -o images/test.jpg");
   // char **command = (char **)malloc(sizeof(char *) * 10);
   // command[0] = "xdg-open";
   // command[1] = "/home/pi/Pictures/test.jpg";
   // command[2] = NULL;

   


   //close(clnt_sock);
   pthread_exit(0);
}

void *cam__openthd(void* arg){

   char **command = (char **)malloc(sizeof(char *) * 10);
            command[0] = "xdg-open";
            command[1] = "/home/pi/Pictures/test.jpg";
            command[2] = NULL;


   pthread_exit(0);
}


void error_handling(char *message){
   fputs(message,stderr);
   fputc('\n',stderr);
   exit(1);
}


int main(int argc, char *argv[]) {
   int serv_sock,clnt_sock=-1;
   int status;
   struct sockaddr_in serv_addr,clnt_addr;
   socklen_t clnt_addr_size;
   
   pthread_t p_thread[2];
   int thr_id;
   thr_id = pthread_create(&p_thread[0],NULL,ultrawave_thd,(void*)0);
   thr_id = pthread_create(&p_thread[1],NULL,ultrawave_thd,(void*)1);

   
   if (-1 == GPIOExport(B_PIN) || -1 == GPIOExport(B_PIN2) || GPIOExport(F_PIN) || GPIOExport(I_PIN))
		return(1);
   
   if (-1 == GPIOExport(POUT) || -1 == GPIOExport(PIN))
		return(1);
   
   if (-1 == GPIOExport(POUT2) || -1 == GPIOExport(PIN2))
		return(1);
   
   if(argc!=2){
      printf("Usage : %s <port>\n",argv[0]);
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
   
      
   pthread_t t_id;
   while(1){
   
   if(clnt_sock<0){
      clnt_addr_size = sizeof(clnt_addr);
      clnt_sock = accept(serv_sock, (struct sockaddr*)&clnt_addr, &clnt_addr_size);

      if(clnt_sock == -1)
         error_handling("accept() error");
   }
   char clnt_msg[3];
   
   read(clnt_sock,clnt_msg,sizeof(clnt_msg));

   if(!strcmp(clnt_msg,"0")){
      pthread_create(&t_id,NULL,distance_thd,(void*)clnt_sock);
      pthread_detach(t_id);
   }
   
   else if(!strcmp(clnt_msg,"1")){
      //printf("GPIORead(F_PIN): %d\n", !GPIORead(F_PIN));
      pthread_create(&t_id,NULL,button_thd,(void*)clnt_sock);
      pthread_detach(t_id);
      
      pthread_create(&t_id,NULL,fire_thd,(void*)clnt_sock);
      pthread_detach(t_id);
      
      pthread_create(&t_id,NULL,camera_thd,(void*)clnt_sock);
      pthread_detach(t_id);
   }
   else if(!strcmp(clnt_msg,"2")){
      pthread_create(&t_id,NULL,fire_thd,(void*)clnt_sock);
      pthread_detach(t_id);
      
      pthread_create(&t_id,NULL,infraRed_thd,(void*)clnt_sock);
      pthread_detach(t_id);
   }
   clnt_sock=-1;
   }
   
   if(-1 == GPIOUnexport(POUT) || GPIOUnexport(PIN) || GPIOUnexport(B_PIN) || GPIOUnexport(F_PIN) || GPIOUnexport(I_PIN))
      return 4;
   if(-1 == GPIOUnexport(POUT2) || GPIOUnexport(PIN2) || GPIOUnexport(B_PIN2))
      return 4;
      
   close(serv_sock);

   return(0);
}
