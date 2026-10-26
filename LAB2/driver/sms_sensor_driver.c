// Purpose: Simulated temperature/humidity sensor character driver.
// Author: Student - replace with your name and student ID.
// Date: 2026-10-25

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/math64.h>

#include "sms_sensor.h"

#define DEFAULT_INTERVAL_MS 100
#define MIN_INTERVAL_MS       1
#define MAX_INTERVAL_MS   60000

static DEFINE_MUTEX(sensor_lock);
static u32 interval_ms = DEFAULT_INTERVAL_MS;
static u64 read_count;
static u64 start_ms;
static int last_temperature_milli = 25000;
static int last_humidity_milli = 50000;

static ssize_t sms_sensor_read(struct file *file,
                               char __user *user_buf,
                               size_t count,
                               loff_t *ppos)
{
    char buffer[96];
    u32 r1, r2;
    u32 delay_ms;
    u64 timestamp;
    int temperature_milli;
    int humidity_milli;
    int len;

    if (count == 0)
        return 0;

    mutex_lock(&sensor_lock);
    delay_ms = interval_ms;
    mutex_unlock(&sensor_lock);

    msleep(delay_ms);

    get_random_bytes(&r1, sizeof(r1));
    get_random_bytes(&r2, sizeof(r2));

    temperature_milli = 15000 + (int)(r1 % 30001);
    humidity_milli = 30000 + (int)(r2 % 60001);
    timestamp = ktime_to_ms(ktime_get());

    mutex_lock(&sensor_lock);
    read_count++;
    last_temperature_milli = temperature_milli;
    last_humidity_milli = humidity_milli;
    mutex_unlock(&sensor_lock);

    len = scnprintf(buffer, sizeof(buffer),
                    "%llu,%d.%03d,%d.%03d\n",
                    (unsigned long long)timestamp,
                    temperature_milli / 1000,
                    temperature_milli % 1000,
                    humidity_milli / 1000,
                    humidity_milli % 1000);

    if (len > count)
        return -EINVAL;

    if (copy_to_user(user_buf, buffer, len))
        return -EFAULT;

    return len;
}

static long sms_sensor_ioctl(struct file *file,
                             unsigned int cmd,
                             unsigned long arg)
{
    u32 new_interval;

    switch (cmd) {
    case SMS_SET_INTERVAL:
        if (copy_from_user(&new_interval, (u32 __user *)arg,
                           sizeof(new_interval)))
            return -EFAULT;

        if (new_interval < MIN_INTERVAL_MS ||
            new_interval > MAX_INTERVAL_MS)
            return -EINVAL;

        mutex_lock(&sensor_lock);
        interval_ms = new_interval;
        mutex_unlock(&sensor_lock);

        pr_info("sms_sensor: interval set to %u ms\n", new_interval);
        return 0;

    default:
        return -ENOTTY;
    }
}

static const struct file_operations sms_sensor_fops = {
    .owner          = THIS_MODULE,
    .read           = sms_sensor_read,
    .unlocked_ioctl = sms_sensor_ioctl,
    .llseek         = no_llseek,
};

static struct miscdevice sms_sensor_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name  = SMS_DEVICE_NAME,
    .fops  = &sms_sensor_fops,
    .mode  = 0666,
};

static int sms_stats_show(struct seq_file *m, void *v)
{
    u64 count, uptime;
    int temperature_milli, humidity_milli;

    mutex_lock(&sensor_lock);
    count = read_count;
    temperature_milli = last_temperature_milli;
    humidity_milli = last_humidity_milli;
    uptime = div_u64(ktime_to_ms(ktime_get()) - start_ms, 1000);
    mutex_unlock(&sensor_lock);

    seq_printf(m, "read_count: %llu\n",
               (unsigned long long)count);
    seq_printf(m, "last_temperature: %d.%03d\n",
               temperature_milli / 1000,
               temperature_milli % 1000);
    seq_printf(m, "last_humidity: %d.%03d\n",
               humidity_milli / 1000,
               humidity_milli % 1000);
    seq_printf(m, "uptime_seconds: %llu\n",
               (unsigned long long)uptime);

    return 0;
}

static int sms_stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, sms_stats_show, NULL);
}

static const struct proc_ops sms_stats_ops = {
    .proc_open    = sms_stats_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init sms_sensor_init(void)
{
    int ret;

    start_ms = ktime_to_ms(ktime_get());
    read_count = 0;
    interval_ms = DEFAULT_INTERVAL_MS;

    ret = misc_register(&sms_sensor_device);
    if (ret) {
        pr_err("sms_sensor: failed to register device: %d\n", ret);
        return ret;
    }

    if (!proc_create(SMS_PROC_NAME, 0444, NULL, &sms_stats_ops)) {
        misc_deregister(&sms_sensor_device);
        pr_err("sms_sensor: failed to create /proc/%s\n", SMS_PROC_NAME);
        return -ENOMEM;
    }

    pr_info("sms_sensor: registered /dev/%s and /proc/%s\n",
            SMS_DEVICE_NAME, SMS_PROC_NAME);
    return 0;
}

static void __exit sms_sensor_exit(void)
{
    remove_proc_entry(SMS_PROC_NAME, NULL);
    misc_deregister(&sms_sensor_device);
    pr_info("sms_sensor: unloaded\n");
}

module_init(sms_sensor_init);
module_exit(sms_sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Simulated temperature and humidity sensor driver");
MODULE_VERSION("1.0");
