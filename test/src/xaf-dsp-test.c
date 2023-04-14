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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>

#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define AUDIO_FRMWK_BUF_SIZE   (256 << 13)
#define AUDIO_COMP_BUF_SIZE    (1024 << 13)

//component parameters

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

/* Global vaiables */
void *p_adev = NULL;

#if 0
/* Dummy unused functions */
XA_ERRORCODE xa_mp3_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_capturer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) { return 0; }
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_pcm_gain(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_aec22(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_dummy_aec23(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_pcm_split(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mimo_mix(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_dummy_wwd(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_dummy_hbuf(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_opus_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_dummy_wwd_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_dummy_hbuf_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4) { return 0; }
XA_ERRORCODE xa_opus_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_microspeech_fe(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_microspeech_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_person_detect_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_keyword_detection_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
#endif

#define AUDIO_FRMWK_BUF_SIZE_MAX    (256<<13)   /* ...this should match the framework buffer size of master core, can vary from test to test or can set to a maximum. */
#undef XF_SHMEM_SIZE
#define XF_SHMEM_SIZE  (AUDIO_FRMWK_BUF_SIZE_MAX * (XAF_MEM_ID_DEV_MAX + 1) + ((1024<<8)*XF_CFG_CORES_NUM))

int main_task(int argc, char **argv)
{
    pUWORD8 ver_info[3] = { 0,0,0 };    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    mem_obj_t* mem_handle;

#ifdef XAF_PROFILE
    xaf_perf_stats_t *pstats = (xaf_perf_stats_t *)&perf_stats;
    frmwk_cycles = 0;
    fread_cycles = 0;
    fwrite_cycles = 0;
    dsp_comps_cycles = 0;
    tot_cycles = 0;
    num_bytes_read = 0;
    num_bytes_write = 0;
    aac_dec_cycles = 0; dec_cycles = 0; mix_cycles = 0; pcm_split_cycles = 0; src_cycles = 0; pcm_gain_cycles = 0;
#endif

    audio_frmwk_buf_size = AUDIO_FRMWK_BUF_SIZE;

    // NOTE: set_wbna() should be called before any other dynamic
    // adjustment of the region attributes for cache.
    set_wbna(&argc, argv);

    /* ...start xos */
    board_id = start_rtos();

   /* ...get xaf version info*/
    TST_CHK_API(xaf_get_verinfo(ver_info), "xaf_get_verinfo");

    /* ...show xaf version info*/
    TST_CHK_API(print_verinfo(ver_info, (pUWORD8)"\'\'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - /'Hosted Main DSP/' Sample App");

    mem_handle = mem_init();

    xaf_adev_config_t adev_config;
    TST_CHK_API(xaf_adev_config_default_init(&adev_config), "xaf_adev_config_default_init");

    adev_config.core = XF_CORE_ID_MASTER;
    adev_config.audio_framework_buffer_size[XAF_MEM_ID_DEV] = audio_frmwk_buf_size;
#if (XF_CFG_CORES_NUM>1)
    adev_config.audio_shmem_buffer_size[0] = XF_SHMEM_SIZE - audio_frmwk_buf_size*(1 + XAF_MEM_ID_DEV_MAX);
    adev_config.pshmem_dsp[0] = shared_mem;
#else
    adev_config.audio_shmem_buffer_size[0] = XF_CFG_REMOTE_IPC_POOL_SIZE;
    adev_config.pshmem_dsp[0] = (pVOID) XF_CFG_SHMEM_ADDRESS(core);
    //adev_config.pshmem_dsp[0] = mem_malloc(adev_config.audio_shmem_buffer_size[0], XAF_MEM_ID_DEV);
#endif //(XF_CFG_CORES_NUM>1)
    audio_comp_buf_size = AUDIO_COMP_BUF_SIZE;
    adev_config.audio_component_buffer_size[XAF_MEM_ID_COMP] = audio_comp_buf_size;
    adev_config.paudio_component_buffer[XAF_MEM_ID_COMP] = mem_malloc(audio_comp_buf_size, XAF_MEM_ID_COMP);
    adev_config.framework_local_buffer_size = FRMWK_APP_IF_BUF_SIZE;
    adev_config.pframework_local_buffer = mem_malloc(FRMWK_APP_IF_BUF_SIZE, XAF_MEM_ID_DEV);

    {
        int k;
        int bufsize = AUDIO_COMP_FAST_BUF_SIZE;
        for(k=XAF_MEM_ID_COMP_FAST; k<= XAF_MEM_ID_COMP_MAX;k++)
        {
       	    adev_config.audio_component_buffer_size[k] = bufsize;
            adev_config.paudio_component_buffer[k] = mem_malloc(bufsize, k);
        }
    }

#ifdef XAF_PROFILE
    memset(&pstats[XF_CORE_ID], 0, sizeof(xaf_perf_stats_t));

    wwd_cycles = hbuf_cycles = 0;
    inference_cycles = microspeech_fe_cycles = 0;
    pd_inference_cycles = 0;
    kd_inference_cycles = 0;

    adev_config.cb_compute_cycles = cb_total_frmwrk_cycles; /* ...callback function pointer to update thread cycles */
    adev_config.cb_stats = (void *)&pstats[XF_CORE_ID]; /* ...pointer to this worker DSP's stats structure, to be updated at the end of execution, in the call-back function. */
#endif //XAF_PROFILE

    TST_CHK_API(xaf_dsp_open(&p_adev, &adev_config),  "xaf_dsp_open");
    FIO_PRINTF(stdout,"Audio Master DSP[%d] Ready [Config:%s]\n", adev_config.core, XCHAL_CORE_ID);

#ifdef XAF_PROFILE
    clk_start();
#endif

    TST_CHK_API(xaf_dsp_close(p_adev), "xaf_dsp_close");
    FIO_PRINTF(stdout,"Audio Main DSP[%d] closed\n\n", adev_config.core);

#ifdef XAF_PROFILE
    compute_total_frmwrk_cycles();
    clk_stop();
#endif

    mem_free(adev_config.pframework_local_buffer, XAF_MEM_ID_DEV);

#if (XF_CFG_CORES_NUM==1)
    //mem_free(adev_config.pshmem_dsp[0], XAF_MEM_ID_DEV);
#endif
    {
        int k;
        for(k=XAF_MEM_ID_COMP;k<= XAF_MEM_ID_COMP_MAX;k++)
        {
    	    mem_free(adev_config.paudio_component_buffer[k], k);
        }
    }

    mem_exit();

    /* ...deinitialize tracing facility */
    TRACE_DEINIT();

    return 0;
}

