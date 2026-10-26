#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define ASGN1_MAGIC 'A'

struct asgn1_stats {
    __u32 open_count;
    __u32 write_count;
    __u32 read_count;
    __u32 buffer_len;
    __u32 last_write_size;
};

#define ASGN1_RESET_BUFFER _IO(ASGN1_MAGIC, 0)
#define ASGN1_GET_STATS    _IOR(ASGN1_MAGIC, 1, struct asgn1_stats)
#define ASGN1_SET_MODE     _IOW(ASGN1_MAGIC, 2, int)
#define ASGN1_GET_VERSION  _IOR(ASGN1_MAGIC, 3, char[64])

#endif
