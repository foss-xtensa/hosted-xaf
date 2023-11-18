/*******************************************************************************
 * xf.h
 *
 * Shared-memory interface to Xtensa DSP codec server.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2013 Tensilica Inc.
 ******************************************************************************/

#ifdef  __XF_H
#error "xf.h included more than once"
#endif

#define __XF_H

/*******************************************************************************
 * Includes
 ******************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/dma-mapping.h>

/*******************************************************************************
 * System-specific includes
 ******************************************************************************/

/* ...configuration options */
#include "xaf-config-user.h"
#include "xf-config.h"

/* ...debugging support */
#include "xf-debug.h"

/* ...opcode compositions */
#include "xf-opcode.h"

/* ...proxy data (tbd) */
#include "xf-proxy.h"

/* ...shared memory interface */
#include "xf-shmem.h"
