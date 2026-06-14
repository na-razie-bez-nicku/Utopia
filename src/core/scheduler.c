#include <scheduler.h>
#include <drivers/memory.h>
#include <drivers/cpu.h>
#include <drivers/screen.h>
#include <string.h>
#include <spinlock.h>

static spinlock_t scheduler_lock;
static thread_t* current_threads[256] = {NULL};
static thread_t* idle_threads[256] = {NULL};
static thread_t* ready_queue_head = NULL;
static thread_t* ready_queue_tail = NULL;
static thread_t* garbage_list = NULL;
static uint32_t next_thread_id = 0;

static void scheduler_enqueue(thread_t* t) {
    spinlock_acquire(&scheduler_lock);
    t->state = THREAD_STATE_READY;
    t->next = NULL;
    if (ready_queue_tail == NULL) {
        ready_queue_head = t;
        ready_queue_tail = t;
    } else {
        ready_queue_tail->next = t;
        ready_queue_tail = t;
    }
    spinlock_release(&scheduler_lock);
}

static thread_t* scheduler_dequeue(void) {
    spinlock_acquire(&scheduler_lock);
    thread_t* t = ready_queue_head;
    if (t != NULL) {
        ready_queue_head = t->next;
        if (ready_queue_head == NULL) {
            ready_queue_tail = NULL;
        }
        t->next = NULL;
    }
    spinlock_release(&scheduler_lock);
    return t;
}

void scheduler_init(void) {
    spinlock_init(&scheduler_lock);
    
    uint32_t cpu_id = current_processor_id();
    printk("Scheduler", "Initializing scheduler on CPU %d", cpu_id);

    thread_t* main_thread = (thread_t*)malloc(sizeof(thread_t));
    if (!main_thread) {
        printk("Scheduler", "Failed to allocate main thread TCB!");
        return;
    }

    main_thread->id = next_thread_id++;
    strcpy(main_thread->name, "main");
    main_thread->state = THREAD_STATE_RUNNING;
    main_thread->stack_base = NULL;
    main_thread->stack_ptr = NULL;
    main_thread->next = NULL;

    current_threads[cpu_id] = main_thread;
    idle_threads[cpu_id] = main_thread;

    printk("Scheduler", "Main thread initialized for CPU %d", cpu_id);
}

void scheduler_ap_init(void) {
    uint32_t cpu_id = current_processor_id();
    printk("Scheduler", "Initializing scheduler on AP CPU %d", cpu_id);

    thread_t* ap_thread = (thread_t*)malloc(sizeof(thread_t));
    if (!ap_thread) {
        printk("Scheduler", "Failed to allocate AP thread TCB for CPU %d!", cpu_id);
        return;
    }

    ap_thread->id = next_thread_id++;
    ap_thread->name[0] = 'a';
    ap_thread->name[1] = 'p';
    ap_thread->name[2] = '_';
    ap_thread->name[3] = '0' + (cpu_id % 10);
    ap_thread->name[4] = '\0';
    
    ap_thread->state = THREAD_STATE_RUNNING;
    ap_thread->stack_base = NULL;
    ap_thread->stack_ptr = NULL;
    ap_thread->next = NULL;

    current_threads[cpu_id] = ap_thread;
    idle_threads[cpu_id] = ap_thread;

    printk("Scheduler", "AP thread initialized for CPU %d", cpu_id);
}

thread_t* thread_create(const char* name, void (*entry_point)(void*), void* arg) {
    thread_t* t = (thread_t*)malloc(sizeof(thread_t));
    if (!t) {
        printk("Scheduler", "Failed to allocate TCB for new thread '%s'!", name);
        return NULL;
    }

    size_t stack_size = 8192; // 8KB stack
    void* stack_base = malloc(stack_size);
    if (!stack_base) {
        printk("Scheduler", "Failed to allocate stack for new thread '%s'!", name);
        free(t);
        return NULL;
    }

    t->id = next_thread_id++;
    strcpy(t->name, name);
    t->state = THREAD_STATE_READY;
    t->stack_base = stack_base;
    t->stack_size = stack_size;
    t->next = NULL;

    // Setup the stack content
    uint64_t stack_bottom = (uint64_t)stack_base + stack_size;
    stack_bottom = stack_bottom & ~15ULL; // 16-byte align

    // Push thread_exit return address
    stack_bottom -= 8;
    *(uint64_t*)stack_bottom = (uint64_t)thread_exit;

    // Allocate registers_t frame on stack
    registers_t* regs = (registers_t*)(stack_bottom - sizeof(registers_t));
    
    // Clear registers frame
    memset(regs, 0, sizeof(registers_t));

    regs->rip = (uint64_t)entry_point;
    regs->rdi = (uint64_t)arg; // first parameter in x86_64 Calling Convention
    regs->cs = 0x08;           // Kernel Code Segment
    regs->ss = 0x10;           // Kernel Data Segment
    regs->rflags = 0x202;      // Enabled interrupts
    regs->rsp = (uint64_t)stack_bottom;

    t->stack_ptr = regs;

    printk("Scheduler", "Thread '%s' (ID %d) created, stack_ptr: %p", t->name, t->id, t->stack_ptr);

    // Enqueue in ready list
    scheduler_enqueue(t);

    return t;
}

registers_t* scheduler_schedule(registers_t* regs) {
    uint32_t cpu_id = current_processor_id();
    thread_t* curr = current_threads[cpu_id];

    // If scheduler hasn't been initialized yet for this CPU, do nothing
    if (!curr) return regs;

    // 1. Process garbage list
    thread_t* garbage = NULL;
    spinlock_acquire(&scheduler_lock);
    garbage = garbage_list;
    garbage_list = NULL;
    spinlock_release(&scheduler_lock);

    while (garbage) {
        thread_t* next_garbage = garbage->next;
        if (garbage->stack_base) {
            free(garbage->stack_base);
        }
        free(garbage);
        garbage = next_garbage;
    }

    // 2. Save current thread's registers
    curr->stack_ptr = regs;

    // 3. Put current thread back to ready queue if it was running
    // (and is not the idle/main thread, which we do NOT put in the ready queue)
    if (curr->state == THREAD_STATE_RUNNING && curr != idle_threads[cpu_id]) {
        curr->state = THREAD_STATE_READY;
        scheduler_enqueue(curr);
    }

    // 4. Get next thread to run
    thread_t* next = scheduler_dequeue();
    if (!next) {
        // No threads in the ready queue!
        if (curr->state != THREAD_STATE_TERMINATED) {
            // Keep running the current thread if it's still alive
            curr->state = THREAD_STATE_RUNNING;
            return regs;
        }
        // If current thread is terminated and nothing else is ready, fallback to idle thread
        next = idle_threads[cpu_id];
    }

    next->state = THREAD_STATE_RUNNING;
    current_threads[cpu_id] = next;

    return next->stack_ptr;
}

void thread_yield(void) {
    asm volatile("int $32");
}

void thread_exit(void) {
    uint32_t cpu_id = current_processor_id();
    thread_t* curr = current_threads[cpu_id];

    if (curr && curr != idle_threads[cpu_id]) {
        curr->state = THREAD_STATE_TERMINATED;

        spinlock_acquire(&scheduler_lock);
        curr->next = garbage_list;
        garbage_list = curr;
        spinlock_release(&scheduler_lock);
    }

    thread_yield();

    // Loop forever if it ever returns (should not)
    while (1) {
        asm volatile("hlt");
    }
}

thread_t* scheduler_get_current_thread(void) {
    uint32_t cpu_id = current_processor_id();
    return current_threads[cpu_id];
}
