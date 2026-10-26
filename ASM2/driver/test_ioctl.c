#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define SMS_IOC_MAGIC 'S'
#define SMS_SET_INTERVAL _IOW(SMS_IOC_MAGIC, 1, uint32_t)

int main(void)
{
    int fd;
    uint32_t interval = 500;

    fd = open("/dev/sms_sensor", O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    if (ioctl(fd, SMS_SET_INTERVAL, &interval) < 0) {
        perror("ioctl SMS_SET_INTERVAL");
        close(fd);
        return 1;
    }

    printf("SMS_SET_INTERVAL set to %u ms successfully\n", interval);
    close(fd);
    return 0;
}
