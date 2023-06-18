#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define MYSIZE 200000
MODULE_LICENSE("GPL");

static struct dentry * dir, * inputdir, * ptreedir;
static struct task_struct * curr;
static struct debugfs_blob_wrapper wrapper;
void printreverse(struct task_struct *task) {
    const int MAX_MESSAGES = MYSIZE;  // Adjust the maximum number of messages as needed
    char messages[MAX_MESSAGES][256];  // Array to store the formatted messages
    int numMessages = 0;


    while (task->pid != 1 && numMessages < MAX_MESSAGES) {
        numMessages += snprintf(messages[numMessages], sizeof(messages[0]), "%s (%d)\n", task->comm, task->pid);
        task = task->real_parent;
    }
    numMessages += snprintf(messages[numMessages], sizeof(messages[0]), "%s (%d)\n", task->comm, task->pid);

    for (int i = numMessages - 1; i >= 0; --i) {
        wrapper.size += snprintf(wrapper.data + wrapper.size, MYSIZE - wrapper.size, "%s", messages[i]);
    }
}



static ssize_t write_pid_to_input(struct file * fp,const char __user * user_buffer,
        size_t length,loff_t * position) {
        pid_t input_pid;

        sscanf(user_buffer, "%u", & input_pid);
        curr = pid_task(find_vpid(input_pid), PIDTYPE_PID);
        if (curr == NULL) {
            printk("Invalid PID\n");
            return length;
        }
        wrapper.size = 0;

        printreverse(curr);
        return length;
}

static const struct file_operations dbfs_fops = {
        .write = write_pid_to_input,
};

static int __init dbfs_module_init(void) {

    //create "ptree" at the root of the debugfs filesystem
    dir = debugfs_create_dir("ptree", NULL);

    if (!dir) {
        printk("Cannot create ptree dir\n");
        return -1;
    }
    inputdir = debugfs_create_file("input", 0644, dir, NULL, & dbfs_fops);
    if (!inputdir){
        printk("Cannot create input file\n");
        return -1;
    }
    ptreedir = debugfs_create_blob("ptree", 0444, dir, & wrapper);
    if (!ptreedir){
        printk("Cannot create ptree file\n");
        return -1;
    }
    static char buffer[MYSIZE];
    wrapper.data = buffer;

    printk("dbfs_ptree module initialize done\n");

    return 0;
}

static void __exit dbfs_module_exit(void) {
    debugfs_remove_recursive(dir);
    printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);