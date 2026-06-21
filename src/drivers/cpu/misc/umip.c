#include <drivers/cpu.h>
#include <drivers/screen.h>

// saw this in linux kernel logs and after some reading thought to implement this here;
// umip (user mode instruction prevention) is a feature that blocks ring 3 threads from
// accessing instructions like `sgdt` which allow to read internal cpu descriptor tables,
// enabling this blocks programs from fingerprinting certain parts of your kernel
void enable_umip(void) {
    unsigned long cr4 = read_cr4();

    cr4 |= CPU_CR4_UMIP;

    write_cr4(cr4);

    printk("Mitigations", "Enabled User Mode Instruction Prevention for CPU %d", current_processor_id());
}
