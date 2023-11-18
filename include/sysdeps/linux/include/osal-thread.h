/*
* Copyright (c) 2015-2023 Cadence Design Systems Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/*******************************************************************************
 * osal-thread.h
 *
 * OS absraction layer (minimalistic) for XOS
 ******************************************************************************/

/*******************************************************************************
 * Includes
 ******************************************************************************/
#ifndef _OSAL_THREAD_H
#define _OSAL_THREAD_H

//#define _BSD_SOURCE
//#define _GNU_SOURCE
#include <stdint.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include "xaf-api.h"

/*******************************************************************************
 * Tracing primitive
 ******************************************************************************/

#define __xf_puts(str)                  \
    puts((str))


/*******************************************************************************
 * Macro Definitions
 ******************************************************************************/
/* ... state of the thread */
#define XF_THREAD_STATE_INVALID 2
#define XF_THREAD_STATE_READY	3
#define XF_THREAD_STATE_RUNNING	4
#define XF_THREAD_STATE_BLOCKED 5
#define XF_THREAD_STATE_EXITED  6 

//#define XF_THREAD_STACK_ALIGN   (sysconf(_SC_PAGESIZE))
#define XF_THREAD_STACK_ALIGN   4096 

/*******************************************************************************
 * Lock operation
 ******************************************************************************/
/* ...lock definition */
typedef pthread_mutex_t     xf_lock_t;

/* ...lock initialization */
static inline void __xf_lock_init(xf_lock_t *lock)
{
    pthread_mutex_init(lock, NULL);
}
/* ...lock deletion */
static inline void __xf_lock_destroy(xf_lock_t *lock)
{
    pthread_mutex_destroy(lock);
}
/* ...lock acquisition */
static inline void __xf_lock(xf_lock_t *lock)
{
    pthread_mutex_lock(lock);
}

/* ...lock release */
static inline void __xf_unlock(xf_lock_t *lock)
{
    pthread_mutex_unlock(lock);
}

/*******************************************************************************
 * Thread support
 ******************************************************************************/

/* ...thread handle definition */
typedef struct {
    pthread_t  thread;
    int created;
} xf_thread_t;

typedef void *xf_entry_t(void *);

static inline int __xf_thread_init(xf_thread_t *thread)
{
    thread->created = 0;
    return 0;
}

/* ...thread creation
 *
 * return: 0 -- OK, negative -- OS-specific error code
 */
//static inline int __xf_thread_create(xf_thread_t *thread, void * (*f)(void *), void *arg)
static inline int __xf_thread_create(xf_thread_t *thread, xf_entry_t *f,
                                     void *arg, const char *name, void *stack,
                                     unsigned int stack_size, int priority)
{
    pthread_attr_t      attr;
    int                 r;
    struct sched_param  param;
    
    /* ...initialize thread attributes - joinable with minimal stack */
    r = -pthread_attr_init(&attr);
    if(r) return r;

    r = -pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    if(r) return r;

    /* ...setup stack size, pointer */
    stack_size = (stack_size < PTHREAD_STACK_MIN) ? PTHREAD_STACK_MIN : stack_size;
    //r = -posix_memalign(&stack, XF_THREAD_STACK_ALIGN, stack_size); /* ...allocates memory of required alignment, size */
    //if(r) return r;

    r = -pthread_attr_setstacksize(&attr, stack_size);
    if(r) return r;
    r = -pthread_attr_setstack(&attr, stack, stack_size);
    if(r) return r;

    r = -pthread_attr_getschedparam (&attr, &param);
    if(r) return r;

    /* ...setup sched policy, priority */
    r = -pthread_attr_setschedpolicy(&attr, SCHED_RR); /* SCHED_OTHER=0, SCHED_FIFO=1, SCHED_RR=2 */
    if(r) return r;
    
    param.sched_priority = priority;
    r = -pthread_attr_setschedparam (&attr, &param);
    if(r) return r;

    r = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if(r) return r;

    /* ...create proxy asynchronous thread managing SHMEM */
    r = -pthread_create(&thread->thread, &attr, f, arg);
    if(r) return r;

    thread->created = 1;

#if 0
    if(!r)
        r = pthread_setname_np(thread, name);
#endif
    /* ...destroy thread attributes */
    r = -pthread_attr_destroy(&attr);

    return r;
}

static inline void __xf_thread_yield(void)
{
    //pthread_yield();
    sched_yield();
}

static inline int __xf_thread_cancel(xf_thread_t *thread)
{
    int r = -pthread_cancel(thread->thread);
    return r;
}

/* TENA-2117*/
static inline int __xf_thread_join(xf_thread_t *thread, int32_t * p_exitcode)
{
    void *r;
    int err = 0;

    if(thread->created == 0) 
        return err;

    pthread_join(thread->thread, (void **)&r);

    if(r == NULL)
    {
        err = 0;
    }
    else
    {
        err = (int)((uint64_t *)r);
    }

    if(p_exitcode)
    {
        *p_exitcode = (int32_t)err;
    }

    return err;
}

/* ...terminate thread operation */
static inline int __xf_thread_destroy(xf_thread_t *thread)
{
#if 1
    /* nothing to be done, pthread_join before this call releases thread resource. 
    * calling pthread_join() 2nd time has undefined behavior */
    thread->created = 0;
    return 0;
#else
    void *r = NULL;
    
    /* ...wait until thread terminates */
    pthread_join(thread->thread, (void **)&r);
    
    /* ...return final status */
    if(r == NULL)
        return 0;

    return (int)(*(uint64_t *)r);
#endif
}

static inline const char *__xf_thread_name(xf_thread_t *thread)
{
#if 0
    char name[16];
    pthread_getname_np(*thread, name);
    return (char *)name;
#else
    return NULL;
#endif
}

/* ... Put calling thread to sleep for at least the specified number of msec */
static inline int32_t __xf_thread_sleep_msec(uint64_t msecs)
{
    //sleep(msecs/1000);
    sleep(msecs/10);
    return 0;
}

/* ... state of the thread */
static inline int32_t __xf_thread_get_state (xf_thread_t *thread)
{
    int ret;

    if (thread->created == 0)
        return XF_THREAD_STATE_INVALID;

#if 0    
    /*...note this does not actually kill or send any signal to the thread */
    ret = pthread_kill(thread->thread, 0);

    switch (ret)
    {
        case ESRCH:
            return XF_THREAD_STATE_EXITED;
        case EINVAL:
            return XF_THREAD_STATE_INVALID;
        default:
            return XF_THREAD_STATE_RUNNING;
    }
#else
    /* ...np for non-portable, using this as a workaround as pthread_kill return value inconsistent across gcc versions */
    ret = pthread_tryjoin_np(thread->thread, NULL);

    switch (ret)
    {
        case 0:
            thread->created = 0;
            return XF_THREAD_STATE_EXITED;
        case EBUSY:
            return XF_THREAD_STATE_RUNNING;
        default:
            return 0;
    }
#endif
}
#endif //_OSAL_THREAD_H
