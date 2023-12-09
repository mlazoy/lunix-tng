/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * < Your name here >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;

/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
/*
 * Declare a prototype so we can define the "unused" attribute and keep
 * the compiler happy. This function is not yet used, because this helpcode
 * is a stub.
 */
//static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *);
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	WARN_ON ( !(sensor = state->sensor));
	/* ? */
	if (state->buf_timestamp != sensor->msr_data[state->type]->last_update){
	return 1;
	}
	return 0;
	/* The following return is bogus, just for the stub to compile */
	//return 0; /* ? */
}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct __attribute__((unused)) *sensor;

	unsigned int type;
	uint32_t new_msr, new_time;
	char sign;
	long int interger,decimals;
	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
	sensor = state->sensor;
	type = state->type;
	spin_lock(&sensor->lock);
	/* ? */
	/* Why use spinlocks? See LDD3, p. 119 */

	/*
	 * Any new data available?
	 */
	new_msr = sensor->msr_data[type]->values[0];
	new_time = sensor->msr_data[type]->last_update;

	spin_unlock(&sensor->lock);
	/* ? */
	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */
	if (!lunix_chrdev_state_needs_refresh(state)){ //is up to date
		return -EAGAIN;
	}

	if (state->raw_data){
		//skip formatting
		//just copy new_msr to buf_data here
		memcpy(state->buf_data, &new_msr, sizeof(new_msr));
		state->buf_lim = sizeof(new_msr);
		return 0;
	}

	
	state->buf_timestamp = new_time;
	//copy new values into buf_data here
	switch(type){
		case BATT: new_msr = lookup_voltage[new_msr];
		break;
		case TEMP: new_msr = lookup_temperature[new_msr];
		break;
		case LIGHT: new_msr = lookup_light[new_msr];
		break;
		default: return -EINVAL; //unknown state ... change this??
	}

	if (new_msr>0){
		sign = '+';
		interger = new_msr/1000;
		decimals = new_msr%1000;
	}

	else if (new_msr<0){
		sign = '-';
		interger = -new_msr/1000;
		decimals = -new_msr%1000;
	}

	else {
	sign = ' ';
	interger = decimals = 0;
	}

	state->buf_lim = snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%c%ld.%03ld\n", sign, interger, decimals);
	
	/* ? */
	

	debug("leaving\n");
	return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
	/* Declarations */
	/* ? */
	struct lunix_chrdev_state_struct *new_state;
	unsigned int NO, TYPE;
	int ret;

	debug("entering\n");
	ret = -ENODEV;
	if ((ret = nonseekable_open(inode, filp)) < 0)
		goto out;

	/*
	 * Associate this open file with the relevant sensor based on
	 * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
	 */
	TYPE = iminor(inode)%8; //mod 8 gets measurement code
	NO = iminor(inode)/8; // div 8 gets sensor num

	if (NO>=LUNIX_SENSOR_CNT || TYPE>=N_LUNIX_MSR){
		ret = -ENODEV;
		goto out;
	}
	//allocate space
	new_state = kzalloc(sizeof(struct lunix_chrdev_state_struct),GFP_KERNEL);
	if (new_state == NULL){
		ret = -ENOMEM; //kzalloc error
		goto out;
	}

	/* Allocate a new Lunix character device private state structure */
	new_state->type = TYPE; //typecasting?
	new_state->sensor = lunix_sensors + NO;
	new_state->buf_timestamp = 0; //fill this in update 
	sema_init(&new_state->lock,1);

	/* ? */
	filp->private_data = new_state; //destroy this in release
	// is this needed ?
	//filp->f_mode = FMODE_READ ; 
	//filp->f_pos = 0; //move to start of file 
out:
	debug("leaving, with ret = %d\n", ret);
	return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	/* ? */
	struct lunix_chrdev_state_struct *find_sensor_struct;
	find_sensor_struct = filp->private_data;
	//only if last file poining to this inode ?
	//hmm... close method is responsible for this
	kfree(find_sensor_struct);
		
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* Why? */
	//chooses between raw and cooked data mode
	struct lunix_chrdev_state_struct *curr_state;
	curr_state = filp->private_data;

	if( _IOC_TYPE(cmd) != LUNIX_IOC_MAGIC || 
	_IOC_NR(cmd) > LUNIX_IOC_MAXNR ) {
		return -ENOTTY; 
	}

	switch(cmd){
		case RAW: curr_state->raw_data = 1;
		break;
		case COOKED: curr_state->raw_data = 0;
		break;
		default: return -ENOTTY; //no such mode
	} 
	return 0;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;

	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;


	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	/* Lock? */
	if(down_interruptible(&state->lock)) return -ERESTARTSYS;

	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */
	if (*f_pos == 0) {
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* ? */
			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */
			up(&state->lock);
			if (filp->f_flags & O_NONBLOCK) 
				return -EAGAIN;

			if (wait_event_interruptible(sensor->wq,lunix_chrdev_state_needs_refresh(state) )) 
				return -ERESTARTSYS;

			if(down_interruptible(&state->lock)) 
				return -ERESTARTSYS;
			
		}
	}

	/* End of file */
	/* ? */
	if (*f_pos > state->buf_lim)
		goto out;
	
	/* Determine the number of cached bytes to copy to userspace */
	/* ? */
	if (*f_pos + cnt >= state->buf_lim) 
		cnt = state->buf_lim - *f_pos;

	if (copy_to_user(usrbuf, state->buf_data + *f_pos, cnt)) {
		ret = -EFAULT;
		goto out;
	}
	*f_pos += cnt;
	ret = cnt;

	/* Auto-rewind on EOF mode? */
	/* ? */
	if (*f_pos == state->buf_lim) 
		*f_pos = 0;

	/*
	 * The next two lines  are just meant to suppress a compiler warning
	 * for the "unused" out: label, and for the uninitialized "ret" value.
	 * It's true, this helpcode is a stub, and doesn't use them properly.
	 * Remove them when you've started working on this code.
	 */
    
out:
	/* Unlock? */
	up(&state->lock);
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
    .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
	
	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	/* ? */
	/* register_chrdev_region? */
	//kobject_set_name(&(lunix_chrdev_cdev.kobj),"lunix_sensors");
	//ret = register_chrdev_region(dev_no,lunix_minor_cnt, kobject_name(&(lunix_chrdev_cdev.kobj)));
	ret = register_chrdev_region(dev_no,lunix_minor_cnt,"LUNIX");
	/* Since this code is a stub, exit early */
	//return 0;
	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}	
	/* ? */
	/* cdev_add? */
	ret = cdev_add(&lunix_chrdev_cdev,dev_no,lunix_minor_cnt);
	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
		
	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}

