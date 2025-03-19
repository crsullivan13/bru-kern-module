#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/kobject.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/string.h>
#include <linux/printk.h>

#define BUF_SIZE 256
#define DEVICE_NAME "bru_driver"

#define N_DOMAINS 1 // TODO: read this from bru registers eventually
#define N_CLIENTS 1 // TODO: read this from bru registers eventually

#define UNMAPPED_BASE 0x20000000 // Reg-map base address
#define MAX  0x20000800 // Reg-map max address

static void __iomem *bru_mmio_base;
#define MAPPED_BASE bru_mmio_base
#define GLOBAL_EN (uintptr_t)(MAPPED_BASE)
#define SETTINGS (uintptr_t)(0x8 + MAPPED_BASE)
#define PERIOD_LEN (uintptr_t)(0x10 + MAPPED_BASE)
// N_DOMAINS # of fields
#define DOMAIN_READ(i) (uintptr_t)( ( 24 + i * 8 ) + MAPPED_BASE )
// Each enable is 1 bit, read 8 bytes at a time
#define CLIENT_EN(i) (uintptr_t)( ( 24 + 8 * N_DOMAINS + 8 * i ) + MAPPED_BASE )
// domain that a client is in, should be one-hot
#define CLIENT_DOMAIN(i) (uintptr_t)( ( 48 + 8 * N_DOMAINS + 8 * i ) + MAPPED_BASE )

static struct dentry *bru_dir;

// Use debugfs to configure, links below are useful and so is the memguard code
// https://www.kernel.org/doc/html/latest/filesystems/debugfs.html
// https://linux-kernel-labs.github.io/refs/heads/master/labs/device_drivers.html
// https://www.kernel.org/doc/html/v6.14-rc7/filesystems/seq_file.html
// https://linux-kernel-labs.github.io/refs/heads/master/labs/device_drivers.html

static int domain_reads_show(struct seq_file *m, void *p) {
    int i;

    seq_printf(m, "Domain N: budget (accesses)\n");

    for ( i = 0; i < N_DOMAINS; i++ ) {
        int budget = readq((volatile void *)DOMAIN_READ(i));

        seq_printf(m, "Domain %d: %d\n", i, budget);
    }

    return 0;
}

static int domain_reads_open(struct inode *inode, struct file *file) {
    return single_open(file, domain_reads_show, NULL);
}

static ssize_t domain_reads_write(struct file *file, const char __user *user_buff, size_t size, loff_t *offset) {
    char buf[BUF_SIZE];
    char *p = buf;
    int i;

    if ( copy_from_user(&buf, user_buff, (size > BUF_SIZE ? BUF_SIZE : size)) )
        return 0;

    for ( i = 0; i < N_DOMAINS; i++ ) {
        uint64_t budget;

        sscanf(p, "%lld", &budget);
        if ( budget == 0 ) {
            pr_err("Domain %d assigned budget of 0\n", i);
            continue;
        }

        writeq(budget, (volatile void *)DOMAIN_READ(i));

        p = strchr(p, ' ');
        if ( !p ) {
            break;
        }
        p++;
    }

    return size;
}

const struct file_operations domain_read_fops = {
    .open = domain_reads_open,
    .write = domain_reads_write,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int global_enable_show(struct seq_file *m, void *p) {
    seq_printf(m, "enabled: %s\n", (readq((volatile void *)GLOBAL_EN) ? "on" : "off"));

    return 0;
}

static int global_enable_open(struct inode *inode, struct file *file) {
    return single_open(file, global_enable_show, NULL);
}

static ssize_t global_enable_write(struct file *file, const char __user *user_buff, size_t size, loff_t *offset) {
    char buf[BUF_SIZE];
    char *p = buf;
    uint64_t field;

    if ( copy_from_user(&buf, user_buff, (size > BUF_SIZE ? BUF_SIZE : size)) )
        return 0;

    sscanf(p, "%lld", &field);
    writeq(field, (volatile void *)GLOBAL_EN);

    return size;
}

const struct file_operations global_enable_fops = {
    .open = global_enable_open,
    .write = global_enable_write,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int global_period_show(struct seq_file *m, void *p) {
    seq_printf(m, "period: %lld cycles\n", readq((volatile void *)PERIOD_LEN));

    return 0;
}

static int global_period_open(struct inode *inode, struct file *file) {
    return single_open(file, global_period_show, NULL);
}

static ssize_t global_period_write(struct file *file, const char __user *user_buff, size_t size, loff_t *offset) {
    char buf[BUF_SIZE];
    char *p = buf;
    uint64_t field;

    if ( copy_from_user(&buf, user_buff, (size > BUF_SIZE ? BUF_SIZE : size)) )
        return 0;

    sscanf(p, "%lld", &field);
    pr_info("BRU Driver: Setting period to %lld\n", field);
    writeq(field, (volatile void *)PERIOD_LEN);

    return size;
}

const struct file_operations global_period_fops = {
    .open = global_period_open,
    .write = global_period_write,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int client_control_show(struct seq_file *m, void *p) {
    int i;
    uint64_t enables, domain, enable_mask;

    seq_printf(m, "Client N: enable | domain\n");

    enables = readq((volatile void *)CLIENT_EN(0)); // TODO: this is weird

    for ( i = 0; i < N_CLIENTS; i++ ) {
        domain = readq((volatile void *)CLIENT_DOMAIN(i));

        enable_mask = 1 << i;

        seq_printf(m, "Client %d: %s\t%lld\n", i, (enables & enable_mask ? "yes" : "no"), domain);
    }

    return 0;
}

static int client_control_open(struct inode *inode, struct file *file) {
    return single_open(file, client_control_show, NULL);
}

static ssize_t client_control_write(struct file *file, const char __user *user_buff, size_t size, loff_t *offset) {
    char buf[BUF_SIZE];
    char *p = buf;
    int i;
    uint64_t enables;

    if ( copy_from_user(&buf, user_buff, (size > BUF_SIZE ? BUF_SIZE : size)) )
        return 0;

    enables = 0;
    for ( i = 0; i < N_CLIENTS; i++ ) {
        uint64_t domain, local_enable;

        sscanf(p, "%lld", &local_enable);
        enables = enables | (local_enable << i);

        p = strchr(p, ' ');

        sscanf(p, "%lld", &domain);
        writeq(domain, (volatile void *)CLIENT_DOMAIN(i));

        p = strchr(p, ' ');
        if ( !p ) {
            break;
        }
        p++;
    }

    writeq(enables, (volatile void *)CLIENT_EN(0)); // TODO: this is weird

    return size;
}

const struct file_operations client_control_fops = {
    .open = client_control_open,
    .write = client_control_write,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int init_bru_debugfs(void) {
    int ret = 0;

    bru_dir = debugfs_create_dir("bru", NULL);
    if ( !bru_dir ) {
        pr_err("BRU Driver: failed to create debugfs dir\n");
        ret = -1;
    } else {
        debugfs_create_file("domain_reads", 0644, bru_dir, NULL, &domain_read_fops);
        debugfs_create_file("global_enable", 0644, bru_dir, NULL, &global_enable_fops);
        debugfs_create_file("global_period", 0644, bru_dir, NULL, &global_period_fops);
        debugfs_create_file("client_control", 0644, bru_dir, NULL, &client_control_fops);
    }

    return ret;
}

static int __init bru_start(void) {
    pr_info("BRU MMIO driver: asking for region\n");
    // ask for the region
    if ( !request_mem_region(UNMAPPED_BASE, MAX, DEVICE_NAME) )
        return -EBUSY;


    pr_info("BRU MMIO driver: mapping region\n");
    // actually map the region
    bru_mmio_base = ioremap(UNMAPPED_BASE, MAX);
    if ( !bru_mmio_base ) {
        release_mem_region(UNMAPPED_BASE, MAX);
        return -ENOMEM;
    }

    init_bru_debugfs();

    pr_info("BRU MMIO driver initialized\n");
    return 0;
}

static void __exit bru_end(void) {
    iounmap(bru_mmio_base);
    release_mem_region(UNMAPPED_BASE, MAX);

    pr_info("BRU MMIO driver clean up\n");
}

module_init(bru_start);
module_exit(bru_end);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Connor Sullivan");
MODULE_DESCRIPTION("LLC BRU driver");