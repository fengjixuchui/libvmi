/*
 * The LibVMI Library is an introspection library that simplifies access to 
 * memory in a target virtual machine or in a file containing a dump of 
 * a system's physical memory.  LibVMI is based on the XenAccess Library.
 *
 * Copyright (C) 2010 Sandia National Laboratories
 * Author: Bryan D. Payne (bpayne@sandia.gov)
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <stdio.h>
#include <libvmi/libvmi.h>
#include <libvmi/private.h>

#ifdef ENABLE_XEN 
int main (int argc, char **argv)
{
    xa_instance_t xai;
    unsigned char *memory = NULL;
    uint32_t offset, next_process, list_head;
    char *name = NULL;
    int pid = 0;
    int tasks_offset, pid_offset, name_offset;

    /* this is the domain ID that we are looking at */
    uint32_t dom = atoi(argv[1]);

    /* initialize the xen access library */
    if (xa_init_vm_id_strict(dom, &xai) == XA_FAILURE){
        perror("failed to init XenAccess library");
        goto error_exit;
    }

    /* init the offset values */
    if (XA_OS_LINUX == xai.os_type){
        tasks_offset = xai.os.linux_instance.tasks_offset;
        name_offset = 0x194; /* pv, xen 3.3.1, centos 2.6.18-92.1.10.el5xen */
        pid_offset = xai.os.linux_instance.pid_offset;
    }
    else if (XA_OS_WINDOWS == xai.os_type){
        tasks_offset = xai.os.windows_instance.tasks_offset;
        name_offset = 0x174; /* Windows XP SP2 */
//        name_offset = 0x14c; /* Windows Vista */
        pid_offset = xai.os.windows_instance.pid_offset;
    }

    /* get the head of the list */
    if (XA_OS_LINUX == xai.os_type){
        memory = xa_access_kernel_sym(&xai, "init_task", &offset, PROT_READ);
        if (NULL == memory){
            perror("failed to get process list head");
            goto error_exit;
        }    
        memcpy(&next_process, memory + offset + tasks_offset, 4);
    }
    else if (XA_OS_WINDOWS == xai.os_type){
        xa_read_long_sym(&xai, "PsInitialSystemProcess", &list_head);
        memory = xa_access_kernel_va(&xai, list_head, &offset, PROT_READ);
        if (NULL == memory){
            perror("failed to get EPROCESS for PsInitialSystemProcess");
            goto error_exit;
        }
        memcpy(&next_process, memory + offset + tasks_offset, 4);
        name = (char *) (memory + offset + name_offset);
        memcpy(&pid, memory + offset + pid_offset, 4);
        printf("[%5d] %s\n", pid, name);
    }
    list_head = next_process;
    munmap(memory, xai.page_size);

    /* walk the task list */
    while (1){

        /* follow the next pointer */
        memory = xa_access_kernel_va(&xai, next_process, &offset, PROT_READ);
        if (NULL == memory){
            perror("failed to map memory for process list pointer");
            goto error_exit;
        }
        memcpy(&next_process, memory + offset, 4);

        /* if we are back at the list head, we are done */
        if (list_head == next_process){
            break;
        }

        /* print out the process name */

        /* Note: the task_struct that we are looking at has a lot of
           information.  However, the process name and id are burried
           nice and deep.  Instead of doing something sane like mapping
           this data to a task_struct, I'm just jumping to the location
           with the info that I want.  This helps to make the example
           code cleaner, if not more fragile.  In a real app, you'd
           want to do this a little more robust :-)  See
           include/linux/sched.h for mode details */
        name = (char *) (memory + offset + name_offset - tasks_offset);
        memcpy(&pid, memory + offset + pid_offset - tasks_offset, 4);

        /* trivial sanity check on data */
        if (pid < 0){
            continue;
        }
        printf("[%5d] %s\n", pid, name);
        munmap(memory, xai.page_size);
    }

error_exit:

    /* sanity check to unmap shared pages */
    if (memory) munmap(memory, xai.page_size);

    /* cleanup any memory associated with the XenAccess instance */
    xa_destroy(&xai);

    return 0;
}

#else

int main (int argc, char **argv)
{
    printf("The process list example is intended to work with a live Xen domain, but\n");
    printf("XenAccess was compiled without support for Xen.  Exiting...\n");
}

#endif /* ENABLE_XEN */
