#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <linux/ioctl.h>
#include <linux/version.h>

#define DEVICE_NAME "vchar_ipc"
#define CLASS_NAME  "vchar_class"
#define BUFFER_SIZE 1024

#define VCHAR_MAGIC        'V'
#define VCHAR_IOC_CLEAR    _IO(VCHAR_MAGIC, 1)
#define VCHAR_IOC_GET_SIZE _IOR(VCHAR_MAGIC, 2, int)

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Riya Rathod");
MODULE_DESCRIPTION("Thread-Safe Virtual Character Device Driver for Userspace IPC");
MODULE_VERSION("1.0");

static dev_t dev_num;
static struct cdev vchar_cdev;
static struct class *vchar_class = NULL;
static struct device *vchar_device = NULL;

static char kernel_buffer[BUFFER_SIZE];
static size_t data_size = 0;
static DEFINE_MUTEX(vchar_mutex);

static int dev_open(struct inode *inodep, struct file *filep)
{
    pr_info("vchar_ipc: Device opened by PID %d\n", current->pid);
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    pr_info("vchar_ipc: Device closed by PID %d\n", current->pid);
    return 0;
}

static ssize_t dev_read(struct file *filep, char __user *buffer, size_t len, loff_t *offset)
{
    ssize_t bytes_to_read;

    if (mutex_lock_interruptible(&vchar_mutex))
        return -ERESTARTSYS;

    if (*offset >= data_size) {
        mutex_unlock(&vchar_mutex);
        return 0;
    }

    bytes_to_read = min(len, data_size - (size_t)*offset);
    if (copy_to_user(buffer, kernel_buffer + *offset, bytes_to_read)) {
        mutex_unlock(&vchar_mutex);
        return -EFAULT;
    }

    *offset += bytes_to_read;
    pr_info("vchar_ipc: Sent %zd bytes to userspace\n", bytes_to_read);
    mutex_unlock(&vchar_mutex);
    return bytes_to_read;
}

static ssize_t dev_write(struct file *filep, const char __user *buffer, size_t len, loff_t *offset)
{
    ssize_t bytes_to_write;

    if (mutex_lock_interruptible(&vchar_mutex))
        return -ERESTARTSYS;

    bytes_to_write = min(len, (size_t)(BUFFER_SIZE - 1));
    memset(kernel_buffer, 0, BUFFER_SIZE);

    if (copy_from_user(kernel_buffer, buffer, bytes_to_write)) {
        mutex_unlock(&vchar_mutex);
        return -EFAULT;
    }

    data_size = bytes_to_write;
    *offset = 0;
    pr_info("vchar_ipc: Received %zd bytes from userspace\n", bytes_to_write);
    mutex_unlock(&vchar_mutex);
    return bytes_to_write;
}

static long dev_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
    int size_val;

    if (mutex_lock_interruptible(&vchar_mutex))
        return -ERESTARTSYS;

    switch (cmd) {
    case VCHAR_IOC_CLEAR:
        memset(kernel_buffer, 0, BUFFER_SIZE);
        data_size = 0;
        pr_info("vchar_ipc: Buffer cleared via ioctl\n");
        break;
    case VCHAR_IOC_GET_SIZE:
        size_val = (int)data_size;
        if (copy_to_user((int __user *)arg, &size_val, sizeof(size_val))) {
            mutex_unlock(&vchar_mutex);
            return -EFAULT;
        }
        break;
    default:
        mutex_unlock(&vchar_mutex);
        return -EINVAL;
    }

    mutex_unlock(&vchar_mutex);
    return 0;
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .open           = dev_open,
    .read           = dev_read,
    .write          = dev_write,
    .unlocked_ioctl = dev_ioctl,
    .release        = dev_release,
};

static int __init vchar_init(void)
{
    int ret;

    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
        return ret;

    cdev_init(&vchar_cdev, &fops);
    ret = cdev_add(&vchar_cdev, dev_num, 1);
    if (ret < 0)
        goto unregister_region;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    vchar_class = class_create(CLASS_NAME);
#else
    vchar_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(vchar_class)) {
        ret = PTR_ERR(vchar_class);
        goto del_cdev;
    }

    vchar_device = device_create(vchar_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(vchar_device)) {
        ret = PTR_ERR(vchar_device);
        goto destroy_class;
    }

    mutex_init(&vchar_mutex);
    pr_info("vchar_ipc: Registered with Major %d, Minor %d\n", MAJOR(dev_num), MINOR(dev_num));
    return 0;

destroy_class:
    class_destroy(vchar_class);
del_cdev:
    cdev_del(&vchar_cdev);
unregister_region:
    unregister_chrdev_region(dev_num, 1);
    return ret;
}

static void __exit vchar_exit(void)
{
    device_destroy(vchar_class, dev_num);
    class_destroy(vchar_class);
    cdev_del(&vchar_cdev);
    unregister_chrdev_region(dev_num, 1);
    pr_info("vchar_ipc: Unloaded module\n");
}

module_init(vchar_init);
module_exit(vchar_exit);
