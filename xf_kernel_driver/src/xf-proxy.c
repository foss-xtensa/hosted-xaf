/*******************************************************************************
 * xf-proxy.c
 *
 * Xtensa HiFi DSP proxy driver
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2013 Tensilica Inc.
 ******************************************************************************/

#define pr_fmt(fmt) "XF: " fmt "\n"

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include "xf.h"
#include <linux/cdev.h>
#include <linux/debugfs.h>

#include <linux/kernel.h>
#include <linux/kthread.h>             //kernel threads
#include <linux/sched.h>               //task_struct 

/*******************************************************************************
 * Local constants definitions - tbd
 ******************************************************************************/

/* ...proxy setup ioctl */
#define XF_PROXY_SETUP_IOCTL            _IO('P', 0x0)

/* ...proxy close ioctl */
#define XF_PROXY_CLOSE_IOCTL            _IO('P', 0x1)

/* ...stub - use coherent mapping between user- and kernel-space */
#define XF_SHMEM_COHERENT               1

#define XF_PROXY_DATA_PAYLOAD_OFFSET	0x4000

#define XF_PROXY_MSG_PAYLOAD_OFFSET     0x40000

/*******************************************************************************
 * Local proxy data
 ******************************************************************************/

/* ...lock object */
typedef spinlock_t              xf_lock_t;

/* ...execution message */
typedef struct xf_message       xf_message_t;

/* ...message queue */
typedef struct xf_msg_queue     xf_msg_queue_t;

/* ...proxy client */
typedef struct xf_client        xf_client_t;

/* ...debugging data */
typedef struct xf_dbg_data      xf_dbg_data_t;

/* ...execution message */
struct xf_message {
	/* ...pointer to next message in a list */
	xf_message_t       *next;

	/* ...session-id */
	xf_msg_id_dtype     id;

	/* ...operation code */
	u32                 opcode;

	/* ...length of data buffer */
	u32                 length;

	/* ...translated data pointer */
	void               *buffer;

	u32		            error;
};

/* ...message queue */
struct xf_msg_queue {
	/* ...pointer to list head */
	xf_message_t       *head;

	/* ...pointer to list tail */
	xf_message_t       *tail;
};

typedef union xf_client_link {
	/* ...index of next client in free list */
	u32                 next;

	/* ...reference to proxy data for allocated client */
	xf_client_t        *client;

}	xf_client_link_t;


/* ...proxy client data */
struct xf_client {
	/* ...pointer to proxy (shmem) interface */
	xf_proxy_t         *proxy;

	/* ...allocated proxy client id */
	xf_msg_id_dtype     id;

	/* ...pending response queue */
	xf_msg_queue_t      queue;

	/* ...response waiting queue */
	wait_queue_head_t   wait;

#if 0
	/* ...virtual memory mapping area */
	struct vm_area_struct  *vma;
#endif

	/* ...virtual memory mapping */
	unsigned long       vm_start;

	/* ...counter of memory mappings (no real use of it yet - tbd) */
	atomic_t            vm_use;
};

/* ...proxy data */
struct xf_proxy {
	/* ...IPC layer data */
	xf_proxy_ipc_data_t     ipc;

	/* ...shared memory status change processing item */
	struct work_struct      work;

	/* ...internal lock */
	xf_lock_t               lock;

	/* ...busy queue (for clients waiting on notification) */
	wait_queue_head_t       busy;

	/* ...waiting queue for synchronous proxy operations */
	wait_queue_head_t       wait;

	/* ...shared memory interface core */
	u32                     core;

	/* ...submitted commands queue */
	xf_msg_queue_t          command;

	/* ...pending responses queue */
	xf_msg_queue_t          response;

	/* ...global message pool */
	xf_message_t            pool[XF_CFG_MESSAGE_POOL_SIZE];

	/* ...pointer to first free message in the pool */
	xf_message_t           *free;
};

/*******************************************************************************
 * Global data definition
 ******************************************************************************/

/* ...global clients pool (item[0] serves as list terminator) */
static xf_client_link_t     xf_client_map[XF_CFG_MAX_IPC_CLIENTS];

/* ...proxy data structures */
static xf_proxy_t           xf_proxy[XF_CFG_MAX_CORES];

/* ...global lock - probably, not needed - tbd */
//static xf_lock_t            xf_global_lock; //unused

/*******************************************************************************
 * Locking support (tbd)
 ******************************************************************************/

static inline void xf_lock_init(xf_lock_t *lock)
{
	spin_lock_init(lock);
}

static inline void xf_lock(xf_lock_t *lock)
{
	spin_lock(lock);
}

static inline void xf_unlock(xf_lock_t *lock)
{
	spin_unlock(lock);
}

/*******************************************************************************
 * Misc helpers
 ******************************************************************************/


static struct task_struct *etx_thread;
int thread_function(void *pv);
	
/* Thread */
int thread_function(void *pv)
{
    wait_queue_head_t *pwaitq; 
    u32 core;
    xf_proxy_ipc_data_t *ipc;
    xf_proxy_t *proxy;
    char *pshmem;
    xf_client_t *client;

    pr_info("Thread Function entry\n");
    client = (xf_client_t *)pv;
    pwaitq = (wait_queue_head_t  *)&client->wait; 
    core = 0;
    ipc = xf_proxy_ipc_data(core);
    proxy = container_of(ipc, xf_proxy_t, ipc);
    pshmem = (char *)proxy->ipc.regs;

    while(!kthread_should_stop())
    {
        //pr_info("In Thread Function %d\n", i);
	    if(0x77 == readb(&pshmem[IRQIN_POL]))
        {
            //pr_info("%s %d Pattern found, waking up\n", __func__,__LINE__);
            wake_up(pwaitq);
        }
        msleep(500);
    }
    pr_info("Exit Thread Function\n");
    return 0;
}

/*******************************************************************************
 * Proxy helpers
 ******************************************************************************/

/* ...shared memory translation - kernel virtual address to shared address */
static inline u32 xf_proxy_b2a(xf_proxy_t *proxy, void *b)
{
	return xf_ipc_b2a(&proxy->ipc, b);
}

/* ...shared memory translation - shared address to kernel virtual address */
static inline void * xf_proxy_a2b(xf_proxy_t *proxy, u32 a)
{
	return xf_ipc_a2b(&proxy->ipc, a);
}

/* ...allocate new client */
static inline xf_client_t * xf_client_alloc(void)
{
	xf_client_t    *client;
	xf_msg_id_dtype  id;

	/* ...try to allocate a client handle */
	if ((id = xf_client_map[0].next) != 0) {
		/* ...allocate client memory */
		client = kmalloc(sizeof(*client), GFP_KERNEL);
		if (!client)
			return ERR_PTR(-ENOMEM);

		/* ...advance the head of free clients */
		xf_client_map[0].next = xf_client_map[id].next;

		/* ...put associate client id with given object */
		xf_client_map[id].client = client;

		/* ...mark client is not yet bound to particular shared memory interface */
		client->proxy = NULL;

		/* ...save global proxy client identifier */
		client->id = id;

		return client;
	} else {
		/* ...number of clients exceeded */
		return ERR_PTR(-EBUSY);
	}
}

/* ...recycle client object */
static inline void xf_client_free(xf_client_t *client)
{
	xf_msg_id_dtype id = client->id;

	/* ...put proxy client id into free clients list */
	xf_client_map[id].next = xf_client_map[0].next, xf_client_map[0].next = id;

	/* ...destroy client data */
	kfree(client);
}

/* ...lookup client basing on id */
static inline xf_client_t * xf_client_lookup(xf_msg_id_dtype id)
{
	if (id >= XF_CFG_MAX_IPC_CLIENTS || xf_client_map[id].next < XF_CFG_MAX_IPC_CLIENTS)
		return NULL;
	else
		return xf_client_map[id].client;
}

/*******************************************************************************
 * Proxy API
 ******************************************************************************/

/* ...IPC data accessor */
xf_proxy_ipc_data_t * xf_proxy_ipc_data(u32 core)
{
	return (core < XF_CFG_MAX_CORES ? &xf_proxy[core].ipc : NULL);
}

/* ...trigger shared memory interface processing */
void xf_proxy_notify(xf_proxy_ipc_data_t *ipc)
{
	xf_proxy_t     *proxy = container_of(ipc, xf_proxy_t, ipc);

	schedule_work(&proxy->work);
}

/*******************************************************************************
 * Messages API. All functions are requiring proxy lock taken
 ******************************************************************************/

/* ...allocate new message from the pool */
static inline xf_message_t * xf_msg_alloc(xf_proxy_t *proxy)
{
	xf_message_t   *m = proxy->free;

	/* ...make sure we have a free message item */
	if (m != NULL) {
		/* ...get message from the pool */
		proxy->free = m->next, m->next = NULL;
	}

	return m;
}

/* ...get message queue head */
static inline xf_message_t * xf_msg_queue_head(xf_msg_queue_t *queue)
{
	return queue->head;
}

/* ...submit message to a queue */
static inline int xf_msg_enqueue(xf_msg_queue_t *queue, xf_message_t *m)
{
	int     first = (queue->head == NULL);

	/* ...set pointer to next item */
	m->next = NULL;

	/* ...advance head/tail pointer as required */
	if (first)
		queue->head = m;
	else
		queue->tail->next = m;

	/* ...new tail points to this message */
	queue->tail = m;

	return first;
}

/* ...retrieve next message from the per-task queue */
static inline xf_message_t * xf_msg_dequeue(xf_msg_queue_t *queue)
{
	xf_message_t   *m = queue->head;

	/* ...check if there is anything in the queue */
	if (m != NULL) {
		/* ...pop message from the head of the list */
		if ((queue->head = m->next) == NULL)
			queue->tail = NULL;
	}

	return m;
}

/* ...initialize message queue */
static inline void xf_msg_queue_init(xf_msg_queue_t *queue)
{
	queue->head = queue->tail = NULL;
}

/* ...return message to the pool of free items */
static inline void xf_msg_free(xf_proxy_t *proxy, xf_message_t *m)
{
	/* ...put message into the head of free items list */
	m->next = proxy->free, proxy->free = m;

	/* ...notify potential client waiting for message */
	wake_up(&proxy->busy);
}

/* ...return all messages from the queue to the pool of free items */
static inline void xf_msg_free_all(xf_proxy_t *proxy, xf_msg_queue_t *queue)
{
	xf_message_t   *m = queue->head;

	/* ...check if there is anything in the queue */
	if (m != NULL) {
		queue->tail->next = proxy->free;
		proxy->free = queue->head;
		queue->head = queue->tail = NULL;

		/* ...notify potential client waiting for message */
		wake_up(&proxy->busy);
	}
}

/* ...process association between response received and intended client */
static inline void xf_cmap(xf_proxy_t *proxy, xf_message_t *m)
{
	xf_msg_id_dtype id = XF_AP_IPC_CLIENT(m->id);
	xf_client_t    *client;


	/* ...process messages addressed to proxy itself */
	if (id == 0) {
		/* ...place message into local response queue */
		xf_msg_enqueue(&proxy->response, m);
		wake_up(&proxy->wait); //this wake_up can be sufficient, the one in kernel-thread_function can be removed
		return;
	}

	/* ...make sure the client ID is sane */
	client = xf_client_lookup(id);
	if (!client) {
		pr_err("rsp[id:%016llx][m->id:%016llx]: client lookup failed", (u64)id, (u64)m->id);
		goto drop;
	}

	/* ...make sure client is bound to this proxy interface */
	if (client->proxy != proxy) {
		pr_err("rsp[id:%016llx]: wrong proxy interface (core=%u)", (u64)m->id, proxy->core);
		goto drop;
	}

	/* ...put the message into client pending response queue */
	if (xf_msg_enqueue(&client->queue, m)) {
		//pr_info("%s %d resp[id:%08x]: (%08x,%u,%p)", __func__, __LINE__, m->id, m->opcode, m->length, m->buffer);
		pr_debug("notify proxy client-%016llx", (u64)client->id);
		wake_up(&client->wait); //the select which is blocked on read at user
	}

	return;

drop:
	/* ...silently drop the message */
	xf_msg_free(proxy, m);
}

/*******************************************************************************
 * Shared memory interface
 ******************************************************************************/

/* ...retrieve pending responses from shared memory ring-buffer */
static inline u32 xf_shmem_process_responses(xf_proxy_t *proxy)
{
	xf_message_t   *m;
	u32             read_idx, write_idx;
	int             status = 0;

	/* ...get current values of read/write pointers in response queue */
	read_idx = XF_PROXY_READ(proxy, rsp_read_idx);
	write_idx = XF_PROXY_READ(proxy, rsp_write_idx);

	if (write_idx ^ read_idx)
		pr_debug("shmem-rsp: %x/%x", read_idx, write_idx);

	/* ...process all committed responses */
	while (!XF_QUEUE_EMPTY(read_idx, write_idx)) {
		xf_proxy_msg_t  *response;

		/* ...allocate execution message */
		if ((m = xf_msg_alloc(proxy)) == NULL)
		{
			break;
		}

		/* ...mark the interface status has changed only if buffer was full */
		status |= (XF_QUEUE_FULL(read_idx, write_idx) ? 0x3 : 0x1);

		/* ...get oldest not yet processed response */
		response = XF_PROXY_RESPONSE(proxy, XF_QUEUE_IDX(read_idx));

#if 0 //divya
		/*	...synchronize memory contents */
		XF_PROXY_INVALIDATE(response, sizeof(*response));
#endif

		/* ...fill message parameters */
		m->id = response->id;
		m->opcode = response->opcode;
		m->length = response->length;
		m->buffer = xf_proxy_a2b(proxy, response->address);
        m->error = response->error;

		/* ...make sure address is sane */
		BUG_ON(m->buffer == (void *)-1);
		BUG_ON(m->buffer && m->length && xf_proxy_a2b(proxy, response->address + m->length - 1) == (void *)-1);

#if !XF_SHMEM_COHERENT
		/* ...invalidate message buffer contents if required */
		if (XF_OPCODE_RDATA(m->opcode) && m->length)
			XF_PROXY_INVALIDATE(m->buffer, m->length);
#endif

		pr_info("%s %d rsp[id:%016llx]:(%08x,%u,%p) response->address:%x", __func__, __LINE__, (u64)m->id, m->opcode, m->length, m->buffer, response->address); //divya

		/* ...advance local reading index copy */
		read_idx = XF_QUEUE_ADVANCE_IDX(read_idx);

		/* ...update shadow copy of reading index */
		XF_PROXY_WRITE(proxy, rsp_read_idx, read_idx); //DSP side index is advanced

		/* ...submit message to proper client */
		xf_cmap(proxy, m);
	}

	return status;
}

/* ...put pending commands into shared memory ring-buffer */
static inline u32 xf_shmem_process_commands(xf_proxy_t *proxy)
{
	xf_message_t   *m;
	u32             read_idx, write_idx;
	int             status = 0;

	/* ...get current value of peer read pointer */
	write_idx = XF_PROXY_READ(proxy, cmd_write_idx);
	read_idx = XF_PROXY_READ(proxy, cmd_read_idx);

	if (write_idx ^ read_idx)
		pr_debug("shmem-cmd: %x/%x", write_idx, read_idx);

	/* ...submit any pending commands */
	while (!XF_QUEUE_FULL(read_idx, write_idx)) {
		xf_proxy_msg_t *command;

		/* ...check if we have a pending command */
		if ((m = xf_msg_dequeue(&proxy->command)) == NULL)
			break;

		/* ...always mark the interface status has changed when we put something */
		status |= 0x3;

		/* ...select the place for the command */
		command = XF_PROXY_COMMAND(proxy, XF_QUEUE_IDX(write_idx));

#if !XF_SHMEM_COHERENT
		/* ...flush message buffer contents to main memory if required */
		if (XF_OPCODE_CDATA(m->opcode) && m->length)
			XF_PROXY_FLUSH(m->buffer, m->length);
#endif

		/* ...put the response message fields */
		command->id = m->id;
		command->opcode = m->opcode;
		command->length = m->length;
		command->address = xf_proxy_b2a(proxy, m->buffer);
        /* command->error = m->error;*/ /* not required for command */

		pr_info("cmd[id:%016llx]: (%08x,%u,%08x)", (u64)command->id, command->opcode, command->length, command->address);

		/* ...flush the content of the caches to main memory */
		//divya XF_PROXY_FLUSH(command, sizeof(*command));

		/* ...return message back to the pool */
		xf_msg_free(proxy, m);

		/* ...advance local writing index copy */
		write_idx = XF_QUEUE_ADVANCE_IDX(write_idx);

		/* ...update shared copy of queue write pointer */
		XF_PROXY_WRITE(proxy, cmd_write_idx, write_idx);

	}

	return status;
}

/* ...shared memory interface maintenance routine */
static void xf_proxy_process(struct work_struct *w)
{
	xf_proxy_t *proxy = container_of(w, xf_proxy_t, work);
	int         status;

	/* ...disable further interrupt delivery */
	xf_ipc_disable(&proxy->ipc);

	/* ...get exclusive access to internal data */
	xf_lock(&proxy->lock);

	do {
		/* ...acknowledge pending interrupt status */
		xf_ipc_ack(&proxy->ipc);

		/* ...process outgoing commands first (frees internal message descriptors) */
		status = xf_shmem_process_commands(proxy);

		/* ...process all pending responses */
		status |= xf_shmem_process_responses(proxy);

		/* ...notify remote peer if needed */
		(status & 0x2 ? xf_ipc_assert(&proxy->ipc), 1 : 0);

	} while (status);

	/* ...unlock internal proxy data */
	xf_unlock(&proxy->lock);

	/* ...re-enable inter-processor interrupt delivery */
	xf_ipc_enable(&proxy->ipc);
}

/* ...submit a command to proxy pending queue (lock released upon return) */
static void xf_proxy_command(xf_proxy_t *proxy, xf_message_t *m)
{
	int     first;

	/* ...submit message to proxy thread */
	first = xf_msg_enqueue(&proxy->command, m);

	/* ...release the lock */
	xf_unlock(&proxy->lock);

	/* ...notify thread about command reception */
	(first ? xf_proxy_notify(&proxy->ipc), 1 : 0);
}

/* ...initialize shared memory interface */
int __init xf_proxy_init(xf_proxy_ipc_data_t *ipc, u32 core, struct device *dev)
{
	xf_proxy_t     *proxy = container_of(ipc, xf_proxy_t, ipc);
	xf_message_t   *m;
	int             i;

	/* ...save proxy core identifier */
	proxy->core = core;

	/* ...create a list of all messages in a pool; set head pointer */
	proxy->free = &proxy->pool[0];

	/* ...put all messages into a single-linked list */
	for (i = 0, m = proxy->free; i < XF_CFG_MESSAGE_POOL_SIZE - 1; i++, m++)
		m->next = m + 1;

	/* ...set list tail pointer */
	m->next = NULL;

    /* ...initialize proxy lock */
	xf_lock_init(&proxy->lock);

	/* ...initialize proxy thread message queues */
	xf_msg_queue_init(&proxy->command);
	xf_msg_queue_init(&proxy->response);

	/* ...initialize global busy queue */
	init_waitqueue_head(&proxy->busy);
	init_waitqueue_head(&proxy->wait);

	/* ...create work structure */
	INIT_WORK(&proxy->work, xf_proxy_process);

	/* ...intialize shared memory interface */
	XF_PROXY_WRITE(proxy, cmd_read_idx, 0);
	XF_PROXY_WRITE(proxy, cmd_write_idx, 0);
	XF_PROXY_WRITE(proxy, rsp_read_idx, 0);
	XF_PROXY_WRITE(proxy, rsp_write_idx, 0);

	/* ...initialization completed */
	pr_debug("IPC-%u interface initialized", core);

	return 0;
}

/* ...destroy shared memory interface */
void __exit xf_proxy_close(xf_proxy_ipc_data_t *ipc, struct device *dev)
{
	xf_proxy_t     *proxy = container_of(ipc, xf_proxy_t, ipc);

	/* ...make sure the work-item is not running */
	cancel_work_sync(&proxy->work);

	/* ...initialization completed */
	pr_debug("proxy-%u interface closed", proxy->core);
}

/*******************************************************************************
 * Public driver interface implementation
 ******************************************************************************/

/* ...helper function for retrieving the client handle */
static inline xf_client_t *xf_get_client(struct file *filp)
{
	xf_client_t    *client;
	xf_msg_id_dtype id;

	client = (xf_client_t *)filp->private_data;
	if (!client)
		return ERR_PTR(-EINVAL);

	id = client->id;
	if (id >= XF_CFG_MAX_IPC_CLIENTS)
		return ERR_PTR(-EINVAL);

	if (xf_client_map[id].client != client)
		return ERR_PTR(-EBADF);

	return client;
}

/* ...helper function for requesting execution message from a pool */
static inline xf_message_t * xf_msg_available(xf_proxy_t *proxy)
{
	xf_message_t   *m;

	/* ...acquire global lock */
	xf_lock(&proxy->lock);

	/* ...try to allocate the message */
	if ((m = xf_msg_alloc(proxy)) == NULL) {
		/* ...failed to allocate message; release lock */
		xf_unlock(&proxy->lock);
	}

	/* ...if successfully allocated, lock is held */
	return m;
}

/* ...helper function for receiving a message from per-client queue */
static inline xf_message_t * xf_msg_received(xf_proxy_t *proxy, xf_msg_queue_t *queue)
{
	xf_message_t   *m;

	/* ...acquire global lock */
	xf_lock(&proxy->lock);

	/* ...try to peek message from the queue */
	if ((m = xf_msg_dequeue(queue)) == NULL) {
		/* ...queue is empty; release lock */
		xf_unlock(&proxy->lock);
	}

	/* ...if message is non-null, lock is held */
	return m;
}

/* ...helper function for asynchronous command execution */
static inline int xf_cmd_send(xf_proxy_t *proxy, xf_msg_id_dtype id, u32 opcode, void *buffer, u32 length)
{
	xf_message_t   *m;

	/* ...retrieve message handle (take the lock on success) */
	if (wait_event_interruptible(proxy->busy, (m = xf_msg_available(proxy)) != NULL))
		return -EINTR;

	/* ...fill-in message parameters (lock is taken) */
	m->id = id, m->opcode = opcode, m->length = length, m->buffer = buffer;

	pr_debug("cmd[%u]: [%016llx,%08x,%u,%p]", proxy->core, (u64)m->id, m->opcode, m->length, m->buffer);

	/* ...submit command to the proxy (release the lock) */
	xf_proxy_command(proxy, m);

	return 0;
}

/* ...wait until message is received into given response queue */
static inline xf_message_t * xf_cmd_recv(xf_proxy_t *proxy, wait_queue_head_t *wq, xf_msg_queue_t *queue, int wait)
{
	xf_message_t   *m;

	/* ...wait for message reception (take lock on success) */
	if (wait_event_interruptible(*wq, (m = xf_msg_received(proxy, queue)) != NULL || !wait))
		return ERR_PTR(-EINTR);

	/* ...return message with a lock taken */
	return m;
}

/* ...helper function for synchronous command execution */
static inline xf_message_t * xf_cmd_send_recv(xf_proxy_t *proxy, xf_msg_id_dtype id, u32 opcode, void *buffer, u32 length)
{
	int     retval;

	/* ...send command to remote proxy */
	retval = xf_cmd_send(proxy, id, opcode, buffer, length);
	if (retval)
		return ERR_PTR(retval);

	/* ...wait for message delivery */
	return xf_cmd_recv(proxy, &proxy->wait, &proxy->response, 1);
}

/* ...allocate memory buffer for kernel use */
static inline int xf_cmd_alloc(xf_proxy_t *proxy, void **buffer, u32 length)
{
	xf_message_t   *m;
	xf_msg_id_dtype id;
	int             retval;

	/* ...send allocation command on behalf of current proxy */
	id = __XF_MSG_ID(__XF_AP_PROXY(proxy->core), __XF_DSP_PROXY(proxy->core));

	/* ...send command to remote proxy */
	m = xf_cmd_send_recv(proxy, id, XF_ALLOC, NULL, length);
	if (IS_ERR(m)) {
		retval = PTR_ERR(m);
		pr_err("proxy-%u: command execution failed: %d", proxy->core, retval);
		return retval;
	}

	/* ...check if response is expected */
	if (m->opcode == XF_ALLOC && m->buffer != NULL) {
		pr_info("proxy-%u: buffer allocated: %p[%u]", proxy->core, m->buffer, m->length);
		*buffer = m->buffer;
		retval = 0;
	} else {
		pr_err("proxy-%u: failed to allocate buffer of size %u", proxy->core, length);
		retval = -ENOMEM;
	}

	/* ...free message and release proxy lock */
	xf_msg_free(proxy, m);
	xf_unlock(&proxy->lock);

	return retval;
}

/* ...free memory buffer */
static inline int xf_cmd_free(xf_proxy_t *proxy, void *buffer, u32 length)
{
	xf_message_t   *m;
	xf_msg_id_dtype id;
	int             retval;

	/* ...send allocation command on behalf of current proxy */
	id = __XF_MSG_ID(__XF_AP_PROXY(proxy->core), __XF_DSP_PROXY(proxy->core));

	/* ...synchronously execute freeing command */
	m = xf_cmd_send_recv(proxy, id, XF_FREE, buffer, length);
	if (IS_ERR(m)) {
		retval = PTR_ERR(m);
		pr_err("proxy-%u: failed to free buffer: %d", proxy->core, retval);
		return retval;
	}

	/* ...check if response is expected */
	if (m->opcode == XF_FREE) {
		pr_info("proxy-%u: buffer freed: %p[%u]", proxy->core, buffer, length);
		retval = 0;
	} else {
		pr_err("proxy-%u: failed to free buffer: %p[%u]", proxy->core, buffer, length);
		retval = -EINVAL;
	}

	/* ...free message and release proxy lock */
	xf_msg_free(proxy, m);
	xf_unlock(&proxy->lock);

	return retval;
}

/*******************************************************************************
 * Internal functions implementation (or internal API? - tbd)
 ******************************************************************************/

/* ...associate client to given shared memory interface */
static inline int xf_client_register(xf_client_t *client, u32 core)
{
	/* ...basic sanity checks */
	if (core >= XF_CFG_MAX_CORES) {
		pr_err("invalid core number: %u", core);
		return -EINVAL;
	}

	/* ...make sure client is not registered yet */
	if (client->proxy != NULL) {
		pr_err("client-%016llx already registered", (u64)client->id);
		return -EBUSY;
	}

	/* ...complete association (no communication with remote proxy here) */
	client->proxy = &xf_proxy[core];

	pr_debug("client-%016llx registered within proxy-%u", (u64)client->id, core);

	return 0;
}

/* ...unregister client from shared memory interface */
static inline int xf_client_unregister(xf_client_t *client)
{
	xf_proxy_t     *proxy = client->proxy;

	/* ...make sure client is registered */
	if (proxy == NULL) {
		pr_err("client-%016llx is not registered", (u64)client->id);
		return -EBUSY;
	}

	/* ...just clean proxy reference */
	client->proxy = NULL;

	pr_debug("client-%016llx registered within proxy-%u", (u64)client->id, proxy->core);

	return 0;
}

/*******************************************************************************
 * File I/O interface implementation
 ******************************************************************************/

/* ...open file-object and allocate client (do not register on DSP side yet) */
static int xf_open(struct inode *inode, struct file *filp)
{
	xf_client_t    *client;

	/* ...basic sanity checks */
	if (!inode || !filp)
		return -EINVAL;

	/* ...allocate new proxy client object */
	client = xf_client_alloc();
	if (IS_ERR(client))
		return PTR_ERR(client);

	/* ...initialize waiting queue */
	init_waitqueue_head(&client->wait);

	/* ...initialize client pending message queue */
	xf_msg_queue_init(&client->queue);

	/* ...mark user data is not mapped */
	client->vm_start = 0;

	/* ...reset mappings counter */
	atomic_set(&client->vm_use, 0);

	/* ...assign client data */
	filp->private_data = (void *)client;

	pr_debug("client-%016llx created: %p", (u64)client->id, client);

	/* ..kernel thread for polling DSP rd/wr status from shmem */
	etx_thread = kthread_create(thread_function, client, "etx Thread");
    if(etx_thread)
	{
        wake_up_process(etx_thread);
    }
	else
	{
        pr_err("Cannot create kthread\n");
    }

	return 0;
}

/* ...device control function */
static long xf_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	xf_client_t    *client;

	/* ...basic sanity checks */
	client = xf_get_client(filp);
	if (IS_ERR(client))
		return PTR_ERR(client);

	/* ...command dispatching */
	switch (cmd) {
	case XF_PROXY_SETUP_IOCTL:
		/* ...register proxy client */
		return xf_client_register(client, (u32)arg);

	case XF_PROXY_CLOSE_IOCTL:
		/* ...unregister proxy client somehow - tbd */
		return 0;

	default:
		return -ENOIOCTLCMD;
	}
}

/* ...pass command to remote DSP */
static ssize_t xf_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
	xf_proxy_t     *proxy;
	xf_client_t    *client;
	xf_proxy_msg_t  msg;
	void           *buffer;
	int             retval;

	/* ...basic sanity checks */
	client = xf_get_client(filp);
	if (IS_ERR(client))
		return PTR_ERR(client);

	/* ...get proxy interface */
	proxy = client->proxy;
	if (!proxy)
		return -EBADF;

	/* ...check parameters are sane */
	if (!buf || count != sizeof(msg))
	{
		pr_info("ERR %s, %d sizeof(msg):%lu count:%lu", __func__, __LINE__, (long unsigned int) sizeof(msg), (long unsigned int)count);
		return -EINVAL;
	}

	/* ...get proxy message from user-space */
	if (copy_from_user(&msg, buf, sizeof(msg)))
	{
		return -EFAULT;
	}

	/* ...make sure message pointer is sane (buffer is mapped to current process) */
	buffer = xf_proxy_a2b(proxy, msg.address);
	if (buffer == (void *)-1)
	{
		return -EFAULT;
	}

	/* ...put current proxy client into message session id */
	msg.id = XF_MSG_AP_FROM_USER(msg.id, client->id);

	pr_info("%s %d write[id:%016llx]: (%08x,%u,%08x)", __func__, __LINE__, (u64)msg.id, msg.opcode, msg.length, msg.address);
	/* ...send asynchronous command to proxy */
	retval = xf_cmd_send(proxy, msg.id, msg.opcode, buffer, msg.length);
	if (retval) {
		pr_err("command sending failed: %d", retval);
		return retval;
	}

	/* ...operation completed successfully */
	return sizeof(msg);
}

/* ...read next response message from proxy interface */
static ssize_t xf_read(struct file *filp, char __user *buf, size_t count, loff_t *ppos)
{
	xf_proxy_t         *proxy;
	xf_client_t        *client;
	xf_message_t       *m;
	xf_proxy_msg_t      msg;

	/* ...basic sanity checks */
	client = xf_get_client(filp);
	if (IS_ERR(client))
		return PTR_ERR(client);

	/* ...get proxy interface */
	proxy = client->proxy;
	if (!proxy)
		return -EPERM;

	/* ...check parameters are sane */
	if (!buf || count < sizeof(msg))
	{
		pr_info("ERR %s, %d sizeof(msg):%lu count:%lu", __func__, __LINE__, (long unsigned int)sizeof(msg), (long unsigned int)count);
		return -EINVAL;
	}

	/* ...check if we have a pending response message (do not wait) */
	m = xf_cmd_recv(proxy, &client->wait, &client->queue, 0);
	if (IS_ERR(m)) {
		pr_err("receiving failed: %d", (int)PTR_ERR(m));
		return PTR_ERR(m);
	}

	/* ...check if there is a response available */
	if (m == NULL)
	{
		return -EAGAIN;
	}

	/* ...prepare message parameters (lock is taken) */
	msg.id = XF_MSG_AP_TO_USER(m->id);
	msg.opcode = m->opcode;
	msg.length = m->length;
	msg.address = xf_proxy_b2a(proxy, m->buffer);
    msg.error = m->error;

	/* ...return the message back to a pool and release lock */
	xf_msg_free(proxy, m);
	xf_unlock(&proxy->lock);

	pr_debug("read[id:%016llx]: (%08x,%u,%08x)", (u64)msg.id, msg.opcode, msg.length, msg.address);
	pr_info("%s %d read[id:%016llx]: (%08x,%u,%08x)", __func__, __LINE__, (u64)msg.id, msg.opcode, msg.length, msg.address);

	/* ...pass message to user */
	if (copy_to_user(buf, &msg, sizeof(msg)))
	{
		pr_info("%s, %d", __func__, __LINE__);
		return -EFAULT;
	}

	return sizeof(msg);
}

/* ...wait until data is available in the response queue */
static unsigned int xf_poll(struct file *filp, poll_table *wait)
{
	xf_proxy_t     *proxy;
	xf_client_t    *client;
	int             mask;

	/* ...basic sanity checks */
	client = xf_get_client(filp);
	if (IS_ERR(client))
		return PTR_ERR(client);

	/* ...get proxy interface */
	proxy = client->proxy;
	if (!proxy)
    {
	    return -EPERM;
    }

	/* ...register client waiting queue */
	poll_wait(filp, &client->wait, wait);

	/* ...return current queue state */
	xf_lock(&proxy->lock);
	mask = (xf_msg_queue_head(&client->queue) ? POLLIN | POLLRDNORM : 0);
	xf_unlock(&proxy->lock);

	xf_proxy_notify(&proxy->ipc);

	pr_debug("poll[id:%016llx]: polling mask: %x", (u64)client->id, mask);

	return mask;
}

/*******************************************************************************
 * Low-level mmap interface
 ******************************************************************************/

/* ...add reference to shared buffer */
static void xf_mmap_open(struct vm_area_struct *vma)
{
	xf_client_t    *client = vma->vm_private_data;

	/* ...probably just increase counter of open references? - tbd */
	atomic_inc(&client->vm_use);

	pr_debug("xf_mmap_open: vma = %p, client = %p", vma, client);
}

/* ...close reference to shared buffer */
static void xf_mmap_close(struct vm_area_struct *vma)
{
	xf_client_t    *client = vma->vm_private_data;

	pr_debug("xf_mmap_close: vma = %p, b = %p", vma, client);

	/* ...decrement number of mapping */
	if (atomic_dec_and_test(&client->vm_use)) {
		/* ...last instance is closed; pass to system-specific hook - tbd */
	}
	pr_info("xf_mmap_close: vma = %p, client = %p Line#%d", vma, client, __LINE__);
}

/* ...memory map operations */
static struct vm_operations_struct xf_mmap_ops = {
	.open   = xf_mmap_open,
	.close  = xf_mmap_close,
};

/* ...shared memory mapping - entire memory is mapped into process address space */
static int xf_mmap(struct file *filp, struct vm_area_struct *vma)
{
	xf_proxy_t     *proxy;
	xf_client_t    *client;
	unsigned long   size;
	//unsigned long   pfn;
	int             r;

	/* ...basic sanity checks */
	client = xf_get_client(filp);
	if (IS_ERR(client))
		return PTR_ERR(client);

	/* ...get proxy interface */
	proxy = client->proxy;
	if (!proxy)
		return -EPERM;

	/* ...check it was not mapped already */
	if (client->vm_start != 0)
		return -EBUSY;

#if 0
	/* ...unclear how to process that yet */
	client->vma = vma;
#endif

	/* ...check mapping flags (tbd) */
	if ((vma->vm_flags & (VM_READ | VM_WRITE | VM_SHARED)) != (VM_READ | VM_WRITE | VM_SHARED))
		return -EPERM;

	/* ...adjust size to page boundary */
	size = (vma->vm_end - vma->vm_start + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

	/* ...check size is valid (tbd) */
	if (size > XF_CFG_REMOTE_IPC_POOL_SIZE)
		return -EFBIG;

	/* ...set memory map operations */
	vma->vm_ops = &xf_mmap_ops;

	/* ...assign private data */
	client->vm_start = vma->vm_start;

	/* ...set private memory data */
	vma->vm_private_data = client;

	/* ...set page number of shared memory */
	//pfn = __pa(XF_SHMEM_DATA(proxy)->buffer) >> PAGE_SHIFT;

#if XF_SHMEM_COHERENT
	/* ...make it non-cached for the moment - tbd */
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
#endif

	/* ...remap shared memory to user-space */
	r = vm_iomap_memory(vma, XF_PROXY_DATA_ADDRESS(core) + XF_PROXY_DATA_PAYLOAD_OFFSET, XF_PROXY_DATA_SIZE(core)); //mmap is ok 
	if (r != 0) {
		pr_err("%s, %d mapping failed: %x", __func__, __LINE__, r);
		return r;
	}

	/* ...system-specific hook for registering shared memory mapping */
	return 0;
}

/* ...close file-object and unregister client on DSP side */
static int xf_release(struct inode *inode, struct file *filp)
{
	xf_proxy_t     *proxy;
	xf_client_t    *client;

	/* ...basic sanity checks */
	client = xf_get_client(filp);
	if (IS_ERR(client))
		return PTR_ERR(client);

	proxy = client->proxy;
	if (proxy) {
        /* ...stop the polling kernel thread before acquiring proxy lock. 
         * If poll calls xf_proxy_process which takes proxy->lock, then the close() from user invoking this function
         * and trying to take proxy->lock might be causing the hang at the end of execution??
         * actually, this can wait on the lock until its released and then take the lock. TODO:is this assumption correct??
         * */
	    kthread_stop(etx_thread);

	    /* ...acquire global proxy lock */
	    xf_lock(&proxy->lock);

	    /* ...release all pending messages */
	    xf_msg_free_all(proxy, &client->queue);

	    /* ...recycle client id and release memory */
	    xf_client_free(client);

	    /* ...release global proxy lock */
	    xf_unlock(&proxy->lock);
	}

	pr_info("%s, %d", __func__, __LINE__);
	pr_debug("release device(inode: %p, filp: %p)", inode, filp);

	return 0;
}

/*******************************************************************************
 * File operations
 ******************************************************************************/

static const struct file_operations xf_fops = {
	.owner              = THIS_MODULE,
	.unlocked_ioctl     = xf_ioctl,
	.read               = xf_read,
	.write              = xf_write,
	.open               = xf_open,
	.poll               = xf_poll,
	.mmap               = xf_mmap,
	.release            = xf_release,
};

/*******************************************************************************
 * Entry points
 ******************************************************************************/

/* ...device major number - allocated dynamically */
#define XF_DEV_MAJOR                    0

/* ...device name */
#define XF_DEV_NAME                     "xtensa-proxy"

/* ...total number of devices supported */
#define XF_DEVICE_NUM                   1

/* ...device class */
static struct class        *xf_class;

/* ...device structure */
static struct device       *xf_device;

/* ...character devices range */
static struct cdev          xf_cdev;

/* ...character device major/minor number */
static dev_t                xf_devt;

#ifndef __io_virt
#define __io_virt(a) ((void __force *)(a))
#endif

int __init xf_ipc_init(struct device *dev)
{
	u32 core = 0;
	xf_proxy_ipc_data_t *ipc;
	uint32_t size, size2;

	ipc = xf_proxy_ipc_data(core);
	if (!ipc) {
		pr_err("invalid IPC core: %u", core);
		return -ENOENT;
	}

	if ((ipc->regs = (void *) ioremap(XF_PROXY_DATA_ADDRESS(core) + 0x100, 0x100)) == NULL)
    {
        printk(KERN_ERR "Mapping Regs failed\n");
        return -1;
    }

	size = (XF_PROXY_DATA_SIZE(core) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	size2 = (sizeof(xf_shmem_data_t) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
	pr_info("%s, %d, %08x, %d size:%d sizeof(xf_shmem_data_t):%d PAGE_SIZE:%ld", __func__, __LINE__, XF_PROXY_DATA_ADDRESS(core), XF_PROXY_DATA_SIZE(core), size, size2, PAGE_SIZE);

	if ((ipc->shmem = (xf_shmem_data_t *) ioremap(XF_PROXY_DATA_ADDRESS(core) + XF_PROXY_DATA_PAYLOAD_OFFSET - 0x3000, size2)) == NULL )
	{
	    printk(KERN_ERR "Mapping Shared RAM failed\n");
	    return -1;
	}
	pr_info("%s, %d, sizeof(ipc->shmem):%lu ipc->shmem:%lX, ipc->regs=%lX, buffer:%lX", __func__, __LINE__, \
			(long unsigned int)sizeof(ipc->shmem), (long unsigned int)ipc->shmem, (long unsigned int)ipc->regs, (long unsigned int)(ipc->shmem)->buffer);

	xf_proxy_init(ipc, core, dev);

	return 0;
}

/* ...module initialization */
static int __init xtensa_proxy_init(void)
{
	int     retval;
	u32     i;

	/* ...allocate character devices range (just one minor; major number is automatically selected) */
	retval = alloc_chrdev_region(&xf_devt, XF_DEV_MAJOR, XF_DEVICE_NUM, XF_DEV_NAME);
	if (retval) {
		pr_err("failed to allocate character device range: %d", retval);
		return retval;
	}

	/* ...create character devices for servicing specified amount of minor numbers */
	cdev_init(&xf_cdev, &xf_fops);
	retval = cdev_add(&xf_cdev, xf_devt, XF_DEVICE_NUM);
	if (retval) {
		pr_err("failed to add character device: %d", retval);
		kobject_put(&xf_cdev.kobj);
		goto err1;
	}

	/* ...create device class to cover all created devices */
	xf_class = class_create(THIS_MODULE, XF_DEV_NAME);
	if (IS_ERR(xf_class)) {
		retval = PTR_ERR(xf_class);
		pr_err("failed to create device class: %d", retval);
		goto err2;
	}

	/* ...create device (in /dev eventually) */
	xf_device = device_create(xf_class, NULL, xf_devt, NULL, XF_DEV_NAME);
	if (IS_ERR(xf_device)) {
		retval = PTR_ERR(xf_device);
		pr_err("device creation failed: %d", retval);
		goto err3;
	}

	/* ...initialize IPC layer */
	retval = xf_ipc_init(xf_device);
	if (retval) {
		pr_err("IPC layer initialization failed: %d", retval);
		goto err4;
	}

	/* ...initialize global lock object - why do I need that? - tbd */
	//xf_lock_init(&xf_global_lock); //unused

	/* ...initialize client association map */
	for (i = 0; i < XF_CFG_MAX_IPC_CLIENTS - 1; i++)
		xf_client_map[i].next = i + 1;

	/* ...set list terminator */
	xf_client_map[i].next = 0;

	pr_info("Xtensa DSP Proxy driver registered");

	return 0;

err4:
	/* ...destroy all devices created */
	device_destroy(xf_class, xf_devt);

err3:
	/* ...destroy device class */
	class_destroy(xf_class);

err2:
	/* ...destroy character device */
	cdev_del(&xf_cdev);

err1:
	/* ...release character device range */
	unregister_chrdev_region(xf_devt, XF_DEVICE_NUM);

	return retval;
}

/* ...module cleanup */
static void __exit xtensa_proxy_exit(void)
{
#if 0
	/* ...destroy IPC layer */
	xf_ipc_exit(xf_device);
#endif

	/* ...destroy all devices created */
	device_unregister(xf_device);

	/* ...destroy device class */
	class_destroy(xf_class);

	/* ...destroy character device */
	cdev_del(&xf_cdev);

	/* ...release character device range */
	unregister_chrdev_region(xf_devt, XF_DEVICE_NUM);

	pr_info("Xtensa DSP Proxy driver unregistered");
}

module_init(xtensa_proxy_init);
module_exit(xtensa_proxy_exit);

MODULE_AUTHOR("Tensilica");
MODULE_DESCRIPTION("Xtensa HiFi DSP Proxy driver");
MODULE_LICENSE("Dual MIT/GPL");
