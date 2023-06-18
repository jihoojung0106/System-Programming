#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#include <linux/pgtable.h>

MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;
struct packet {
    pid_t pid;
    unsigned long vaddr;
    unsigned long paddr;
};
static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
    struct packet *pac = (struct packet *)user_buffer;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    task = pid_task(find_vpid(pac->pid), PIDTYPE_PID);
    if (task == NULL) {
        return -1;
    }
    pgd = pgd_offset(task->mm, pac->vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)){
//        printk("not mapped in pgd\n");
        return -1;}

    p4d = p4d_offset(pgd, pac->vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
    return 0;

pud = pud_offset(p4d, pac->vaddr);
    if (pud_none(*pud) || pud_bad(*pud)){
//        printk("not mapped in pud\n");
    return -1;}
    pmd = pmd_offset(pud, pac->vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)){
//        printk("not mapped in pmd\n");
    return -1;}
    pte = (pte_t *)pte_offset_kernel(pmd,pac->vaddr);
    if(pte_none(*pte)){
//    printk("not mapped in pte\n");
    return -1;
    }

unsigned long page_addr = 0;
unsigned long page_offset = 0;

page_addr=pte_pfn(*pte)<<PAGE_SHIFT;
page_offset=pac->vaddr &(~PAGE_MASK);

pac->paddr = page_addr | page_offset;
//printk("page_addr = %lx, page_offset = %lx\n", page_addr, page_offset);
//printk("vaddr = %lx, paddr = %lx\n", vaddr, paddr);

return 0;
}

static const struct file_operations dbfs_fops = {
        .read = read_output,
};

static int __init dbfs_module_init(void)
{

        dir = debugfs_create_dir("paddr", NULL);
        if (!dir) {
                printk("Cannot create paddr dir\n");
                return -1;
        }
        output = debugfs_create_file("output",  0444, dir, NULL, & dbfs_fops);
       printk("dbfs_paddr module initialize done\n");
        return 0;
}

static void __exit dbfs_module_exit(void)
{
    debugfs_remove_recursive(dir);
   printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
