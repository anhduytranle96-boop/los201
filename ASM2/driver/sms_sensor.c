#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/mutex.h>
#include <linux/ktime.h>
#include <linux/version.h>

#define DEVICE_NAME "sms_sensor"
#define PROC_NAME "sms_stats"
#define SAMPLE_SIZE 64

static DEFINE_MUTEX(sensor_lock);
static u64 total_bytes_read;
static int min_temperature;
static int max_temperature;
static bool has_sample;
static struct proc_dir_entry *stats_entry;

static ssize_t sensor_read(struct file *file, char __user *buf,
                           size_t count, loff_t *ppos)
{
    char sample[SAMPLE_SIZE];
    int temp, humidity;
    int len;
    size_t n;

    if (count == 0)
        return 0;

    if (*ppos != 0)
        return 0;

    get_random_bytes(&temp, sizeof(temp));
    get_random_bytes(&humidity, sizeof(humidity));

    temp = 1500 + (temp & 0x0fff) % 3001;
    humidity = 3000 + (humidity & 0x0fff) % 6001;

    len = scnprintf(sample, sizeof(sample), "%lld,%d.%02d,%d.%02d\n",
                    (long long)ktime_get_real_seconds(),
                    temp / 100, temp % 100,
                    humidity / 100, humidity % 100);

    n = min(count, (size_t)len);

    if (copy_to_user(buf, sample, n))
        return -EFAULT;

    mutex_lock(&sensor_lock);
    total_bytes_read += n;

    if (!has_sample) {
        min_temperature = temp;
        max_temperature = temp;
        has_sample = true;
    } else {
        if (temp < min_temperature)
            min_temperature = temp;
        if (temp > max_temperature)
            max_temperature = temp;
    }
    mutex_unlock(&sensor_lock);

    *ppos += n;
    return n;
}

static const struct file_operations sensor_fops = {
    .owner = THIS_MODULE,
    .read = sensor_read,
    .llseek = no_llseek,
};

static int stats_show(struct seq_file *m, void *v)
{
    mutex_lock(&sensor_lock);
    seq_printf(m, "total_bytes_read: %llu\n",
               (unsigned long long)total_bytes_read);

    if (has_sample) {
        seq_printf(m, "min_temperature: %d.%02d\n",
                   min_temperature / 100, min_temperature % 100);
        seq_printf(m, "max_temperature: %d.%02d\n",
                   max_temperature / 100, max_temperature % 100);
    } else {
        seq_puts(m, "min_temperature: N/A\n");
        seq_puts(m, "max_temperature: N/A\n");
    }
    mutex_unlock(&sensor_lock);
    return 0;
}

static int stats_open(struct inode *inode, struct file *file)
{
    return single_open(file, stats_show, NULL);
}

static const struct proc_ops stats_ops = {
    .proc_open = stats_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static struct miscdevice sensor_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DEVICE_NAME,
    .fops = &sensor_fops,
    .mode = 0444,
};

static int __init sensor_init(void)
{
    int ret;

    ret = misc_register(&sensor_device);
    if (ret)
        return ret;

    stats_entry = proc_create(PROC_NAME, 0444, NULL, &stats_ops);
    if (!stats_entry) {
        misc_deregister(&sensor_device);
        return -ENOMEM;
    }

    pr_info("sms_sensor: loaded\n");
    return 0;
}

static void __exit sensor_exit(void)
{
    proc_remove(stats_entry);
    misc_deregister(&sensor_device);
    pr_info("sms_sensor: unloaded\n");
}

module_init(sensor_init);
module_exit(sensor_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ASM2");
MODULE_DESCRIPTION("SMS sensor driver with byte and temperature statistics");
