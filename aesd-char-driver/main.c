/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 @ref Assignment9 is based on driver of assignmrnt 8 in which I referenced Parth Varsani's work, though i have tried to change and implement read-write functions.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include "aesdchar.h"
#include "aesd_ioctl.h"

int aesd_major = 0;  // use dynamic major
int aesd_minor = 0;

MODULE_AUTHOR("Parth Patel");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    filp->private_data = &aesd_device;
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    filp->private_data = NULL;
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *user_buf, size_t buf_len, loff_t *offset)
{
    struct aesd_dev *drv = filp->private_data;
    struct aesd_buffer_entry *target_entry = NULL;
    size_t internal_offset = 0, actual_copy = 0;
    ssize_t ret = 0;

    PDEBUG("read %zu bytes @ offset %lld", buf_len, *offset);

    if (mutex_lock_interruptible(&drv->lock))
        return -ERESTARTSYS;

    target_entry = aesd_circular_buffer_find_entry_offset_for_fpos(&drv->buffer, *offset, &internal_offset);
    if (!target_entry) {
        mutex_unlock(&drv->lock);
        return 0;
    }

    actual_copy = min(buf_len, target_entry->size - internal_offset);

    if (copy_to_user(user_buf, target_entry->buffptr + internal_offset, actual_copy)) {
        ret = -EFAULT;
    } else {
        *offset += actual_copy;
        ret = actual_copy;
    }

    mutex_unlock(&drv->lock);
    return ret;
}

ssize_t aesd_write(struct file *filp, const char __user *user_data, size_t len, loff_t *offset)
{
    struct aesd_dev *dev_ptr = filp->private_data;
    ssize_t result = len;
    size_t newline_idx = 0;
    char *combined_buf = NULL;
    bool complete_cmd = false;

    PDEBUG("write %zu bytes @ offset %lld", len, *offset);

    combined_buf = kmalloc(dev_ptr->partial_size + len, GFP_KERNEL);
    if (!combined_buf) return -ENOMEM;

    if (dev_ptr->partial_write) {
        memcpy(combined_buf, dev_ptr->partial_write, dev_ptr->partial_size);
        kfree(dev_ptr->partial_write);
        dev_ptr->partial_write = NULL;
    }

    if (copy_from_user(combined_buf + dev_ptr->partial_size, user_data, len)) {
        kfree(combined_buf);
        return -EFAULT;
    }

    for (newline_idx = 0; newline_idx < len; newline_idx++) {
        if (user_data[newline_idx] == '\n') {
            complete_cmd = true;
            break;
        }
    }

    mutex_lock(&dev_ptr->lock);

    if (complete_cmd) {
        struct aesd_buffer_entry new_entry = {
            .buffptr = combined_buf,
            .size = dev_ptr->partial_size + len
        };

        if (dev_ptr->buffer.full)
            kfree(dev_ptr->buffer.entry[dev_ptr->buffer.out_offs].buffptr);

        aesd_circular_buffer_add_entry(&dev_ptr->buffer, &new_entry);
        dev_ptr->partial_size = 0;
    } else {
        dev_ptr->partial_write = combined_buf;
        dev_ptr->partial_size += len;
    }

    mutex_unlock(&dev_ptr->lock);
    return result;
}



static loff_t aesd_llseek(struct file *file, loff_t user_offset, int origin)
{
    struct aesd_dev *device = file->private_data;
    loff_t position = 0;
    loff_t cumulative_size = 0;
    int i;

    if (mutex_lock_interruptible(&device->lock))
        return -ERESTARTSYS;

    // Accumulate the total size from all valid entries
    for (i = 0; i < AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i++) {
        if (device->buffer.entry[i].buffptr != NULL) {
            cumulative_size += device->buffer.entry[i].size;
        }
    }

    // Decide new file position based on origin
    switch (origin) {
        case SEEK_CUR:
            position = file->f_pos + user_offset;
            break;
        case SEEK_END:
            position = cumulative_size + user_offset;
            break;
        case SEEK_SET:
            position = user_offset;
            break;
        default:
            mutex_unlock(&device->lock);
            return -EINVAL;
    }

    // Ensure valid bounds
    if (position < 0 || position > cumulative_size) {
        mutex_unlock(&device->lock);
        return -EINVAL;
    }

    file->f_pos = position;
    mutex_unlock(&device->lock);

    return position;
}

static long aesd_ioctl(struct file *file, unsigned int ioctl_cmd, unsigned long ioctl_arg)
{
    struct aesd_seekto user_seek_params;
    struct aesd_dev *dev = file->private_data;
    loff_t seek_offset = 0;
    int command_count = 0;
    int entry_index = 0;
	
	PDEBUG("aesd_ioctl called");
    if (ioctl_cmd != AESDCHAR_IOCSEEKTO)
        return -ENOTTY;

    if (copy_from_user(&user_seek_params, (struct aesd_seekto __user *)ioctl_arg, sizeof(user_seek_params)))
        return -EFAULT;

    if (mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;

    // Calculate total valid entries in buffer
    command_count = dev->buffer.full ? AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED : dev->buffer.in_offs;

    if (user_seek_params.write_cmd >= command_count) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    entry_index = (dev->buffer.out_offs + user_seek_params.write_cmd) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    struct aesd_buffer_entry *target_entry = &dev->buffer.entry[entry_index];

    if (!target_entry->buffptr || user_seek_params.write_cmd_offset >= target_entry->size) {
        mutex_unlock(&dev->lock);
        return -EINVAL;
    }

    // Calculate seek offset by summing up sizes of previous entries
    for (int i = 0; i < user_seek_params.write_cmd; i++) {
        int idx = (dev->buffer.out_offs + i) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        seek_offset += dev->buffer.entry[idx].size;
    }

    seek_offset += user_seek_params.write_cmd_offset;
    file->f_pos = seek_offset;

    PDEBUG("IOCTL -> write_cmd=%u, offset=%u, phys_idx=%d, entry_size=%zu, new_f_pos=%lld",
           user_seek_params.write_cmd,
           user_seek_params.write_cmd_offset,
           entry_index,
           target_entry->size,
           seek_offset);

    mutex_unlock(&dev->lock);
    return 0;
}


struct file_operations aesd_fops = {
    .owner = THIS_MODULE,
    .read = aesd_read,
    .write = aesd_write,
    .open = aesd_open,
    .release = aesd_release,
    .llseek = aesd_llseek, 
    .unlocked_ioctl = aesd_ioctl,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add(&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}

int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1, "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }

    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);
    aesd_device.partial_write = NULL;
    aesd_device.partial_size = 0;

    result = aesd_setup_cdev(&aesd_device);
    if (result) {
        unregister_chrdev_region(dev, 1);
    }
    return result;
}

void aesd_cleanup_module(void)
{
    struct aesd_buffer_entry *entry;
    uint8_t index;
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        if (entry->buffptr) {
            kfree(entry->buffptr);
        }
    }

    if (aesd_device.partial_write) {
        kfree(aesd_device.partial_write);
    }

    unregister_chrdev_region(devno, 1);
}

module_init(aesd_init_module);
module_exit(aesd_cleanup_module);

