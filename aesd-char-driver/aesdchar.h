/*
 * aesdchar.h
 *
 *  Created on: Oct 23, 2019
 *      Author: Dan Walkes
 */



#ifndef AESD_CHAR_DRIVER_AESDCHAR_H_
#define AESD_CHAR_DRIVER_AESDCHAR_H_

#define AESD_DEBUG 1  //Remove comment on this line to enable debug
#define AESDCHAR_MAX_HISTORY 10  // Store last 10 write commands

#undef PDEBUG             /* undef it, just in case */
#ifdef AESD_DEBUG
#  ifdef __KERNEL__
     /* This one if debugging is on, and kernel space */
#    define PDEBUG(fmt, args...) printk( KERN_DEBUG "aesdchar: " fmt, ## args)
#  else
     /* This one for user space */
#    define PDEBUG(fmt, args...) fprintf(stderr, fmt, ## args)
#  endif
#else
#  define PDEBUG(fmt, args...) /* not debugging: nothing */
#endif

#define AESDCHAR_MAX_HISTORY 10  // Store last 10 write commands
/**
 * Structure to store a single write entry in the circular buffer
 */
struct aesd_buffer_entry {
    char *data;   // Pointer to dynamically allocated memory for write data
    size_t size;  // Size of the stored data
};
struct aesd_dev
{
    struct cdev cdev;     /* Char device structure      */
    struct aesd_buffer_entry history[AESDCHAR_MAX_HISTORY];
    int write_index;
    struct mutex lock;
};

int aesd_open(struct inode *inode, struct file *filp);
int aesd_release(struct inode *inode, struct file *filp);
ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);
int aesd_init_module(void);
void aesd_cleanup_module(void);


#endif /* AESD_CHAR_DRIVER_AESDCHAR_H_ */
