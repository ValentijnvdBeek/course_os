#include <stdio.h>
#include <stdint.h>
#include <vp_array_list.h>
#include <stdlib.h>
#include <vas2.h>

#include "./include/process.h"
#include "./include/thread.h"

Process *create_process(void *entry, Process *parent) {
    Process *process = kmalloc(sizeof(Process));

    process->parent = parent;
    process->priority = DEFAULT_PROCESS_PRIORITY;
    process->vas = create_vas();
    process->threads = vpa_create(10);

    Thread *thread = create_thread(entry, process);
    vpa_push(process->threads, thread);

    return process;
}

void free_process(Process *process) {
    free_vas(process->vas);
    vpa_free(process->threads, (FreeFunc) free_thread);

    kfree(process);
}