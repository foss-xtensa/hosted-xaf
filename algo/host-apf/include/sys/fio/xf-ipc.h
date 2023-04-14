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
 * xf-ipc.h
 *
 * IPC layer for File I/O configuration
 *******************************************************************************/

#ifndef __XF_H
#error "xf-ipc.h mustn't be included directly"
#endif

/*******************************************************************************
 * Types definitions
 ******************************************************************************/

#define FD_READ_READY		1
#define FD_SELECT_INTERRUPT	9

/* ...proxy IPC data */
typedef struct xf_proxy_ipc_data
{
    /* ...shared memory buffer pointer */
    void                   *shmem;

    /* ...file descriptor */
    int                     fd;

    /* ...pipe for asynchronous response delivery */
    int                     lresp_pipe[2];

#ifdef DELAYED_SYNC_RESPONSE
    int                     lresp_pipe_delayed[2];
#endif

    /* ...pipe for self-pipe-trick to interrupt ipc_wait */
    int		                wakeup_pipe[2];

    /* ...wait object to return timeout error on no-response */
    xf_wait_t               wait;

    /* ...number of wait response msg available (in pipe/fifo)  */
    WORD32 		            num_msg_in_wait;

    /* ...wait object to return timeout error on no-response (delayed sync response) */
    xf_wait_t               wait_delayed;

    /* ...number of wait response msg available (in pipe/fifo) (delayed sync response) */
    WORD32 		            num_msg_in_wait_delayed;

}   xf_proxy_ipc_data_t;

/*******************************************************************************
 * Helpers for asynchronous response delivery
 ******************************************************************************/

#define xf_proxy_ipc_response_put(ipc, msg) \
    (write((ipc).lresp_pipe[1], (msg), sizeof(*(msg))) == sizeof(*(msg)) ? 0 : -errno)

#define xf_proxy_ipc_response_get(ipc, msg) \
    (read((ipc).lresp_pipe[0], (msg), sizeof(*(msg))) == sizeof(*(msg)) ? 0 : -errno)

#ifdef DELAYED_SYNC_RESPONSE
#define xf_proxy_ipc_response_put_delayed(ipc, msg) \
    (write((ipc).lresp_pipe_delayed[1], (msg), sizeof(*(msg))) == sizeof(*(msg)) ? 0 : -errno)

#define xf_proxy_ipc_response_get_delayed(ipc, msg) \
    (read((ipc).lresp_pipe_delayed[0], (msg), sizeof(*(msg))) == sizeof(*(msg)) ? 0 : -errno)
#endif

/*******************************************************************************
 * Shared memory translation
 ******************************************************************************/
 /* ...NULL-address specification */
#define XF_PROXY_NULL       (~0U)

/* ...invalid proxy address */
#define XF_PROXY_BADADDR    XF_CFG_REMOTE_IPC_POOL_SIZE

/* ...translate proxy shared address into local virtual address */
static inline void * xf_ipc_a2b(xf_proxy_ipc_data_t *ipc, UWORD32 address)
{
    if (address < XF_CFG_REMOTE_IPC_POOL_SIZE)
        return ipc->shmem + address;
    else if (address == XF_PROXY_NULL)
        return NULL;
    else
        return (void *) -1;
}

/* ...translate local virtual address into shared proxy address */
static inline UWORD32 xf_ipc_b2a(xf_proxy_ipc_data_t *ipc, void *b)
{
    UWORD32     a;
    
    if (b == NULL)
        return XF_PROXY_NULL;
    if ((a = (UWORD32)((UWORD8 *)b - (UWORD8 *)ipc->shmem)) < XF_CFG_REMOTE_IPC_POOL_SIZE)
        return a;
    else
        return XF_PROXY_BADADDR;
}

/*******************************************************************************
 * Component inter-process communication
 ******************************************************************************/

typedef struct xf_ipc_data
{
    /* ...pipe for asynchronous response message delivery */
    int resp_pipe[2];

    /* ...wait object to return timeout error on no-response */
    xf_wait_t wait;
    
    /* ...number of wait response msg available (in pipe/fifo)  */
    WORD32    num_msg_in_wait;

}   xf_ipc_data_t;

/*******************************************************************************
 * Helpers for asynchronous response delivery
 ******************************************************************************/

#define xf_ipc_response_put(ipc, msg)       \
    (write((ipc).resp_pipe[1], (msg), sizeof(*(msg))) == sizeof(*(msg)) ? 0 : -errno)

#define xf_ipc_response_get(ipc, msg)       \
    (read((ipc).resp_pipe[0], (msg), sizeof(*(msg))) == sizeof(*(msg)) ? 0 : -errno)

#define xf_ipc_data_init(ipc)               \
    (pipe((ipc)->resp_pipe) == 0 ? 0 : -errno)

#define xf_ipc_data_destroy(ipc)            \
    (close((ipc)->resp_pipe[0]), close((ipc)->resp_pipe[1]))

/*******************************************************************************
* API functions
 ******************************************************************************/

/* ...send asynchronous command */
extern int  xf_ipc_send(xf_proxy_ipc_data_t *ipc, xf_proxy_msg_t *msg, void *b);

/* ...wait for response from DSP Interface Layer */
extern int  xf_ipc_wait(xf_proxy_ipc_data_t *ipc, UWORD32 timeout);

extern int  xf_ipc_wait_interruptible(xf_proxy_ipc_data_t *ipc, UWORD32 timeout);

/* ...receive response from IPC layer */
extern int  xf_ipc_recv(xf_proxy_ipc_data_t *ipc, xf_proxy_msg_t *msg, void **b);

/* ...open proxy interface on proper DSP partition */
extern int  xf_ipc_open(xf_proxy_ipc_data_t *proxy, UWORD32 core);

/* ...close proxy handle */
extern void xf_ipc_close(xf_proxy_ipc_data_t *proxy, UWORD32 core);

/* ...set event to close proxy handle */
extern int xf_ipc_close_set_event(xf_proxy_ipc_data_t *ipc, UWORD32 core);

