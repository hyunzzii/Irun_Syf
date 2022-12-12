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


#include <wiringPi.h>
#include <wiringPiI2C.h>


#define BUFFER_MAX 3
#define DIRECTION_MAX 256

#define IN 0
#define OUT 1

#define LOW 0
#define HIGH 1
#define VALUE_MAX 256

#define PIN 17

#define C 131
#define D 147
#define E 165
#define F 175
#define G 196
#define A 220
#define B 247

#define C2 267
#define D2 294
#define E2 330
#define F2 349
#define G2 392
#define A2 440
#define B2 494

#define LCDAddr 0x27

int button[2]={0,};
int song = 0;

static int PWMExport(int pwmnum) {
   char buffer[BUFFER_MAX];
   int bytes_written;
   int fd;
   
   fd = open("/sys/class/pwm/pwmchip0/export", O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open in export!\n");
      return(-1);
   }
   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
   write(fd, buffer, bytes_written);
   close(fd);
   sleep(1);
   return(0);
}

static int PWMUnexport(int pwmnum) {
   char buffer[BUFFER_MAX];
   ssize_t bytes_written;
   int fd;
   
   fd = open("/sys/class/pwm/pwmchip0/unexport", O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open in unexport!\n");
      return(-1);
   }
   
   bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pwmnum);
   write(fd, buffer, bytes_written);
   close(fd);
   
   sleep(1);
   
   return(0);
}

static int PWMEnable(int pwmnum) {
   static const char s_unenable_str[] = "0";
   static const char s_enable_str[] = "1";
   
   char path[DIRECTION_MAX];
   int fd;
   
   snprintf(path, DIRECTION_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open in enable!\n");
      return -1;
   }
   
   write(fd, s_unenable_str, strlen(s_unenable_str));
   close(fd);
   
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open in enable!\n");
      return -1;
   }
   
   write(fd, s_enable_str, strlen(s_enable_str));
   close(fd);
   return(0);
}

static int PWMUnable(int pwmnum) {
   static const char s_unable_str[] = "0";
   char path[DIRECTION_MAX];
   int fd;
   
   snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/enable", pwmnum);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open in enable!\n");
      return(-1);
   }
   
   write(fd, s_unable_str, strlen(s_unable_str));
   close(fd);
   
   return(0);
}

static int PWMWritePeriod(int pwmnum, int value) {
   char s_values_str[VALUE_MAX];
   char path[VALUE_MAX];
   int fd, byte;
   
   snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/period", pwmnum);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open in period!\n");
      return(-1);
   }
   
   byte = snprintf(s_values_str, 10, "%d", value);
   
   if (-1 == write(fd, s_values_str, byte)) {
      fprintf(stderr, "Failed to wirte value in period!\n");
      close(fd);
      return(-1);
   }
   
   close(fd);
   return(0);
}

static int PWMWriteDutyCycle(int pwmnum, int value) {
   char s_values_str[VALUE_MAX];
   char path[VALUE_MAX];
   int fd, byte;
   
   snprintf(path, VALUE_MAX, "/sys/class/pwm/pwmchip0/pwm%d/duty_cycle", pwmnum);
   fd = open(path, O_WRONLY);
   if (-1 == fd) {
      fprintf(stderr, "Failed to open in duty_cycle!\n");
      return(-1);
   }
   
   byte = snprintf(s_values_str, 10, "%d", value);
   
   if (-1 == write(fd, s_values_str, byte)) {
      fprintf(stderr, "Failed to write value in duty_cycle!\n");
      close(fd);
      return(-1);
   }
   
   close(fd);
   return(0);
}
int fd;
int BLEN=1;
 
void write_word(int data){
    int temp = data;
    if ( BLEN == 1 )
        temp |= 0x08;
    else
        temp &= 0xF7;
    wiringPiI2CWrite(fd, temp);
}

void send_command(int comm){
    int buf;
    // Send bit7-4 firstly
    buf = comm & 0xF0;
    buf |= 0x04;                    // RS = 0, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB;                    // Make EN = 0
    write_word(buf);

    // Send bit3-0 secondly
    buf = (comm & 0x0F) << 4;
    buf |= 0x04;                    // RS = 0, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB;                    // Make EN = 0
    write_word(buf);
}

void send_data(int data){
    int buf;
    // Send bit7-4 firstly
    buf = data & 0xF0;
    buf |= 0x05;                    // RS = 1, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB;                    // Make EN = 0
    write_word(buf);

    // Send bit3-0 secondly
    buf = (data & 0x0F) << 4;
    buf |= 0x05;                    // RS = 1, RW = 0, EN = 1
    write_word(buf);
    delay(2);
    buf &= 0xFB;                    // Make EN = 0
    write_word(buf);
}

void init(){
    send_command(0x33);     // Must initialize to 8-line mode at first
    delay(5);
    send_command(0x32);     // Then initialize to 4-line mode
    delay(5);
    send_command(0x28);     // 2 Lines & 5*7 dots
    delay(5);
    send_command(0x0C);     // Enable display without cursor
    delay(5);
    send_command(0x01);     // Clear Screen
    wiringPiI2CWrite(fd, 0x08);
}

void clear(){
    send_command(0x01);     //clear Screen
}

void write_l(int x, int y, char data[]){
    int addr, i;
    int tmp;
    if (x < 0)  x = 0;
    if (x > 15) x = 15;
    if (y < 0)  y = 0;
    if (y > 1)  y = 1;

    // Move cursor
    addr = 0x80 + 0x40 * y + x;
    send_command(addr);

    tmp = strlen(data);
    for (i = 0; i < tmp; i++){
        send_data(data[i]);
    }
}

void *lcd_thd(int num){
	char s[10];
	
    fd = wiringPiI2CSetup(LCDAddr);
    init();

   if(button[0]==1 || button[1]==1)
      snprintf(s,10,"%d Zone",num);
   else if(button[0]==4 || button[1]==4)
      snprintf(s,10,"Fire!!!!");
   // else if(button[0]==3 || button[1]==3)
   //    snprintf(s, 15, "CCTV is on");

    write_l(0, 0, "Emergency!");
    write_l(1, 1, s);
    delay(15000);
    clear();
    
    pthread_exit(0);
}

int ftoT(int f){
	int T;
	T=((float)1/f)*1000000000;
	return T;
}

void *howls_moving_castle(int PWM){

   PWMExport(PWM); // pwm0 is gpio18
   PWMWriteDutyCycle(PWM, 1700000);
   PWMEnable(PWM);
   
   PWMWritePeriod(PWM, ftoT(E));
   usleep(500000);
   PWMWritePeriod(PWM, ftoT(A));
   usleep(500000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(500000);
   
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(800000);
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(800000);
   
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(500000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(500000);
   PWMWritePeriod(PWM, ftoT(B));
   usleep(500000);
   
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(1600000);
   
   PWMWritePeriod(PWM, ftoT(A));
   usleep(500000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(500000);
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(500000);
   
   PWMWritePeriod(PWM, ftoT(A2));
   usleep(800000);
   
   PWMWritePeriod(PWM, ftoT(A2));
   usleep(600000);
   
   PWMWritePeriod(PWM, ftoT(A2));
   usleep(500000);
   PWMWritePeriod(PWM, ftoT(B2));
   usleep(500000);
   PWMWritePeriod(PWM, ftoT(G2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(F2));
   usleep(250000);
   
   PWMWritePeriod(PWM, ftoT(G2));
   usleep(1200000);
   
   PWMUnexport(PWM);

   song = 0;

   pthread_exit(0);
	
}

void *under_the_sea(int PWM){

   PWMExport(PWM); // pwm0 is gpio18
   PWMWriteDutyCycle(PWM, 1700000);
   PWMEnable(PWM);
   
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(350000);
   PWMWritePeriod(PWM, ftoT(A));
   usleep(600000);
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(350000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(600000);

   PWMWritePeriod(PWM, ftoT(E2));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(350000);
   PWMWritePeriod(PWM, ftoT(A));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(300000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(350000);
   PWMWritePeriod(PWM, ftoT(A));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(500000);

   PWMWritePeriod(PWM, ftoT(D2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(350000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(300000);
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(600000);

   PWMWritePeriod(PWM, ftoT(E2));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(350000);
   PWMWritePeriod(PWM, ftoT(A));
   usleep(600000);
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(350000);
   PWMWritePeriod(PWM, ftoT(G));
   usleep(600000);
   PWMWritePeriod(PWM, ftoT(E2));
   usleep(400000);
   PWMWritePeriod(PWM, ftoT(D2));
   usleep(250000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(350000);
   PWMWritePeriod(PWM, ftoT(C2));
   usleep(600000);
   
   PWMUnexport(PWM);
   pthread_exit(0);
	
}

void error_handling(char *message){
   fputs(message,stderr);
   fputc('\n',stderr);
   exit(1);
}


int main(int argc, char *argv[]){
   int sock;
	struct sockaddr_in serv_addr;
	char msg[10],buf[3]="1";
	int str_len;
	double time;
	if(argc!=3){
		printf("Usage : %s <IP> <port>\n",argv[0]);
		exit(1);
	}

	sock = socket(PF_INET, SOCK_STREAM, 0);
	
	if(sock == -1)
		error_handling("socket() error");

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	serv_addr.sin_port = htons(atoi(argv[2]));
	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
		error_handling("connect() error");
    
    write(sock,buf,sizeof(buf));
     
   pthread_t p_thread[2];
	int thr_id;
	
    while(1){
	   str_len = read(sock,msg,sizeof(msg));
	       
	   if(str_len == -1)
		 error_handling("read() error");
		
      printf("msg = %s\n", msg);

	   button[0]=atoi(strtok(msg," "));
	   button[1]=atoi(strtok(NULL," "));
	   
	   if(button[0] || button[1]){
         if((button[0]==1 || button[1]==1) && song == 0){
            song = 1;
		      thr_id = pthread_create(&p_thread[0],NULL,howls_moving_castle,0);
         }
         if(button[0]==4 || button[1]==4)
		      thr_id = pthread_create(&p_thread[0],NULL,under_the_sea,0);
		   thr_id = pthread_create(&p_thread[1],NULL,lcd_thd,button[0] ? 1 : 2);
		}
	   usleep(500*1000);
	   //usleep(500*1000);
   }
   
   close(sock);	
   return 0;
	
}
