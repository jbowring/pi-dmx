#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stropts.h>
#include <sys/stat.h>
#include <asm/termios.h>
#include <unistd.h>
#include <stdbool.h>
#include <wiringPi.h>
#include <signal.h>
#include <time.h>

static const char fit[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 23, 29, 34, 39, 45, 52, 61, 73, 85, 95, 102, 107, 112, 116, 120, 123, 126, 129, 132, 135, 138, 140, 143, 145, 148, 150, 152, 155, 157, 159, 161, 163, 166, 168, 170, 172, 174, 176, 177, 179, 181, 183, 185, 186, 188, 189, 191, 192, 193, 195, 196, 197, 198, 199, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 213, 214, 215, 215, 216, 217, 217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 223, 223, 224, 224, 225, 225, 225, 226, 226, 227, 227, 227, 228, 228, 228, 229, 229, 230, 230, 230, 231, 231, 231, 232, 232, 232, 232, 233, 233, 233, 234, 234, 234, 235, 235, 235, 235, 236, 236, 236, 236, 237, 237, 237, 237, 238, 238, 238, 238, 239, 239, 239, 239, 240, 240, 240, 240, 241, 241, 241, 241, 241, 242, 242, 242, 242, 242, 243, 243, 243, 243, 243, 244, 244, 244, 244, 244, 245, 245, 245, 245, 245, 246, 246, 246, 246, 246, 246, 247, 247, 247, 247, 247, 247, 248, 248, 248, 248, 248, 248, 249, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250, 250, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255};

int uart0_filestream = -1;
int DMX_pipe = -1;

int setBaud(int fh, int rate)
{
    struct termios2 tio;

    if (fh==-1) return -1;

    if (ioctl(fh, TCGETS2, &tio) < 0)   // get current uart state
        return -1;
    tio.c_cflag &= ~CBAUD;
    tio.c_cflag |= BOTHER | CSTOPB;
    tio.c_ispeed = rate;
    tio.c_ospeed = rate;      // set custom speed directly
    if (ioctl(fh, TCSETS2, &tio) < 0)   // push uart state
       return -1;
       
    if (ioctl(fh, TCGETS2, &tio) < 0)   // get current uart state
        printf("Error getting altered settings from port\n");
    else
        printf("Port speeds are %i in and %i out\n", tio.c_ispeed, tio.c_ospeed);

   return 0;
}

void writeDMX(unsigned char values[][2], short int bytes) {
    #define MBB 1000
    #define MAB 12  //12
    #define SFB 1000 //176
    #define MAX 5
    static const struct timespec MBB_t = {0, MBB*1000};
    static const struct timespec MAB_t = {0, MAB*1000};
    static const struct timespec SFB_t = {0, SFB*1000};
    static unsigned char data[MAX];
    
    static unsigned char max = 0, channel = 0, value = 0;
    
    for (bytes -= 1; bytes >= 0; bytes--) {
        channel = values[bytes][0];
        value = values[bytes][1];
        if (channel != 0 && channel < MAX) {
            printf("Setting channel %d to %d\n", channel, value);
            data[channel] = value;
            if (channel > max)
                max = channel;
        }
    }
    
    max = (max == 0 ? MAX : max + 1);
    
    nanosleep(&MBB_t, NULL);
    digitalWrite(1, 0);
    nanosleep(&SFB_t, NULL);
    digitalWrite(1, 1);
    nanosleep(&MAB_t, NULL);
    write(uart0_filestream, data, max);
}

void setStrobe(int speed, int brightness) {
    #define CHANNEL 1
    unsigned char data[2][2] = {{CHANNEL,fit[speed]},{CHANNEL+1,brightness}};
    writeDMX(data, 2);
}

void ramp(bool up) {
    if(up)
        for(int speed = 0; speed < 256; speed++)
            setStrobe(speed, speed/8);
    else
        for(int speed = 255; speed >= 0; speed--)
            setStrobe(speed, speed/8);
}

void sig_handler(int signo) {
    if(signo == SIGINT)
        printf("\nSIGINT RECIEVED\n");
    else if(signo == SIGQUIT)
        printf("\nSIGQUIT RECIEVED\n");
    else
        printf("\nSIGNAL RECIEVED\n");
        
    setStrobe(0, 0);
    close(uart0_filestream);
    close(DMX_pipe);
    exit(0);
}

void setup() {
    uart0_filestream = open("/dev/ttyAMA0", O_RDWR | O_NOCTTY | O_NDELAY);		//Open in non blocking read/write mode
	if (uart0_filestream == -1) {
	    perror("Error - Unable to open UART");
	    exit(-1);
	}
	
	if(setBaud(uart0_filestream, 250000) == -1) {
	    perror("Error - Setting Baud rate failed");
	    exit(-1);
	}
	
	if (signal(SIGINT, sig_handler) == SIG_ERR)
        perror("Can't catch SIGINT");
    if (signal(SIGQUIT, sig_handler) == SIG_ERR)
        perror("Can't catch SIGQUIT");
	
	if (wiringPiSetup() == -1) {
	    perror("Error - Setting up GPIO failed");
        exit(-1);
    }
        
    pinMode(1, OUTPUT);
    digitalWrite(1, 1);
    
    mkfifo("/var/dmx/pipe", 0777);
    
    DMX_pipe = open("/var/dmx/pipe", O_NONBLOCK | O_RDONLY);
    if (DMX_pipe == -1)
	    perror("Error - Unable to open pipe");
}

void printy(char * buf) {
    for(size_t n = 0; n < 64; ++n)
        buf[n] ? putchar(buf[n]) : fputs("\\0", stdout);
    puts("\n");
}

int countChar(char * str, char c) {
    int i = 1;
    char *pch = strchr(str, c);
    while (pch) {
      i++;
      pch = strchr(pch + 1, c);
    }
    return i;
}

int main (void){
	setup();
	
	const struct timespec milli = {0, 1000000};
	
	#define MAX_READ 64
	
	char buf[MAX_READ];
	char * token;
	unsigned char channel, value, commands;
	int i = 0, j = 0, bytes = 0;
	
	while(true) {
	    for(j = 0; j < 100; j++) {  // Read pipe and pause x100
            bytes = read(DMX_pipe, buf, MAX_READ);  // Read pipe
            if(bytes > 0) {	        
                for(i = bytes - 1; i < MAX_READ; i++)   // Annul rest of buffer
                    buf[i] = '\0';
            
                commands = countChar(buf, ' ');
                unsigned char data[commands][2];
            
                i = 0;
                for(token = strtok(buf, ":"); token; token = strtok(NULL, ":")) {
                    if(token)
                        channel = strtol(token, NULL, 10);
                    else {
                        commands -= 1;
                        continue;
                    }
                    
                    if(token = strtok(NULL, " "))
                        value = strtol(token, NULL, 10);
                    else {
                        commands -= 1;
                        continue;
                    }
                
                    data[i][0] = channel;
                    data[i++][1] = value;
                    printf("Got Channel: '%d'\tValue: '%d'\n", channel, value);
                }
            
                writeDMX(data, commands);   // Write data
                j = 0;
            }
            nanosleep(&milli, NULL);    // Wait 1 ms
        }
        writeDMX(NULL, 0);  // Refresh DMX data
	}
	
	return 0;
}