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

#ifndef PACK_WS_DUMMY
#include "audio/xa_mp3_dec_api.h"
#endif //PACK_WS_DUMMY
#include "xaf-utils-test.h"
#include "xaf-fio-test.h"

#define PRINT_USAGE FIO_PRINTF(stdout, "\nUsage: %s -infile:filename.mp3 -outfile:filename.pcm\n\n", argv[0]);

#define AUDIO_FRMWK_BUF_SIZE   (256 << 8)
#define AUDIO_COMP_BUF_SIZE    (1024 << 7)

#define NUM_COMP_IN_GRAPH       1

//component parameters
#define MP3_DEC_PCM_WIDTH       16

unsigned int num_bytes_read, num_bytes_write;
extern int audio_frmwk_buf_size;
extern int audio_comp_buf_size;
double strm_duration;

#ifdef XAF_PROFILE
    extern long long tot_cycles, frmwk_cycles, fread_cycles, fwrite_cycles;
    extern long long dsp_comps_cycles, dec_cycles;
    extern double dsp_mcps;
#endif

/* Dummy unused functions */
XA_ERRORCODE xa_aac_decoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mixer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_pcm_gain(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_mp3_encoder(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_src_pp_fx(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_renderer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_capturer(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
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
XA_ERRORCODE xa_opus_decoder(xa_codec_handle_t p_xa_module_obj, WORD32 i_cmd, WORD32 i_idx, pVOID pv_value) {return 0;}
XA_ERRORCODE xa_microspeech_fe(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_microspeech_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_person_detect_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}
XA_ERRORCODE xa_keyword_detection_inference(xa_codec_handle_t var1, WORD32 var2, WORD32 var3, pVOID var4){return 0;}

static int mp3_setup(void *p_decoder)
{
#ifndef PACK_WS_DUMMY

/* ...used in internal regressions, to test decoder-initialization without input feature */
#ifndef XA_DEC_INIT_WO_INP_TEST
    int param[2];

    param[0] = XA_MP3DEC_CONFIG_PARAM_PCM_WDSZ;
    param[1] = MP3_DEC_PCM_WIDTH;

    return(xaf_comp_set_config(p_decoder, 1, &param[0]));
#else
    /* ... for internal regression testing */
    int param[4];

    param[0] = XA_MP3DEC_CONFIG_PARAM_PCM_WDSZ;
    param[1] = MP3_DEC_PCM_WIDTH;

    param[2] = XAF_COMP_CONFIG_PARAM_DEC_INIT_WO_INP;
    param[3] = 1;

    return(xaf_comp_set_config(p_decoder, 2, &param[0]));
#endif
#else //PACK_WS_DUMMY
    return 0;
#endif //PACK_WS_DUMMY
}


static int get_comp_config(void *p_comp, xaf_format_t *comp_format)
{
#ifndef PACK_WS_DUMMY
    int param[6];
    int ret;


    TST_CHK_PTR(p_comp, "get_comp_config");
    TST_CHK_PTR(comp_format, "get_comp_config");

    param[0] = XA_MP3DEC_CONFIG_PARAM_NUM_CHANNELS;
    param[2] = XA_MP3DEC_CONFIG_PARAM_PCM_WDSZ;
    param[4] = XA_MP3DEC_CONFIG_PARAM_SAMP_FREQ;


    ret = xaf_comp_get_config(p_comp, 3, &param[0]);
    if(ret < 0)
        return ret;

    comp_format->channels = param[1];
    comp_format->pcm_width = param[3];
    comp_format->sample_rate = param[5];
#endif //PACK_WS_DUMMY
    return 0;
}

void fio_quit()
{
    return;
}

int main_task(int argc, char **argv)
{
    void *p_adev = NULL;
    void *p_decoder = NULL;
    void *p_input, *p_output;
    xf_thread_t dec_thread;
#ifdef XAF_HOSTED_AP
    unsigned char dec_stack[STACK_SIZE] __attribute__ ((aligned(XF_THREAD_STACK_ALIGN)));
#else /* XAF_HOSTED_AP */
    unsigned char dec_stack[STACK_SIZE];
#endif /* XAF_HOSTED_AP */

    xaf_comp_status dec_status;
    uVOID dec_info[4];
    char *filename_ptr;
    void *dec_thread_args[NUM_THREAD_ARGS];
    FILE *fp, *ofp;
    void *dec_inbuf[2];
#ifndef XA_DEC_INIT_WO_INP_TEST
    int buf_length = XAF_INBUF_SIZE;
#else
    /* ... for internal regression testing */
    int buf_length = 100;
#endif
    int read_length;
    int i;
    xaf_comp_type comp_type;
    xf_id_t dec_id;
    int (*dec_setup)(void *p_comp);
    const char *ext;

    xaf_format_t dec_format;
    int num_comp;
    pUWORD8 ver_info[3] = {0,0,0};    //{ver,lib_rev,api_rev}
    unsigned short board_id = 0;
    mem_obj_t* mem_handle;


#ifdef XAF_PROFILE
    frmwk_cycles = 0;
    fread_cycles = 0;
    fwrite_cycles = 0;
    dec_cycles = 0;
    dsp_comps_cycles = 0;
    tot_cycles = 0;
    num_bytes_read = 0;
    num_bytes_write = 0;
#endif


    memset(&dec_format, 0, sizeof(xaf_format_t));
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
    TST_CHK_API(print_verinfo(ver_info,(pUWORD8)"\'Mp3 Decoder\'"), "print_verinfo");

    /* ...initialize tracing facility */
    TRACE_INIT("Xtensa Audio Framework - \'Mp3 Decoder\' Sample App");

    /* ...check input arguments */
    if (argc != 3)
    {
        PRINT_USAGE;
        return -1;
    }

    if(NULL != strstr(argv[1], "-infile:"))
    {
        filename_ptr = (char *)&(argv[1][8]);
        ext = strrchr(argv[1], '.');
        if(ext!=NULL)
        {
            ext++;
            if (!strcmp(ext, "mp3")) {
                dec_id    = "audio-decoder/mp3";
                dec_setup = mp3_setup;
            }
            else {
                FIO_PRINTF(stderr, "Unknown Decoder Extension '%s'\n", ext);
                return -1;
            }
		}
        else
        {
            FIO_PRINTF(stderr, "Failed to open infile\n");
            return -1;
        }

        /* ...open file */
        if ((fp = fio_fopen(filename_ptr, "rb")) == NULL)
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

    if(NULL != strstr(argv[2], "-outfile:"))
    {
        filename_ptr = (char *)&(argv[2][9]);
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

    p_input  = fp;
    p_output = ofp;

    mem_handle = mem_init();

    xaf_adev_config_t adev_config;
    TST_CHK_API(xaf_adev_config_default_init(&adev_config), "xaf_adev_config_default_init");

    adev_config.audio_framework_buffer_size[XAF_MEM_ID_DEV] =  audio_frmwk_buf_size;
    adev_config.audio_component_buffer_size[XAF_MEM_ID_COMP] = audio_comp_buf_size;
    adev_config.core = XF_CORE_ID_MASTER;
#if (XF_CFG_CORES_NUM>1) && !defined(XAF_HOSTED_AP)
    adev_config.audio_shmem_buffer_size = XF_SHMEM_SIZE - audio_frmwk_buf_size*(1 + XAF_MEM_ID_DEV_MAX);
    adev_config.pshmem_dsp = shared_mem;
#endif //(XF_CFG_CORES_NUM>1)
    TST_CHK_API_ADEV_OPEN(p_adev, adev_config,  "xaf_adev_open");

    FIO_PRINTF(stdout, "Audio Device Ready\n");

    /* ...create decoder component */
    comp_type = XAF_DECODER;
    TST_CHK_API_COMP_CREATE(p_adev, XF_CORE_ID, &p_decoder, dec_id, 2, 1, &dec_inbuf[0], comp_type, "xaf_comp_create");
    TST_CHK_API(dec_setup(p_decoder), "dec_setup");

    /* ...start decoder component */
    TST_CHK_API(xaf_comp_process(p_adev, p_decoder, NULL, 0, XAF_START_FLAG), "xaf_comp_process");

    /* ...feed input to decoder component */
    for (i=0; i<2; i++)
    {
        TST_CHK_API(read_input(dec_inbuf[i], buf_length, &read_length, p_input, comp_type), "read_input");

        if (read_length)
            TST_CHK_API(xaf_comp_process(p_adev, p_decoder, dec_inbuf[i], read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
        else
        {
            TST_CHK_API(xaf_comp_process(p_adev, p_decoder, NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
            break;
        }
    }

    /* ...initialization loop */
    while (1)
    {
        TST_CHK_API(xaf_comp_get_status(p_adev, p_decoder, &dec_status, &dec_info[0]), "xaf_comp_get_status");

        if (dec_status == XAF_INIT_DONE || dec_status == XAF_EXEC_DONE) break;

        if (dec_status == XAF_NEED_INPUT)
        {
            void *p_buf = (void *) dec_info[0];
            int size    = dec_info[1];

            TST_CHK_API(read_input(p_buf, size, &read_length, p_input, comp_type), "read_input");

            if (read_length)
                TST_CHK_API(xaf_comp_process(p_adev, p_decoder, p_buf, read_length, XAF_INPUT_READY_FLAG), "xaf_comp_process");
            else
            {
                TST_CHK_API(xaf_comp_process(p_adev, p_decoder, NULL, 0, XAF_INPUT_OVER_FLAG), "xaf_comp_process");
                break;
            }
        }
    }

    if (dec_status != XAF_INIT_DONE)
    {
        FIO_PRINTF(stderr, "Failed to init");
        TST_CHK_API(ADEV_CLOSE_SIGNAL, "Stream Initialization");
    }

    TST_CHK_API(get_comp_config(p_decoder, &dec_format), "get_comp_config");

#ifdef XAF_PROFILE
    clk_start();

#endif

    dec_thread_args[0]= p_adev;
    dec_thread_args[1]= p_decoder;
    dec_thread_args[2]= p_input;
    dec_thread_args[3]= p_output;
    dec_thread_args[4]= &comp_type;
    dec_thread_args[5]= (void *)dec_id;
    dec_thread_args[6]= (void *)&i; //dummy

    /* ...init done, begin execution thread */
    __xf_thread_create(&dec_thread, comp_process_entry, &dec_thread_args[0], "Decoder Thread", dec_stack, STACK_SIZE, XAF_APP_THREADS_PRIORITY);

    __xf_thread_join(&dec_thread, NULL);

#ifdef XAF_PROFILE
    compute_total_frmwrk_cycles();
    clk_stop();

#endif

    {
        /* collect memory stats before closing the device */
        dsp_stats(p_adev, &adev_config);
    }
    /* ...exec done, clean-up */
    __xf_thread_destroy(&dec_thread);
    TST_CHK_API(xaf_comp_delete(p_decoder), "xaf_comp_delete");
    TST_CHK_API_ADEV_CLOSE(p_adev, XAF_ADEV_NORMAL_CLOSE, adev_config, "xaf_adev_close");
    FIO_PRINTF(stdout,"Audio device closed\n\n");

    mem_exit();

#ifdef XAF_PROFILE
    dsp_mcps = compute_comp_mcps(num_bytes_write, dsp_comps_cycles, dec_format, &strm_duration);

#endif
    TST_CHK_API(print_mem_mcps_info(mem_handle, num_comp), "print_mem_mcps_info");

    if (fp)  fio_fclose(fp);
    if (ofp) fio_fclose(ofp);

    fio_quit();

    /* ...deinitialize tracing facility */
    TRACE_DEINIT();

    return 0;
}

