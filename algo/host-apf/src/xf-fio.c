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
 * xf-fio.c
 *
 * Linux file-IO-based IPC layer Implementation
 ******************************************************************************/

#define MODULE_TAG                      FIO

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "xf.h"
#include <sys/ioctl.h>
#include <sys/mman.h>

/*******************************************************************************
 * Local constants - tbd
 ******************************************************************************/

/* ...proxy setup ioctl */
#define XF_PROXY_SETUP_IOCTL            _IO('P', 0x0)

/* ...proxy close ioctl */
#define XF_PROXY_CLOSE_IOCTL            _IO('P', 0x1)

/*******************************************************************************
 * Internal IPC API implementation
 ******************************************************************************/

/* ...pass command to remote DSP */
int xf_ipc_send(xf_proxy_ipc_data_t *ipc, xf_proxy_msg_t *msg, void *b)
{
    int     fd = ipc->fd;

    TRACE(CMD, _b("C[%016llx]:(%x,%08x,%u)"), (UWORD64)msg->id, msg->opcode, msg->address, msg->length);    

    /* ...pass message to kernel driver */
    XF_CHK_ERR(write(fd, msg, sizeof(*msg)) == sizeof(*msg), -errno);

    /* ...communication mutex is still locked! */
    return 0;
}

/* ...wait for response availability */
int  xf_ipc_wait(xf_proxy_ipc_data_t *ipc, UWORD32 timeout)
{
    int             fd = ipc->fd;
    fd_set          rfds;
    struct timeval  tv;
    
    /* ...specify waiting set */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    
    /* ...set timeout value if given */
    (timeout ? tv.tv_sec = timeout / 1000, tv.tv_usec = (timeout % 1000) * 1000 : 0);
    
    /* ...wait until there is a data in file */
    XF_CHK_ERR(select(fd + 1, &rfds, NULL, NULL, (timeout ? &tv : NULL)) >= 0, -errno);
    
    /* ...check if descriptor is set */
    return (FD_ISSET(fd, &rfds) ? 0 : -ETIMEDOUT);
}

/* ...Helper function, may not be necessary */
void set_nonblock(int fd) {
    int flags;
    if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
        flags = 0;
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* ...wait for response availability, interruptible with self-pipe-trick */
int xf_ipc_wait_interruptible(xf_proxy_ipc_data_t *ipc, UWORD32 timeout)
{
    int             fd = ipc->fd;
    fd_set          rfds;
    struct timeval  tv;
    
    /* ...specify waiting set */
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    
    set_nonblock(ipc->wakeup_pipe[0]);
    FD_SET(ipc->wakeup_pipe[0], &rfds);
   
    int nfds = (fd > ipc->wakeup_pipe[0]) ? fd: ipc->wakeup_pipe[0]; 

    /* ...set timeout value if given */
    (timeout ? tv.tv_sec = timeout / 1000, tv.tv_usec = (timeout % 1000) * 1000 : 0);
    
    /* ...wait until there is a data in file */
    XF_CHK_ERR(select(nfds + 1, &rfds, NULL, NULL, (timeout ? &tv : NULL)) >= 0, -errno);
    
    /* ...return value indicates if any descriptor is set */
    
    if (FD_ISSET(fd, &rfds))
	    return FD_READ_READY;
    else if (FD_ISSET(ipc->wakeup_pipe[0], &rfds))
    {
        TRACE(INIT, _b("thread interface closed with SELECT"));
	    return FD_SELECT_INTERRUPT;
    }
    else
    {
	    return -ETIMEDOUT;
    }
}

/* ...read response from proxy */
int xf_ipc_recv(xf_proxy_ipc_data_t *ipc, xf_proxy_msg_t *msg, void **buffer)
{
    int     fd = ipc->fd;
    int     r;
    
    /* ...get message header from file */
    if ((r = read(fd, msg, sizeof(*msg))) == sizeof(*msg))
    {
        TRACE(RSP, _b("R[%016llx]:(%x,%u,%08x)"), (UWORD64)msg->id, msg->opcode, msg->length, msg->address);

        /* ...translate shared address into local pointer */
        XF_CHK_ERR((*buffer = xf_ipc_a2b(ipc, msg->address)) != (void *)-1, -EBADFD);

        /* ...return positive result indicating the message has been received */
        return sizeof(*msg);
    }
    else
    {
        /* ...if no response is available, return 0 result */
        return XF_CHK_API(errno == EAGAIN ? 0 : -errno);
    }
}

/*******************************************************************************
 * Internal API functions implementation
 ******************************************************************************/

/* ...open proxy interface on proper DSP partition */
int  xf_ipc_open(xf_proxy_ipc_data_t *ipc, UWORD32 core)
{
    /* ...open file handle */
    XF_CHK_ERR((ipc->fd = open("/dev/xtensa-proxy", O_RDWR)) >= 0, -errno);

    /* ...pass shread memory core for this proxy instance */
    XF_CHK_ERR(ioctl(ipc->fd, XF_PROXY_SETUP_IOCTL, core) >= 0, -errno);    

    /* ...create pipe for asynchronous response delivery */
    XF_CHK_ERR(pipe(ipc->lresp_pipe) == 0, -errno);

    /* ... initialize wait object for response timeout */
    __xf_wait_init(&ipc->wait);
    ipc->num_msg_in_wait = 0; /* ... initialize msg counetr in wait queue/fifo */

#ifdef DELAYED_SYNC_RESPONSE
    XF_CHK_ERR(pipe(ipc->lresp_pipe_delayed) == 0, -errno);
    
    /* ... initialize wait object for response timeout */
    __xf_wait_init(&ipc->wait_delayed);
    ipc->num_msg_in_wait_delayed = 0; /* ... initialize msg counetr in wait queue/fifo */
#endif

    /* ...self-pipe-trick to interrupt select on ipc->fd */
    XF_CHK_ERR(pipe(ipc->wakeup_pipe) == 0, -errno);

    /* ...map entire shared memory region (not too good - tbd) */
    XF_CHK_ERR((ipc->shmem = mmap(NULL, XF_CFG_REMOTE_IPC_POOL_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, ipc->fd, 0)) != MAP_FAILED, -errno);
    
    TRACE(INIT, _b("proxy-%u interface opened"), core);
    
    return 0;
}

/* ...set event to close proxy handle */
int xf_ipc_close_set_event(xf_proxy_ipc_data_t *ipc, UWORD32 core)
{
    char buf[1] = {0};
    XF_CHK_ERR(write(ipc->wakeup_pipe[1], buf, sizeof(buf)) == sizeof(buf), errno);
}

/* ...close proxy handle */
void xf_ipc_close(xf_proxy_ipc_data_t *ipc, UWORD32 core)
{
    /* ...unmap shared memory region */
    (void)munmap(ipc->shmem, XF_CFG_REMOTE_IPC_POOL_SIZE);

    /* ...close asynchronous response delivery pipe */
    close(ipc->lresp_pipe[0]), close(ipc->lresp_pipe[1]);
    
#ifdef DELAYED_SYNC_RESPONSE
    /* ...close asynchronous response delivery pipe */
    close(ipc->lresp_pipe_delayed[0]), close(ipc->lresp_pipe_delayed[1]);
#endif
    
    /* ...close ipc_wait_interrupt wake-up pipe */
    close(ipc->wakeup_pipe[0]), close(ipc->wakeup_pipe[1]);

    /* ...close proxy file handle */
    close(ipc->fd);

    TRACE(INIT, _b("proxy-%u interface closed"), core);
}
