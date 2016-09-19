/******************************************************************************
** Copyright (c) 2016, Intel Corporation                                     **
** All rights reserved.                                                      **
**                                                                           **
** Redistribution and use in source and binary forms, with or without        **
** modification, are permitted provided that the following conditions        **
** are met:                                                                  **
** 1. Redistributions of source code must retain the above copyright         **
**    notice, this list of conditions and the following disclaimer.          **
** 2. Redistributions in binary form must reproduce the above copyright      **
**    notice, this list of conditions and the following disclaimer in the    **
**    documentation and/or other materials provided with the distribution.   **
** 3. Neither the name of the copyright holder nor the names of its          **
**    contributors may be used to endorse or promote products derived        **
**    from this software without specific prior written permission.          **
**                                                                           **
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS       **
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT         **
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR     **
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT      **
** HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,    **
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED  **
** TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR    **
** PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF    **
** LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING      **
** NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS        **
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.              **
******************************************************************************/
/* Alexander Heinecke (Intel Corp.), Hans Pabst (Intel Corp.)
******************************************************************************/
#include "libxsmm_dnn_conv_fwd_custom_custom.h"

LIBXSMM_INLINE LIBXSMM_RETARGETABLE void internal_convolve_st_fwd_custom_custom_fp32_fallback(libxsmm_dnn_conv_handle* handle, int start_thread, int tid, int num_threads)
{
  typedef float element_input_type;
  typedef float element_output_type;
  typedef float element_filter_type;
# include "template/libxsmm_dnn_convolve_st_fwd_custom_custom_fallback.tpl.c"
}

LIBXSMM_INLINE LIBXSMM_RETARGETABLE void internal_convolve_st_fwd_custom_custom_int16_fallback(libxsmm_dnn_conv_handle* handle, int start_thread, int tid, int num_threads)
{
  typedef short element_input_type;
  typedef int element_output_type;
  typedef short element_filter_type;
# include "template/libxsmm_dnn_convolve_st_fwd_custom_custom_fallback.tpl.c"
}

LIBXSMM_INLINE LIBXSMM_RETARGETABLE void internal_convolve_st_fwd_custom_custom_fp32_opt(libxsmm_dnn_conv_handle* handle, int start_thread, int tid, int num_threads)
{
  int imgofm1, img, ofm1, ifm1, oj, ij, oi, ii;
  /* computing first logical thread */
  const int ltid = tid-start_thread;
  /* number of tasks that could be run in parallel */
  const int work = handle->desc.N*handle->blocksofm;
  /* compute chunck size */
  const int chunksize = (work % num_threads == 0) ? (work / num_threads) : (work / num_threads) + 1;
  /* compute thr_begin and thr_end */
  const int thr_begin = (ltid * chunksize < work) ? (ltid * chunksize) : work;
  const int thr_end = ((ltid + 1) * chunksize < work) ? ((ltid + 1) * chunksize) : work;
  libxsmm_sconvfunction jitted_sconv_fp_noweight_pf = handle->code_fwd[1].sconv;
  libxsmm_sconvfunction jitted_sconv_fp_weight_pf = handle->code_fwd[2].sconv;
  /*libxsmm_sconvfunction jitted_sconv_fp_weightnooutput_pf = handle->code_fwd[3].sconv;*/
#if defined(LIBXSMM_CONV_NO_PREFETCH)
  libxsmm_sconvfunction jitted_sconv_fp_no_pf = handle->code_fwd[0].sconv;
#endif
  const float *l_input, *l_wt;
  float* l_output;

  float *const out = ((float*)handle->output->data) + (handle->desc.pad_h_out * handle->ofwp + handle->desc.pad_w_out) * handle->ofmblock;
  LIBXSMM_VLA_DECL(5, float, output, out, handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock);
  LIBXSMM_VLA_DECL(5, const float, input, (float*)handle->input->data, handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
  LIBXSMM_VLA_DECL(6, const float, weight, (float*)handle->filter->data, handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock);

  for (imgofm1 = thr_begin; imgofm1 < thr_end; ++imgofm1) {
    img = imgofm1/handle->blocksofm;
    ofm1 = imgofm1%handle->blocksofm;
    for (ifm1 = 0; ifm1 < handle->blocksifm; ++ifm1) {
      for (oj = 0; oj < handle->ofh; oj += handle->fwd_ofh_rb) {
        ij = oj * handle->desc.u;
        for (oi = 0; oi < handle->ofw; oi += handle->fwd_ofw_rb) {
          ii = oi * handle->desc.v;
          l_input  = &LIBXSMM_VLA_ACCESS(5, input, img, ifm1, ij, ii, 0,
                      handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
          l_wt     = &LIBXSMM_VLA_ACCESS(6, weight, ofm1, ifm1, 0, 0, 0, 0,
                      handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock);
          l_output = &LIBXSMM_VLA_ACCESS(5, output, img, ofm1, oj, oi, 0,
                      handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock);
#if !defined(LIBXSMM_CONV_NO_PREFETCH)
          /* check we are not at the end */
          if (oj < handle->ofh-handle->fwd_ofh_rb) {
            jitted_sconv_fp_noweight_pf(l_input, l_wt, l_output,
              &LIBXSMM_VLA_ACCESS(5, input, img, ifm1, (oj + handle->fwd_ofh_rb) * handle->desc.u, ii, 0,
                                     handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), NULL,
              &LIBXSMM_VLA_ACCESS(5, output, img, ofm1, oj + handle->fwd_ofh_rb, oi, 0,
                                     handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock));
          }
          else {
            if ((ofm1+1 == handle->blocksofm) &&  (ifm1+1 == handle->blocksifm)) {
              jitted_sconv_fp_weight_pf(l_input, l_wt, l_output,
                &LIBXSMM_VLA_ACCESS(5, input, img + 1, 0, 0, 0, 0,
                  handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock),
                &LIBXSMM_VLA_ACCESS(6, weight, 0, 0, 0, 0, 0, 0,
                  handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock),
                &LIBXSMM_VLA_ACCESS(5, output, img + 1, 0, 0, 0, 0,
                  handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock));
            }
            else {
              if ((ifm1+1 == handle->blocksifm)) {
                jitted_sconv_fp_weight_pf(l_input, l_wt, l_output,
                  &LIBXSMM_VLA_ACCESS(5, input, img, 0, 0, 0, 0,
                    handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock),
                  &LIBXSMM_VLA_ACCESS(6, weight, ofm1 + 1, 0, 0, 0, 0, 0,
                    handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock),
                  &LIBXSMM_VLA_ACCESS(5, output, img, ofm1 + 1, 0, 0, 0,
                    handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock));
              }
              else {
                jitted_sconv_fp_weight_pf(l_input, l_wt, l_output,
                  &LIBXSMM_VLA_ACCESS(5, input, ifm1 + 1, 0, 0, 0, 0,
                    handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock),
                  &LIBXSMM_VLA_ACCESS(6, weight, ofm1, ifm1 + 1, 0, 0, 0, 0,
                    handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock),
                  &LIBXSMM_VLA_ACCESS(5, output, img, ofm1, 0, 0, 0,
                    handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock));
              }
            }
          }
#else
          jitted_sconv_fp_no_pf(l_input, l_wt, l_output, NULL, NULL, NULL);
#endif
        }
      }
    }
  }
}


LIBXSMM_INLINE LIBXSMM_RETARGETABLE void internal_convolve_st_fwd_custom_custom_fp32_img_parallel_opt(libxsmm_dnn_conv_handle* handle, int start_thread, int tid, int num_threads)
{
  int ifm1, oj, ij, oi, ii;
  /* calculate local thread ids */
  const int ltid = tid - start_thread;
  /* calculate group sizes */
  const int l_l1 = handle->desc.N * handle->blocksofm;
  const int l_l3 = handle->ofh / handle->fwd_ofh_rb;
  /* number of threads need in the ofh loop (as we have l_l1 global parallel tasks) */
  const int l_l1_gs = num_threads / l_l1;
  /* number of elemens of ofh loop per thread */
  const int l_l2_ts = (l_l3 % l_l1_gs == 0) ? (l_l3 / l_l1_gs) : ((l_l3 / l_l1_gs) + 1);
  /* get group id */
  const int l_tidgroup = ltid / l_l1_gs;
  /* compute img and ofm1 based on group */
  const int img = l_tidgroup / handle->blocksofm;
  const int ofm1 = l_tidgroup % handle->blocksofm;
  int start_ofh = l_l2_ts * (ltid % l_l1_gs);
  const int end_ofh = ((start_ofh + l_l2_ts) <= handle->ofh) ? (start_ofh + l_l2_ts) : handle->ofh;
  libxsmm_sconvfunction jitted_sconv_fp_noweight_pf = handle->code_fwd[1].sconv;
  libxsmm_sconvfunction jitted_sconv_fp_weight_pf = handle->code_fwd[2].sconv;
  /*libxsmm_sconvfunction jitted_sconv_fp_weightnooutput_pf = handle->code_fwd[3].sconv;*/
#if defined(LIBXSMM_CONV_NO_PREFETCH)
  libxsmm_sconvfunction jitted_sconv_fp_no_pf = handle->code_fwd[0].sconv;
#endif
  const float *l_input, *l_wt;
  float* l_output;

  float *const out = ((float*)handle->output->data) + (handle->desc.pad_h_out * handle->ofwp + handle->desc.pad_w_out) * handle->ofmblock;
  LIBXSMM_VLA_DECL(5, float, output, out, handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock);
  LIBXSMM_VLA_DECL(5, const float, input, (float*)handle->input->data, handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
  LIBXSMM_VLA_DECL(6, const float, weight, (float*)handle->filter->data, handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock);

  /* avoid ouf of bounds (dirty) */
  start_ofh = (img < handle->desc.N && ofm1 < handle->blocksofm) ? start_ofh : handle->ofh;
  for (ifm1 = 0; ifm1 < handle->blocksifm; ++ifm1) {
    for (oj = start_ofh; oj < end_ofh; oj += handle->fwd_ofh_rb) {
      ij = oj * handle->desc.u;
      for (oi = 0; oi < handle->ofw; oi += handle->fwd_ofw_rb) {
        ii = oi * handle->desc.v;
        l_input  = &LIBXSMM_VLA_ACCESS(5, input, img, ifm1, ij, ii, 0,
                    handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock);
        l_wt     = &LIBXSMM_VLA_ACCESS(6, weight, ofm1, ifm1, 0, 0, 0, 0,
                    handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock);
        l_output = &LIBXSMM_VLA_ACCESS(5, output, img, ofm1, oj, oi, 0,
                    handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock);
#if !defined(LIBXSMM_CONV_NO_PREFETCH)
        /* check we are not at the end, we prefetch inside the image */
        if (oi < handle->ofw-handle->fwd_ofw_rb) {
          jitted_sconv_fp_noweight_pf(l_input, l_wt, l_output,
            &LIBXSMM_VLA_ACCESS(5, input, img, ifm1, ij, (oi + handle->fwd_ofw_rb) * handle->desc.v, 0,
              handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), NULL,
            &LIBXSMM_VLA_ACCESS(5, output, img, ofm1, oj, oi + handle->fwd_ofw_rb, 0,
              handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock));
        }
        else {
          if (oj < end_ofh-handle->fwd_ofh_rb) {
            jitted_sconv_fp_noweight_pf(l_input, l_wt, l_output,
              &LIBXSMM_VLA_ACCESS(5, input, img, ifm1, (oj + handle->fwd_ofw_rb) * handle->desc.u, ii, 0,
                handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock), NULL,
              &LIBXSMM_VLA_ACCESS(5, output, img, ofm1, oj + handle->fwd_ofw_rb, oi, 0,
                handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock));
          }
          else {
            jitted_sconv_fp_weight_pf(l_input, l_wt, l_output,
              &LIBXSMM_VLA_ACCESS(5, input, img, ifm1 + 1, 0, 0, 0,
                handle->blocksifm, handle->ifhp, handle->ifwp, handle->ifmblock),
              &LIBXSMM_VLA_ACCESS(6, weight, ofm1, ifm1 + 1, 0, 0, 0, 0,
                handle->blocksifm, handle->desc.R, handle->desc.S, handle->ifmblock, handle->ofmblock),
              &LIBXSMM_VLA_ACCESS(5, output, img, ofm1, 0, 0, 0,
                handle->blocksofm, handle->ofhp, handle->ofwp, handle->ofmblock));
          }
        }
#else
        jitted_sconv_fp_no_pf(l_input, l_wt, l_output, NULL, NULL, NULL);
#endif
      }
    }
  }
}


LIBXSMM_API_DEFINITION libxsmm_dnn_err_t libxsmm_dnn_convolve_st_fwd_custom_custom(libxsmm_dnn_conv_handle* handle, int start_thread, int tid, int num_threads)
{
  libxsmm_dnn_err_t status = LIBXSMM_DNN_SUCCESS;

  /* check if we have input, output and filter */
  if (handle->input == 0 || handle->output == 0 || handle->filter == 0) {
    status = LIBXSMM_DNN_ERR_DATA_NOT_BOUND;
    return status;
  }

  /* check if we have a kernel JITed */
  if (handle->code_fwd[0].sconv == 0) {
    if (1 == handle->desc.splits) {
      switch (handle->datatype) {
        case LIBXSMM_DNN_DATATYPE_FP32: { 
          internal_convolve_st_fwd_custom_custom_fp32_fallback(handle, start_thread, tid, num_threads);
        } break;
        case LIBXSMM_DNN_DATATYPE_INT16: { 
          internal_convolve_st_fwd_custom_custom_int16_fallback(handle, start_thread, tid, num_threads);
        } break;
        default: {
          status = LIBXSMM_DNN_ERR_UNSUPPORTED_DATATYPE;
          return status;
        }
      }
    } else {
      status = LIBXSMM_DNN_ERR_GENERAL;
      return status;
    }
  }
  else {
    if (1 == handle->desc.splits) {
      switch (handle->datatype) {
        case LIBXSMM_DNN_DATATYPE_FP32: {
          if (handle->desc.N*handle->blocksofm >= num_threads) {
            internal_convolve_st_fwd_custom_custom_fp32_opt(handle, start_thread, tid, num_threads);
          }
          else {
            internal_convolve_st_fwd_custom_custom_fp32_img_parallel_opt(handle, start_thread, tid, num_threads);
          }
        } break;
        default: {
          status = LIBXSMM_DNN_ERR_UNSUPPORTED_DATATYPE;
          return status;
        }
      }
    } else {
      status = LIBXSMM_DNN_ERR_GENERAL;
      return status;
    }
  }

  return status;
}
