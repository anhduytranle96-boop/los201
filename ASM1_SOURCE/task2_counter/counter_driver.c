#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "counter"

static atomic_t counter_value = ATOMIC_INIT(0);

static ssize_t counter_read(struct file *file,
                            char __user *user_buf,
                            size_t count,
                            loff_t *ppos)
{
    char buffer[32];
    int value;
    int len;

    if (*ppos > 0 || count == 0)
        return 0;

    len = snprintf(buffer, sizeof(buffer), "%d\n",
                   atomic_read(&counter_value));

    if (count < len)
        return -EINVAL;

    value = atomic_fetch_inc(&counter_value);
    len = snprintf(buffer, sizeof(buffer), "%d\n", value);

    if (copy_to_user(user_buf, buffer, len))
        return -EFAULT;

    *ppos += len;
    pr_info("counter: returned %d\n", value);
    return len;
}

static const struct file_operations counter_fops = {
    .owner = THIS_MODULE,
    .read = counter_read,
    .llseek = no_llseek,
};

static struct miscdevice counter_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = DRIVER_NAME,
    .fops = &counter_fops,
    .mode = 0666,
};

static int __init counter_init(void)
{
    int ret;

    atomic_set(&counter_value, 0);
    ret = misc_register(&counter_device);

    if (ret) {
        pr_err("counter: registration failed: %d\n", ret);
        return ret;
    }

    pr_info("counter: registered, major=10, minor=%d\n",
            counter_device.minor);
    return 0;
}

static void __exit counter_exit(void)
{
    misc_deregister(&counter_device);
    pr_info("counter: unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Student");
MODULE_DESCRIPTION("Atomic counter misc device driver");
MODULE_VERSION("1.0");
