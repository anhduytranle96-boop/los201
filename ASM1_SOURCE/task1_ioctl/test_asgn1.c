#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "asgn1_ioctl.h"

int main(void)
{
    int fd, mode;
    char buf[256] = {0};
    char version[64] = {0};
    struct asgn1_stats stats;

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open /dev/asgn1");
        return 1;
    }

    printf("=== TEST 1: SET_MODE = 1 ===\n");
    mode = 1;
    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0)
        perror("ASGN1_SET_MODE 1");
    else
        printf("Set mode 1 OK\n");

    close(fd);

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (write(fd, "Hello ASGN1", 11) != 11)
        perror("write");
    close(fd);

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    if (read(fd, buf, sizeof(buf) - 1) < 0)
        perror("read");
    else
        printf("Read back: %s\n", buf);
    close(fd);

    printf("\n=== TEST 2: SET_MODE = 0 ===\n");
    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    mode = 0;
    if (ioctl(fd, ASGN1_SET_MODE, &mode) < 0)
        perror("ASGN1_SET_MODE 0");
    else
        printf("Set mode 0 OK\n");
    close(fd);

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (write(fd, "ABC", 3) != 3)
        perror("write");
    close(fd);

    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    if (read(fd, buf, sizeof(buf) - 1) < 0)
        perror("read");
    else
        printf("Read back: %s\n", buf);
    close(fd);

    printf("\n=== TEST 3: GET_STATS ===\n");
    fd = open("/dev/asgn1", O_RDWR);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    memset(&stats, 0, sizeof(stats));
    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("ASGN1_GET_STATS");
    } else {
        printf("open_count      = %u\n", stats.open_count);
        printf("write_count     = %u\n", stats.write_count);
        printf("read_count      = %u\n", stats.read_count);
        printf("buffer_len      = %u\n", stats.buffer_len);
        printf("last_write_size = %u\n", stats.last_write_size);
    }

    printf("\n=== TEST 4: GET_VERSION ===\n");
    if (ioctl(fd, ASGN1_GET_VERSION, version) < 0)
        perror("ASGN1_GET_VERSION");
    else
        printf("Driver version: %s\n", version);

    printf("\n=== TEST 5: RESET_BUFFER ===\n");
    if (ioctl(fd, ASGN1_RESET_BUFFER) < 0)
        perror("ASGN1_RESET_BUFFER");
    else
        printf("Reset buffer OK\n");

    memset(&stats, 0, sizeof(stats));
    if (ioctl(fd, ASGN1_GET_STATS, &stats) < 0) {
        perror("ASGN1_GET_STATS after reset");
    } else {
        printf("After reset: write_count=%u, read_count=%u, "
               "buffer_len=%u, last_write_size=%u\n",
               stats.write_count, stats.read_count,
               stats.buffer_len, stats.last_write_size);
    }

    close(fd);
    return 0;
}
