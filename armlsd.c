/* 
   ADuC702x Serial Downloader
   $Id: armlsd.c 36 2006-08-14 16:13:54Z okazaki $
   
   Atsuya Okazaki (okazaki at users.sourceforge.jp)

   Usage
   $ ./armwsd sample.hex

   Serial port: /dev/ttyS0

   References
   [1] AN-724: ADuC702x Serial Download Protocol
     http://www.analog.com/UploadedFiles/Application_Notes/409819638AN_724_A.pdf
   [2] Serial Programming HOWTO
     3. Program Examples
       3.2. Non-Canonical Input Processing
       http://www.tldp.org/HOWTO/Serial-Programming-HOWTO/x115.html#AEN129 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <strings.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>

unsigned char optparse(int argc, char *argv[], char **device, char **hexfile);
#define CMD_ERROR (0x00)
#define CMD_ERASE (0x01)
#define CMD_WRITE (0x02)
#define CMD_VERIFY (0x04)
#define CMD_RUN (0x08)
void usage(char *argv[]);

/* #define BAUDRATE B38400 */
#define BAUDRATE B9600
#define DEFAULT_DEVICE "/dev/ttyS0"
#define _POSIX_SOURCE 1 /* POSIX compliant source */
        
int connect(char *device, struct termios *oldtio);
#define CONNECT_TIMEOUT_SEC (3)
struct intelhex {
    unsigned char bytecount;
    unsigned char address[2];
    unsigned char recordtype;
    unsigned char databyte[0xFF];
    unsigned char checksum;
};
#define RECORDTYPE_DATA (0x00)
#define RECORDTYPE_EOF  (0x01)
int interpret(struct intelhex *ih, char *linebuf);
#define INTERPRET_UNDEF_FMT (1)
#define INTERPRET_UNIMPLEMENTED_RECORDTYPE (2)
struct packet {
    unsigned char startid[2];
    unsigned char numdata;
    unsigned char cmd;
    unsigned char address[4];
    unsigned char rest[251];	/* Data 250 + Checksum 1 */
};
int mkpkt(struct packet *p, struct intelhex *ih, unsigned char cmd);
unsigned char mkchecksum(struct packet *p);
int send(int fd, struct packet *p);
#define SEND_NOERR (0)
#define SEND_NACK  (1)
#define SEND_UNDEF (2)

#define MAX_LINE_SIZE (259)

int main(int argc, char *argv[])
{
    unsigned char cmd = CMD_ERROR;
    char *device = DEFAULT_DEVICE;
    char *hexfile = NULL;
    FILE *hexfd;
    char linebuf[259];
    int lineno = 0;
    struct intelhex ih;
    struct packet p;
    int ret = 0;
    int serfd;
    struct termios oldtio;

    /* Parse a command line */
    cmd = optparse(argc, argv, &device, &hexfile);
    if (cmd == CMD_ERROR) {
	usage(argv);
	return 1;
    }

    /* Connect to the serial device */
    serfd = connect(device, &oldtio);
    if (serfd < 0) {
	return 1;
    }

    /* Erase the entire user code space */
    if (cmd & CMD_ERASE) {
	printf("Erase ... ");
	fflush(stdout);
	cmd &= ~CMD_ERASE;	/* cmd - CMD_ERASE */
	mkpkt(&p, NULL, CMD_ERASE);
	if (send(serfd, &p)) {
	    fprintf(stderr, "ERROR: sent a mass erase command.\n");
	    goto resume_exit;
	}
	printf("done\n");
    }

    /* Write and verify */
    if (cmd & (CMD_WRITE | CMD_VERIFY)) {
	if(!(hexfd = fopen(hexfile, "r"))) {
	    perror(hexfile);
	    goto resume_exit;
	}
	while (cmd) {
	    if (cmd & CMD_WRITE) {
		printf("Write ");
	    } else {
		printf("Verify ");
	    }
	    while (fgets(linebuf, MAX_LINE_SIZE, hexfd)) {
		lineno++;
		ret = interpret(&ih, linebuf);
		if (ret == INTERPRET_UNIMPLEMENTED_RECORDTYPE) {
		    continue;
		} else if (ret == INTERPRET_UNDEF_FMT) {
		    goto resume_exit;
		}
		if (cmd & CMD_WRITE) {
		    mkpkt(&p, &ih, CMD_WRITE);
		} else {
		    mkpkt(&p, &ih, CMD_VERIFY);
		}
		if (send(serfd, &p) != SEND_NOERR) {
		    goto resume_exit;
		}
		printf(".");
		fflush(stdout);
	    }
	    if (cmd & CMD_WRITE) {
		cmd &= ~CMD_WRITE; /* cmd - CMD_WRITE */
		fseek(hexfd, 0, SEEK_SET);
	    } else {
		cmd &= ~CMD_VERIFY; /* cmd - CMD_VERIFY */
	    }
	    printf("\ndone\n");
	}
	fclose(hexfd);
	/* Run */
	mkpkt(&p, NULL, CMD_RUN);
	if (send(serfd, &p)) {
	    fprintf(stderr, "ERROR: send a run command.\n");
	    goto resume_exit;
	}
	printf("Run: done\n");
    }

  resume_exit:
    tcsetattr(serfd, TCSANOW, &oldtio);
    return 0;
}

unsigned char optparse(int argc, char *argv[], char **device, char **hexfile)
{
    int c;
    unsigned char cmd = CMD_ERROR;

    while ((c = getopt(argc, argv, "ep:wv")) != -1) {
        switch (c) {
        case 'e': /* erase the entire user code space */
	    cmd |= CMD_ERASE;
            break;
        case 'p':
	    *device = optarg;
            break;
        case 'w':		/* write */
	    cmd |= CMD_WRITE;
            break;
        case 'v':		/* verify */
	    cmd |= CMD_VERIFY;
            break;
	default:
	    return CMD_ERROR;
	}
    }
    if (optind < argc) {
	*hexfile = argv[optind];
    }
    if (cmd != CMD_ERASE && *hexfile == NULL) {
	cmd = CMD_ERROR;
    } else if (cmd == CMD_ERROR) {
	cmd = CMD_WRITE;	/* no option: write command */
    }

    return cmd;
}

void usage(char *argv[])
{
    printf("usage: %s [-e|-w|-v|-p port] [file]\n", argv[0]);
    printf("\t-e\t\terase the entire user code space\n");
    printf("\t-p port\t\tdefault: /dev/ttyS0\n");
    printf("\t-w input.hex\twrite\n");
    printf("\t-v input.hex\tverify\n");
}

int connect(char *device, struct termios *oldtio)
{
    struct termios newtio;
    int fd;
    int res;
    char buf;
    int i;
    
    fd_set fds;
    struct timeval timeout;    

    fd = open(device, O_RDWR | O_NOCTTY); 
    if (fd < 0) {
	perror(device);
	return fd;
    }

    tcgetattr(fd, oldtio); /* save current port settings */
        
    bzero(&newtio, sizeof(newtio));
#ifdef FREEBSD
    newtio.c_cflag = BAUDRATE | CS8 | CLOCAL | CREAD;
#else
    newtio.c_cflag = CS8 | CLOCAL | CREAD;
#endif
    newtio.c_iflag = IGNPAR | ICRNL;
    newtio.c_oflag = 0;
        
    /* set input mode (non-canonical, no echo,...) */
    newtio.c_lflag = 0;
#ifdef FREEBSD
    cfsetospeed(&newtio, BAUDRATE);
    cfsetispeed(&newtio, BAUDRATE);
#endif
    newtio.c_cc[VTIME] = 0;	/* inter-character timer unused */
    newtio.c_cc[VMIN] = 1;  /* blocking read until 1 chars received */
        
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &newtio);

    /* send '\b' */
    buf = '\b';
    write(fd, &buf, 1);

    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    timeout.tv_sec = CONNECT_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    res = select(FD_SETSIZE, &fds, (fd_set *)0, (fd_set *)0, &timeout);
    switch (res) {
    case -1:
	perror("select");
	tcsetattr(fd, TCSANOW, oldtio); /* resume tio */
	exit(-1);
    case 0:
	printf("timeout\n");
	tcsetattr(fd, TCSANOW, oldtio); /* resume tio */
	exit(1);
    default:
	printf("Device ID data packet: ");
	for (i = 0; i < 24; i++) {
	    res = read(fd, &buf, 1); /* returns after 1 chars have been input */
	    printf("%c", buf);
	}
	break;
    }

    return fd;
}

int interpret(struct intelhex *ih, char *linebuf)
{
    int bufc = 0;
    char buf[3];
    int i;

    /* : */
    if (linebuf[bufc] != ':') {
	fprintf(stderr, "ERROR: Undefined format \':\'\n");
	return INTERPRET_UNDEF_FMT;
    }
    bufc += 1;

    /* byte count except for ":BB" */
    strncpy(buf, &linebuf[bufc], 2); 
    bufc += 2;
    buf[2] = '\0';
    ih->bytecount = strtol(buf, NULL, 16);

    /* address */
    for (i = 0; i < 2; i++) {
	strncpy(buf, &linebuf[bufc], 2); 
	bufc += 2;
	buf[2] = '\0';
	ih->address[i] = strtol(buf, NULL, 16);
    }

    /* record type: 02, 03, 04 and 05 are not implemented. */
    strncpy(buf, &linebuf[bufc], 2);
    bufc += 2;
    buf[2] = '\0';
    ih->recordtype = strtol(buf, NULL, 16);
    if (!(ih->recordtype == RECORDTYPE_DATA)) {
	printf("\n%s", linebuf);
	printf("skip line: the record type 0x%02x", ih->recordtype);
	return INTERPRET_UNIMPLEMENTED_RECORDTYPE;
    }
    
    /* data byte */
    for (i = 0; i < ih->bytecount; i++) {
	strncpy(buf, &linebuf[bufc], 2); 
	bufc += 2;
	buf[2] = '\0';
	ih->databyte[i] = strtol(buf, NULL, 16);
    }

    /* checksum */
    strncpy(buf, &linebuf[bufc], 2);
    bufc += 2;
    buf[2] = '\0';
    ih->checksum = strtol(buf, NULL, 16);
    /* verifychecksum(); */

    return 0;
}

int mkpkt(struct packet *p, struct intelhex *ih, unsigned char cmd)
{
    int i = 0;
    int checksumpos;

    /* debug print */
/*     printf("byte count: %02X address: ", ih->bytecount); */
/*     for (i = 0; i < 2; i++) { */
/* 	printf("%02X", ih->address[i]); */
/*     } */
/*     printf(" record type: %02X data byte: ", ih->recordtype); */
/*     for (i = 0; i < ih->bytecount; i++) { */
/* 	printf("%02X", ih->databyte[i]); */
/*     } */
/*     printf(" checksum: %02X\n", ih->checksum); */

    /* Start ID */
    p->startid[0] = 0x07;
    p->startid[1] = 0x0E;

    /* No. of Data Bytes */
    switch (cmd) {
      case CMD_ERASE:	p->numdata = 0x06;	break;
      case CMD_RUN:	p->numdata = 0x05;	break;
    default:			/* CMD_WRITE | CMD_VERIFY */
	p->numdata = ih->bytecount + 5;	/* cmd (1 byte), address (4 bytes) */
	break;
    }

    /* CMD */
    switch (cmd) {
      case CMD_ERASE:	p->cmd = 'E';	break;
      case CMD_WRITE:	p->cmd = 'W';	break;
      case CMD_VERIFY:	p->cmd = 'V';	break;
      case CMD_RUN:	p->cmd = 'R';	break;
    }

    /* Address: writing flash memory: from the address 0x00000000 */
    p->address[0] = 0x00;
    p->address[1] = 0x00;
    if (cmd & (CMD_ERASE | CMD_RUN)) {
	p->address[2] = 0x00;
	p->address[3] = 0x00;
    } else { 			/* CMD_WRITE | CMD_VERIFY */
	p->address[2] = ih->address[0];
	p->address[3] = ih->address[1];
    }

    /* Data */
    i = 0;
    if (cmd & CMD_ERASE) {
	p->rest[i++] = 0x00;	/* a mass erase command */
    } else if (cmd & (CMD_WRITE | CMD_VERIFY)) { /* CMD_WRITE | CMD_VERIFY */
	for (i = 0; i < ih->bytecount; i++) {
	    p->rest[i] = ih->databyte[i];
	}
    }
    checksumpos = i;

    /* Shift for verify command */    
    if (cmd & CMD_VERIFY) {
	unsigned char high = 0, low = 0;
	unsigned char *databytes = &p->cmd;
	for (i = 0; i < p->numdata; i++) {
	    high = (*databytes) << 3;
	    low = (*databytes) >> 5;
	    *databytes++ = (high & 0xF8) | (low & 0x07);
	}
    }

    /* Checksum */
    p->rest[checksumpos] = mkchecksum(p);
    return 0;
}

unsigned char mkchecksum(struct packet *p)
{
    unsigned char *databytes = &p->numdata;
    unsigned char checksum = 0x00;
    int i;

    /* No. Data Bytes + \sum_N(Data Byte) */
    for (i = 0; i < p->numdata + 1; i++) { /* 1: No. of Data Byte */
	checksum += *databytes++;
    }

    return ((~checksum) + 1);	/* tows complement */
}

int send(int fd, struct packet *p)
{
    int count;
    unsigned char res[256];
    
    /* debug print */
/*     int i; */
/*     unsigned char *bufp = p->startid; */
/*     for (i = 0; i < p->numdata + 4; i++) { */
/* 	printf("%02X", bufp[i]); */
/*     } */
/*     printf("\n"); */

    write(fd, p->startid, p->numdata + 4); /* 4: Start ID, No. of Data Byte, Checksum */
    count = read(fd, res, 255);
    res[count] = '\0';
    if (res[0] == 0x07) { 	/* NACK: BEL (0x07) */
	fprintf(stderr, "ERROR: The loader routine responded with NACK.\n");
	return SEND_NACK;
    } else if (res[0] != 0x06) { /* ACK: 0x06 ACK */
	fprintf(stderr, "ERROR: The loader routine responded with a undefined character.\n");
	return SEND_UNDEF;
    }
    return SEND_NOERR;
}
