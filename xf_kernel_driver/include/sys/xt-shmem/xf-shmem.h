/*******************************************************************************
 * xf-shmem.h
 *
 * Shared memory interface for Lager board
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
#error "xf-shmem.h mustn't be included directly"
#endif

/*******************************************************************************
 * Global constants
 ******************************************************************************/

/* ...cache-line size on DSP */
//#define XF_PROXY_ALIGNMENT              64
#define XF_PROXY_ALIGNMENT              128	/* to match DSP cache-line size and check cmd_write_idx, cmd_read_idx updates from proxy to DSP */

/* ...shared memory alignment property */
#define __xf_shmem__    __attribute__((__aligned__(XF_PROXY_ALIGNMENT)))

#define XF_PROXY_DATA_PAYLOAD_OFFSET	0x4000
//#define XF_PROXY_MSG_PAYLOAD_OFFSET     0x40000

/*******************************************************************************
 * Macros definitions
 ******************************************************************************/

/* ...basic cache operations */
#define XF_PROXY_INVALIDATE(addr, len)  \
	do {\
	unsigned long phys_addr = XF_PROXY_DATA_ADDRESS(core) + XF_PROXY_DATA_PAYLOAD_OFFSET - 0x3000 + (unsigned long)((size_t)addr - (size_t)ipc->shmem);\
	dma_sync_single_for_cpu(xf_device, phys_addr, len, DMA_FROM_DEVICE); mb();\
	/*dma_sync_single_for_cpu(NULL, virt_to_phys(addr), len, DMA_FROM_DEVICE); mb();*/\
	} while (0)

#define XF_PROXY_FLUSH(addr, len)       \
	do {\
	unsigned long phys_addr = XF_PROXY_DATA_ADDRESS(core) + XF_PROXY_DATA_PAYLOAD_OFFSET - 0x3000 + (unsigned long)((size_t)addr - (size_t)ipc->shmem);\
	mb(); dma_sync_single_for_device(xf_device, phys_addr, len, DMA_TO_DEVICE);\
	/*mb(); dma_sync_single_for_device(NULL, virt_to_phys(addr), len, DMA_TO_DEVICE);*/\
	} while (0)

/*******************************************************************************
 * Memory structures
 ******************************************************************************/

/* ...data managed by host CPU (remote) - in case of shunt it is a IPC layer */
struct xf_proxy_host_data
{
	/* ...command queue */
	xf_proxy_msg_t      command[XF_PROXY_MESSAGE_QUEUE_LENGTH];

	/* ...writing index into command queue */
	u32                 cmd_write_idx;

	/* ...reading index for response queue */
	u32                 rsp_read_idx;

}	__attribute__((__packed__, __aligned__(XF_PROXY_ALIGNMENT)));

/* ...data managed by DSP (local) */
struct xf_proxy_dsp_data
{
	/* ...response queue */
	xf_proxy_msg_t      response[XF_PROXY_MESSAGE_QUEUE_LENGTH];

	/* ...writing index into response queue */
	u32                 rsp_write_idx;

	/* ...reading index for command queue */
	u32                 cmd_read_idx;

}	__attribute__((__packed__, __aligned__(XF_PROXY_ALIGNMENT)));

/* ...shared memory data */
typedef struct xf_shmem_data
{
	/* ...outgoing data (maintained by host CPU (remote side)) */
	struct xf_proxy_host_data   local   __xf_shmem__;

	/* ...ingoing data (maintained by DSP (local side)) */
	struct xf_proxy_dsp_data    remote  __xf_shmem__;

	/* ...shared memory pool - must be page-aligned */
	u8                          buffer[XF_CFG_REMOTE_IPC_POOL_SIZE]   __attribute__((__aligned__(PAGE_SIZE)));

}	__attribute__((__aligned__(PAGE_SIZE))) xf_shmem_data_t;

/*******************************************************************************
 * Accessors
 ******************************************************************************/

/* ...shared memory data accessor */
#define XF_SHMEM_DATA(proxy)                \
	((proxy)->ipc.shmem)

#if 1
/* ...direct access of ioremap/memremapped memory leads to runtime memory exceptions 
 * Hence using readl/writel kernel utility functions. Examples are running ok with this.
 * TODO: is cache management required on top of this? if yes to find equivalent of virt_to_phys
 * used in FLUSH/INVALIDATE macros
 * */
/* ...atomic reading */
#define __XF_PROXY_READ_ATOMIC(var)         \
	({\
	/*XF_PROXY_INVALIDATE(&(var), sizeof(var));*/\
	readl((volatile u32 *)&(var));\
	})

/* ...atomic writing */
#define __XF_PROXY_WRITE_ATOMIC(var, value) \
	({\
	writel((value), (volatile u32 *)&(var));\
	/*XF_PROXY_FLUSH(&(var), sizeof(var));*/\
    (value);\
	})
#else
/* ...atomic reading */
#define __XF_PROXY_READ_ATOMIC(var)         \
	({ XF_PROXY_INVALIDATE(&(var), sizeof(var)); *(volatile u32 *)&(var); })

/* ...atomic writing */
#define __XF_PROXY_WRITE_ATOMIC(var, value) \
	({*(volatile u32 *)&(var) = (value); XF_PROXY_FLUSH(&(var), sizeof(var)); (value); })
#endif

/* ...accessors */
#define XF_PROXY_READ(proxy, field)          \
	__XF_PROXY_READ_##field(XF_SHMEM_DATA(proxy))

#define XF_PROXY_WRITE(proxy, field, v)      \
	__XF_PROXY_WRITE_##field(XF_SHMEM_DATA(proxy), (v))

/* ...individual fields reading */
#define __XF_PROXY_READ_cmd_write_idx(shmem)        \
	shmem->local.cmd_write_idx

#define __XF_PROXY_READ_cmd_read_idx(shmem)         \
	__XF_PROXY_READ_ATOMIC(shmem->remote.cmd_read_idx)

#define __XF_PROXY_READ_rsp_write_idx(shmem)        \
	__XF_PROXY_READ_ATOMIC(shmem->remote.rsp_write_idx)

#define __XF_PROXY_READ_rsp_read_idx(shmem)         \
	shmem->local.rsp_read_idx

/* ...individual fields writings */
#define __XF_PROXY_WRITE_cmd_write_idx(shmem, v)    \
	__XF_PROXY_WRITE_ATOMIC(shmem->local.cmd_write_idx, v)

#define __XF_PROXY_WRITE_cmd_read_idx(shmem, v)     \
	__XF_PROXY_WRITE_ATOMIC(shmem->remote.cmd_read_idx, v)

#define __XF_PROXY_WRITE_rsp_read_idx(shmem, v)     \
	__XF_PROXY_WRITE_ATOMIC(shmem->local.rsp_read_idx, v)

#define __XF_PROXY_WRITE_rsp_write_idx(shmem, v)    \
	__XF_PROXY_WRITE_ATOMIC(shmem->remote.rsp_write_idx, v)

/* ...command buffer accessor */
#define XF_PROXY_COMMAND(proxy, idx)                \
	(&XF_SHMEM_DATA(proxy)->local.command[(idx)])

/* ...response buffer accessor */
#define XF_PROXY_RESPONSE(proxy, idx)               \
	(&XF_SHMEM_DATA(proxy)->remote.response[(idx)])

/*******************************************************************************
 * API functions
 ******************************************************************************/

/* ...NULL-address specification */
#define XF_PROXY_NULL       (~0U)

/* ...invalid proxy address */
#define XF_PROXY_BADADDR    XF_CFG_REMOTE_IPC_POOL_SIZE

/*******************************************************************************
 * IPC data definition
 ******************************************************************************/

typedef struct xf_proxy_ipc_data {
	/* ...shared memory data pointer */
	xf_shmem_data_t        *shmem;

	/* ...core identifier */
	u32                     core;

	/* ...ADSP registers memory */
	void __iomem           *regs;
    
	/* ...ADSP clock handle */
	struct clk             *clk;

	/* ...TCM memories */
	struct resource        *tcm_mmio[2];

	/* ...mapped memory data */
	void __iomem           *tcm[2];

    /* ...IPMMU configuration */
    void __iomem           *ipmmu;

}	xf_proxy_ipc_data_t;

/*******************************************************************************
 * ADSP registers access
 ******************************************************************************/

static inline u32 lx_adsp_read(xf_proxy_ipc_data_t *ipc, u32 reg)
{
	return ioread32(ipc->regs + reg);
}

static inline void lx_adsp_write(xf_proxy_ipc_data_t *ipc, u32 reg, u32 val)
{
	iowrite32(val, ipc->regs + reg);
}

/* ...ADSP registers offsets */
#define BRESET		                    0x00
#define RUNSTALL	                    0x04
#define PWAITMODE	                    0x08
#define STATVECTORSEL	                0x14
#define IRQOUT_STS	                    0x40
#define IRQOUT_CLR	                    0x48
#define IRQOUT_SYN	                    0x80
#define IRQIN_SET	                    0x84
#define IRQIN_POL	                    0x8c
#define IRQIN_SEL	                    0x90

/*******************************************************************************
 * IPMMU registers access
 ******************************************************************************/


/*******************************************************************************
 * Helper functions
 ******************************************************************************/

/* ...translate buffer address to shared proxy address */
static inline u32 xf_ipc_b2a(xf_proxy_ipc_data_t *ipc, void *b)
{
	xf_shmem_data_t    *shmem = ipc->shmem;
	void               *start = shmem->buffer;

	if (b == NULL)
		return XF_PROXY_NULL;
	else if ((u32)(b - start) < XF_CFG_REMOTE_IPC_POOL_SIZE)
		return (u32)(b - start);
	else
		return XF_PROXY_BADADDR;
}

/* ...translate shared proxy address to local pointer */
static inline void * xf_ipc_a2b(xf_proxy_ipc_data_t *ipc, u32 address)
{
	xf_shmem_data_t    *shmem = ipc->shmem;
	void               *start = shmem->buffer;

	if (address < XF_CFG_REMOTE_IPC_POOL_SIZE)
		return start + address;
	else if (address == XF_PROXY_NULL)
		return NULL;
	else
		return (void *)-1;
}

/* ...shared interrupt enabling */
static inline void xf_ipc_enable(xf_proxy_ipc_data_t *ipc)
{
	/* do nothing - tbd */
}

/* ...shared interrupt disabling */
static inline void xf_ipc_disable(xf_proxy_ipc_data_t *ipc)
{
	/* do nothing - tbd */
}

/* ...acknowledge pending interrupts delivery */
static inline int xf_ipc_ack(xf_proxy_ipc_data_t *ipc)
{
	return 0;
}

/* ...notify communication processor about inerface status change */
static inline void xf_ipc_assert(xf_proxy_ipc_data_t *ipc)
{
    unsigned int interrupt_bit_mask = 0x1 << 1; /* ... this bit mask has to match with that of the DSP interrupt */

	lx_adsp_write(ipc, IRQIN_SET, interrupt_bit_mask);
}

/*******************************************************************************
 * Internal API
 ******************************************************************************/

/* ...IPC layer initialization */
extern int  xf_ipc_init(struct device *dev);

/* ...IPC layer cleanup */
extern void xf_ipc_exit(struct device *dev);
