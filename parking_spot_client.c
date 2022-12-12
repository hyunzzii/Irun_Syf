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
#include <wiringPi.h>
#include <wiringPiI2C.h>

#define LCDAddr 0x27

#define IN 0
#define OUT 1
#define LOW 0
#define HIGH 1

#define R1 21
#define G1 20
#define B1 16

#define R2 26
#define G2 19
#define B2 13

#define R3 24
#define G3 23
#define B3 18

#define R4 22 
#define G4 27
#define B4 17

char msg[10] ="0000";

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



void *lcd_thd(){
	char s[10];
	
    fd = wiringPiI2CSetup(LCDAddr);
    init();


	if(msg[0]=='3' || msg[2]=='3'){
		write_l(0, 0, "Don't worry!");
		write_l(1, 1, "CCTV is on.");
	}
	else if(msg[0]=='4' || msg[2]=='4'){
		write_l(0, 0, "Emergency!!");
		write_l(1, 1, "Fire!!");
	}
    delay(15000);
    clear();
    
    pthread_exit(0);
}

void *lcd_main(){
    int thr_id;
    int status;
    pthread_t p_thread;
    
    while(1){
        if(msg[0] != '3' && msg[0]!='4') continue;
    
    thr_id = pthread_create(&p_thread,NULL,lcd_thd,NULL);
    pthread_join(p_thread,&status);
    }
}


int ftoT(int f){
	int T;
	T=((float)1/f)*1000000000;
	return T;
}


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

    #define DIRECTION_MAX 40
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
    #define VALUE_MAX 40
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

static int
GPIOWrite(int pin, int value)
{
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
    }

    close(fd);
    return(0);
}

void error_handling(char *message){
    fputs(message,stderr);
    fputc('\n',stderr);
    exit(1);
}

void *led(){
     //Enable GPIO pins
    if (-1 == GPIOExport(R1) || -1 == GPIOExport(R2)|| -1 == GPIOExport(R3)|| -1 == GPIOExport(R4))
        return(1);
    if (-1 == GPIOExport(G1) || -1 == GPIOExport(G2)|| -1 == GPIOExport(G3)|| -1 == GPIOExport(G4))
        return(1);
    if (-1 == GPIOExport(B1) || -1 == GPIOExport(B2)|| -1 == GPIOExport(B3)|| -1 == GPIOExport(B4))
        return(1);    

    //Set GPIO directions
    if (-1 == GPIODirection(R1, OUT) || -1 == GPIODirection(R2, OUT) || -1 == GPIODirection(R3, OUT) || -1 == GPIODirection(R4, OUT))
        return(2);
    if (-1 == GPIODirection(G1, OUT) || -1 == GPIODirection(G2, OUT) || -1 == GPIODirection(G3, OUT) || -1 == GPIODirection(G4, OUT))
        return(2);
    if (-1 == GPIODirection(B1, OUT) || -1 == GPIODirection(B2, OUT) || -1 == GPIODirection(B3, OUT) || -1 == GPIODirection(B4, OUT))
        return(2); 
        
    while(1){		

        if(msg[0]=='0'){
            GPIOWrite(G1, 1);
            GPIOWrite(R1, 0);
            GPIOWrite(B1, 0);
        }
        if(msg[0]=='1'){
			GPIOWrite(R1, 1);
            GPIOWrite(G1, 0);
            GPIOWrite(B1, 0);
        }
        if(msg[0]=='2'){
            GPIOWrite(B1, 1);
            GPIOWrite(G1, 0);
            GPIOWrite(R1, 0);
        }
        if(msg[1]=='0'){
            GPIOWrite(G2, 1);
            GPIOWrite(R2, 0);
            GPIOWrite(B2, 0);
        }
        if(msg[1]=='1'){
			GPIOWrite(R2, 1);
            GPIOWrite(G2, 0);
            GPIOWrite(B2, 0);
        }
        if(msg[1]=='2'){
            GPIOWrite(B2, 1);
            GPIOWrite(G2, 0);
            GPIOWrite(R2, 0);
        }
        if(msg[2]=='0'){
            GPIOWrite(G3, 1);
            GPIOWrite(R3, 0);
            GPIOWrite(B3, 0);
        }
        if(msg[2]=='1'){
			GPIOWrite(R3, 1);
            GPIOWrite(G3, 0);
            GPIOWrite(B3, 0);
        }
        if(msg[2]=='2'){
            GPIOWrite(B3, 1);
            GPIOWrite(G3, 0);
            GPIOWrite(R3, 0);
        }
        if(msg[3]=='0'){
            GPIOWrite(G4, 1);
            GPIOWrite(R4, 0);
            GPIOWrite(B4, 0);
        }
        if(msg[3]=='1'){
			GPIOWrite(R4, 1);
            GPIOWrite(G4, 0);
            GPIOWrite(B4, 0);
        }
        if(msg[3]=='2'){
            GPIOWrite(B4, 1);
            GPIOWrite(G4, 0);
            GPIOWrite(R4, 0);
        }
        usleep(100*100);
    }
    if (-1 == GPIOUnexport(R1)||-1 == GPIOUnexport(R2)||-1 == GPIOUnexport(R3)||-1 == GPIOUnexport(R4))
        return(4);
    if (-1 == GPIOUnexport(G1)||-1 == GPIOUnexport(G2)||-1 == GPIOUnexport(G3)||-1 == GPIOUnexport(G4))
        return(4);
    if (-1 == GPIOUnexport(B1)||-1 == GPIOUnexport(B2)||-1 == GPIOUnexport(B3)||-1 == GPIOUnexport(B4))
        return(4);
        
    pthread_exit(0);
}

int main(int argc, char *argv[]) {
	int sock;
	int str_len;
    int mode;
    char buf[3]="2";
    struct sockaddr_in serv_addr;
    
    printf("mode input :");
    scanf("%d",&mode);
    

    pthread_t p_thread[3];
	int thr_id;
	int status;
	char p1[] = "thread_1";
	char p2[] = "thread_2";

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
    
    if(!mode)
        thr_id = pthread_create(&p_thread[0], NULL,led,(void*)p1);
	else
        thr_id = pthread_create(&p_thread[1],NULL,lcd_main,(void*)p2);
    
	while(1){
		str_len = read(sock, msg, sizeof(msg));
        printf("msg = %s\nread value length: %d\n",msg,strlen(msg));
        
        if(str_len == -1)
			error_handling("read() error");
		usleep(100*100);
	}
	
    //close(sock);

    //Disable GPIO pins
   
    return(0);
    }
