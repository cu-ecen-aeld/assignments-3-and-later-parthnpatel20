/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Parth Patel"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

int aesd_open(struct inode *inode, struct file *filp)
{
    struct aesd_dev *dev;

    PDEBUG("open");

    // Get the aesd_dev structure from inode->i_cdev
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    if (!dev) {
        PDEBUG("Error: aesd_dev structure is NULL");
        return -ENODEV;  // Return error if dev is NULL
    }

    filp->private_data = dev;  // Store device structure in filp->private_data

    return 0;  // Success
}


int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = 0;
    size_t copied = 0;
    size_t remaining;
    int i;
    struct aesd_dev *dev = filp->private_data;

    PDEBUG("read %zu bytes, f_pos = %lld", count, *f_pos);

    mutex_lock(&dev->lock);

    // Find where f_pos is pointing within the stored data
    for (i = 0; i < AESDCHAR_MAX_HISTORY; i++) {
        if (!dev->history[i].data)
            continue;

        if (*f_pos < dev->history[i].size) {
            // Calculate how much can be read
            remaining = dev->history[i].size - *f_pos;
            copied = (count < remaining) ? count : remaining;

            // Copy to user space
            if (copy_to_user(buf, dev->history[i].data + *f_pos, copied)) {
                mutex_unlock(&dev->lock);
                return -EFAULT;
            }

            *f_pos += copied;  // Update file position
            retval = copied;
            break;
        }

        // Move f_pos forward
        *f_pos -= dev->history[i].size;
    }

    mutex_unlock(&dev->lock);
    return retval;
}



ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    char *kbuf;
    
    PDEBUG("write %zu bytes", count);

    // Allocate kernel memory
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf)
        return -ENOMEM;

    // Copy data from user space to kernel space
    if (copy_from_user(kbuf, buf, count)) {
        kfree(kbuf);
        return -EFAULT;
    }

    mutex_lock(&aesd_device.lock);

    // Free the oldest entry if buffer is full
    if (aesd_device.history[aesd_device.write_index].data) {
        kfree(aesd_device.history[aesd_device.write_index].data);
    }

    // Store new data in circular buffer
    aesd_device.history[aesd_device.write_index].data = kbuf;
    aesd_device.history[aesd_device.write_index].size = count;
    aesd_device.write_index = (aesd_device.write_index + 1) % AESDCHAR_MAX_HISTORY;

    mutex_unlock(&aesd_device.lock);

    return count;
}


struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

 

    mutex_init(&aesd_device.lock);
    aesd_device.write_index = 0;
    for (int i = 0; i < AESDCHAR_MAX_HISTORY; i++) {
        aesd_device.history[i].data = NULL;
        aesd_device.history[i].size = 0;
    }

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

   mutex_lock(&aesd_device.lock);
    for (int i = 0; i < AESDCHAR_MAX_HISTORY; i++) {
        if (aesd_device.history[i].data)
            kfree(aesd_device.history[i].data);
    }
    mutex_unlock(&aesd_device.lock);

    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
