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
 * Xtensa IPC mechanism
 *******************************************************************************/

#ifndef __XF_H
#error "xf-ipc.h mustn't be included directly"
#endif

/*******************************************************************************
 * Includes
 ******************************************************************************/

/* ...system-specific shared memory configuration */
#include "xf-shmem.h"

/*******************************************************************************
 * Remote IPI interrupt mode
 ******************************************************************************/

/* ...enable/disable IPI interrupt */
static inline void xf_ipi_enable(UWORD32 core, int on)
{
}

/* ...wait in low-power mode for interrupt arrival if "ptr" is 0 */
static inline int xf_ipi_wait(UWORD32 core)
{
    xf_core_ro_data_t *ro = XF_CORE_RO_DATA(core);
    xf_event_t *msgq_event = ro->ipc.msgq_event;

    __xf_event_wait_any(msgq_event, CMD_MSGQ_READY | DSP_DIE_MSGQ_ENTRY);
    if (__xf_event_get(msgq_event) & DSP_DIE_MSGQ_ENTRY)
    {
        return 0;
    }
	__xf_event_clear(msgq_event, CMD_MSGQ_READY | DSP_DIE_MSGQ_ENTRY);

    return 1;
}

/* ...complete IPI waiting (may be called from any context on local core) */
static inline void xf_ipi_resume(UWORD32 core)
{
}

/* ...complete IPI waiting and resume dsp thread (non ISR-safe) */
static inline void xf_ipi_resume_dsp(UWORD32 core)
{
#if XAF_HOSTED_DSP
    __xf_event_set(xf_g_dsp->pmsgq_event, CMD_MSGQ_READY);
#else //XAF_HOSTED_DSP
    if(core == XF_CORE_ID_MASTER)
    {
    	xf_core_ro_data_t *ro = XF_CORE_RO_DATA(core);

	    __xf_event_set(ro->ipc.msgq_event, CMD_MSGQ_READY);
    }
#if (XF_CFG_CORES_NUM > 1)
    else
    {
        __xf_event_set(xf_g_dsp->pmsgq_event, CMD_MSGQ_READY);
    }
#endif
#endif //XAF_HOSTED_DSP
}

/* ...complete IPI waiting and resume dsp thread (ISR safe) */
static inline void xf_ipi_resume_dsp_isr(UWORD32 core)
{
#if XAF_HOSTED_DSP
    __xf_event_set(xf_g_dsp->pmsgq_event, CMD_MSGQ_READY);
#else //XAF_HOSTED_DSP
    if(core == XF_CORE_ID_MASTER)
    {
        xf_core_ro_data_t *ro = XF_CORE_RO_DATA(core);

        __xf_event_set_isr(ro->ipc.msgq_event, CMD_MSGQ_READY);
    }
#if (XF_CFG_CORES_NUM > 1)
    else
    {
        __xf_event_set_isr(xf_g_dsp->pmsgq_event, CMD_MSGQ_READY);
    }
#endif
#endif //XAF_HOSTED_DSP
}

#if defined(XAF_HOSTED_DSP)
#define MEM_START                       XF_CFG_SHMEM_BASE_ADDRESS
#define IRQOUT_SYNC_ADDR                (MEM_START + 0x100 + 0x80)
#define IRQOUT_DSP0_ADDR                (MEM_START + 0x100 + 0x84)
#define IRQOUT_POL_ADDR                 (MEM_START + 0x100 + 0x8c)
#define IRQOUT_SEL_ADDR                 (MEM_START + 0x100 + 0x90)
#define DSP_TO_HOST_NOTIFY_PATTERN       0x77
#define DSP_TO_HOST_SYNC_PATTERN         0x66
#define HOST_TO_DSP_SYNC_PATTERN         0x55
#define WDSP_TO_DSP0_NOTIFY_PATTERN      (0x1 << 1)

#define XF_PROXY_DATA_PAYLOAD_OFFSET    0x00004000
#define MEM_START_DATA                  (MEM_START + XF_PROXY_DATA_PAYLOAD_OFFSET)

/* ...NULL-address specification */
#define XF_PROXY_NULL                   (~0U)
/* ...invalid proxy address */
#define XF_PROXY_BADADDR                XF_CFG_REMOTE_IPC_POOL_SIZE
#endif //XAF_HOSTED_DSP

/* ...assert IPI interrupt on remote core - board-specific */
static inline void xf_ipi_assert(UWORD32 this_core, UWORD32 core)
{
#if defined(XAF_HOSTED_DSP)
    if(this_core == XF_CORE_ID_MASTER)
    {
      char *pmem_pattern = (char *)IRQOUT_POL_ADDR;
      pmem_pattern[0] = DSP_TO_HOST_NOTIFY_PATTERN;
    }
    else
    {
      char *pmem_pattern = (char *)IRQOUT_DSP0_ADDR;
      pmem_pattern[0] = WDSP_TO_DSP0_NOTIFY_PATTERN;
    }
#else //XAF_HOSTED_DSP
	xf_core_ro_data_t *ro = XF_CORE_RO_DATA(core);

	__xf_event_set(ro->ipc.msgq_event, RESP_MSGQ_READY);
#endif //XAF_HOSTED_DSP
}

/* ...initialize IPI subsystem */
static inline int xf_ipi_init(UWORD32 core)
{
    return 0;
}

/* ...deinitialize IPI subsystem */
static inline int xf_ipi_deinit(UWORD32 core)
{
    return 0;
}

/*******************************************************************************
 * Shared memory operations
 ******************************************************************************/

/* ...translate buffer address to shared proxy address */
static inline UWORD32 xf_ipc_b2a(UWORD32 core, void *b)
{
#if XAF_HOSTED_DSP
    void               *start = (void *)MEM_START_DATA;
    UWORD32 val;

    if (b == NULL)
        val = XF_PROXY_NULL;
    else if ((WORD32)(b - start) < XF_CFG_REMOTE_IPC_POOL_SIZE)
        val = (UWORD32)(b - start);
    else
        val = XF_PROXY_BADADDR;

    return val;
#else
    return (UWORD32) b;
#endif
}

/* ...translate shared proxy address to local pointer */
static inline void * xf_ipc_a2b(UWORD32 core, UWORD32 address)
{
#if XAF_HOSTED_DSP
    void               *start = (void *)MEM_START_DATA;
    void *pval;
    
    if (address < XF_CFG_REMOTE_IPC_POOL_SIZE)
        pval = start + address;
    else if (address == XF_PROXY_NULL)
        pval = (void *)NULL;
    else
        pval = (void *)-1;

    return pval;
#else
    return (void *) address;
#endif
}

/* ...system-specific IPC layer initialization */
extern int xf_ipc_init(UWORD32 core);

/* ...system-specific IPC layer deinitialization */
extern int xf_ipc_deinit(UWORD32 core);
