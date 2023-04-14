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

#if 0
#include <stddef.h> //size_t

typedef struct {
    uint32_t queue;
} *xf_msgq_t;

static inline int __xf_msgq_get_size(size_t n_items, size_t item_size)
{
    xf_msgq_t q;
    return (sizeof(*q) + (n_items * item_size));
}

/* ...open proxy interface on proper DSP partition */
static inline xf_msgq_t __xf_msgq_create(size_t n_items, size_t item_size, void *pbuf)
{
    xf_msgq_t q = (xf_msgq_t )pbuf;
    //if (xos_msgq_create(&q->queue, (uint16_t)n_items, (uint32_t)item_size, (uint16_t)0) == XOS_OK)
    return q;
    //return NULL;
}

/* ...close proxy handle */
static inline void __xf_msgq_destroy(xf_msgq_t q)
{
    //xos_msgq_delete(&q->queue);
    return;
}

static inline int __xf_msgq_send(xf_msgq_t q, const void *data, size_t sz)
{
    //return xos_msgq_put(&q->queue, data) == XOS_OK ? XAF_NO_ERR : XAF_RTOS_ERR;
    data=data;
    sz=sz;
    q=q;

    return XAF_NO_ERR;
}

static inline int __xf_msgq_recv_blocking(xf_msgq_t q, void *data, size_t sz)
{
    data=data;
    sz=sz;
    q=q;

    return  0;
}

static inline int __xf_msgq_recv(xf_msgq_t q, void *data, size_t sz)
{
    data=data;
    sz=sz;
    q=q;
    return 0;
}

static inline int __xf_msgq_empty(xf_msgq_t q)
{
    q=q;
    return 0;
}

static inline int __xf_msgq_full(xf_msgq_t q)
{
    q=q;
    return 0;
}

#endif
#endif //_OSAL_MSGQ_H
