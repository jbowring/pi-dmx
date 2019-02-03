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
#include <sys/select.h>

int setBaud(int fh, int rate);
void writeDMX(unsigned char data[], int bytes);
void sig_handler(int signo);
void setup();

int uart0_filestream = -1;
int DMX_pipe = -1;

int main (int argc, char **argv){
    if(argc != 2 || strtol(argv[1], NULL, 10) < 1 || 512 < strtol(argv[1], NULL, 10)) {
        fprintf(stderr, "usage: DMX universe_size (0-512)\n", argc);
        exit(-1);
    }

    const int MAX_CHANNEL = strtol(argv[1], NULL, 10);
    unsigned char data[MAX_CHANNEL + 1];
    
    memset(data, 0, MAX_CHANNEL + 1);

	setup();
	
	#define MAX_READ 64
	
	struct timespec deci = {
	    .tv_nsec = 50000000
	};
	char buf[MAX_READ], * token;
	long channel, value;
	int bytes, max;
	fd_set readfds;
	
	while(true) {
        FD_SET(DMX_pipe, &readfds);
	    if (pselect(DMX_pipe + 1, &readfds, NULL, NULL, &deci, NULL) > 0) {
            bytes = read(DMX_pipe, buf, MAX_READ);  // Read pipe
            if(bytes > 0) {
                buf[bytes] = '\0';  // Terminate buffer	        
                max = 0;
            
                for(token = strtok(buf, ":"); token; token = strtok(NULL, ":")) {
                    if(token)
                        channel = strtol(token, NULL, 10);
                    else
                        continue;
                    
                    if(token = strtok(NULL, " "))
                        value = strtol(token, NULL, 10);
                    else
                        continue;

                    if (1 <= channel && channel <= MAX_CHANNEL && 0 <= value && value <= 255) {
                        fprintf(stderr, "Setting channel %d to %d\n", (int) channel, (int) value);
                        data[channel] = value;
                        if (channel > max)
                            max = channel;
                    } else
                        fprintf(stderr, "Channel %ld or value %ld out of range\n", channel, value);
                }
            }
        }
        writeDMX(data, MAX_CHANNEL + 1);
	}
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
    
    mkfifo("/run/dmx_pipe", 0777);
    chown("/run/dmx_pipe", 1000, 0);
    
    DMX_pipe = open("/run/dmx_pipe", O_NONBLOCK | O_RDONLY);
    if (DMX_pipe == -1)
	    perror("Error - Unable to open pipe");
}

int setBaud(int fh, int rate) {
    struct termios2 tio;

    if (ioctl(fh, TCGETS2, &tio) < 0)   // get current uart state
        return -1;

    tio.c_cflag &= ~CBAUD;
//     tio.c_cflag &= ~(CBAUD & CREAD);
//     tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
//     tio.c_oflag &= ~OPOST;
    tio.c_cflag |= BOTHER | CSTOPB;
    tio.c_ispeed = rate;
    tio.c_ospeed = rate;      // set custom speed directly
    if (ioctl(fh, TCSETS2, &tio) < 0)   // set uart state
       return -1;
       
    if (ioctl(fh, TCGETS2, &tio) < 0)   // get current uart state
        printf("Error getting altered settings from UART\n");
    else
        printf("UART speed: %i baud\n", tio.c_ospeed);

   return 0;
}

void sig_handler(int signo) {
    if(signo == SIGINT)
        fprintf(stderr, "\nSIGINT RECEIVED\n");
    else if(signo == SIGQUIT)
        fprintf(stderr, "\nSIGQUIT RECEIVED\n");
    else
        fprintf(stderr, "\nSIGNAL RECEIVED\n");
        
    unsigned char data[513] = {0};
    writeDMX(data, 513);
    close(uart0_filestream);
    close(DMX_pipe);
    exit(0);
}

void writeDMX(unsigned char data[], int bytes) {
    #define MBB 1000    * 1000
    #define SFB 1000    * 1000  //176
    #define MAB 12      * 1000  //12
    
    static struct timespec MBB_t, MAB_t, SFB_t;
    
    MBB_t.tv_nsec = MBB;
    SFB_t.tv_nsec = SFB;
    MAB_t.tv_nsec = MAB;

    while(nanosleep(&MBB_t, &MBB_t));
    digitalWrite(1, 0);
    while(nanosleep(&SFB_t, &SFB_t));
    digitalWrite(1, 1);
    while(nanosleep(&MAB_t, &MAB_t));
    write(uart0_filestream, data, bytes);
}

// void setStrobe(int speed, int brightness) {
//     static const char fit[] = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 23, 29, 34, 39, 45, 52, 61, 73, 85, 95, 102, 107, 112, 116, 120, 123, 126, 129, 132, 135, 138, 140, 143, 145, 148, 150, 152, 155, 157, 159, 161, 163, 166, 168, 170, 172, 174, 176, 177, 179, 181, 183, 185, 186, 188, 189, 191, 192, 193, 195, 196, 197, 198, 199, 201, 202, 203, 204, 205, 206, 207, 208, 209, 210, 211, 212, 213, 213, 214, 215, 215, 216, 217, 217, 218, 218, 219, 219, 220, 220, 221, 221, 222, 222, 223, 223, 223, 223, 224, 224, 225, 225, 225, 226, 226, 227, 227, 227, 228, 228, 228, 229, 229, 230, 230, 230, 231, 231, 231, 232, 232, 232, 232, 233, 233, 233, 234, 234, 234, 235, 235, 235, 235, 236, 236, 236, 236, 237, 237, 237, 237, 238, 238, 238, 238, 239, 239, 239, 239, 240, 240, 240, 240, 241, 241, 241, 241, 241, 242, 242, 242, 242, 242, 243, 243, 243, 243, 243, 244, 244, 244, 244, 244, 245, 245, 245, 245, 245, 246, 246, 246, 246, 246, 246, 247, 247, 247, 247, 247, 247, 248, 248, 248, 248, 248, 248, 249, 249, 249, 249, 249, 249, 250, 250, 250, 250, 250, 250, 250, 251, 251, 251, 251, 251, 251, 252, 252, 252, 252, 252, 252, 252, 253, 253, 253, 253, 253, 253, 253, 253, 254, 254, 254, 254, 254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255};
//     #define CHANNEL 1
//     unsigned char data[2][2] = {{CHANNEL,fit[speed]},{CHANNEL+1,brightness}};
//     writeDMX(data, 2);
// }

// void print(char * buf, int bytes) {
//     size_t n;
//     for(n = 0; n < bytes; ++n)
//         buf[n] ? putchar(buf[n]) : fputs("\\0", stdout);
//     puts("\n");
// }

// void ramp(bool up) {
//     if(up)
//         for(int speed = 0; speed < 256; speed++)
//             setStrobe(speed, speed/8);
//     else
//         for(int speed = 255; speed >= 0; speed--)
//             setStrobe(speed, speed/8);
// }

// int countChar(char * str, char c) {
//     int i = 1;
//     char *pch = strchr(str, c);
//     while (pch) {
//       i++;
//       pch = strchr(pch + 1, c);
//     }
//     return i;
// }
