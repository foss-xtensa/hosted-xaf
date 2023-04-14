/*******************************************************************************
 * xf-proxy.h
 *
 * Proxy commmand/response messages
 *
 * Copyright (c) 2013 Tensilica Inc. ALL RIGHTS RESERVED.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *******************************************************************************/

#ifndef __XF_H
#error "xf-proxy.h mustn't be included directly"
#endif

/*******************************************************************************
 * Types definitions
 ******************************************************************************/

/* ...command/response message */
typedef struct xf_proxy_msg {
	/* ...session ID */
	xf_msg_id_dtype     id;

	/* ...proxy API command/reponse code */
	u32                 opcode;

	/* ...length of attached buffer */
	u32                 length;

	/* ...shared logical address of message buffer */
	u32                 address;

    u32                 error;

}   __attribute__((__packed__, __aligned__(sizeof(xf_msg_id_dtype)))) xf_proxy_msg_t;

/*******************************************************************************
 * Ring buffer support
 ******************************************************************************/

/* ...total length of shared memory queue (for commands and responses) */
#define XF_PROXY_MESSAGE_QUEUE_LENGTH   (1 << 8)

/* ...index mask */
#define XF_PROXY_MESSAGE_QUEUE_MASK     0xFF

/* ...ring-buffer index */
#define __XF_QUEUE_IDX(idx, counter)    \
	(((idx) & XF_PROXY_MESSAGE_QUEUE_MASK) | ((counter) << 16))

/* ...retrieve ring-buffer index */
#define XF_QUEUE_IDX(idx)               \
	((idx) & XF_PROXY_MESSAGE_QUEUE_MASK)

/* ...increment ring-buffer index */
#define XF_QUEUE_ADVANCE_IDX(idx)       \
	(((idx) + 0x10001) & (0xFFFF0000 | XF_PROXY_MESSAGE_QUEUE_MASK))

/* ...test if ring buffer is empty */
#define XF_QUEUE_EMPTY(read, write)     \
	((read) == (write))

/* ...test if ring buffer is full */
#define XF_QUEUE_FULL(read, write)      \
	((write) == (read) + (XF_PROXY_MESSAGE_QUEUE_LENGTH << 16))

/*******************************************************************************
 * Internal API
 ******************************************************************************/

/* ...forward (opaque) declarations */
typedef struct xf_proxy             xf_proxy_t;
typedef struct xf_proxy_ipc_data    xf_proxy_ipc_data_t;

/* ...initialize shared memory interface */
extern int xf_proxy_init(xf_proxy_ipc_data_t *ipc, u32 core, struct device *dev);

/* ...destroy shared memory interface */
extern void xf_proxy_close(xf_proxy_ipc_data_t *ipc, struct device *dev);

/* ...internal proxy API */
extern void xf_proxy_notify(xf_proxy_ipc_data_t *ipc);

/* ...allocate proxy IPC data */
extern xf_proxy_ipc_data_t * xf_proxy_ipc_data(u32 core);
