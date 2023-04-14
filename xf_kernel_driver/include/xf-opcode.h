/*******************************************************************************
 * xf-opcode.h
 *
 * Xtensa audio processing framework. Message API
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
 ******************************************************************************/

#ifndef __XF_H
#error "xf-opcode.h mustn't be included directly"
#endif

/*******************************************************************************
 * Opcode composition
 ******************************************************************************/

C_BUG(XF_CFG_MAX_CORES > ((UWORD32)1<<XF_DSP_CORE_BITS));
C_BUG(XF_CFG_MAX_CLIENTS > ((UWORD32)1<<XF_AP_CLIENT_BITS));
C_BUG(XF_CFG_MAX_PORTS > ((UWORD32)1<<XF_DSP_PORT_BITS));
C_BUG(XF_CFG_MAX_IPC_CLIENTS > ((UWORD32)1<<XF_AP_IPC_CLIENT_BITS));
C_BUG(XF_CFG_MAX_COMPONENTS > 512);

/*******************************************************************************
 * Message routing composition - move somewhere else - tbd
 ******************************************************************************/
#if (XF_MSG_ID_BITS > 16)
#define UNIT    (1ULL)
#else
#define UNIT    (1UL)
#endif

/* ...adjust IPC client of message going from user-space */
#define XF_MSG_AP_FROM_USER(id, client) \
    (((id) & ~(((UNIT<<XF_AP_IPC_CLIENT_BITS)-1) << XF_DSP_CORE_BITS)) | (client << XF_DSP_CORE_BITS))

/* ...wipe out IPC client from message going to user-space */
#define XF_MSG_AP_TO_USER(id)           \
    ((id) & ~(((UNIT<<XF_AP_IPC_CLIENT_BITS)-1) << (XF_DSP_CORE_BITS + XF_MSG_ID_BITS)))

/* ...port specification (12 bits) */
#define __XF_PORT_SPEC(core, id, port)  ((xf_msg_id_dtype)(core) | ((id) << XF_DSP_CORE_BITS) | ((xf_msg_id_dtype)(port) << (XF_DSP_PORT_SHIFT_BITS)))
#define __XF_PORT_SPEC2(id, port)       ((xf_msg_id_dtype)(id) | ((xf_msg_id_dtype)(port) << (XF_DSP_PORT_SHIFT_BITS)))
#define XF_PORT_CORE(spec)              (UWORD32)((spec) & ((UNIT<<XF_DSP_CORE_BITS)-1))
#define XF_PORT_CLIENT(spec)            (UWORD32)(((spec) >> XF_DSP_CLIENT_SHIFT_BITS) & ((UNIT<<XF_DSP_CLIENT_BITS)-1))
#define XF_PORT_ID(spec)                (UWORD32)(((spec) >> XF_DSP_PORT_SHIFT_BITS) & ((UNIT<<XF_DSP_PORT_BITS)-1))

/* ...message id contains source and destination ports specification */
#define __XF_MSG_ID(src, dst)           (((xf_msg_id_dtype)(src) & ((UNIT<<XF_MSG_ID_BITS)-1)) | (((xf_msg_id_dtype)(dst) & ((UNIT<<XF_MSG_ID_BITS)-1)) << XF_MSG_ID_BITS))

#define XF_MSG_SRC(id)                  (UWORD32)(((id) >> 0) & ((UNIT<<XF_MSG_ID_BITS)-1))
#define XF_MSG_SRC_CORE(id)             (UWORD32)(((id) >> 0) & ((UNIT<<XF_DSP_CORE_BITS)-1))
#define XF_MSG_SRC_CLIENT(id)           (UWORD32)(((id) >> XF_DSP_CLIENT_SHIFT_BITS) & ((UNIT<<XF_DSP_CLIENT_BITS)-1))
#define XF_MSG_SRC_ID(id)               (UWORD32)(((id) >> 0) & ((UNIT<<(XF_DSP_CLIENT_BITS+XF_DSP_CORE_BITS))-1))
#define XF_MSG_SRC_PORT(id)             (UWORD32)(((id) >> XF_DSP_PORT_SHIFT_BITS) & ((UNIT<<XF_DSP_PORT_BITS)-1))
#define XF_MSG_SRC_PROXY(id)            (UWORD32)(((id) >> (XF_MSG_ID_BITS-1)) & 0x1)

#define XF_MSG_DST(id)                  (UWORD32)(((id) >> XF_MSG_ID_BITS) & ((UNIT<<XF_MSG_ID_BITS)-1))
#define XF_MSG_DST_CORE(id)             (UWORD32)(((id) >> XF_MSG_ID_BITS) & ((UNIT<<XF_DSP_CORE_BITS)-1))
#define XF_MSG_DST_CLIENT(id)           (UWORD32)(((id) >> (XF_MSG_ID_BITS + XF_DSP_CLIENT_SHIFT_BITS)) & ((UNIT<<XF_DSP_CLIENT_BITS)-1))
#define XF_MSG_DST_ID(id)               (UWORD32)(((id) >> XF_MSG_ID_BITS) & ((UNIT<<(XF_DSP_CLIENT_BITS+XF_DSP_CORE_BITS))-1))
#define XF_MSG_DST_PORT(id)             (UWORD32)(((id) >> (XF_MSG_ID_BITS + XF_DSP_PORT_SHIFT_BITS)) & ((UNIT<<XF_DSP_PORT_BITS)-1))
#define XF_MSG_DST_PROXY(id)            (UWORD32)(((id) >> (2*XF_MSG_ID_BITS-1)) & 0x1)

/* ...special treatment of AP-proxy destination field */
#define XF_AP_IPC_CLIENT(id)            (UWORD32)(((id) >> (XF_MSG_ID_BITS + XF_DSP_CORE_BITS)) & ((UNIT<<XF_AP_IPC_CLIENT_BITS)-1))
#define XF_AP_CLIENT(id)                (UWORD32)(((id) >> (XF_MSG_ID_BITS + XF_DSP_CORE_BITS + XF_AP_IPC_CLIENT_BITS)) & ((UNIT << (XF_AP_CLIENT_BITS))-1))
#define __XF_AP_PROXY(core)             ((xf_msg_id_dtype)(core) | (UNIT << (XF_MSG_ID_BITS-1)))
#define __XF_DSP_PROXY(core)            ((xf_msg_id_dtype)(core) | (UNIT << (XF_MSG_ID_BITS-1)))
#define __XF_AP_CLIENT(core, client)    ((xf_msg_id_dtype)(core) | ((client) << (XF_AP_IPC_CLIENT_BITS + XF_DSP_CORE_BITS)) | (UNIT << (XF_MSG_ID_BITS-1)))

/* ...separate treatement for synchronous message with delayed response */
#define XF_AP_CLIENT_SRC(id)            (UWORD32)(((id) >> (XF_DSP_CORE_BITS + XF_AP_IPC_CLIENT_BITS)) & ((UNIT << (XF_AP_CLIENT_BITS))-1))

/*******************************************************************************
 * Opcode composition
 ******************************************************************************/

/* ...opcode composition with command/response data tags */
#define __XF_OPCODE(c, r, op)           ((UWORD32)((((UWORD32)c) << 31) | ((r) << 30) | ((op) & 0x3F)))

/* ...accessors */
#define XF_OPCODE_CDATA(opcode)         ((opcode) & (1 << 31))
#define XF_OPCODE_RDATA(opcode)         ((opcode) & (1 << 30))
#define XF_OPCODE_TYPE(opcode)          ((opcode) & (0x3F))

/*******************************************************************************
 * Opcode types
 ******************************************************************************/

/* ...unregister client */
#define XF_UNREGISTER                   __XF_OPCODE(0, 0, 0)

/* ...register client at proxy */
#define XF_REGISTER                     __XF_OPCODE(1, 0, 1)

/* ...port routing command */
#define XF_ROUTE                        __XF_OPCODE(1, 0, 2)

/* ...port unrouting command */
#define XF_UNROUTE                      __XF_OPCODE(1, 0, 3)

/* ...shared buffer allocation */
#define XF_ALLOC                        __XF_OPCODE(0, 0, 4)

/* ...shared buffer freeing */
#define XF_FREE                         __XF_OPCODE(0, 0, 5)

/* ...set component parameters */
#define XF_SET_PARAM                    __XF_OPCODE(1, 0, 6)

/* ...get component parameters */
#define XF_GET_PARAM                    __XF_OPCODE(1, 1, 7)

/* ...input buffer reception */
#define XF_EMPTY_THIS_BUFFER            __XF_OPCODE(1, 0, 8)

/* ...output buffer reception */
#define XF_FILL_THIS_BUFFER             __XF_OPCODE(0, 1, 9)

/* ...flush specific port */
#define XF_FLUSH                        __XF_OPCODE(0, 0, 10)

/* ...start component operation */
#define XF_START                        __XF_OPCODE(0, 0, 11)

/* ...stop component operation */
#define XF_STOP                         __XF_OPCODE(0, 0, 12)

/* ...pause component operation */
#define XF_PAUSE                        __XF_OPCODE(0, 0, 13)

/* ...resume component operation */
#define XF_RESUME                       __XF_OPCODE(0, 0, 14)

/* ...extended parameter setting function */
#define XF_SET_PARAM_EXT                __XF_OPCODE(1, 0, 15)

/* ...extended parameter retrieval function */
#define XF_GET_PARAM_EXT                __XF_OPCODE(1, 1, 16)

/* ...set priorities */
#define XF_SET_PRIORITIES               __XF_OPCODE(1, 0, 17)

/* ...channel setup */
#define XF_EVENT_CHANNEL_CREATE         __XF_OPCODE(1, 0, 18)

/* ...channel delete */
#define XF_EVENT_CHANNEL_DELETE         __XF_OPCODE(1, 0, 19)

/* ...channel setup */
#define XF_EVENT                        __XF_OPCODE(1, 1, 20)

/* ...total amount of supported decoder commands */
#define __XF_OP_NUM                     21
