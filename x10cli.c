/*
 * (C) Copyright 2011, Dave McCaldon <davem@mccaldon.com>
 * All Rights Reserved.
 *
 * X10 Command Line Interface
 * 
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "commands.h"

//#define I2C_DEBUG 1

int i2c_debug            = 0;
int i2c_bus              = 0;    // Default I2C bus
int i2c_fd               = -1;   // I2C device handle.
unsigned char slave_addr = 0x28; // slave address

int init_i2c()
{
    char device[PATH_MAX];

    sprintf(device, "/dev/i2c-%d", i2c_bus);

    printf("init_i2c: Opening device %s\n", device);

    if((i2c_fd = open(device,O_RDWR)) < 0) {
        perror("could not open device");
        return -1;
    } else {
        return 0;
    }
}

//
// Send and receive bytes on I2C bus.
//
int send_i2c(char * w, int w_len, char * r, int r_len)
{
    struct i2c_rdwr_ioctl_data msgset;
    struct i2c_msg             msg[2];

    msg[0].addr = slave_addr;
    msg[0].flags = 0;
    msg[0].len = w_len;
    msg[0].buf = w;

    msg[1].addr = slave_addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = r_len;
    msg[1].buf = r;

    msgset.msgs = msg;
    msgset.nmsgs = 2;


    if(ioctl(i2c_fd,I2C_RDWR,&msgset) < 0) {
        perror("ioctl");
        printf("tried to send: ");
        int i;
        for (i = 0; i < w_len; ++i)
            printf("%02X ", w[i]);
        printf("\n");
        return -1;
    }

    if (i2c_debug) {
        int i;
        for (i = 0; i < w_len; ++i)
            printf("%02X ", w[i]);
        printf("=> ");
        for (i = 0; i < r_len; ++i)
            printf("%02X ", r[i]);
        printf("\n");
    }

    return 0;
}

// Ping the i2c device
//
int do_ping(void)
{
    unsigned char commands[]  = { X10_MASTER_COMMAND_PING };
    unsigned char buffer[4+1] = { '?', '?', '?', '?', 0 };

    printf("do_ping: Sending PING\n");

    // Read length
    if (send_i2c(commands, sizeof(commands), buffer, 4) < 0) {
        return -1;
    }

    printf("    -> %s\n", buffer);

    return 0;
}

// Read the device uptime (in ticks)
//
int do_uptime(void)
{
    unsigned char commands[] = { X10_MASTER_COMMAND_UPTIME };
    unsigned char buffer[4]  = { 0, 0, 0, 0 };

    printf("do_uptime: Sending UPTIME\n");

    // Dispatch UPTIME request
    if (send_i2c(commands, sizeof(commands), buffer, 4) < 0) {
        return -1;
    }

    unsigned int uptime = buffer[0]
                          | (buffer[1] << 8)
                          | (buffer[2] << 16)
                          | (buffer[3] << 24);
    printf("    -> %u\n", uptime);

    return 0;
}

// Read the device status
//
int do_status(void)
{
    unsigned char commands[] = { X10_MASTER_COMMAND_STATUS };
    unsigned char status     = 0;

    printf("do_status: Sending STATUS\n");

    // Dispatch STATUS request
    if (send_i2c(commands, sizeof(commands), &status, 1) < 0) {
        return -1;
    }

    printf("    -> %02lX\n", status);
    if (status & 0x01) printf("        LOGOVERFLOW\n");
    if (status & 0x02) printf("        X10ERROR\n");

    return 0;
}

// Send a trash command
//
int do_trash(void)
{
    unsigned char commands[] = { '@' };
    unsigned char response = 0;

    printf("do_trash(): Sending trash!\n");

    // Dispatch STATUS request
    if (send_i2c(commands, sizeof(commands), &response, 1) < 0) {
        return -1;
    }

    printf("    -> %02lX\n", response);

    return 0;
}

int do_readlog()
{
    unsigned char commands[] = { X10_MASTER_COMMAND_READLOG };
    unsigned char len        = 0;
    unsigned char buffer[64];
    int i;

    printf("do_readlog: Sending READLOG\n");

    // Send command
    if (send_i2c(commands, sizeof(commands), &len, 1) < 0) {
        return -1;
    }

    while (len > 0) {
        // Read buffer
        if (send_i2c("", 0, buffer, len) < 0) {
            return -1;
        }

        for (i = 0; i < len; i++) {
            printf("%02X", buffer[i]);
            if ((i % 8) == 0) printf(":");
            else printf(" ");

            if ((i % 16) == 0) {
                printf("\n");
            }
        }

        if (i % 16) printf("\n");

        // Read next length
        len = 0;
        if (send_i2c("", 0, &len, 1) < 0) {
            return -1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int    i;
    time_t now;

    i2c_debug = 1;


    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-') {
            switch (argv[i][1]) {
            case 'b': // Different bus
                i2c_bus = atoi(argv[++i]);
                break;
            default:
                fprintf(stderr, "Usage: %s: [-b <bus>]\n", argv[0]);
                return 1;
            }
        }
    }

    time(&now);

    printf("i2cx10: %s\nSlave Addr: %02X\n", ctime(&now), slave_addr);

    if (init_i2c() < 0) {
        return 1;
    }

    do_ping();
    do_status();
    do_uptime();
    do_trash();
    do_readlog();

    close(i2c_fd);


    return 0;
}

/*
 * End-of-file
 *
 */
