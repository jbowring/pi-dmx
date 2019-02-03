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
