/*******************************************************************************
 * xf-config.h
 *
 * Target-specific configuration for Lager board
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2013 Tensilica Inc.
 ******************************************************************************/

#ifndef  __XF_H
#error "xf-config.h mustn't be included directly"
#endif

/*******************************************************************************
 * Global configuration constants
 ******************************************************************************/

/* ...maximal number of cores */
#define XF_CFG_MAX_CORES                1

/* ...maximal number of DSP components per each core */
#define XF_CFG_MAX_CLIENTS              512

/* ...maximal number of ports per each component */
#define XF_CFG_MAX_PORTS                16

/* ...maximal number of IPC clients per proxy */
#define XF_CFG_MAX_IPC_CLIENTS          16

/* ...maximal number of components handled by each IPC client */
#define XF_CFG_MAX_COMPONENTS           512

/* ...size of the internal message pool (make it equal to at least ring-buffer) */
#define XF_CFG_MESSAGE_POOL_SIZE        256

/* ...debug pool size */
#define XF_CFG_DEBUG_BUFFERS_NUM        2

/* ...size of debug buffer */
#define XF_CFG_DEBUG_BUFFER_SIZE        (4 << 10)

/* ...remote IPC buffer pool size (must be equal to the value used in firmware) */
#define XF_CFG_REMOTE_IPC_POOL_SIZE     (256 << 10)

/* ...address of shared memory data (we have single core; address is hardcoded inside FW) */
#define XF_PROXY_DATA_ADDRESS(core)     0xe0000000

#define XF_PROXY_DATA_SIZE(core)        XF_CFG_REMOTE_IPC_POOL_SIZE

/* ...message ID bit mask constants (default is 64 bit message ID with 32-bit ech for source, dest) */

/* ...message-ID bits for source or dest, together form 2*XF_MSG_ID_BITS header */
#define XF_MSG_ID_BITS              32

/* ...DSP-core bits. increase this along with XF_MSG_ID_BITS for the subsystem
 *  * to support larger #of cores */
#define XF_DSP_CORE_BITS            8

#define XF_DSP_PORT_BITS            4
#define XF_DSP_CLIENT_BITS          6
#define XF_DSP_CLIENT_SHIFT_BITS    (XF_DSP_CORE_BITS)
#define XF_DSP_PORT_SHIFT_BITS      (XF_DSP_CLIENT_BITS + XF_DSP_CORE_BITS)

#define XF_AP_IPC_CLIENT_BITS       4
#define XF_AP_CLIENT_BITS           9


#if (XF_MSG_ID_BITS > 16)
typedef u64 xf_msg_id_dtype __attribute__((aligned(sizeof(u64))));
#else
typedef u32 xf_msg_id_dtype;
#endif

#define UWORD32 u32
