#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <wiringPi.h>
#include <wiringPiI2C.h>


#define BUFFER_MAX 3
#define DIRECTION_MAX 256

#define IN 0
#define OUT 1
//#define PWM 0

#define LOW 0
#define HIGH 1

#define POUT 20

#define VALUE_MAX 256

#define LCDAddr    0x27

int distance[2]={0,};
int car_num[5];
clock_t car_time[5];

static int GPIOExport(int pin)
{
#define BUFFER_MAX 3
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/export", O_WRONLY);
	if (-1 == fd) 
	{
		fprintf(stderr, "Failed to open export for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

static int GPIODirection(int pin, int dir)
{
	static const char s_directions_str[]  = "in\0out";
	char path[DIRECTION_MAX] = "/sys/class/gpio/gpio17/direction";
	int fd;

	snprintf(path, DIRECTION_MAX, "/sys/class/gpio/gpio%d/direction", pin);
	fd = open(path, O_WRONLY);
	if (-1 == fd) {
		fprintf(stderr, "Failed to open gpio direction for writing!\n");
		return(-1);
	}

	if (-1 == write(fd, &s_directions_str[IN == dir ? 0 : 3], IN == dir ? 2 : 3)) 
	{
		fprintf(stderr, "Failed to set direction!\n");
		return(-1);
	}

	close(fd);
	return(0);
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

static int GPIOUnexport(int pin)
{
	char buffer[BUFFER_MAX];
	ssize_t bytes_written;
	int fd;

	fd = open("/sys/class/gpio/unexport", O_WRONLY);
	if (-1 == fd) 
	{
		fprintf(stderr, "Failed to open unexport for writing!\n");
		return(-1);
	}

	bytes_written = snprintf(buffer, BUFFER_MAX, "%d", pin);
	write(fd, buffer, bytes_written);
	close(fd);
	return(0);
}

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

// ------------- LCD -------------

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

void *lcd_thd(char s[]){
    char* string[2];
    
    string[0] = strtok(s," ");
    string[1] = strtok(NULL," ");
    
    strcat(string[1]," won\0");
    
    fd = wiringPiI2CSetup(LCDAddr);
    init();
    write_l(0, 0, string[0]);
    write_l(1, 1, string[1]);
    delay(8000);
    clear();
    
    pthread_exit(0);
}

void *led_thd(){
   int repeat = 80;

   do{
      if (-1 == GPIOWrite(POUT, repeat % 2))
	   pthread_exit(0);
      usleep(1000 * 100);
   }while (repeat--);
	
   pthread_exit(0);
}

void *input_thd(int PWM){
   int num=0;
   
   PWMExport(PWM); // pwm0 is gpio18
   PWMWritePeriod(PWM, 20000000);
   PWMWriteDutyCycle(PWM, 1700000);
   PWMEnable(PWM);
   
   while(1){
      if(!distance[0] || distance[0]>10)
	 continue;
      
      
      printf("<IN>\nInput car number: ");
      scanf("%d",&car_num[num]);
      car_time[num]=clock();
      
      for(int i = 1700000; i >= 700000; i -=20000) {
	 PWMWriteDutyCycle(PWM, i);
	 usleep(20000);
      }
      usleep(4000000);
       for(int i = 700000; i < 1700000; i +=20000) {
	 PWMWriteDutyCycle(PWM, i);
	 usleep(20000);
      }
      
   }
   
   PWMUnexport(PWM);
   pthread_exit(0);
}


void *output_thd(int PWM){
   int num;
   char s[10];
   
   PWMExport(PWM); // pwm0 is gpio18
   PWMWritePeriod(PWM, 20000000);
   PWMWriteDutyCycle(PWM, 1700000);
   PWMEnable(PWM);
   
   while(1){
      if(!distance[1] || distance[1]>10)
	 continue;
      
      printf("<OUT>\nInput car number: ");
      scanf("%d",&num);
      
      for(int i = 0;i<5;i++)
      {
	 if(num==car_num[i]){
	    num=i;
	    break;
	 }
      }
      //snprintf(s,5,"%d",car_num[num]);
            
	int park_time = (int) clock() - car_time[num];
	snprintf(s,10,"%d %d",car_num[num],park_time/1000000);
      
      pthread_t p_thread[2];
      int thr_id;
      int status;
      
      thr_id = pthread_create(&p_thread,NULL,lcd_thd,s);
      thr_id = pthread_create(&p_thread,NULL,led_thd,s);
      
      printf("time: %d\n",park_time);
      for(int i = 1700000; i >= 700000; i -=20000) {
	 PWMWriteDutyCycle(PWM, i);
	 usleep(20000);
      }
      usleep(4000000);
      for(int i = 700000; i < 1700000; i +=20000) {
	 PWMWriteDutyCycle(PWM, i);
	 usleep(20000);
      }
      
      pthread_join(p_thread[0],(void**)&status);
      pthread_join(p_thread[1],(void**)&status);
      
   }
   
   PWMUnexport(PWM);
   pthread_exit(0);
}

int flag=0;
void stop(int PWM){
   PWMExport(PWM); // pwm0 is gpio18
   PWMWritePeriod(PWM, 20000000);
   PWMWriteDutyCycle(PWM, 1700000);
   PWMEnable(PWM);
}   


void error_handling(char *message){
	fputs(message,stderr);
	fputc('\n',stderr);
	exit(1);
}

int main(int argc, char *argv[]) {
	int sock;
	struct sockaddr_in serv_addr;
	char msg[10],buf[3]="0";
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
	
	if (-1 == GPIOExport(POUT)){
	    error_handling("Export Error!\n");
	    pthread_exit(0);
	}
		
	// Set GPIO directions
	if (-1 == GPIODirection(POUT, OUT)){
	    error_handling("Direction Error!\n");
	    pthread_exit(0);
	}
		

	pthread_t p_thread[2];
	int thr_id;
	int status;
	
	thr_id = pthread_create(&p_thread[0],NULL,input_thd,1);
	thr_id = pthread_create(&p_thread[1],NULL,output_thd,0);
	
	while(1){
	       str_len = read(sock,msg,sizeof(msg));
	       
	       if(str_len == -1)
		  error_handling("read() error");

	       //printf("msg = %s\n",msg);
	       
	       distance[1]=atoi(strtok(msg," "));
	       distance[0]=atoi(strtok(NULL," "));
    
	       usleep(500 * 100);
	 }
	// Disable GPIO pins
	GPIOUnexport(POUT);
	
	pthread_join(p_thread[0],(void**)&status);
	pthread_join(p_thread[1],(void**)&status);
	close(sock);

	return(0);
}

