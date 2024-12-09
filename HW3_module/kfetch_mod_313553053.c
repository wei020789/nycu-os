#include <linux/atomic.h> 
#include <linux/cdev.h> 
#include <linux/fs.h> 
#include <linux/init.h> 
#include <linux/kernel.h> /* for sprintf() */ 
#include <linux/module.h> 
#include <linux/uaccess.h> /* for get_user and put_user */ 
#include <linux/version.h> 
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/utsname.h>
#include <linux/sched/signal.h>
#include <linux/time.h>
#include <linux/time_namespace.h>
#include <asm/errno.h>
#include <asm/processor.h>
 
static int kfetch_open(struct inode *, struct file *);
static int kfetch_release(struct inode *, struct file *);
static ssize_t kfetch_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t kfetch_write(struct file *, const char __user *, size_t, loff_t *);

#define SUCCESS 0 
#define DEVICE_NAME "kfetch" /* Dev name as it appears in /proc/devices   */ 
#define Proc
#define BUF_LEN 400 /* Max length of the message from the device */ 
#define PROCFS_MAX_SIZE 2048UL 

#define KFETCH_NUM_INFO 6

#define KFETCH_RELEASE   (1 << 0)
#define KFETCH_NUM_CPUS  (1 << 1)
#define KFETCH_CPU_MODEL (1 << 2)
#define KFETCH_MEM       (1 << 3)
#define KFETCH_UPTIME    (1 << 4)
#define KFETCH_NUM_PROCS (1 << 5)

#define KFETCH_FULL_INFO ((1 << KFETCH_NUM_INFO) - 1)

/* Global variables are declared as static, so are global within the file. */

static int major; /* major number assigned to our device driver */ 
 
enum { 
    CDEV_NOT_USED, 
    CDEV_EXCLUSIVE_OPEN , 
}; 
 
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED); 
 
static char msg[BUF_LEN + 1]; /* The msg the device will give when asked */ 
static struct class *cls;
unsigned int mask = KFETCH_FULL_INFO;

const char logo[7][20] = {
	"        .-.        ",
	"       (.. |       ",
	"       <>  |       ",
	"      / --- \\      ",
	"     ( |   | |     ",
	"   |\\\\_)___/\\)/\\   ",
	"  <__)------(__/   "
};

const static struct file_operations kfetch_ops = { 
   	.owner = THIS_MODULE,
	.read = kfetch_read,
   	.write = kfetch_write, 
	.open = kfetch_open, 
	.release = kfetch_release 
}; 

static int kfetch_open(struct inode *inode, struct file *file) { 
	if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN)) 
		return -EBUSY; 
 
	try_module_get(THIS_MODULE); 
 
	return 0;
} 
 
static int kfetch_release(struct inode *inode, struct file *file) { 
	atomic_set(&already_open, CDEV_NOT_USED); 
 	module_put(THIS_MODULE); 
 
	return SUCCESS; 
}

static ssize_t kfetch_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset) {
    int bytes_read = 0, count = 0, len = 0;
    struct new_utsname *uts = utsname();
    struct sysinfo si;
    struct timespec64 uptime;
    unsigned int num_procs = 0;
    struct task_struct *task;

    // Hostname
    bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%19s%s\n", "", uts->nodename);
    len = bytes_read - 19;

	// Seperation line
	bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%s", logo[count++]);

	for (int i = 0; i < len - 1 && bytes_read < BUF_LEN - 1; i++) {
		bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "-");
	}

	bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "\n");

    // Kernel
    if (mask & KFETCH_RELEASE) {
        bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%sKernel:\t%s\n", logo[count++], uts->release);
    }

    // CPU
    if (mask & KFETCH_CPU_MODEL) {
        struct cpuinfo_x86 *c = &boot_cpu_data;
        bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%sCPU:\t\t%s\n", logo[count++], c->x86_model_id);
    }

    // CPUs
    if (mask & KFETCH_NUM_CPUS) {
        bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%sCPUs:\t%d / %d\n", logo[count++], num_online_cpus(), num_active_cpus());
    }

    // Mem
    if (mask & KFETCH_MEM) {
        si_meminfo(&si);
        bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%sMem:\t\t%lu MB / %lu MB\n", 
                               logo[count++], 
                               si.freeram * si.mem_unit / 1024 / 1024, 
                               si.totalram * si.mem_unit / 1024 / 1024);
    }

    // Procs
    if (mask & KFETCH_NUM_PROCS) {
        for_each_process(task) {
            num_procs++;
        }
        bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%sProcs:\t%u\n", logo[count++], num_procs);
    }

    // Uptime
    if (mask & KFETCH_UPTIME) {
        ktime_get_boottime_ts64(&uptime);
        timens_add_boottime(&uptime);
        bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%sUptime:\t%lu mins\n", 
                               logo[count++], 
                               (unsigned long)(uptime.tv_sec / 60));
    }

    // Fill remaining logo lines
    while (count < 7) {
        bytes_read += snprintf(msg + bytes_read, BUF_LEN - bytes_read, "%s\n", logo[count++]);
    }

    // copy to user space
    if (copy_to_user(buffer, msg, bytes_read)) {
        pr_alert("Failed to copy data to user");
        return -EFAULT;
    }

    return bytes_read;
}

 
static ssize_t kfetch_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset) {
	int mask_info; 
	if (copy_from_user(&mask_info, buffer, length)) {
		pr_alert("Failed to copy data from user");
		return 0;
	}
	mask = mask_info;
	return 0;
} 

static int __init kfetch_init(void) { 
    major = register_chrdev(0, DEVICE_NAME, &kfetch_ops); 
 
    if (major < 0) { 
        pr_alert("Registering char device failed with %d\n", major); 
        return major; 
    } 
 
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0) 
    cls = class_create(DEVICE_NAME); 
#else 
    cls = class_create(THIS_MODULE, DEVICE_NAME); 
#endif 
    device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME); 
 
    return SUCCESS; 
} 
 
static void __exit kfetch_exit(void) { 
    device_destroy(cls, MKDEV(major, 0)); 
    class_destroy(cls); 
 
    unregister_chrdev(major, DEVICE_NAME); 
} 
 
module_init(kfetch_init); 
module_exit(kfetch_exit); 
 
MODULE_LICENSE("GPL");
MODULE_AUTHOR("313553053");
MODULE_DESCRIPTION("Fetch kernel module information");