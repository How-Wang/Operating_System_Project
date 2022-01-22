// SPDX-License-Identifier: GPL-2.0
#include "my_info.h"

extern void si_meminfo(struct  sysinfo *val);
static int my_info_proc_show(struct seq_file *m, void *v)
{
    struct cpuinfo_x86 cpuinfo;
    struct sysinfo sys_info;
    struct timespec uptime;
    struct timespec idle;
    uint64_t nsec;
    uint32_t rem;
    int i=0;
    int j=0;

    seq_printf(m, "\n================Version================\n");
    seq_printf(m, "Linux version %s\n", UTS_RELEASE);
    seq_printf(m, "\n================CPU====================\n");
    for_each_possible_cpu(j)
    {
        cpuinfo = cpu_data(j);
        seq_printf(m, "processor          : %d\n", cpuinfo.cpu_index );
        seq_printf(m, "model name         : %s\n", cpuinfo.x86_model_id);
        seq_printf(m, "physical id        : %d\n", cpuinfo.phys_proc_id);
        seq_printf(m, "core id            : %d\n", cpuinfo.cpu_core_id);
        seq_printf(m, "cpu cores          : %d\n", cpuinfo.booted_cores);
        seq_printf(m, "cache size         : %d\n", cpuinfo.x86_cache_size);
        seq_printf(m, "clflush size       : %d\n", cpuinfo.x86_clflush_size);
        seq_printf(m, "cache_alignment    : %d\n", cpuinfo.x86_cache_alignment);
        seq_printf(m, "address sizes      : %d bits physical, %d bits virtual\n\n", cpuinfo.x86_phys_bits,cpuinfo.x86_virt_bits);
    }
    si_meminfo(&sys_info);
    seq_printf(m, "\n================Memory=================\n");
    seq_printf(m, "MemTotal           : %ld kb\n", sys_info.totalram<<(PAGE_SHIFT - 10));
    seq_printf(m, "MemFree            : %ld kb\n", sys_info.freeram<<(PAGE_SHIFT - 10));
    seq_printf(m, "Buffers            : %ld kb\n", sys_info.bufferram<<(PAGE_SHIFT - 10));
    seq_printf(m, "Active             : %ld kb\n", (global_node_page_state(NR_LRU_BASE + LRU_ACTIVE_ANON) +
               global_node_page_state(NR_LRU_BASE + LRU_ACTIVE_FILE)   )  << (PAGE_SHIFT - 10) );
    seq_printf(m, "Inactive           : %ld kb\n", (global_node_page_state(NR_LRU_BASE + LRU_INACTIVE_ANON) +
               global_node_page_state(NR_LRU_BASE + LRU_INACTIVE_FILE)  ) << (PAGE_SHIFT - 10) );
    seq_printf(m, "Shmem              : %ld kb\n", sys_info.sharedram<<(PAGE_SHIFT - 10));
    seq_printf(m, "Dirty              : %ld kb\n", global_node_page_state(NR_FILE_DIRTY) << (PAGE_SHIFT - 10));
    seq_printf(m, "Writeback          : %ld kb\n", global_node_page_state(NR_WRITEBACK) << (PAGE_SHIFT - 10));
    seq_printf(m, "Kernelstack        : %ld kb\n", global_zone_page_state(NR_KERNEL_STACK_KB));
    seq_printf(m, "PageTables         : %ld kb\n", global_zone_page_state(NR_PAGETABLE) << (PAGE_SHIFT - 10));

    nsec = 0;
    for_each_possible_cpu(i)
    nsec += (__force u64) kcpustat_cpu(i).cpustat[CPUTIME_IDLE];
    get_monotonic_boottime(&uptime);
    idle.tv_sec = div_u64_rem(nsec, NSEC_PER_SEC, &rem);
    idle.tv_nsec = rem;
    seq_printf(m, "\n================Time===================\n");
    seq_printf(m, "Uptime             : %lu.%02lu (s)\nIdletime           : %lu.%02lu (s)\n",
               (unsigned long) uptime.tv_sec,    (uptime.tv_nsec / (NSEC_PER_SEC / 100)),
               (unsigned long) idle.tv_sec,      (idle.tv_nsec / (NSEC_PER_SEC / 100)));
    return 0;
}


static int my_info_proc_open(struct inode *inode, struct  file *file)
{
    return single_open(file, my_info_proc_show, NULL);
}

static const struct file_operations my_info_proc_fops =
{
    .owner = THIS_MODULE,
    .open = my_info_proc_open,
    .read = seq_read,
    .llseek = seq_lseek,
    .release = single_release,
};

static int __init my_info_proc_init(void)
{
    proc_create("my_info", 0, NULL, &my_info_proc_fops);
    return 0;
}

static void __exit my_info_proc_exit(void)
{
    remove_proc_entry("my_info", NULL);
}

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Howard Wang");

module_init(my_info_proc_init);
module_exit(my_info_proc_exit);
