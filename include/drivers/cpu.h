#pragma once

#include <types.h>

#define __cpuid(level, a, b, c, d) \
    __asm__ volatile ("cpuid" \
        : "=a" (a), "=b" (b), "=c" (c), "=d" (d) \
        : "0" (level))

static inline uint32_t current_processor_id(void) {
    uint32_t eax, ebx, ecx, edx;
    __cpuid(1, eax, ebx, ecx, edx);
    return (ebx >> 24);
}

extern void boot_all_aps(uint8_t total_cores);
void gdt_init();

static inline uint64_t save_interrupts(void) {
    uint64_t rflags;
    __asm__ volatile("pushfq; pop %0" : "=r"(rflags) :: "memory");
    __asm__ volatile("cli" ::: "memory");
    return rflags;
}

static inline void restore_interrupts(uint64_t rflags) {
    if (rflags & (1 << 9)) {
        __asm__ volatile("sti" ::: "memory");
    }
}
