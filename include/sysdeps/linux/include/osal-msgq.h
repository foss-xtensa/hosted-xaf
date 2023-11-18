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
#ifndef _OSAL_MSGQ_H
#define _OSAL_MSGQ_H

/*******************************************************************************
 * Includes
 ******************************************************************************/
#include "xaf-api.h"
#include <sys/time.h>

/*******************************************************************************
 * Macro Definitions
 ******************************************************************************/
#define MAXIMUM_TIMEOUT 1000000

/*******************************************************************************
 * Waiting object
 ******************************************************************************/

/* ...waiting object handle */
typedef struct __xf_wait
{
    /* ...conditional variable */
    pthread_cond_t      wait;
    
    /* ...waiting mutex */
    pthread_mutex_t     mutex;

}   xf_wait_t;


/* ...initialize waiting object */
static inline void __xf_wait_init(xf_wait_t *w)
{
    pthread_cond_init(&w->wait, NULL);
    pthread_mutex_init(&w->mutex, NULL);
}

/* ...prepare to waiting */
static inline void __xf_wait_prepare(xf_wait_t *w)
{
    pthread_mutex_lock(&w->mutex);
}

/* ...wait until event is signalled */
static inline int __xf_wait(xf_wait_t *w, uint32_t timeout)
{
    struct timespec ts;
    struct timeval  tv;
    int             r;

    /* ...wait with or without timeout (communication mutex is taken) */
    if (!timeout)
    {
        r = -pthread_cond_wait(&w->wait, &w->mutex);
    }
    else
    {
        /* ...get current time */        
        gettimeofday(&tv, NULL);

        /* ...set absolute timeout */
        ts.tv_sec = tv.tv_sec + timeout / 1000;
        ts.tv_nsec = tv.tv_usec * 1000 + (timeout % 1000) * 1000000;
        (ts.tv_nsec >= 1000000000 ? ts.tv_sec++, ts.tv_nsec -= 1000000000 : 0);
        
        /* ...wait conditionally with absolute timeout*/
        r = -pthread_cond_timedwait(&w->wait, &w->mutex, &ts);
    }
    r = (r == -ETIMEDOUT) ? XAF_TIMEOUT_ERR : r;

    /* ...leave with communication mutex taken */
    return r;    
}

/* ...wake up waiting handle */
static inline void __xf_wakeup(xf_wait_t *w)
{
    /* ...take communication mutex before signaling */
    pthread_mutex_lock(&w->mutex);

    /* ...signalling will resume waiting thread */
    pthread_cond_signal(&w->wait);

    /* ...assure that waiting task will not resume until we say this - is that really needed? - tbd */
    pthread_mutex_unlock(&w->mutex);
}

/* ...complete waiting operation */
static inline void __xf_wait_complete(xf_wait_t *w)
{
    pthread_mutex_unlock(&w->mutex);
}

#endif //_OSAL_MSGQ_H
