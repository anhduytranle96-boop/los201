#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/version.h>
#include "asgn1_ioctl.h"

#define DEVICE_NAME "asgn1"
#define CLASS_NAME  "asgn1_class"
#define BUFFER_SIZE 1024
#define DRIVER_MAJOR 241

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("ASGN1 Character Driver with ioctl");
MODULE_VERSION("1.0");

static int major_number = DRIVER_MAJOR;
static struct class *asgn1_class;
static struct device *asgn1_device;

static char device_buffer[BUFFER_SIZE];
static size_t buffer_len;
static unsigned int open_count;
static unsigned int write_count;
static unsigned int read_count;
static unsigned int last_write_size;
static int write_mode = 1; /* 0: prefix [KERNEL], 1: echo */

static DEFINE_MUTEX(asgn1_mutex);

static int asgn1_open(struct inode *inode, struct file *file)
{
    mutex_lock(&asgn1_mutex);
    open_count++;
    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1: device opened\n");
    return 0;
}

static int asgn1_release(struct inode *inode, struct file *file)
{
    pr_info("asgn1: device closed\n");
    return 0;
}

static ssize_t asgn1_read(struct file *file, char __user *buf,
                          size_t count, loff_t *offset)
{
    size_t available;
    size_t len;

    if (count == 0)
        return 0;

    mutex_lock(&asgn1_mutex);

    if (*offset < 0 || (size_t)*offset >= buffer_len) {
        mutex_unlock(&asgn1_mutex);
        return 0;
    }

    available = buffer_len - (size_t)*offset;
    len = min(count, available);

    if (copy_to_user(buf, device_buffer + *offset, len)) {
        mutex_unlock(&asgn1_mutex);
        return -EFAULT;
    }

    *offset += len;
    read_count++;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1: read %zu bytes\n", len);
    return (ssize_t)len;
}

static ssize_t asgn1_write(struct file *file, const char __user *buf,
                           size_t len, loff_t *offset)
{
    char temp[BUFFER_SIZE];
    size_t prefix_len = 0;
    size_t total_len;
    int mode;

    if (len == 0)
        return 0;

    if (len >= BUFFER_SIZE)
        return -EINVAL;

    if (copy_from_user(temp, buf, len))
        return -EFAULT;

    mutex_lock(&asgn1_mutex);

    mode = write_mode;

    if (mode == 0) {
        prefix_len = strlen("[KERNEL]");

        if (prefix_len + len >= BUFFER_SIZE) {
            mutex_unlock(&asgn1_mutex);
            return -ENOSPC;
        }

        memcpy(device_buffer, "[KERNEL]", prefix_len);
        memcpy(device_buffer + prefix_len, temp, len);
        total_len = prefix_len + len;
    } else {
        memcpy(device_buffer, temp, len);
        total_len = len;
    }

    device_buffer[total_len] = '\0';
    buffer_len = total_len;
    write_count++;
    last_write_size = len;

    mutex_unlock(&asgn1_mutex);

    pr_info("asgn1: wrote %zu bytes, mode=%d\n", len, mode);
    return (ssize_t)len;
}

static long asgn1_ioctl(struct file *file, unsigned int cmd,
                        unsigned long arg)
{
    int mode;
    struct asgn1_stats stats;
    char version[64] = "ASGN1 Driver Version 1.0";

    pr_info("asgn1: ioctl command=0x%x\n", cmd);

    switch (cmd) {
    case ASGN1_RESET_BUFFER:
        mutex_lock(&asgn1_mutex);
        memset(device_buffer, 0, sizeof(device_buffer));
        buffer_len = 0;
        write_count = 0;
        read_count = 0;
        last_write_size = 0;
        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1: buffer and read/write statistics reset\n");
        return 0;

    case ASGN1_GET_STATS:
        mutex_lock(&asgn1_mutex);
        stats.open_count = open_count;
        stats.write_count = write_count;
        stats.read_count = read_count;
        stats.buffer_len = buffer_len;
        stats.last_write_size = last_write_size;
        mutex_unlock(&asgn1_mutex);

        if (copy_to_user((void __user *)arg, &stats, sizeof(stats)))
            return -EFAULT;

        return 0;

    case ASGN1_SET_MODE:
        if (copy_from_user(&mode, (void __user *)arg, sizeof(mode)))
            return -EFAULT;

        if (mode != 0 && mode != 1)
            return -EINVAL;

        mutex_lock(&asgn1_mutex);
        write_mode = mode;
        mutex_unlock(&asgn1_mutex);

        pr_info("asgn1: write mode set to %d\n", mode);
        return 0;

    case ASGN1_GET_VERSION:
        if (copy_to_user((void __user *)arg, version, sizeof(version)))
            return -EFAULT;

        pr_info("asgn1: version returned\n");
        return 0;

    default:
        pr_warn("asgn1: unknown ioctl command 0x%x\n", cmd);
        return -ENOTTY;
    }
}

static const struct file_operations asgn1_fops = {
    .owner          = THIS_MODULE,
    .open           = asgn1_open,
    .release        = asgn1_release,
    .read           = asgn1_read,
    .write          = asgn1_write,
    .unlocked_ioctl = asgn1_ioctl,
};

static int __init asgn1_init(void)
{
    int ret;

    ret = register_chrdev(major_number, DEVICE_NAME, &asgn1_fops);
    if (ret < 0) {
        pr_err("asgn1: failed to register major %d: %d\n",
               major_number, ret);
        return ret;
    }

    asgn1_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(asgn1_class)) {
        ret = PTR_ERR(asgn1_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    asgn1_device = device_create(asgn1_class, NULL,
                                 MKDEV(major_number, 0),
                                 NULL, DEVICE_NAME);
    if (IS_ERR(asgn1_device)) {
        ret = PTR_ERR(asgn1_device);
        class_destroy(asgn1_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return ret;
    }

    pr_info("asgn1: loaded, major=%d\n", major_number);
    return 0;
}

static void __exit asgn1_exit(void)
{
    device_destroy(asgn1_class, MKDEV(major_number, 0));
    class_destroy(asgn1_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("asgn1: unloaded\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);
