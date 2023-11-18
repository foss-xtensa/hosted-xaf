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

#include "audio/xa-microspeech-frontend-api.h"
#include "audio/xa-microspeech-inference-api.h"
#include "audio/xa-capturer-api.h"
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s  -outfile:out_filename.pcm -samples:<samples-per-channel to be captured(zero for endless capturing)>\n", argv[0]);\
    FIO_PRINTF(stdout, "\nNote: Capturer-plugin expects input file named 'capturer_in.pcm' to be present in the execution directory.\n\n");

#define AUDIO_FRMWK_BUF_SIZE   (256 << 9)
#define AUDIO_COMP_BUF_SIZE    (1024 << 8)

#define NUM_COMP_IN_GRAPH       3

//component parameters
#define CAPTURER_PCM_WIDTH             (16)
#define CAPTURER_SAMPLE_RATE           (16000)
#define CAPTURER_NUM_CH                (1)
#define XA_CAPTURER_FRAME_SIZE         (16*2*20) // 20 ms

#define MICROSPEECH_FE_SAMPLE_WIDTH    (16)
#define MICROSPEECH_FE_NUM_CH          (1)
#define MICROSPEECH_FE_SAMPLE_RATE     (16000)

#define INFERENCE_SAMPLE_WIDTH    (16)
#define INFERENCE_NUM_CH          (1)
#define INFERENCE_SAMPLE_RATE     (16000)

#define THREAD_SCRATCH_SIZE         (1024)

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

/* Dummy unused functions */
XA_ERRORCODE xa_pcm_gain(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_amr_wb_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_hotword_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value){return 0;}
XA_ERRORCODE xa_vorbis_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_aec22(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_dummy_aec23(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_pcm_split(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_mimo_mix(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_dummy_wwd(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_hbuf(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_opus_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_wwd_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_dummy_hbuf_msg(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_person_detect_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_keyword_detection_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_opus_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

static int capturer_setup(void *p_capturer,xaf_format_t capturer_format,UWORD64 sample_end)
{
    int param[][2] = {
        {
            XA_CAPTURER_CONFIG_PARAM_PCM_WIDTH,
            capturer_format.pcm_width,
        }, {
            XA_CAPTURER_CONFIG_PARAM_CHANNELS,
            capturer_format.channels,
        }, {
            XA_CAPTURER_CONFIG_PARAM_SAMPLE_RATE,
            capturer_format.sample_rate,
        }, {
            XA_CAPTURER_CONFIG_PARAM_SAMPLE_END,
            sample_end,
        }, {
            XA_CAPTURER_CONFIG_PARAM_FRAME_SIZE,
            XA_CAPTURER_FRAME_SIZE,
        },
    };

    return(xaf_comp_set_config(p_capturer, ARRAY_SIZE(param), param[0]));
}

static int microspeech_fe_setup(void *p_comp,xaf_format_t comp_format)
{
    int param[][2] = {
        {
            XA_MICROSPEECH_FE_CONFIG_PARAM_CHANNELS,
            comp_format.channels,
        }, {
            XA_MICROSPEECH_FE_CONFIG_PARAM_SAMPLE_RATE,
            comp_format.sample_rate,
        }, {
            XA_MICROSPEECH_FE_CONFIG_PARAM_PCM_WIDTH,
            comp_format.pcm_width,
        },
    };

    return(xaf_comp_set_config(p_comp, ARRAY_SIZE(param), param[0]));
}

static int inference_setup(void *p_comp,xaf_format_t comp_format)
{
    int param[][2] = {
        {
            XA_MICROSPEECH_INFERENCE_CONFIG_PARAM_CHANNELS,
            comp_format.channels,
        }, {
            XA_MICROSPEECH_INFERENCE_CONFIG_PARAM_SAMPLE_RATE,
            comp_format.sample_rate,
        }, {
            XA_MICROSPEECH_INFERENCE_CONFIG_PARAM_PCM_WIDTH,
            comp_format.pcm_width,
        },
    };

    return(xaf_comp_set_config(p_comp, ARRAY_SIZE(param), param[0]));
}

static int capturer_start_operation(void *p_capturer)
{
    int param[][2] = {
        {
            XA_CAPTURER_CONFIG_PARAM_STATE,
            XA_CAPTURER_STATE_START,
        },
    };

    return(xaf_comp_set_config(p_capturer, ARRAY_SIZE(param), param[0]));
}

static int get_comp_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[6];
    int ret;


    TST_CHK_PTR(p_comp, "get_comp_config");
    TST_CHK_PTR(comp_format, "get_comp_config");

    param[0] = XA_MICROSPEECH_FE_CONFIG_PARAM_CHANNELS;
    param[2] = XA_MICROSPEECH_FE_CONFIG_PARAM_PCM_WIDTH;
    param[4] = XA_MICROSPEECH_FE_CONFIG_PARAM_SAMPLE_RATE;

    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if(ret < 0)
        return ret;

    comp_format->channels = param[1];
    comp_format->pcm_width = param[3];
    comp_format->sample_rate = param[5];

    return 0;
}

static int capturer_get_config(void *p_comp, xaf_format_t *comp_format)
{
    int param[8];
    int ret;


    TST_CHK_PTR(p_comp, "get_mp3_enc_config");
    TST_CHK_PTR(comp_format, "get_mp3_enc_config");

    param[0] = XA_CAPTURER_CONFIG_PARAM_CHANNELS;
    param[2] = XA_CAPTURER_CONFIG_PARAM_PCM_WIDTH;
    param[4] = XA_CAPTURER_CONFIG_PARAM_SAMPLE_RATE;
    param[6] = XA_CAPTURER_CONFIG_PARAM_BYTES_PRODUCED;

    ret = xaf_comp_get_config(p_comp, 4, &param[0]);
    if(ret < 0)
        return ret;

    comp_format->channels = param[1];
    comp_format->pcm_width = param[3];
    comp_format->sample_rate = param[5];
    comp_format->output_produced = param[7];
    return 0;
}

void fio_quit()
{
    return;
}


int main_task(int argc, char **argv)
{

    void *p_adev = NULL;
    void *p_output;
    xf_thread_t inference_thread;
#ifdef XAF_HOSTED_AP
    unsigned char inference_stack[STACK_SIZE] __attribute__ ((aligned(XF_THREAD_STACK_ALIGN)));
#else /* XAF_HOSTED_AP */
    unsigned char inference_stack[STACK_SIZE];
#endif /* XAF_HOSTED_AP */

    void * p_microspeech_fe  = NULL;
    void * p_inference  = NULL;
    void * p_capturer  = NULL;
    xaf_comp_status microspeech_fe_status;
    xaf_comp_status inference_status;
    xaf_comp_status capturer_status;
    int microspeech_fe_info[4];
    int capturer_info[4];
    char *filename_ptr;
    char *sample_cnt_ptr;
    void *inference_fe_thread_args[NUM_THREAD_ARGS];
    FILE *ofp = NULL;
    int i=0;
    xaf_format_t microspeech_fe_format;
    xaf_format_t inference_format;
    xaf_format_t capturer_format;
    const char *ext;
    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    int num_comp;
    int ret = 0;
    mem_obj_t* mem_handle;
    xaf_comp_type comp_type;
    long long int sammple_end_capturer = 0;
    double microspeech_fe_mcps = 0;
    double inference_mcps = 0;
    xf_id_t comp_id;
#ifndef XAF_HOSTED_AP
    int dsp_comp_scratch_size = THREAD_SCRATCH_SIZE;
#endif //!XAF_HOSTED_AP

#ifdef XAF_PROFILE
    frmwk_cycles = 0;
    fread_cycles = 0;
    fwrite_cycles = 0;
    dsp_comps_cycles = 0;
    microspeech_fe_cycles = 0;
    inference_cycles = 0;
    capturer_cycles = 0;
    tot_cycles = 0;
    num_bytes_read = 0;
    num_bytes_write = 0;
#endif

    memset(&microspeech_fe_format, 0, sizeof(xaf_format_t));
    memset(&inference_format, 0, sizeof(xaf_format_t));
    memset(&capturer_format, 0, sizeof(xaf_format_t));

    audio_frmwk_buf_size = AUDIO_FRMWK_BUF_SIZE;
    audio_comp_buf_size = AUDIO_COMP_BUF_SIZE;
    num_comp = NUM_COMP_IN_GRAPH;

    // NOTE: set_wbna() should be called before any other dynamic
    // adjustment of the region attributes for cache.
    set_wbna(&argc, argv);

    /* ...start xos */
    board_id = start_rtos();

    /* ...get xaf version info*/
    TST_CHK_API(xaf_get_verinfo(ver_info), "xaf_get_verinfo");

    /* ...show xaf version info*/
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Capturer + Micro speech FE + inference \'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Capturer + Micro speech FE + inference  \' Sample App");

    /* ...check input arguments */
    if (argc != 3)
    {
        PRINT_USAGE;
        return -1;
    }

    if(NULL != strstr(argv[1], "-outfile:"))
    {
        filename_ptr = (char *)&(argv[1][9]);
        ext = strrchr(argv[1], '.');
        if(ext!=NULL)
        {
            ext++;
            if (strcmp(ext, "pcm"))
            {

                /*comp_id_pcm_gain    = "post-proc/pcm_gain";
                comp_setup = pcm_gain_setup;*/
                FIO_PRINTF(stderr, "Unknown input file format '%s'\n", ext);
                return -1;
            }
        }
        else
        {
            FIO_PRINTF(stderr, "Failed to open outfile\n");
            return -1;
        }

        /* ...open file */
        if ((ofp = fio_fopen(filename_ptr, "wb")) == NULL)
        {
           FIO_PRINTF(stderr, "Failed to open '%s': %d\n", filename_ptr, errno);
           return -1;
        }
    }
    else
    {
        PRINT_USAGE;
        return -1;
    }
    if(NULL != strstr(argv[2], "-samples:"))
    {
        sample_cnt_ptr = (char *)&(argv[2][9]);

        if ((*sample_cnt_ptr) == '\0' )
        {
            FIO_PRINTF(stderr, "samples-per-channel to be produced is not entererd\n");
            return -1;
        }
        sammple_end_capturer = atoi(sample_cnt_ptr);


    }
    else
    {
        PRINT_USAGE;
        return -1;
    }


    p_output = ofp;
    mem_handle = mem_init();
    xaf_adev_config_t adev_config;
    TST_CHK_API(xaf_adev_config_default_init(&adev_config), "xaf_adev_config_default_init");

    adev_config.audio_framework_buffer_size[XAF_MEM_ID_DEV] =  audio_frmwk_buf_size;
    adev_config.audio_component_buffer_size[XAF_MEM_ID_COMP] = audio_comp_buf_size;
    adev_config.core = XF_CORE_ID;
    //setting scratch size for worker thread
#ifndef XAF_HOSTED_AP
    adev_config.worker_thread_scratch_size[0] = dsp_comp_scratch_size;
#endif //!XAF_HOSTED_AP
#if (XF_CFG_CORES_NUM>1) && !defined(XAF_HOSTED_AP)
    adev_config.audio_shmem_buffer_size = XF_SHMEM_SIZE - audio_frmwk_buf_size*(1 + XAF_MEM_ID_DEV_MAX);
    adev_config.pshmem_dsp = shared_mem;
    FIO_PRINTF(stdout,"core[%d] shmem:%p\n", adev_config.core, shared_mem);
#endif //(XF_CFG_CORES_NUM>1)
    TST_CHK_API_ADEV_OPEN(p_adev, adev_config,  "xaf_adev_open");
    FIO_PRINTF(stdout,"Audio Device Ready\n");

    /* ...create capturer component */
    comp_type = XAF_CAPTURER;
    capturer_format.sample_rate = CAPTURER_SAMPLE_RATE;
    capturer_format.channels = CAPTURER_NUM_CH;
    capturer_format.pcm_width = CAPTURER_PCM_WIDTH;
    TST_CHK_API_COMP_CREATE(p_adev, XF_CORE_ID, &p_capturer, "capturer", 0, 0, NULL, comp_type, "xaf_comp_create");
    TST_CHK_API(capturer_setup(p_capturer, capturer_format,sammple_end_capturer), "capturer_setup");

     /* ...create micro speech component */
    microspeech_fe_format.sample_rate = MICROSPEECH_FE_SAMPLE_RATE;
    microspeech_fe_format.channels = MICROSPEECH_FE_NUM_CH;
    microspeech_fe_format.pcm_width = MICROSPEECH_FE_SAMPLE_WIDTH;
    TST_CHK_API_COMP_CREATE(p_adev, XF_CORE_ID, &p_microspeech_fe, "post-proc/microspeech_fe", 0, 0, NULL, XAF_POST_PROC, "xaf_comp_create");
    TST_CHK_API(microspeech_fe_setup(p_microspeech_fe,microspeech_fe_format), "microspeech_fe_setup");

    inference_format.sample_rate = INFERENCE_SAMPLE_RATE;
    inference_format.channels = INFERENCE_NUM_CH;
    inference_format.pcm_width = INFERENCE_SAMPLE_WIDTH;
    TST_CHK_API_COMP_CREATE(p_adev, XF_CORE_ID, &p_inference, "post-proc/microspeech_inference", 0, 1, NULL, XAF_POST_PROC, "xaf_comp_create");
    TST_CHK_API(inference_setup(p_inference,inference_format), "inference_setup");

    /* ...start capturer component */
    TST_CHK_API(xaf_comp_process(p_adev, p_capturer, NULL, 0, XAF_START_FLAG),"xaf_comp_process");

    TST_CHK_API(xaf_comp_get_status(p_adev, p_capturer, &capturer_status, &capturer_info[0]), "xaf_comp_get_status");

    if (capturer_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stderr, "Failed to init");
        TST_CHK_API(ADEV_CLOSE_SIGNAL, "Capturer Initialization");
    }

    TST_CHK_API(xaf_connect(p_capturer, 0, p_microspeech_fe, 0, 4), "xaf_connect");

    TST_CHK_API(capturer_start_operation(p_capturer), "capturer_start_operation");

    TST_CHK_API(xaf_comp_process(p_adev, p_microspeech_fe, NULL, 0, XAF_START_FLAG), "xaf_comp_process");

    TST_CHK_API(xaf_comp_get_status(p_adev, p_microspeech_fe, &microspeech_fe_status, &microspeech_fe_info[0]), "xaf_comp_get_status");
    if (microspeech_fe_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stdout,"Microspeech FE Failed to init \n");
        TST_CHK_API(ADEV_CLOSE_SIGNAL, "Microspeech FE Initialization");
    }

    TST_CHK_API(xaf_connect(p_microspeech_fe, 1, p_inference, 0, 4), "xaf_connect");


    TST_CHK_API(xaf_comp_process(p_adev, p_inference, NULL, 0, XAF_START_FLAG), "xaf_comp_process");

    TST_CHK_API(xaf_comp_get_status(p_adev, p_inference, &inference_status, &microspeech_fe_info[0]), "xaf_comp_get_status");
    if (inference_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stdout,"Inference Failed to init \n");
        TST_CHK_API(ADEV_CLOSE_SIGNAL, "Inference Initialization");
    }
#ifdef XAF_PROFILE
    clk_start();

#endif
    comp_id="post-proc/microspeech_inference";
    comp_type = XAF_POST_PROC;
    inference_fe_thread_args[0] = p_adev;
    inference_fe_thread_args[1] = p_inference;
    inference_fe_thread_args[2] = NULL;
    inference_fe_thread_args[3] = p_output;
    inference_fe_thread_args[4] = &comp_type;
    inference_fe_thread_args[5] = (void *)comp_id;
    inference_fe_thread_args[6] = (void *)&i;
    ret = __xf_thread_create(&inference_thread, comp_process_entry, &inference_fe_thread_args[0], "Inference Thread", inference_stack, STACK_SIZE, 7);
    if (ret)
    {
        FIO_PRINTF(stdout,"Failed to create inference thread  : %d\n", ret);
        TST_CHK_API(ADEV_CLOSE_SIGNAL, "Inference Thread Initialization");
    }
    ret = __xf_thread_join(&inference_thread, NULL);

    if (ret)
    {
        FIO_PRINTF(stdout,"Decoder thread exit Failed : %d \n", ret);
        TST_CHK_API(ADEV_CLOSE_SIGNAL, "Inference Thread Join");
    }

#ifdef XAF_PROFILE
    compute_total_frmwrk_cycles();
    clk_stop();
#endif

    TST_CHK_API(capturer_get_config(p_capturer, &capturer_format), "capturer get config");
    TST_CHK_API(get_comp_config(p_microspeech_fe, &microspeech_fe_format), "capturer get config");

    {
        /* collect memory stats before closing the device */
        dsp_stats(p_adev, &adev_config);
    }

    /* ...exec done, clean-up */
    __xf_thread_destroy(&inference_thread);
    TST_CHK_API(xaf_comp_delete(p_inference), "xaf_comp_delete");
    TST_CHK_API(xaf_comp_delete(p_microspeech_fe), "xaf_comp_delete");
    TST_CHK_API(xaf_comp_delete(p_capturer), "xaf_comp_delete");
    TST_CHK_API_ADEV_CLOSE(p_adev, XAF_ADEV_NORMAL_CLOSE, adev_config, "xaf_adev_close");
    FIO_PRINTF(stdout,"Audio device closed\n\n");
    mem_exit();
#ifdef XAF_PROFILE
    {
        UWORD64 tmp_bytes_produced = capturer_format.output_produced - 32000; // inference start after 1 sec.
#ifdef XAF_HOSTED_AP
        /* ...microspeech_fe_mcps or inference_mcps both cannot be accurately calcualted at once as of now, hence retaining only one: TODO */
        //microspeech_fe_mcps = compute_comp_mcps(capturer_format.output_produced, dsp_comps_cycles, microspeech_fe_format, &strm_duration);
        inference_mcps      = compute_comp_mcps(tmp_bytes_produced, dsp_comps_cycles, inference_format, &strm_duration);
        dsp_mcps = inference_mcps;
#else //XAF_HOSTED_AP
        dsp_comps_cycles = capturer_cycles + microspeech_fe_cycles + inference_cycles;
        dsp_mcps = compute_comp_mcps(capturer_format.output_produced, capturer_cycles, capturer_format, &strm_duration);
        microspeech_fe_mcps = compute_comp_mcps(capturer_format.output_produced, microspeech_fe_cycles, microspeech_fe_format, &strm_duration);
        inference_mcps      = compute_comp_mcps(tmp_bytes_produced, inference_cycles, inference_format, &strm_duration);
        dsp_mcps += microspeech_fe_mcps + inference_mcps;
#endif //XAF_HOSTED_AP
    }
#endif //XAF_PROFILE
    TST_CHK_API(print_mem_mcps_info(mem_handle, num_comp), "print_mem_mcps_info");

    if (ofp) fio_fclose(ofp);

    fio_quit();

    /* ...deinitialize tracing facility */
    TRACE_DEINIT();

    return 0;
}


