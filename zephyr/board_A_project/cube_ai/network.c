/**
  ******************************************************************************
  * @file    network.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-04-30T09:26:14+0000
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#include "ai_lite_inspect.h"
#include "ai_platform_interface.h"
#include "layers.h"
#include "core_convert.h"
#include "network.h"
#include "network_details.h"
#include "network_data.h"
#include "stai_events.h"

#include "ai_lite_inspect.h"

#include "lite_operators.h"
/*****************************************************************************/
#define STAI_INTERNAL_API_MAJOR               (1)
#define STAI_INTERNAL_API_MINOR               (0)
#define STAI_INTERNAL_API_MICRO               (0)

#define STAI_MAGIC                            (0xB1C00100)

/*****************************************************************************/
#define _STAI_CONCAT_ARG(a, b)     a ## b
#define STAI_CONCAT(a, b)         _STAI_CONCAT_ARG(a, b)

/*!  STAI_CAST SECTION                       *********************************/
#define STAI_CAST(type, expr) \
  ((type)(expr))


/*****************************************************************************/
#define STAI_SIZE(_size) \
  ((stai_size)(_size))

/*****************************************************************************/
#define STAI_INIT_BUFFER(_flags, _size, _address) \
  { \
    .size = (_size), \
    .address = (uintptr_t)(_address), \
    .flags = (_flags), \
  }

#define STAI_INIT_TENSOR(_name, _flags, _fmt, _size_bytes, _shape, _scale, _zeropoint) \
  { \
    .size_bytes = (_size_bytes), \
    .flags = (_flags), \
    .format = (stai_format)(_fmt), \
    .shape = STAI_PACK(_shape), \
    .scale = STAI_PACK(_scale), \
    .zeropoint = STAI_PACK(_zeropoint), \
    .name = (_name) \
  }

#define STAI_INIT_ARRAY(_size, _ptr) \
  { .size = STAI_SIZE(_size), .data = STAI_PACK(_ptr) }


#define STAI_CAST_ARRAY(_type, _size, _ptr) \
  { .size = STAI_SIZE(_size), .data = (_type)STAI_PACK(_ptr) }


#define STAI_DECLARE_ARRAY(_type, _size, ...) \
  { .size = STAI_SIZE(_size), .data = (_type[_size]) { STAI_PACK(__VA_ARGS__) } }


#define STAI_EMPTY_ARRAY() \
  { .size = 0, .data = NULL }


#define STAI_INIT_VERSION(_major, _minor, _micro) \
  { .major = (_major), .minor = (_minor), .micro = (_micro), .reserved = 0x0 }

/*****************************************************************************/
/**  Getters and setters  **/

#define STAI_GET_ARRAY_SIZE(nd_array) \
  (nd_array.size)


#define STAI_GET_ARRAY_ELEM(nd_array, pos) \
  (nd_array.data[(pos)])

#define _STAI_SET_ERROR(net_ctx, cond, value, exit) { \
  if (!(net_ctx)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE; } \
  if (((uintptr_t)net_ctx) & (_STAI_CONTEXT_ALIGNMENT-1)) { return STAI_ERROR_NETWORK_INVALID_CONTEXT_ALIGNMENT; } \
  if (((value) >= STAI_ERROR_GENERIC) && (cond)) { \
    if ((net_ctx)->_return_code == STAI_SUCCESS) { \
      (net_ctx)->_return_code = (value); \
    } \
    return (exit); \
  } \
}

/*****************************************************************************/
/* TODO REMOVE THESE TWO MACROS */
#define STAI_EVENT_NODE_START_CB
#define STAI_EVENT_NODE_STOP_CB

#ifdef STAI_EVENT_NODE_START_CB
#ifndef _STAI_NETWORK_EVENT_NODE_START_CB
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _start_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(const stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_START, (const void*)&_start_event); \
  }
#endif
#else
  #define _STAI_NETWORK_EVENT_NODE_START_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_START_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_START_CB */

#ifdef STAI_EVENT_NODE_STOP_CB
#ifndef _STAI_NETWORK_EVENT_NODE_STOP_CB
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
  if (net_ctx->_callback) { \
    const stai_event_node_start_stop _stop_event = { \
      .node_id=(_node_id), \
      .buffers={ \
        .size=(_buffers_size), \
        .data=(stai_ptr const*)(stai_ptr[_buffers_size])STAI_PACK(__VA_ARGS__) \
      } \
    }; \
    net_ctx->_callback(net_ctx->_callback_cookie, STAI_EVENT_NODE_STOP, (const void*)&_stop_event); \
  }
#endif
#else
  #define _STAI_NETWORK_EVENT_NODE_STOP_CB(_node_id, _buffers_size, ...) \
    do { /* _STAI_NETWORK_EVENT_NODE_STOP_CB() */ } while(0);
#endif      /* STAI_EVENT_NODE_STOP_CB */


/*****************************************************************************/
#define _STAI_NETWORK_MODEL_SIGNATURE     "0xd6c7bdbc8995fcf93fe86b3417f8ca81"
#define _STAI_NETWORK_DATETIME            "2026-04-30T09:26:14+0000"
#define _STAI_NETWORK_COMPILE_DATETIME    __DATE__ " " __TIME__

#define _STAI_CONTEXT_ALIGNMENT        STAI_NETWORK_CONTEXT_ALIGNMENT

/*****************************************************************************/
#define g_network_activations_1     (NULL)




#if defined(HAVE_NETWORK_INFO)
/*****************************************************************************/
static const stai_network_info g_network_info = {
  .model_signature = _STAI_NETWORK_MODEL_SIGNATURE,
  .c_compile_datetime = _STAI_NETWORK_COMPILE_DATETIME,
  .c_model_name = STAI_NETWORK_MODEL_NAME,
  .c_model_datetime = _STAI_NETWORK_DATETIME,
  .c_model_signature = 0x0,
  .runtime_version = STAI_INIT_VERSION(12, 0, 0),
  .tool_version = STAI_INIT_VERSION(4, 0, 0),
  .api_version = STAI_INIT_VERSION(1, 0, 0),
  .n_macc = STAI_NETWORK_MACC_NUM,
  .n_nodes = STAI_NETWORK_NODES_NUM,
  .flags = STAI_NETWORK_FLAGS,
  .n_inputs = STAI_NETWORK_IN_NUM,
  .n_outputs = STAI_NETWORK_OUT_NUM,
  .n_activations = STAI_NETWORK_ACTIVATIONS_NUM,
  .n_weights = STAI_NETWORK_WEIGHTS_NUM,
  .n_states = STAI_NETWORK_STATES_NUM,
  .inputs = (stai_tensor[STAI_NETWORK_IN_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_IN_1_NAME,
      STAI_NETWORK_IN_1_FLAGS,
      STAI_NETWORK_IN_1_FORMAT,
      STAI_NETWORK_IN_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 4, 1, 1, 192, 192),
      STAI_DECLARE_ARRAY(float, 1, 0.003921568859368563f),
      STAI_DECLARE_ARRAY(int16_t, 1, -128)),
    },
    .outputs = (stai_tensor[STAI_NETWORK_OUT_NUM]) {
    STAI_INIT_TENSOR(
      STAI_NETWORK_OUT_1_NAME,
      STAI_NETWORK_OUT_1_FLAGS,
      STAI_NETWORK_OUT_1_FORMAT,
      STAI_NETWORK_OUT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 4, 756),
      STAI_DECLARE_ARRAY(float, 1, 1.9618016481399536f),
      STAI_DECLARE_ARRAY(int16_t, 1, -125)),
    STAI_INIT_TENSOR(
      STAI_NETWORK_OUT_2_NAME,
      STAI_NETWORK_OUT_2_FLAGS,
      STAI_NETWORK_OUT_2_FORMAT,
      STAI_NETWORK_OUT_2_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 3, 1, 1, 756),
      STAI_DECLARE_ARRAY(float, 1, 0.0031456942670047283f),
      STAI_DECLARE_ARRAY(int16_t, 1, -128)),
    },
  .activations = (stai_tensor[STAI_NETWORK_ACTIVATIONS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_ACTIVATION_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_ACTIVATION_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 410112),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },
  .weights = (stai_tensor[STAI_NETWORK_WEIGHTS_NUM]) {
    STAI_INIT_TENSOR(
      (NULL),
      STAI_NETWORK_WEIGHT_1_FLAGS,
      STAI_FORMAT_U8,
      STAI_NETWORK_WEIGHT_1_SIZE_BYTES,
      STAI_DECLARE_ARRAY(int32_t, 1, 617296),
      STAI_EMPTY_ARRAY(),
      STAI_EMPTY_ARRAY()),
    },

  .states = NULL
};
#endif

#define _STAI_CONTEXT_ACQUIRE(_net_ctx, _net_handle) \
  _stai_network_context* _net_ctx = (_stai_network_context*)(_net_handle); \
  STAI_ASSERT(_net_ctx != NULL) \
  _STAI_SET_ERROR(_net_ctx, _net_ctx->_magic != STAI_MAGIC, \
                  STAI_ERROR_NETWORK_INVALID_CONTEXT_HANDLE, _net_ctx->_return_code)


/*****************************************************************************/
static
void _stai_network_check(_stai_network_context* net_ctx)
{
  stai_size idx;

// Check activations status
  for (idx=0; idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    if (net_ctx->_activations[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_ACTIVATIONS_NUM) ? STAI_FLAG_ACTIVATIONS : STAI_FLAG_NONE;
// Check inputs status
  for (idx=0; idx<STAI_NETWORK_IN_NUM; idx++) {
    if (net_ctx->_inputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_IN_NUM) ? STAI_FLAG_INPUTS : STAI_FLAG_NONE;

  // Check outputs status
  for (idx=0; idx<STAI_NETWORK_OUT_NUM; idx++) {
    if (net_ctx->_outputs[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_OUT_NUM) ? STAI_FLAG_OUTPUTS : STAI_FLAG_NONE;

// Check weights status
  for (idx=0; idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    if (net_ctx->_weights[idx] == NULL) break;
  }
  net_ctx->_flags |= (idx == STAI_NETWORK_WEIGHTS_NUM) ? STAI_FLAG_WEIGHTS : STAI_FLAG_NONE;
STAI_PRINT("  [_stai_network_check] flags: 0x%08x\n", net_ctx->_flags)
}


/*****************************************************************************/
STAI_API_ENTRY
stai_return_code stai_network_init(
  stai_network* network)
{
  /* Memory where to store internal context is provided by applications as a raw byte buffer */
  _stai_network_context* net_ctx = (_stai_network_context*)(network);
  net_ctx->_return_code = STAI_SUCCESS;
  STAI_PRINT("[Entering Network Init] network(%p) context_size(%d)\n", net_ctx, (int32_t)sizeof(_stai_network_context))

  _STAI_SET_ERROR(net_ctx, STAI_NETWORK_CONTEXT_SIZE != sizeof(_stai_network_context),
                 STAI_ERROR_NETWORK_INVALID_CONTEXT_SIZE, net_ctx->_return_code)

  {
    const _stai_network_context _network_context = {
      ._magic = STAI_MAGIC,
      ._signature = STAI_NETWORK_MODEL_SIGNATURE,
      ._flags = STAI_NETWORK_FLAGS,
      ._return_code = STAI_SUCCESS,
      ._callback = NULL,
      ._callback_cookie = NULL,
      ._activations = {
      (stai_ptr)g_network_activations_1
      },
      ._weights = {
      (stai_ptr)g_network_weights_array
      },
      ._inputs = {
    NULL},
      ._outputs = {
    NULL,NULL},
    };

    // Deep copy of internal context to opaque buffer provided by app
    *net_ctx = _network_context;

    _stai_network_check(net_ctx);
  }

  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_deinit(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /*  Reset flags to initial state  */
  net_ctx->_flags = STAI_NETWORK_FLAGS;
  return net_ctx->_return_code;
}

/*****************************************************************************/



/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_0_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.7047433853149414f),
    AI_PACK_INTQ_ZP(-5)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_0_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_0_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.3665452003479004f),
    AI_PACK_INTQ_ZP(-127)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.24923212826251984f),
    AI_PACK_INTQ_ZP(48)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07827641069889069f),
    AI_PACK_INTQ_ZP(-124)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2619330883026123f),
    AI_PACK_INTQ_ZP(43)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08771698921918869f),
    AI_PACK_INTQ_ZP(-125)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_Split_output_0_output0_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08771698921918869f),
    AI_PACK_INTQ_ZP(-125)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_Split_output_0_output1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08771698921918869f),
    AI_PACK_INTQ_ZP(-125)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_m_0_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10575424879789352f),
    AI_PACK_INTQ_ZP(5)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_m_0_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921558149158955f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_m_0_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.051501937210559845f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #14 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_m_0_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.13174638152122498f),
    AI_PACK_INTQ_ZP(31)))

/* Int quant #15 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_m_0_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921555820852518f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #16 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_m_0_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05070328712463379f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #17 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_m_0_Add_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07474589347839355f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #18 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08880893141031265f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #19 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.14203006029129028f),
    AI_PACK_INTQ_ZP(22)))

/* Int quant #20 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0039215669967234135f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #21 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_2_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05937632918357849f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #22 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_3_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10724584013223648f),
    AI_PACK_INTQ_ZP(-6)))

/* Int quant #23 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_3_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0039215655997395515f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #24 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_3_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05681946128606796f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #25 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10862993448972702f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #26 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921567462384701f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #27 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05976736173033714f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #28 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_Split_output_0_output0_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05976736173033714f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #29 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_Split_output_0_output1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05976736173033714f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #30 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_0_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10748530179262161f),
    AI_PACK_INTQ_ZP(-3)))

/* Int quant #31 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_0_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921564668416977f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #32 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_0_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05574588105082512f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #33 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_0_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10500090569257736f),
    AI_PACK_INTQ_ZP(16)))

/* Int quant #34 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_0_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921534400433302f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #35 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_0_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.046805787831544876f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #36 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_0_Add_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04983390122652054f),
    AI_PACK_INTQ_ZP(-117)))

/* Int quant #37 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_1_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09995905309915543f),
    AI_PACK_INTQ_ZP(39)))

/* Int quant #38 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_1_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003920987248420715f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #39 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_1_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03565911203622818f),
    AI_PACK_INTQ_ZP(-120)))

/* Int quant #40 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_1_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10154405236244202f),
    AI_PACK_INTQ_ZP(-20)))

/* Int quant #41 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_1_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921567462384701f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #42 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_1_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.059788238257169724f),
    AI_PACK_INTQ_ZP(-123)))

/* Int quant #43 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_m_1_Add_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06870301067829132f),
    AI_PACK_INTQ_ZP(-116)))

/* Int quant #44 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06870301067829132f),
    AI_PACK_INTQ_ZP(-116)))

/* Int quant #45 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0986211895942688f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #46 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0039215534925460815f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #47 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_4_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05003619194030762f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #48 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_5_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07586664706468582f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #49 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_5_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921307623386383f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #50 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_5_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03880762308835983f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #51 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07475388795137405f),
    AI_PACK_INTQ_ZP(-16)))

/* Int quant #52 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0039214822463691235f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #53 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04313071444630623f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #54 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_Split_output_0_output0_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04313071444630623f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #55 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_Split_output_0_output1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04313071444630623f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #56 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_0_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07193232327699661f),
    AI_PACK_INTQ_ZP(-6)))

/* Int quant #57 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_0_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921288065612316f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #58 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_0_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03852131962776184f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #59 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_0_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05817737057805061f),
    AI_PACK_INTQ_ZP(7)))

/* Int quant #60 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_0_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0039179506711661816f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #61 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_0_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02846820279955864f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #62 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_0_Add_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.030677510425448418f),
    AI_PACK_INTQ_ZP(-110)))

/* Int quant #63 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_1_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.057439856231212616f),
    AI_PACK_INTQ_ZP(11)))

/* Int quant #64 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_1_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003916566725820303f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #65 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_1_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.027188777923583984f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #66 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_1_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06489749997854233f),
    AI_PACK_INTQ_ZP(11)))

/* Int quant #67 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_1_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003919501788914204f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #68 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_1_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.030675169080495834f),
    AI_PACK_INTQ_ZP(-119)))

/* Int quant #69 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_m_1_Add_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.044514771550893784f),
    AI_PACK_INTQ_ZP(-109)))

/* Int quant #70 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04531298577785492f),
    AI_PACK_INTQ_ZP(-110)))

/* Int quant #71 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06418560445308685f),
    AI_PACK_INTQ_ZP(1)))

/* Int quant #72 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003920334856957197f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #73 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_6_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.032705314457416534f),
    AI_PACK_INTQ_ZP(-119)))

/* Int quant #74 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_7_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05027378350496292f),
    AI_PACK_INTQ_ZP(-13)))

/* Int quant #75 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_7_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003918128088116646f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #76 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_7_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02866680547595024f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #77 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04314478114247322f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #78 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003911545965820551f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #79 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.024431629106402397f),
    AI_PACK_INTQ_ZP(-117)))

/* Int quant #80 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_Split_output_0_output0_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.024431629106402397f),
    AI_PACK_INTQ_ZP(-117)))

/* Int quant #81 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_Split_output_0_output1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.024431629106402397f),
    AI_PACK_INTQ_ZP(-117)))

/* Int quant #82 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_m_0_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.030549263581633568f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #83 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_m_0_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003851796966046095f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #84 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_m_0_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01654183864593506f),
    AI_PACK_INTQ_ZP(-111)))

/* Int quant #85 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_m_0_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.037787217646837234f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #86 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_m_0_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0039005011785775423f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #87 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_m_0_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.021457040682435036f),
    AI_PACK_INTQ_ZP(-115)))

/* Int quant #88 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_m_0_Add_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.029858563095331192f),
    AI_PACK_INTQ_ZP(-109)))

/* Int quant #89 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.029858563095331192f),
    AI_PACK_INTQ_ZP(-109)))

/* Int quant #90 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04047897085547447f),
    AI_PACK_INTQ_ZP(5)))

/* Int quant #91 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0038941064849495888f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #92 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_8_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02038503624498844f),
    AI_PACK_INTQ_ZP(-114)))

/* Int quant #93 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_9_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.040920440107584f),
    AI_PACK_INTQ_ZP(-16)))

/* Int quant #94 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_9_m_MaxPool_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.040920440107584f),
    AI_PACK_INTQ_ZP(-16)))

/* Int quant #95 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_9_m_1_MaxPool_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.040920440107584f),
    AI_PACK_INTQ_ZP(-16)))

/* Int quant #96 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_9_m_2_MaxPool_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.040920440107584f),
    AI_PACK_INTQ_ZP(-16)))

/* Int quant #97 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_9_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.040920440107584f),
    AI_PACK_INTQ_ZP(-16)))

/* Int quant #98 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_9_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03179952874779701f),
    AI_PACK_INTQ_ZP(-6)))

/* Int quant #99 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_9_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0038661069702357054f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #100 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_9_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017500974237918854f),
    AI_PACK_INTQ_ZP(-112)))

/* Int quant #101 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_10_Resize_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017500974237918854f),
    AI_PACK_INTQ_ZP(-112)))

/* Int quant #102 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_11_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.032705314457416534f),
    AI_PACK_INTQ_ZP(-119)))

/* Int quant #103 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05183536186814308f),
    AI_PACK_INTQ_ZP(-10)))

/* Int quant #104 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003918407950550318f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #105 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02900150790810585f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #106 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_Split_output_0_output0_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02900150790810585f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #107 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_Split_output_0_output1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02900150790810585f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #108 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_m_0_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0595129132270813f),
    AI_PACK_INTQ_ZP(-6)))

/* Int quant #109 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_m_0_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003920156043022871f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #110 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_m_0_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03217185288667679f),
    AI_PACK_INTQ_ZP(-119)))

/* Int quant #111 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_m_0_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.055566348135471344f),
    AI_PACK_INTQ_ZP(12)))

/* Int quant #112 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_m_0_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003914950881153345f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #113 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_m_0_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.026080317795276642f),
    AI_PACK_INTQ_ZP(-117)))

/* Int quant #114 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02900150790810585f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #115 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.061407145112752914f),
    AI_PACK_INTQ_ZP(-10)))

/* Int quant #116 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003920723684132099f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #117 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_12_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03419308364391327f),
    AI_PACK_INTQ_ZP(-120)))

/* Int quant #118 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_13_Resize_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03419308364391327f),
    AI_PACK_INTQ_ZP(-120)))

/* Int quant #119 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_14_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05003619194030762f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #120 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.06764547526836395f),
    AI_PACK_INTQ_ZP(13)))

/* Int quant #121 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003919860813766718f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #122 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0314263254404068f),
    AI_PACK_INTQ_ZP(-119)))

/* Int quant #123 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_Split_output_0_output0_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0314263254404068f),
    AI_PACK_INTQ_ZP(-119)))

/* Int quant #124 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_Split_output_0_output1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0314263254404068f),
    AI_PACK_INTQ_ZP(-119)))

/* Int quant #125 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_m_0_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09491623193025589f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #126 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_m_0_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921546041965485f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #127 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_m_0_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0484776571393013f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #128 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_m_0_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08017320185899734f),
    AI_PACK_INTQ_ZP(-22)))

/* Int quant #129 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_m_0_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921542316675186f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #130 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_m_0_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.047851625829935074f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #131 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.047851625829935074f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #132 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08608002960681915f),
    AI_PACK_INTQ_ZP(-5)))

/* Int quant #133 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921522758901119f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #134 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_15_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.045634496957063675f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #135 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_16_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05743343010544777f),
    AI_PACK_INTQ_ZP(-33)))

/* Int quant #136 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_16_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00392116978764534f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #137 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_16_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03713902831077576f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #138 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_17_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03713902831077576f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #139 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05417117476463318f),
    AI_PACK_INTQ_ZP(-5)))

/* Int quant #140 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003918575122952461f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #141 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02921578288078308f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #142 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_Split_output_0_output0_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02921578288078308f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #143 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_Split_output_0_output1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02921578288078308f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #144 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_m_0_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04476507008075714f),
    AI_PACK_INTQ_ZP(-19)))

/* Int quant #145 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_m_0_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003915943671017885f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #146 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_m_0_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02672426402568817f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #147 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_m_0_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04318301007151604f),
    AI_PACK_INTQ_ZP(-22)))

/* Int quant #148 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_m_0_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003915372770279646f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #149 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_m_0_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.026341278105974197f),
    AI_PACK_INTQ_ZP(-117)))

/* Int quant #150 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02921578288078308f),
    AI_PACK_INTQ_ZP(-118)))

/* Int quant #151 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.053707871586084366f),
    AI_PACK_INTQ_ZP(-14)))

/* Int quant #152 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003919569309800863f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #153 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_18_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03080553002655506f),
    AI_PACK_INTQ_ZP(-119)))

/* Int quant #154 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_19_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03670447692275047f),
    AI_PACK_INTQ_ZP(19)))

/* Int quant #155 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_19_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0038475841283798218f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #156 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_19_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.016295142471790314f),
    AI_PACK_INTQ_ZP(-111)))

/* Int quant #157 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_20_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.017500974237918854f),
    AI_PACK_INTQ_ZP(-112)))

/* Int quant #158 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03340889886021614f),
    AI_PACK_INTQ_ZP(21)))

/* Int quant #159 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003811205504462123f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #160 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014591030776500702f),
    AI_PACK_INTQ_ZP(-109)))

/* Int quant #161 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_Split_output_0_output0_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014591030776500702f),
    AI_PACK_INTQ_ZP(-109)))

/* Int quant #162 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_Split_output_0_output1_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014591030776500702f),
    AI_PACK_INTQ_ZP(-109)))

/* Int quant #163 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_m_0_cv1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02495408244431019f),
    AI_PACK_INTQ_ZP(-1)))

/* Int quant #164 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_m_0_cv1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003767156507819891f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #165 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_m_0_cv1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.013126016594469547f),
    AI_PACK_INTQ_ZP(-107)))

/* Int quant #166 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_m_0_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020877396687865257f),
    AI_PACK_INTQ_ZP(-11)))

/* Int quant #167 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_m_0_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003713018726557493f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #168 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_m_0_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01178336888551712f),
    AI_PACK_INTQ_ZP(-104)))

/* Int quant #169 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014591030776500702f),
    AI_PACK_INTQ_ZP(-109)))

/* Int quant #170 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_cv2_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.025828802958130836f),
    AI_PACK_INTQ_ZP(-29)))

/* Int quant #171 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_cv2_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0038537438958883286f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #172 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_21_cv2_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.01666065864264965f),
    AI_PACK_INTQ_ZP(-111)))

/* Int quant #173 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02846970595419407f),
    AI_PACK_INTQ_ZP(-5)))

/* Int quant #174 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0038331751711666584f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #175 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_2_cv2_2_0_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015541751869022846f),
    AI_PACK_INTQ_ZP(-110)))

/* Int quant #176 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.026427119970321655f),
    AI_PACK_INTQ_ZP(-14)))

/* Int quant #177 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003828442422673106f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #178 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_2_cv2_2_1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015319498255848885f),
    AI_PACK_INTQ_ZP(-110)))

/* Int quant #179 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02233886532485485f),
    AI_PACK_INTQ_ZP(-39)))

/* Int quant #180 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0038271236699074507f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #181 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_2_cv3_2_0_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015259467996656895f),
    AI_PACK_INTQ_ZP(-110)))

/* Int quant #182 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.020661186426877975f),
    AI_PACK_INTQ_ZP(7)))

/* Int quant #183 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003620705334469676f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #184 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_2_cv3_2_1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010099491104483604f),
    AI_PACK_INTQ_ZP(-100)))

/* Int quant #185 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_2_cv3_2_2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10650981217622757f),
    AI_PACK_INTQ_ZP(127)))

/* Int quant #186 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_2_cv3_2_2_Conv_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.021715059876441956f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #187 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04314032196998596f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #188 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0039079622365534306f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #189 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_1_cv2_1_0_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.023212028667330742f),
    AI_PACK_INTQ_ZP(-116)))

/* Int quant #190 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.04184595122933388f),
    AI_PACK_INTQ_ZP(-23)))

/* Int quant #191 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.00391417508944869f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #192 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_1_cv2_1_1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.025640754029154778f),
    AI_PACK_INTQ_ZP(-117)))

/* Int quant #193 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03219177573919296f),
    AI_PACK_INTQ_ZP(-16)))

/* Int quant #194 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003882120130583644f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #195 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_1_cv3_1_0_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.018907619640231133f),
    AI_PACK_INTQ_ZP(-113)))

/* Int quant #196 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03759874030947685f),
    AI_PACK_INTQ_ZP(34)))

/* Int quant #197 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003806075779721141f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #198 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_1_cv3_1_1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.014394811354577541f),
    AI_PACK_INTQ_ZP(-109)))

/* Int quant #199 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_1_cv3_1_2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.060218676924705505f),
    AI_PACK_INTQ_ZP(127)))

/* Int quant #200 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_1_cv3_1_2_Conv_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.010842151008546352f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #201 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09617367386817932f),
    AI_PACK_INTQ_ZP(6)))

/* Int quant #202 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0039215353317558765f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #203 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_0_cv2_0_0_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0468796007335186f),
    AI_PACK_INTQ_ZP(-122)))

/* Int quant #204 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05101950839161873f),
    AI_PACK_INTQ_ZP(7)))

/* Int quant #205 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003913105931133032f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #206 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_0_cv2_0_1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.02510436251759529f),
    AI_PACK_INTQ_ZP(-117)))

/* Int quant #207 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_2_cv2_2_2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.011250725947320461f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #208 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_1_cv2_1_2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0409509614109993f),
    AI_PACK_INTQ_ZP(-110)))

/* Int quant #209 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv2_0_cv2_0_2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08558892458677292f),
    AI_PACK_INTQ_ZP(-52)))

/* Int quant #210 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Concat_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08558892458677292f),
    AI_PACK_INTQ_ZP(-52)))

/* Int quant #211 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_dfl_Reshape_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08558892458677292f),
    AI_PACK_INTQ_ZP(-52)))

/* Int quant #212 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_dfl_Transpose_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08558892458677292f),
    AI_PACK_INTQ_ZP(-52)))

/* Int quant #213 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #214 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_dfl_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05635429918766022f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #215 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_dfl_conv_Conv_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.11811023950576782f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #216 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_dfl_Reshape_1_output_0_to_chlast_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05635429918766022f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #217 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Slice_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05635429918766022f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #218 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Add_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.13186882436275482f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #219 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Constant_13_output_0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18503937125205994f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #220 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Slice_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.05635429918766022f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #221 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Sub_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.12994639575481415f),
    AI_PACK_INTQ_ZP(-53)))

/* Int quant #222 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Constant_12_output_0_DequantizeLinear_Output_const_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.18503937125205994f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #223 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Sub_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09199133515357971f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #224 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Add_2_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.19126345217227936f),
    AI_PACK_INTQ_ZP(-124)))

/* Int quant #225 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Div_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09563172608613968f),
    AI_PACK_INTQ_ZP(-124)))

/* Int quant #226 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.015748031437397003f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #227 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Concat_2_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09563172608613968f),
    AI_PACK_INTQ_ZP(-124)))

/* Int quant #228 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Mul_2_output_0_QuantizeLinear_Input_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.9618016481399536f),
    AI_PACK_INTQ_ZP(-125)))

/* Int quant #229 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.25196850299835205f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #230 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(1.9618016481399536f),
    AI_PACK_INTQ_ZP(-125)))

/* Int quant #231 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.07984063029289246f),
    AI_PACK_INTQ_ZP(4)))

/* Int quant #232 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921357914805412f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #233 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_0_cv3_0_0_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.039647117257118225f),
    AI_PACK_INTQ_ZP(-121)))

/* Int quant #234 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.19499097764492035f),
    AI_PACK_INTQ_ZP(-4)))

/* Int quant #235 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.003921568859368563f),
    AI_PACK_INTQ_ZP(-128)))

/* Int quant #236 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_0_cv3_0_1_act_Mul_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.10118859261274338f),
    AI_PACK_INTQ_ZP(-125)))

/* Int quant #237 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_0_cv3_0_2_Conv_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2220187485218048f),
    AI_PACK_INTQ_ZP(121)))

/* Int quant #238 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_cv3_0_cv3_0_2_Conv_output_0_weights_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.012356976047158241f),
    AI_PACK_INTQ_ZP(0)))

/* Int quant #239 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Concat_1_output_0_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.2220187485218048f),
    AI_PACK_INTQ_ZP(121)))

/* Int quant #240 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(_model_22_Sigmoid_output_0_QuantizeLinear_Input_output_array_intq, AI_STATIC,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0031456942670047283f),
    AI_PACK_INTQ_ZP(-128)))



/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  _model_0_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 73728, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  _model_0_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 73728, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  _model_0_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 73728, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  _model_1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  _model_1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  _model_1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_Split_output_0_output0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_Split_output_0_output1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_Split_output_0_num_or_size_splits_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_m_0_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_m_0_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_m_0_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_m_0_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_m_0_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_m_0_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_m_0_Add_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 55296, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  _model_2_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  _model_3_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  _model_3_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  _model_3_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_Split_output_0_output0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_Split_output_0_output1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#31 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_Split_output_0_num_or_size_splits_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#32 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_0_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#33 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_0_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#34 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_0_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#35 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_0_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#36 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_0_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#37 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_0_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#38 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_0_Add_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#39 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_1_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#40 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_1_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#41 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_1_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#42 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_1_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#43 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_1_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#44 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_1_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#45 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_m_1_Add_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#46 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#47 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#48 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#49 */
AI_ARRAY_OBJ_DECLARE(
  _model_4_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#50 */
AI_ARRAY_OBJ_DECLARE(
  _model_5_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#51 */
AI_ARRAY_OBJ_DECLARE(
  _model_5_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#52 */
AI_ARRAY_OBJ_DECLARE(
  _model_5_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#53 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#54 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#55 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#56 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_Split_output_0_output0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#57 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_Split_output_0_output1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#58 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_Split_output_0_num_or_size_splits_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#59 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_0_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#60 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_0_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#61 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_0_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#62 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_0_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#63 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_0_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#64 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_0_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#65 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_0_Add_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#66 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_1_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#67 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_1_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#68 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_1_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#69 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_1_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#70 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_1_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#71 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_1_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#72 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_m_1_Add_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#73 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#74 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#75 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#76 */
AI_ARRAY_OBJ_DECLARE(
  _model_6_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#77 */
AI_ARRAY_OBJ_DECLARE(
  _model_7_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#78 */
AI_ARRAY_OBJ_DECLARE(
  _model_7_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#79 */
AI_ARRAY_OBJ_DECLARE(
  _model_7_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#80 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#81 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#82 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#83 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_Split_output_0_output0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#84 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_Split_output_0_output1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#85 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_Split_output_0_num_or_size_splits_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#86 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_m_0_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#87 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_m_0_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#88 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_m_0_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#89 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_m_0_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#90 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_m_0_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#91 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_m_0_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#92 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_m_0_Add_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#93 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3456, AI_STATIC)

/* Array#94 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#95 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#96 */
AI_ARRAY_OBJ_DECLARE(
  _model_8_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#97 */
AI_ARRAY_OBJ_DECLARE(
  _model_9_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#98 */
AI_ARRAY_OBJ_DECLARE(
  _model_9_m_MaxPool_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#99 */
AI_ARRAY_OBJ_DECLARE(
  _model_9_m_1_MaxPool_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#100 */
AI_ARRAY_OBJ_DECLARE(
  _model_9_m_2_MaxPool_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#101 */
AI_ARRAY_OBJ_DECLARE(
  _model_9_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#102 */
AI_ARRAY_OBJ_DECLARE(
  _model_9_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#103 */
AI_ARRAY_OBJ_DECLARE(
  _model_9_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#104 */
AI_ARRAY_OBJ_DECLARE(
  _model_9_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#105 */
AI_ARRAY_OBJ_DECLARE(
  _model_10_Resize_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#106 */
AI_ARRAY_OBJ_DECLARE(
  _model_11_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#107 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#108 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#109 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#110 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_Split_output_0_output0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#111 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_Split_output_0_output1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#112 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_Split_output_0_num_or_size_splits_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#113 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_m_0_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#114 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_m_0_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#115 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_m_0_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#116 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_m_0_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#117 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_m_0_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#118 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_m_0_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#119 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 13824, AI_STATIC)

/* Array#120 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#121 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#122 */
AI_ARRAY_OBJ_DECLARE(
  _model_12_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#123 */
AI_ARRAY_OBJ_DECLARE(
  _model_13_Resize_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#124 */
AI_ARRAY_OBJ_DECLARE(
  _model_14_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 55296, AI_STATIC)

/* Array#125 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#126 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#127 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#128 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_Split_output_0_output0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#129 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_Split_output_0_output1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#130 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_Split_output_0_num_or_size_splits_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#131 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_m_0_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#132 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_m_0_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#133 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_m_0_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#134 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_m_0_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#135 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_m_0_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#136 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_m_0_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#137 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 27648, AI_STATIC)

/* Array#138 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#139 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#140 */
AI_ARRAY_OBJ_DECLARE(
  _model_15_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#141 */
AI_ARRAY_OBJ_DECLARE(
  _model_16_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#142 */
AI_ARRAY_OBJ_DECLARE(
  _model_16_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#143 */
AI_ARRAY_OBJ_DECLARE(
  _model_16_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#144 */
AI_ARRAY_OBJ_DECLARE(
  _model_17_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 13824, AI_STATIC)

/* Array#145 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#146 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#147 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#148 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_Split_output_0_output0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#149 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_Split_output_0_output1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#150 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_Split_output_0_num_or_size_splits_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#151 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_m_0_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#152 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_m_0_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#153 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_m_0_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#154 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_m_0_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#155 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_m_0_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#156 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_m_0_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#157 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 13824, AI_STATIC)

/* Array#158 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#159 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#160 */
AI_ARRAY_OBJ_DECLARE(
  _model_18_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#161 */
AI_ARRAY_OBJ_DECLARE(
  _model_19_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#162 */
AI_ARRAY_OBJ_DECLARE(
  _model_19_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#163 */
AI_ARRAY_OBJ_DECLARE(
  _model_19_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#164 */
AI_ARRAY_OBJ_DECLARE(
  _model_20_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#165 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#166 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#167 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#168 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_Split_output_0_output0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#169 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_Split_output_0_output1_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#170 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_Split_output_0_num_or_size_splits_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#171 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_m_0_cv1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#172 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_m_0_cv1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#173 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_m_0_cv1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#174 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_m_0_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#175 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_m_0_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#176 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_m_0_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#177 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3456, AI_STATIC)

/* Array#178 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_cv2_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#179 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_cv2_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#180 */
AI_ARRAY_OBJ_DECLARE(
  _model_21_cv2_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#181 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#182 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#183 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#184 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#185 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#186 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#187 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#188 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#189 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#190 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#191 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#192 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1152, AI_STATIC)

/* Array#193 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36, AI_STATIC)

/* Array#194 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 32, AI_STATIC)

/* Array#195 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#196 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#197 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#198 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#199 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#200 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#201 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#202 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#203 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#204 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#205 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#206 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#207 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#208 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 4608, AI_STATIC)

/* Array#209 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 144, AI_STATIC)

/* Array#210 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 32, AI_STATIC)

/* Array#211 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#212 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#213 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#214 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#215 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#216 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#217 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#218 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#219 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_2_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2304, AI_STATIC)

/* Array#220 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_2_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 9216, AI_STATIC)

/* Array#221 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_2_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 36864, AI_STATIC)

/* Array#222 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Concat_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 48384, AI_STATIC)

/* Array#223 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_dfl_Reshape_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 48384, AI_STATIC)

/* Array#224 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_dfl_Transpose_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 48384, AI_STATIC)

/* Array#225 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 48384, AI_STATIC)

/* Array#226 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3024, AI_STATIC)

/* Array#227 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 16, AI_STATIC)

/* Array#228 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#229 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 64, AI_STATIC)

/* Array#230 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_dfl_Reshape_1_output_0_to_chlast_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3024, AI_STATIC)

/* Array#231 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Slice_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#232 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Add_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#233 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Constant_13_output_0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#234 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Slice_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#235 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Sub_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#236 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Constant_12_output_0_DequantizeLinear_Output_const_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#237 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Sub_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#238 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Add_2_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#239 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Div_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1512, AI_STATIC)

/* Array#240 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 1, AI_STATIC)

/* Array#241 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Concat_2_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3024, AI_STATIC)

/* Array#242 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Mul_2_output_0_QuantizeLinear_Input_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 3024, AI_STATIC)

/* Array#243 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 756, AI_STATIC)

/* Array#244 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output_array, AI_ARRAY_FORMAT_S8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 3024, AI_STATIC)

/* Array#245 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#246 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#247 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#248 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#249 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#250 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_act_Mul_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 18432, AI_STATIC)

/* Array#251 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 576, AI_STATIC)

/* Array#252 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 32, AI_STATIC)

/* Array#253 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 1, AI_STATIC)

/* Array#254 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 128, AI_STATIC)

/* Array#255 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Concat_1_output_0_output_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 756, AI_STATIC)

/* Array#256 */
AI_ARRAY_OBJ_DECLARE(
  _model_22_Sigmoid_output_0_QuantizeLinear_Input_output_array, AI_ARRAY_FORMAT_S8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 756, AI_STATIC)



/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  _model_0_act_Sigmoid_output_0_output, AI_STATIC,
  1, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 96, 96), AI_STRIDE_INIT(4, 1, 1, 8, 768),
  1, &_model_0_act_Sigmoid_output_0_output_array, &_model_0_act_Sigmoid_output_0_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  _model_0_conv_Conv_output_0_output, AI_STATIC,
  3, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 96, 96), AI_STRIDE_INIT(4, 1, 1, 8, 768),
  1, &_model_0_conv_Conv_output_0_output_array, &_model_0_conv_Conv_output_0_output_array_intq)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  _model_0_act_Mul_output_0_output, AI_STATIC,
  0, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 96, 96), AI_STRIDE_INIT(4, 1, 1, 8, 768),
  1, &_model_0_act_Mul_output_0_output_array, &_model_0_act_Mul_output_0_output_array_intq)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  _model_1_act_Sigmoid_output_0_output, AI_STATIC,
  116, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_1_act_Sigmoid_output_0_output_array, &_model_1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  _model_1_conv_Conv_output_0_output, AI_STATIC,
  118, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_1_conv_Conv_output_0_output_array, &_model_1_conv_Conv_output_0_output_array_intq)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  _model_1_act_Mul_output_0_output, AI_STATIC,
  115, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_1_act_Mul_output_0_output_array, &_model_1_act_Mul_output_0_output_array_intq)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  300, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_2_cv1_act_Sigmoid_output_0_output_array, &_model_2_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_cv1_conv_Conv_output_0_output, AI_STATIC,
  302, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_2_cv1_conv_Conv_output_0_output_array, &_model_2_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_cv1_act_Mul_output_0_output, AI_STATIC,
  299, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_2_cv1_act_Mul_output_0_output_array, &_model_2_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_Split_output_0_num_or_size_splits, AI_STATIC,
  296, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_2_Split_output_0_num_or_size_splits_array, NULL)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_Split_output_0_output0, AI_STATIC,
  297, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_Split_output_0_output0_array, &_model_2_Split_output_0_output0_array_intq)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_Split_output_0_output1, AI_STATIC,
  298, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_Split_output_0_output1_array, &_model_2_Split_output_0_output1_array_intq)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_m_0_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  313, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_m_0_cv1_act_Sigmoid_output_0_output_array, &_model_2_m_0_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_m_0_cv1_conv_Conv_output_0_output, AI_STATIC,
  315, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_m_0_cv1_conv_Conv_output_0_output_array, &_model_2_m_0_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_m_0_cv1_act_Mul_output_0_output, AI_STATIC,
  312, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_m_0_cv1_act_Mul_output_0_output_array, &_model_2_m_0_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_m_0_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  319, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_m_0_cv2_act_Sigmoid_output_0_output_array, &_model_2_m_0_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_m_0_cv2_conv_Conv_output_0_output, AI_STATIC,
  321, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_m_0_cv2_conv_Conv_output_0_output_array, &_model_2_m_0_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_m_0_cv2_act_Mul_output_0_output, AI_STATIC,
  318, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_m_0_cv2_act_Mul_output_0_output_array, &_model_2_m_0_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_m_0_Add_output_0_output, AI_STATIC,
  311, 0x1,
  AI_SHAPE_INIT(4, 1, 8, 48, 48), AI_STRIDE_INIT(4, 1, 1, 8, 384),
  1, &_model_2_m_0_Add_output_0_output_array, &_model_2_m_0_Add_output_0_output_array_intq)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_Concat_output_0_output, AI_STATIC,
  295, 0x1,
  AI_SHAPE_INIT(4, 1, 24, 48, 48), AI_STRIDE_INIT(4, 1, 1, 24, 1152),
  1, &_model_2_Concat_output_0_output_array, &_model_2_Concat_output_0_output_array_intq)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  306, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_2_cv2_act_Sigmoid_output_0_output_array, &_model_2_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_cv2_conv_Conv_output_0_output, AI_STATIC,
  308, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_2_cv2_conv_Conv_output_0_output_array, &_model_2_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  _model_2_cv2_act_Mul_output_0_output, AI_STATIC,
  305, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 48, 48), AI_STRIDE_INIT(4, 1, 1, 16, 768),
  1, &_model_2_cv2_act_Mul_output_0_output_array, &_model_2_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  _model_3_act_Sigmoid_output_0_output, AI_STATIC,
  325, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_3_act_Sigmoid_output_0_output_array, &_model_3_act_Sigmoid_output_0_output_array_intq)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  _model_3_conv_Conv_output_0_output, AI_STATIC,
  327, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_3_conv_Conv_output_0_output_array, &_model_3_conv_Conv_output_0_output_array_intq)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  _model_3_act_Mul_output_0_output, AI_STATIC,
  324, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_3_act_Mul_output_0_output_array, &_model_3_act_Mul_output_0_output_array_intq)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  335, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_4_cv1_act_Sigmoid_output_0_output_array, &_model_4_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_cv1_conv_Conv_output_0_output, AI_STATIC,
  337, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_4_cv1_conv_Conv_output_0_output_array, &_model_4_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_cv1_act_Mul_output_0_output, AI_STATIC,
  334, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_4_cv1_act_Mul_output_0_output_array, &_model_4_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_Split_output_0_num_or_size_splits, AI_STATIC,
  331, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_4_Split_output_0_num_or_size_splits_array, NULL)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_Split_output_0_output0, AI_STATIC,
  332, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_Split_output_0_output0_array, &_model_4_Split_output_0_output0_array_intq)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_Split_output_0_output1, AI_STATIC,
  333, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_Split_output_0_output1_array, &_model_4_Split_output_0_output1_array_intq)

/* Tensor #32 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_0_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  348, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_0_cv1_act_Sigmoid_output_0_output_array, &_model_4_m_0_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #33 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_0_cv1_conv_Conv_output_0_output, AI_STATIC,
  350, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_0_cv1_conv_Conv_output_0_output_array, &_model_4_m_0_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #34 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_0_cv1_act_Mul_output_0_output, AI_STATIC,
  347, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_0_cv1_act_Mul_output_0_output_array, &_model_4_m_0_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #35 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_0_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  355, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_0_cv2_act_Sigmoid_output_0_output_array, &_model_4_m_0_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #36 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_0_cv2_conv_Conv_output_0_output, AI_STATIC,
  357, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_0_cv2_conv_Conv_output_0_output_array, &_model_4_m_0_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #37 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_0_cv2_act_Mul_output_0_output, AI_STATIC,
  354, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_0_cv2_act_Mul_output_0_output_array, &_model_4_m_0_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #38 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_0_Add_output_0_output, AI_STATIC,
  346, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_0_Add_output_0_output_array, &_model_4_m_0_Add_output_0_output_array_intq)

/* Tensor #39 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_1_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  363, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_1_cv1_act_Sigmoid_output_0_output_array, &_model_4_m_1_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #40 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_1_cv1_conv_Conv_output_0_output, AI_STATIC,
  365, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_1_cv1_conv_Conv_output_0_output_array, &_model_4_m_1_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #41 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_1_cv1_act_Mul_output_0_output, AI_STATIC,
  362, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_1_cv1_act_Mul_output_0_output_array, &_model_4_m_1_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #42 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_1_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  370, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_1_cv2_act_Sigmoid_output_0_output_array, &_model_4_m_1_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #43 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_1_cv2_conv_Conv_output_0_output, AI_STATIC,
  372, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_1_cv2_conv_Conv_output_0_output_array, &_model_4_m_1_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #44 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_1_cv2_act_Mul_output_0_output, AI_STATIC,
  369, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_1_cv2_act_Mul_output_0_output_array, &_model_4_m_1_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #45 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_m_1_Add_output_0_output, AI_STATIC,
  361, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_4_m_1_Add_output_0_output_array, &_model_4_m_1_Add_output_0_output_array_intq)

/* Tensor #46 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_Concat_output_0_output, AI_STATIC,
  330, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 24, 24), AI_STRIDE_INIT(4, 1, 1, 64, 1536),
  1, &_model_4_Concat_output_0_output_array, &_model_4_Concat_output_0_output_array_intq)

/* Tensor #47 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  341, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_4_cv2_act_Sigmoid_output_0_output_array, &_model_4_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #48 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_cv2_conv_Conv_output_0_output, AI_STATIC,
  343, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_4_cv2_conv_Conv_output_0_output_array, &_model_4_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #49 */
AI_TENSOR_OBJ_DECLARE(
  _model_4_cv2_act_Mul_output_0_output, AI_STATIC,
  340, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_4_cv2_act_Mul_output_0_output_array, &_model_4_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #50 */
AI_TENSOR_OBJ_DECLARE(
  _model_5_act_Sigmoid_output_0_output, AI_STATIC,
  377, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_5_act_Sigmoid_output_0_output_array, &_model_5_act_Sigmoid_output_0_output_array_intq)

/* Tensor #51 */
AI_TENSOR_OBJ_DECLARE(
  _model_5_conv_Conv_output_0_output, AI_STATIC,
  379, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_5_conv_Conv_output_0_output_array, &_model_5_conv_Conv_output_0_output_array_intq)

/* Tensor #52 */
AI_TENSOR_OBJ_DECLARE(
  _model_5_act_Mul_output_0_output, AI_STATIC,
  376, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_5_act_Mul_output_0_output_array, &_model_5_act_Mul_output_0_output_array_intq)

/* Tensor #53 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  388, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_6_cv1_act_Sigmoid_output_0_output_array, &_model_6_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #54 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_cv1_conv_Conv_output_0_output, AI_STATIC,
  390, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_6_cv1_conv_Conv_output_0_output_array, &_model_6_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #55 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_cv1_act_Mul_output_0_output, AI_STATIC,
  387, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_6_cv1_act_Mul_output_0_output_array, &_model_6_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #56 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_Split_output_0_num_or_size_splits, AI_STATIC,
  384, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_6_Split_output_0_num_or_size_splits_array, NULL)

/* Tensor #57 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_Split_output_0_output0, AI_STATIC,
  385, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_Split_output_0_output0_array, &_model_6_Split_output_0_output0_array_intq)

/* Tensor #58 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_Split_output_0_output1, AI_STATIC,
  386, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_Split_output_0_output1_array, &_model_6_Split_output_0_output1_array_intq)

/* Tensor #59 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_0_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  401, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_0_cv1_act_Sigmoid_output_0_output_array, &_model_6_m_0_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #60 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_0_cv1_conv_Conv_output_0_output, AI_STATIC,
  403, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_0_cv1_conv_Conv_output_0_output_array, &_model_6_m_0_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #61 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_0_cv1_act_Mul_output_0_output, AI_STATIC,
  400, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_0_cv1_act_Mul_output_0_output_array, &_model_6_m_0_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #62 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_0_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  408, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_0_cv2_act_Sigmoid_output_0_output_array, &_model_6_m_0_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #63 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_0_cv2_conv_Conv_output_0_output, AI_STATIC,
  410, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_0_cv2_conv_Conv_output_0_output_array, &_model_6_m_0_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #64 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_0_cv2_act_Mul_output_0_output, AI_STATIC,
  407, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_0_cv2_act_Mul_output_0_output_array, &_model_6_m_0_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #65 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_0_Add_output_0_output, AI_STATIC,
  399, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_0_Add_output_0_output_array, &_model_6_m_0_Add_output_0_output_array_intq)

/* Tensor #66 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_1_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  416, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_1_cv1_act_Sigmoid_output_0_output_array, &_model_6_m_1_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #67 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_1_cv1_conv_Conv_output_0_output, AI_STATIC,
  418, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_1_cv1_conv_Conv_output_0_output_array, &_model_6_m_1_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #68 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_1_cv1_act_Mul_output_0_output, AI_STATIC,
  415, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_1_cv1_act_Mul_output_0_output_array, &_model_6_m_1_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #69 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_1_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  423, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_1_cv2_act_Sigmoid_output_0_output_array, &_model_6_m_1_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #70 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_1_cv2_conv_Conv_output_0_output, AI_STATIC,
  425, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_1_cv2_conv_Conv_output_0_output_array, &_model_6_m_1_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #71 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_1_cv2_act_Mul_output_0_output, AI_STATIC,
  422, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_1_cv2_act_Mul_output_0_output_array, &_model_6_m_1_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #72 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_m_1_Add_output_0_output, AI_STATIC,
  414, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_6_m_1_Add_output_0_output_array, &_model_6_m_1_Add_output_0_output_array_intq)

/* Tensor #73 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_Concat_output_0_output, AI_STATIC,
  383, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 12, 12), AI_STRIDE_INIT(4, 1, 1, 128, 1536),
  1, &_model_6_Concat_output_0_output_array, &_model_6_Concat_output_0_output_array_intq)

/* Tensor #74 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  394, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_6_cv2_act_Sigmoid_output_0_output_array, &_model_6_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #75 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_cv2_conv_Conv_output_0_output, AI_STATIC,
  396, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_6_cv2_conv_Conv_output_0_output_array, &_model_6_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #76 */
AI_TENSOR_OBJ_DECLARE(
  _model_6_cv2_act_Mul_output_0_output, AI_STATIC,
  393, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_6_cv2_act_Mul_output_0_output_array, &_model_6_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #77 */
AI_TENSOR_OBJ_DECLARE(
  _model_7_act_Sigmoid_output_0_output, AI_STATIC,
  430, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_7_act_Sigmoid_output_0_output_array, &_model_7_act_Sigmoid_output_0_output_array_intq)

/* Tensor #78 */
AI_TENSOR_OBJ_DECLARE(
  _model_7_conv_Conv_output_0_output, AI_STATIC,
  432, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_7_conv_Conv_output_0_output_array, &_model_7_conv_Conv_output_0_output_array_intq)

/* Tensor #79 */
AI_TENSOR_OBJ_DECLARE(
  _model_7_act_Mul_output_0_output, AI_STATIC,
  429, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_7_act_Mul_output_0_output_array, &_model_7_act_Mul_output_0_output_array_intq)

/* Tensor #80 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  441, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_8_cv1_act_Sigmoid_output_0_output_array, &_model_8_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #81 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_cv1_conv_Conv_output_0_output, AI_STATIC,
  443, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_8_cv1_conv_Conv_output_0_output_array, &_model_8_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #82 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_cv1_act_Mul_output_0_output, AI_STATIC,
  440, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_8_cv1_act_Mul_output_0_output_array, &_model_8_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #83 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_Split_output_0_num_or_size_splits, AI_STATIC,
  437, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_8_Split_output_0_num_or_size_splits_array, NULL)

/* Tensor #84 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_Split_output_0_output0, AI_STATIC,
  438, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_Split_output_0_output0_array, &_model_8_Split_output_0_output0_array_intq)

/* Tensor #85 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_Split_output_0_output1, AI_STATIC,
  439, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_Split_output_0_output1_array, &_model_8_Split_output_0_output1_array_intq)

/* Tensor #86 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_m_0_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  454, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_m_0_cv1_act_Sigmoid_output_0_output_array, &_model_8_m_0_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #87 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_m_0_cv1_conv_Conv_output_0_output, AI_STATIC,
  456, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_m_0_cv1_conv_Conv_output_0_output_array, &_model_8_m_0_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #88 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_m_0_cv1_act_Mul_output_0_output, AI_STATIC,
  453, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_m_0_cv1_act_Mul_output_0_output_array, &_model_8_m_0_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #89 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_m_0_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  461, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_m_0_cv2_act_Sigmoid_output_0_output_array, &_model_8_m_0_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #90 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_m_0_cv2_conv_Conv_output_0_output, AI_STATIC,
  463, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_m_0_cv2_conv_Conv_output_0_output_array, &_model_8_m_0_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #91 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_m_0_cv2_act_Mul_output_0_output, AI_STATIC,
  460, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_m_0_cv2_act_Mul_output_0_output_array, &_model_8_m_0_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #92 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_m_0_Add_output_0_output, AI_STATIC,
  452, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_8_m_0_Add_output_0_output_array, &_model_8_m_0_Add_output_0_output_array_intq)

/* Tensor #93 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_Concat_output_0_output, AI_STATIC,
  436, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 6, 6), AI_STRIDE_INIT(4, 1, 1, 96, 576),
  1, &_model_8_Concat_output_0_output_array, &_model_8_Concat_output_0_output_array_intq)

/* Tensor #94 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  447, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_8_cv2_act_Sigmoid_output_0_output_array, &_model_8_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #95 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_cv2_conv_Conv_output_0_output, AI_STATIC,
  449, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_8_cv2_conv_Conv_output_0_output_array, &_model_8_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #96 */
AI_TENSOR_OBJ_DECLARE(
  _model_8_cv2_act_Mul_output_0_output, AI_STATIC,
  446, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_8_cv2_act_Mul_output_0_output_array, &_model_8_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #97 */
AI_TENSOR_OBJ_DECLARE(
  _model_9_Concat_output_0_output, AI_STATIC,
  467, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 6, 6), AI_STRIDE_INIT(4, 1, 1, 128, 768),
  1, &_model_9_Concat_output_0_output_array, &_model_9_Concat_output_0_output_array_intq)

/* Tensor #98 */
AI_TENSOR_OBJ_DECLARE(
  _model_9_cv1_conv_Conv_output_0_output, AI_STATIC,
  469, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_9_cv1_conv_Conv_output_0_output_array, &_model_9_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #99 */
AI_TENSOR_OBJ_DECLARE(
  _model_9_m_1_MaxPool_output_0_output, AI_STATIC,
  478, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_9_m_1_MaxPool_output_0_output_array, &_model_9_m_1_MaxPool_output_0_output_array_intq)

/* Tensor #100 */
AI_TENSOR_OBJ_DECLARE(
  _model_9_m_2_MaxPool_output_0_output, AI_STATIC,
  479, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_9_m_2_MaxPool_output_0_output_array, &_model_9_m_2_MaxPool_output_0_output_array_intq)

/* Tensor #101 */
AI_TENSOR_OBJ_DECLARE(
  _model_9_m_MaxPool_output_0_output, AI_STATIC,
  480, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_9_m_MaxPool_output_0_output_array, &_model_9_m_MaxPool_output_0_output_array_intq)

/* Tensor #102 */
AI_TENSOR_OBJ_DECLARE(
  _model_9_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  473, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_9_cv2_act_Sigmoid_output_0_output_array, &_model_9_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #103 */
AI_TENSOR_OBJ_DECLARE(
  _model_9_cv2_conv_Conv_output_0_output, AI_STATIC,
  475, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_9_cv2_conv_Conv_output_0_output_array, &_model_9_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #104 */
AI_TENSOR_OBJ_DECLARE(
  _model_9_cv2_act_Mul_output_0_output, AI_STATIC,
  472, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_9_cv2_act_Mul_output_0_output_array, &_model_9_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #105 */
AI_TENSOR_OBJ_DECLARE(
  _model_10_Resize_output_0_output, AI_STATIC,
  6, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_10_Resize_output_0_output_array, &_model_10_Resize_output_0_output_array_intq)

/* Tensor #106 */
AI_TENSOR_OBJ_DECLARE(
  _model_11_Concat_output_0_output, AI_STATIC,
  7, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 12, 12), AI_STRIDE_INIT(4, 1, 1, 128, 1536),
  1, &_model_11_Concat_output_0_output_array, &_model_11_Concat_output_0_output_array_intq)

/* Tensor #107 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  13, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_12_cv1_act_Sigmoid_output_0_output_array, &_model_12_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #108 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_cv1_conv_Conv_output_0_output, AI_STATIC,
  15, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_12_cv1_conv_Conv_output_0_output_array, &_model_12_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #109 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_cv1_act_Mul_output_0_output, AI_STATIC,
  12, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_12_cv1_act_Mul_output_0_output_array, &_model_12_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #110 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_Split_output_0_num_or_size_splits, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_12_Split_output_0_num_or_size_splits_array, NULL)

/* Tensor #111 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_Split_output_0_output0, AI_STATIC,
  10, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_12_Split_output_0_output0_array, &_model_12_Split_output_0_output0_array_intq)

/* Tensor #112 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_Split_output_0_output1, AI_STATIC,
  11, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_12_Split_output_0_output1_array, &_model_12_Split_output_0_output1_array_intq)

/* Tensor #113 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_m_0_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  25, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_12_m_0_cv1_act_Sigmoid_output_0_output_array, &_model_12_m_0_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #114 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_m_0_cv1_conv_Conv_output_0_output, AI_STATIC,
  27, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_12_m_0_cv1_conv_Conv_output_0_output_array, &_model_12_m_0_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #115 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_m_0_cv1_act_Mul_output_0_output, AI_STATIC,
  24, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_12_m_0_cv1_act_Mul_output_0_output_array, &_model_12_m_0_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #116 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_m_0_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  32, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_12_m_0_cv2_act_Sigmoid_output_0_output_array, &_model_12_m_0_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #117 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_m_0_cv2_conv_Conv_output_0_output, AI_STATIC,
  34, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_12_m_0_cv2_conv_Conv_output_0_output_array, &_model_12_m_0_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #118 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_m_0_cv2_act_Mul_output_0_output, AI_STATIC,
  31, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_12_m_0_cv2_act_Mul_output_0_output_array, &_model_12_m_0_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #119 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_Concat_output_0_output, AI_STATIC,
  8, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 12, 12), AI_STRIDE_INIT(4, 1, 1, 96, 1152),
  1, &_model_12_Concat_output_0_output_array, &_model_12_Concat_output_0_output_array_intq)

/* Tensor #120 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  19, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_12_cv2_act_Sigmoid_output_0_output_array, &_model_12_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #121 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_cv2_conv_Conv_output_0_output, AI_STATIC,
  21, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_12_cv2_conv_Conv_output_0_output_array, &_model_12_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #122 */
AI_TENSOR_OBJ_DECLARE(
  _model_12_cv2_act_Mul_output_0_output, AI_STATIC,
  18, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_12_cv2_act_Mul_output_0_output_array, &_model_12_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #123 */
AI_TENSOR_OBJ_DECLARE(
  _model_13_Resize_output_0_output, AI_STATIC,
  38, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 24, 24), AI_STRIDE_INIT(4, 1, 1, 64, 1536),
  1, &_model_13_Resize_output_0_output_array, &_model_13_Resize_output_0_output_array_intq)

/* Tensor #124 */
AI_TENSOR_OBJ_DECLARE(
  _model_14_Concat_output_0_output, AI_STATIC,
  39, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 24, 24), AI_STRIDE_INIT(4, 1, 1, 96, 2304),
  1, &_model_14_Concat_output_0_output_array, &_model_14_Concat_output_0_output_array_intq)

/* Tensor #125 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  45, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_15_cv1_act_Sigmoid_output_0_output_array, &_model_15_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #126 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_cv1_conv_Conv_output_0_output, AI_STATIC,
  47, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_15_cv1_conv_Conv_output_0_output_array, &_model_15_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #127 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_cv1_act_Mul_output_0_output, AI_STATIC,
  44, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_15_cv1_act_Mul_output_0_output_array, &_model_15_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #128 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_Split_output_0_num_or_size_splits, AI_STATIC,
  41, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_15_Split_output_0_num_or_size_splits_array, NULL)

/* Tensor #129 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_Split_output_0_output0, AI_STATIC,
  42, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_15_Split_output_0_output0_array, &_model_15_Split_output_0_output0_array_intq)

/* Tensor #130 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_Split_output_0_output1, AI_STATIC,
  43, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_15_Split_output_0_output1_array, &_model_15_Split_output_0_output1_array_intq)

/* Tensor #131 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_m_0_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  57, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_15_m_0_cv1_act_Sigmoid_output_0_output_array, &_model_15_m_0_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #132 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_m_0_cv1_conv_Conv_output_0_output, AI_STATIC,
  59, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_15_m_0_cv1_conv_Conv_output_0_output_array, &_model_15_m_0_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #133 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_m_0_cv1_act_Mul_output_0_output, AI_STATIC,
  56, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_15_m_0_cv1_act_Mul_output_0_output_array, &_model_15_m_0_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #134 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_m_0_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  64, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_15_m_0_cv2_act_Sigmoid_output_0_output_array, &_model_15_m_0_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #135 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_m_0_cv2_conv_Conv_output_0_output, AI_STATIC,
  66, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_15_m_0_cv2_conv_Conv_output_0_output_array, &_model_15_m_0_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #136 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_m_0_cv2_act_Mul_output_0_output, AI_STATIC,
  63, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 24, 24), AI_STRIDE_INIT(4, 1, 1, 16, 384),
  1, &_model_15_m_0_cv2_act_Mul_output_0_output_array, &_model_15_m_0_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #137 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_Concat_output_0_output, AI_STATIC,
  40, 0x1,
  AI_SHAPE_INIT(4, 1, 48, 24, 24), AI_STRIDE_INIT(4, 1, 1, 48, 1152),
  1, &_model_15_Concat_output_0_output_array, &_model_15_Concat_output_0_output_array_intq)

/* Tensor #138 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  51, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_15_cv2_act_Sigmoid_output_0_output_array, &_model_15_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #139 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_cv2_conv_Conv_output_0_output, AI_STATIC,
  53, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_15_cv2_conv_Conv_output_0_output_array, &_model_15_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #140 */
AI_TENSOR_OBJ_DECLARE(
  _model_15_cv2_act_Mul_output_0_output, AI_STATIC,
  50, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_15_cv2_act_Mul_output_0_output_array, &_model_15_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #141 */
AI_TENSOR_OBJ_DECLARE(
  _model_16_act_Sigmoid_output_0_output, AI_STATIC,
  71, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_16_act_Sigmoid_output_0_output_array, &_model_16_act_Sigmoid_output_0_output_array_intq)

/* Tensor #142 */
AI_TENSOR_OBJ_DECLARE(
  _model_16_conv_Conv_output_0_output, AI_STATIC,
  73, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_16_conv_Conv_output_0_output_array, &_model_16_conv_Conv_output_0_output_array_intq)

/* Tensor #143 */
AI_TENSOR_OBJ_DECLARE(
  _model_16_act_Mul_output_0_output, AI_STATIC,
  70, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_16_act_Mul_output_0_output_array, &_model_16_act_Mul_output_0_output_array_intq)

/* Tensor #144 */
AI_TENSOR_OBJ_DECLARE(
  _model_17_Concat_output_0_output, AI_STATIC,
  77, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 12, 12), AI_STRIDE_INIT(4, 1, 1, 96, 1152),
  1, &_model_17_Concat_output_0_output_array, &_model_17_Concat_output_0_output_array_intq)

/* Tensor #145 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  83, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_18_cv1_act_Sigmoid_output_0_output_array, &_model_18_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #146 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_cv1_conv_Conv_output_0_output, AI_STATIC,
  85, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_18_cv1_conv_Conv_output_0_output_array, &_model_18_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #147 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_cv1_act_Mul_output_0_output, AI_STATIC,
  82, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_18_cv1_act_Mul_output_0_output_array, &_model_18_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #148 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_Split_output_0_num_or_size_splits, AI_STATIC,
  79, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_18_Split_output_0_num_or_size_splits_array, NULL)

/* Tensor #149 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_Split_output_0_output0, AI_STATIC,
  80, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_18_Split_output_0_output0_array, &_model_18_Split_output_0_output0_array_intq)

/* Tensor #150 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_Split_output_0_output1, AI_STATIC,
  81, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_18_Split_output_0_output1_array, &_model_18_Split_output_0_output1_array_intq)

/* Tensor #151 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_m_0_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  95, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_18_m_0_cv1_act_Sigmoid_output_0_output_array, &_model_18_m_0_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #152 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_m_0_cv1_conv_Conv_output_0_output, AI_STATIC,
  97, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_18_m_0_cv1_conv_Conv_output_0_output_array, &_model_18_m_0_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #153 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_m_0_cv1_act_Mul_output_0_output, AI_STATIC,
  94, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_18_m_0_cv1_act_Mul_output_0_output_array, &_model_18_m_0_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #154 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_m_0_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  102, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_18_m_0_cv2_act_Sigmoid_output_0_output_array, &_model_18_m_0_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #155 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_m_0_cv2_conv_Conv_output_0_output, AI_STATIC,
  104, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_18_m_0_cv2_conv_Conv_output_0_output_array, &_model_18_m_0_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #156 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_m_0_cv2_act_Mul_output_0_output, AI_STATIC,
  101, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_18_m_0_cv2_act_Mul_output_0_output_array, &_model_18_m_0_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #157 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_Concat_output_0_output, AI_STATIC,
  78, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 12, 12), AI_STRIDE_INIT(4, 1, 1, 96, 1152),
  1, &_model_18_Concat_output_0_output_array, &_model_18_Concat_output_0_output_array_intq)

/* Tensor #158 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  89, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_18_cv2_act_Sigmoid_output_0_output_array, &_model_18_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #159 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_cv2_conv_Conv_output_0_output, AI_STATIC,
  91, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_18_cv2_conv_Conv_output_0_output_array, &_model_18_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #160 */
AI_TENSOR_OBJ_DECLARE(
  _model_18_cv2_act_Mul_output_0_output, AI_STATIC,
  88, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_18_cv2_act_Mul_output_0_output_array, &_model_18_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #161 */
AI_TENSOR_OBJ_DECLARE(
  _model_19_act_Sigmoid_output_0_output, AI_STATIC,
  109, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_19_act_Sigmoid_output_0_output_array, &_model_19_act_Sigmoid_output_0_output_array_intq)

/* Tensor #162 */
AI_TENSOR_OBJ_DECLARE(
  _model_19_conv_Conv_output_0_output, AI_STATIC,
  111, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_19_conv_Conv_output_0_output_array, &_model_19_conv_Conv_output_0_output_array_intq)

/* Tensor #163 */
AI_TENSOR_OBJ_DECLARE(
  _model_19_act_Mul_output_0_output, AI_STATIC,
  108, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_19_act_Mul_output_0_output_array, &_model_19_act_Mul_output_0_output_array_intq)

/* Tensor #164 */
AI_TENSOR_OBJ_DECLARE(
  _model_20_Concat_output_0_output, AI_STATIC,
  121, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 6, 6), AI_STRIDE_INIT(4, 1, 1, 128, 768),
  1, &_model_20_Concat_output_0_output_array, &_model_20_Concat_output_0_output_array_intq)

/* Tensor #165 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  127, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_21_cv1_act_Sigmoid_output_0_output_array, &_model_21_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #166 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_cv1_conv_Conv_output_0_output, AI_STATIC,
  129, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_21_cv1_conv_Conv_output_0_output_array, &_model_21_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #167 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_cv1_act_Mul_output_0_output, AI_STATIC,
  126, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_21_cv1_act_Mul_output_0_output_array, &_model_21_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #168 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_Split_output_0_num_or_size_splits, AI_STATIC,
  123, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_21_Split_output_0_num_or_size_splits_array, NULL)

/* Tensor #169 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_Split_output_0_output0, AI_STATIC,
  124, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_21_Split_output_0_output0_array, &_model_21_Split_output_0_output0_array_intq)

/* Tensor #170 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_Split_output_0_output1, AI_STATIC,
  125, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_21_Split_output_0_output1_array, &_model_21_Split_output_0_output1_array_intq)

/* Tensor #171 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_m_0_cv1_act_Sigmoid_output_0_output, AI_STATIC,
  139, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_21_m_0_cv1_act_Sigmoid_output_0_output_array, &_model_21_m_0_cv1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #172 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_m_0_cv1_conv_Conv_output_0_output, AI_STATIC,
  141, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_21_m_0_cv1_conv_Conv_output_0_output_array, &_model_21_m_0_cv1_conv_Conv_output_0_output_array_intq)

/* Tensor #173 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_m_0_cv1_act_Mul_output_0_output, AI_STATIC,
  138, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_21_m_0_cv1_act_Mul_output_0_output_array, &_model_21_m_0_cv1_act_Mul_output_0_output_array_intq)

/* Tensor #174 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_m_0_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  146, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_21_m_0_cv2_act_Sigmoid_output_0_output_array, &_model_21_m_0_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #175 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_m_0_cv2_conv_Conv_output_0_output, AI_STATIC,
  148, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_21_m_0_cv2_conv_Conv_output_0_output_array, &_model_21_m_0_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #176 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_m_0_cv2_act_Mul_output_0_output, AI_STATIC,
  145, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_21_m_0_cv2_act_Mul_output_0_output_array, &_model_21_m_0_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #177 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_Concat_output_0_output, AI_STATIC,
  122, 0x1,
  AI_SHAPE_INIT(4, 1, 96, 6, 6), AI_STRIDE_INIT(4, 1, 1, 96, 576),
  1, &_model_21_Concat_output_0_output_array, &_model_21_Concat_output_0_output_array_intq)

/* Tensor #178 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_cv2_act_Sigmoid_output_0_output, AI_STATIC,
  133, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_21_cv2_act_Sigmoid_output_0_output_array, &_model_21_cv2_act_Sigmoid_output_0_output_array_intq)

/* Tensor #179 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_cv2_conv_Conv_output_0_output, AI_STATIC,
  135, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_21_cv2_conv_Conv_output_0_output_array, &_model_21_cv2_conv_Conv_output_0_output_array_intq)

/* Tensor #180 */
AI_TENSOR_OBJ_DECLARE(
  _model_21_cv2_act_Mul_output_0_output, AI_STATIC,
  132, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_21_cv2_act_Mul_output_0_output_array, &_model_21_cv2_act_Mul_output_0_output_array_intq)

/* Tensor #181 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output, AI_STATIC,
  208, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output_array, &_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output_array_intq)

/* Tensor #182 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output, AI_STATIC,
  210, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output_array, &_model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output_array_intq)

/* Tensor #183 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_act_Mul_output_0_output, AI_STATIC,
  207, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_22_cv2_2_cv2_2_0_act_Mul_output_0_output_array, &_model_22_cv2_2_cv2_2_0_act_Mul_output_0_output_array_intq)

/* Tensor #184 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output, AI_STATIC,
  215, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output_array, &_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #185 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output, AI_STATIC,
  217, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output_array, &_model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output_array_intq)

/* Tensor #186 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_act_Mul_output_0_output, AI_STATIC,
  214, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 6, 6), AI_STRIDE_INIT(4, 1, 1, 64, 384),
  1, &_model_22_cv2_2_cv2_2_1_act_Mul_output_0_output_array, &_model_22_cv2_2_cv2_2_1_act_Mul_output_0_output_array_intq)

/* Tensor #187 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output, AI_STATIC,
  265, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output_array, &_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output_array_intq)

/* Tensor #188 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output, AI_STATIC,
  267, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output_array, &_model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output_array_intq)

/* Tensor #189 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_act_Mul_output_0_output, AI_STATIC,
  264, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_22_cv3_2_cv3_2_0_act_Mul_output_0_output_array, &_model_22_cv3_2_cv3_2_0_act_Mul_output_0_output_array_intq)

/* Tensor #190 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output, AI_STATIC,
  272, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output_array, &_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #191 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output, AI_STATIC,
  274, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output_array, &_model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output_array_intq)

/* Tensor #192 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_act_Mul_output_0_output, AI_STATIC,
  271, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 6, 6), AI_STRIDE_INIT(4, 1, 1, 32, 192),
  1, &_model_22_cv3_2_cv3_2_1_act_Mul_output_0_output_array, &_model_22_cv3_2_cv3_2_1_act_Mul_output_0_output_array_intq)

/* Tensor #193 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_bias, AI_STATIC,
  278, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_22_cv3_2_cv3_2_2_Conv_output_0_bias_array, NULL)

/* Tensor #194 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_output, AI_STATIC,
  279, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 6, 6), AI_STRIDE_INIT(4, 1, 1, 1, 6),
  1, &_model_22_cv3_2_cv3_2_2_Conv_output_0_output_array, &_model_22_cv3_2_cv3_2_2_Conv_output_0_output_array_intq)

/* Tensor #195 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_scratch0, AI_STATIC,
  281, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &_model_22_cv3_2_cv3_2_2_Conv_output_0_scratch0_array, NULL)

/* Tensor #196 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_weights, AI_STATIC,
  282, 0x1,
  AI_SHAPE_INIT(4, 32, 1, 1, 1), AI_STRIDE_INIT(4, 1, 32, 32, 32),
  1, &_model_22_cv3_2_cv3_2_2_Conv_output_0_weights_array, &_model_22_cv3_2_cv3_2_2_Conv_output_0_weights_array_intq)

/* Tensor #197 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output, AI_STATIC,
  189, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output_array, &_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output_array_intq)

/* Tensor #198 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output, AI_STATIC,
  191, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output_array, &_model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output_array_intq)

/* Tensor #199 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_act_Mul_output_0_output, AI_STATIC,
  188, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_22_cv2_1_cv2_1_0_act_Mul_output_0_output_array, &_model_22_cv2_1_cv2_1_0_act_Mul_output_0_output_array_intq)

/* Tensor #200 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output, AI_STATIC,
  196, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output_array, &_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #201 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output, AI_STATIC,
  198, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output_array, &_model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output_array_intq)

/* Tensor #202 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_act_Mul_output_0_output, AI_STATIC,
  195, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 12, 12), AI_STRIDE_INIT(4, 1, 1, 64, 768),
  1, &_model_22_cv2_1_cv2_1_1_act_Mul_output_0_output_array, &_model_22_cv2_1_cv2_1_1_act_Mul_output_0_output_array_intq)

/* Tensor #203 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output, AI_STATIC,
  246, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output_array, &_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output_array_intq)

/* Tensor #204 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output, AI_STATIC,
  248, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output_array, &_model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output_array_intq)

/* Tensor #205 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_act_Mul_output_0_output, AI_STATIC,
  245, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_22_cv3_1_cv3_1_0_act_Mul_output_0_output_array, &_model_22_cv3_1_cv3_1_0_act_Mul_output_0_output_array_intq)

/* Tensor #206 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output, AI_STATIC,
  253, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output_array, &_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #207 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output, AI_STATIC,
  255, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output_array, &_model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output_array_intq)

/* Tensor #208 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_act_Mul_output_0_output, AI_STATIC,
  252, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 12, 12), AI_STRIDE_INIT(4, 1, 1, 32, 384),
  1, &_model_22_cv3_1_cv3_1_1_act_Mul_output_0_output_array, &_model_22_cv3_1_cv3_1_1_act_Mul_output_0_output_array_intq)

/* Tensor #209 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_bias, AI_STATIC,
  259, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_22_cv3_1_cv3_1_2_Conv_output_0_bias_array, NULL)

/* Tensor #210 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_output, AI_STATIC,
  260, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 12, 12), AI_STRIDE_INIT(4, 1, 1, 1, 12),
  1, &_model_22_cv3_1_cv3_1_2_Conv_output_0_output_array, &_model_22_cv3_1_cv3_1_2_Conv_output_0_output_array_intq)

/* Tensor #211 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_scratch0, AI_STATIC,
  262, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &_model_22_cv3_1_cv3_1_2_Conv_output_0_scratch0_array, NULL)

/* Tensor #212 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_weights, AI_STATIC,
  263, 0x1,
  AI_SHAPE_INIT(4, 32, 1, 1, 1), AI_STRIDE_INIT(4, 1, 32, 32, 32),
  1, &_model_22_cv3_1_cv3_1_2_Conv_output_0_weights_array, &_model_22_cv3_1_cv3_1_2_Conv_output_0_weights_array_intq)

/* Tensor #213 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output, AI_STATIC,
  170, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 24, 24), AI_STRIDE_INIT(4, 1, 1, 64, 1536),
  1, &_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output_array, &_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output_array_intq)

/* Tensor #214 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output, AI_STATIC,
  172, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 24, 24), AI_STRIDE_INIT(4, 1, 1, 64, 1536),
  1, &_model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output_array, &_model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output_array_intq)

/* Tensor #215 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_act_Mul_output_0_output, AI_STATIC,
  169, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 24, 24), AI_STRIDE_INIT(4, 1, 1, 64, 1536),
  1, &_model_22_cv2_0_cv2_0_0_act_Mul_output_0_output_array, &_model_22_cv2_0_cv2_0_0_act_Mul_output_0_output_array_intq)

/* Tensor #216 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output, AI_STATIC,
  177, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 24, 24), AI_STRIDE_INIT(4, 1, 1, 64, 1536),
  1, &_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output_array, &_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #217 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output, AI_STATIC,
  179, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 24, 24), AI_STRIDE_INIT(4, 1, 1, 64, 1536),
  1, &_model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output_array, &_model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output_array_intq)

/* Tensor #218 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_act_Mul_output_0_output, AI_STATIC,
  176, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 24, 24), AI_STRIDE_INIT(4, 1, 1, 64, 1536),
  1, &_model_22_cv2_0_cv2_0_1_act_Mul_output_0_output_array, &_model_22_cv2_0_cv2_0_1_act_Mul_output_0_output_array_intq)

/* Tensor #219 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Concat_output_0_output, AI_STATIC,
  156, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 756), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_model_22_Concat_output_0_output_array, &_model_22_Concat_output_0_output_array_intq)

/* Tensor #220 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_2_Conv_output_0_output0, AI_STATIC,
  185, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 576), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_model_22_cv2_0_cv2_0_2_Conv_output_0_output_array, &_model_22_cv2_0_cv2_0_2_Conv_output_0_output_array_intq)

/* Tensor #221 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_2_Conv_output_0_output0, AI_STATIC,
  204, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 144), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_model_22_cv2_1_cv2_1_2_Conv_output_0_output_array, &_model_22_cv2_1_cv2_1_2_Conv_output_0_output_array_intq)

/* Tensor #222 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_2_Conv_output_0_output0, AI_STATIC,
  223, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 36), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_model_22_cv2_2_cv2_2_2_Conv_output_0_output_array, &_model_22_cv2_2_cv2_2_2_Conv_output_0_output_array_intq)

/* Tensor #223 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_Reshape_output_0_to_chlast_output, AI_STATIC,
  285, 0x1,
  AI_SHAPE_INIT(4, 1, 756, 1, 64), AI_STRIDE_INIT(4, 1, 1, 756, 756),
  1, &_model_22_dfl_Reshape_output_0_to_chlast_output_array, &_model_22_dfl_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #224 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_Reshape_output_0_to_chlast_output0, AI_STATIC,
  286, 0x1,
  AI_SHAPE_INIT(4, 1, 756, 16, 4), AI_STRIDE_INIT(4, 1, 1, 756, 12096),
  1, &_model_22_dfl_Reshape_output_0_to_chlast_output_array, &_model_22_dfl_Reshape_output_0_to_chlast_output_array_intq)

/* Tensor #225 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_Transpose_output_0_output, AI_STATIC,
  290, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 756, 4), AI_STRIDE_INIT(4, 1, 1, 16, 12096),
  1, &_model_22_dfl_Transpose_output_0_output_array, &_model_22_dfl_Transpose_output_0_output_array_intq)

/* Tensor #226 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output, AI_STATIC,
  287, 0x1,
  AI_SHAPE_INIT(4, 1, 16, 756, 4), AI_STRIDE_INIT(4, 1, 1, 16, 12096),
  1, &_model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output_array, &_model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output_array_intq)

/* Tensor #227 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_bias, AI_STATIC,
  291, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_22_dfl_conv_Conv_output_0_bias_array, NULL)

/* Tensor #228 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_output, AI_STATIC,
  292, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 756, 4), AI_STRIDE_INIT(4, 1, 1, 1, 756),
  1, &_model_22_dfl_conv_Conv_output_0_output_array, &_model_22_dfl_conv_Conv_output_0_output_array_intq)

/* Tensor #229 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_scratch0, AI_STATIC,
  293, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &_model_22_dfl_conv_Conv_output_0_scratch0_array, NULL)

/* Tensor #230 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_weights, AI_STATIC,
  294, 0x1,
  AI_SHAPE_INIT(4, 16, 1, 1, 1), AI_STRIDE_INIT(4, 1, 16, 16, 16),
  1, &_model_22_dfl_conv_Conv_output_0_weights_array, &_model_22_dfl_conv_Conv_output_0_weights_array_intq)

/* Tensor #231 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_Reshape_1_output_0_to_chlast_output, AI_STATIC,
  283, 0x1,
  AI_SHAPE_INIT(4, 1, 4, 756, 1), AI_STRIDE_INIT(4, 1, 1, 4, 3024),
  1, &_model_22_dfl_Reshape_1_output_0_to_chlast_output_array, &_model_22_dfl_Reshape_1_output_0_to_chlast_output_array_intq)

/* Tensor #232 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Slice_1_output_0_output, AI_STATIC,
  165, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Slice_1_output_0_output_array, &_model_22_Slice_1_output_0_output_array_intq)

/* Tensor #233 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_dfl_Reshape_1_output_0_to_chlast_output0, AI_STATIC,
  284, 0x1,
  AI_SHAPE_INIT(4, 1, 4, 1, 756), AI_STRIDE_INIT(4, 1, 1, 4, 4),
  1, &_model_22_dfl_Reshape_1_output_0_to_chlast_output_array, &_model_22_dfl_Reshape_1_output_0_to_chlast_output_array_intq)

/* Tensor #234 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Add_1_output_0_output, AI_STATIC,
  152, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Add_1_output_0_output_array, &_model_22_Add_1_output_0_output_array_intq)

/* Tensor #235 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Constant_13_output_0_DequantizeLinear_Output_const, AI_STATIC,
  158, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Constant_13_output_0_DequantizeLinear_Output_const_array, &_model_22_Constant_13_output_0_DequantizeLinear_Output_const_array_intq)

/* Tensor #236 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Slice_output_0_output, AI_STATIC,
  166, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Slice_output_0_output_array, &_model_22_Slice_output_0_output_array_intq)

/* Tensor #237 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Constant_12_output_0_DequantizeLinear_Output_const, AI_STATIC,
  157, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Constant_12_output_0_DequantizeLinear_Output_const_array, &_model_22_Constant_12_output_0_DequantizeLinear_Output_const_array_intq)

/* Tensor #238 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Sub_output_0_output, AI_STATIC,
  168, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Sub_output_0_output_array, &_model_22_Sub_output_0_output_array_intq)

/* Tensor #239 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Sub_1_output_0_output, AI_STATIC,
  167, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Sub_1_output_0_output_array, &_model_22_Sub_1_output_0_output_array_intq)

/* Tensor #240 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Add_2_output_0_output, AI_STATIC,
  153, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Add_2_output_0_output_array, &_model_22_Add_2_output_0_output_array_intq)

/* Tensor #241 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D, AI_STATIC,
  159, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &_model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D_array, &_model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #242 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Div_1_output_0_output, AI_STATIC,
  161, 0x1,
  AI_SHAPE_INIT(4, 1, 2, 1, 756), AI_STRIDE_INIT(4, 1, 1, 2, 2),
  1, &_model_22_Div_1_output_0_output_array, &_model_22_Div_1_output_0_output_array_intq)

/* Tensor #243 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Concat_2_output_0_output, AI_STATIC,
  155, 0x1,
  AI_SHAPE_INIT(4, 1, 4, 1, 756), AI_STRIDE_INIT(4, 1, 1, 4, 4),
  1, &_model_22_Concat_2_output_0_output_array, &_model_22_Concat_2_output_0_output_array_intq)

/* Tensor #244 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D, AI_STATIC,
  160, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 756), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &_model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D_array, &_model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D_array_intq)

/* Tensor #245 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Mul_2_output_0_QuantizeLinear_Input_output, AI_STATIC,
  163, 0x1,
  AI_SHAPE_INIT(4, 1, 4, 1, 756), AI_STRIDE_INIT(4, 1, 1, 4, 4),
  1, &_model_22_Mul_2_output_0_QuantizeLinear_Input_output_array, &_model_22_Mul_2_output_0_QuantizeLinear_Input_output_array_intq)

/* Tensor #246 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output, AI_STATIC,
  162, 0x1,
  AI_SHAPE_INIT(4, 1, 756, 1, 4), AI_STRIDE_INIT(4, 1, 1, 756, 756),
  1, &_model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output_array, &_model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output_array_intq)

/* Tensor #247 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output, AI_STATIC,
  227, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output_array, &_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output_array_intq)

/* Tensor #248 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output, AI_STATIC,
  229, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output_array, &_model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output_array_intq)

/* Tensor #249 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_act_Mul_output_0_output, AI_STATIC,
  226, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_22_cv3_0_cv3_0_0_act_Mul_output_0_output_array, &_model_22_cv3_0_cv3_0_0_act_Mul_output_0_output_array_intq)

/* Tensor #250 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output, AI_STATIC,
  234, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output_array, &_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output_array_intq)

/* Tensor #251 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output, AI_STATIC,
  236, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output_array, &_model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output_array_intq)

/* Tensor #252 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_act_Mul_output_0_output, AI_STATIC,
  233, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 24, 24), AI_STRIDE_INIT(4, 1, 1, 32, 768),
  1, &_model_22_cv3_0_cv3_0_1_act_Mul_output_0_output_array, &_model_22_cv3_0_cv3_0_1_act_Mul_output_0_output_array_intq)

/* Tensor #253 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_bias, AI_STATIC,
  240, 0x0,
  AI_SHAPE_INIT(4, 1, 1, 1, 1), AI_STRIDE_INIT(4, 4, 4, 4, 4),
  1, &_model_22_cv3_0_cv3_0_2_Conv_output_0_bias_array, NULL)

/* Tensor #254 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_output, AI_STATIC,
  241, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 24, 24), AI_STRIDE_INIT(4, 1, 1, 1, 24),
  1, &_model_22_cv3_0_cv3_0_2_Conv_output_0_output_array, &_model_22_cv3_0_cv3_0_2_Conv_output_0_output_array_intq)

/* Tensor #255 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_scratch0, AI_STATIC,
  243, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &_model_22_cv3_0_cv3_0_2_Conv_output_0_scratch0_array, NULL)

/* Tensor #256 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_weights, AI_STATIC,
  244, 0x1,
  AI_SHAPE_INIT(4, 32, 1, 1, 1), AI_STRIDE_INIT(4, 1, 32, 32, 32),
  1, &_model_22_cv3_0_cv3_0_2_Conv_output_0_weights_array, &_model_22_cv3_0_cv3_0_2_Conv_output_0_weights_array_intq)

/* Tensor #257 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Concat_1_output_0_output, AI_STATIC,
  154, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 756), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &_model_22_Concat_1_output_0_output_array, &_model_22_Concat_1_output_0_output_array_intq)

/* Tensor #258 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_output0, AI_STATIC,
  242, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 576), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &_model_22_cv3_0_cv3_0_2_Conv_output_0_output_array, &_model_22_cv3_0_cv3_0_2_Conv_output_0_output_array_intq)

/* Tensor #259 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_output0, AI_STATIC,
  261, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 144), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &_model_22_cv3_1_cv3_1_2_Conv_output_0_output_array, &_model_22_cv3_1_cv3_1_2_Conv_output_0_output_array_intq)

/* Tensor #260 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_output0, AI_STATIC,
  280, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 36), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &_model_22_cv3_2_cv3_2_2_Conv_output_0_output_array, &_model_22_cv3_2_cv3_2_2_Conv_output_0_output_array_intq)

/* Tensor #261 */
AI_TENSOR_OBJ_DECLARE(
  _model_22_Sigmoid_output_0_QuantizeLinear_Input_output, AI_STATIC,
  164, 0x1,
  AI_SHAPE_INIT(4, 1, 1, 1, 756), AI_STRIDE_INIT(4, 1, 1, 1, 1),
  1, &_model_22_Sigmoid_output_0_QuantizeLinear_Input_output_array, &_model_22_Sigmoid_output_0_QuantizeLinear_Input_output_array_intq)



AI_STATIC_CONST ai_i8 _model_0_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -126, -124, -121, -114, -101, -78, -44, -1, 43, 77, 100, 113, 120, 123, 125, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_0_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_0_act_Sigmoid_output_0_nl_params_data, _model_0_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_0_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_0_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_0_act_Sigmoid_output_0_layer, 137,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_0_act_Sigmoid_output_0_chain,
  NULL, &_model_0_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_0_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_0_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_0_conv_Conv_output_0_output, &_model_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_0_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_0_act_Mul_output_0_layer, 140,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_0_act_Mul_output_0_chain,
  NULL, &_model_0_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -126, -126, -125, -124, -123, -122, -120, -118, -116, -113, -109, -104, -97, -90, -81, -71, -59, -46, -32, -16, -1, 15, 31, 45, 58, 70, 80, 89, 96, 103, 108, 112, 115, 117, 119, 121, 122, 123, 124, 125, 125, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_1_act_Sigmoid_output_0_nl_params_data, _model_1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_1_act_Sigmoid_output_0_layer, 146,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_1_act_Sigmoid_output_0_chain,
  NULL, &_model_1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_1_conv_Conv_output_0_output, &_model_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_1_act_Mul_output_0_layer, 149,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_1_act_Mul_output_0_chain,
  NULL, &_model_1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_2_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -126, -126, -125, -124, -123, -122, -120, -117, -114, -111, -106, -100, -93, -84, -74, -62, -48, -33, -17, -1, 16, 32, 47, 61, 73, 83, 92, 99, 105, 110, 113, 116, 119, 121, 122, 123, 124, 125, 125, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_2_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_2_cv1_act_Sigmoid_output_0_nl_params_data, _model_2_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_cv1_act_Sigmoid_output_0_layer, 155,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_2_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_2_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_2_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_2_cv1_conv_Conv_output_0_output, &_model_2_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_cv1_act_Mul_output_0_layer, 158,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_2_cv1_act_Mul_output_0_chain,
  NULL, &_model_2_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_Split_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_2_Split_output_0_output0, &_model_2_Split_output_0_output1),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_Split_output_0_num_or_size_splits),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_Split_output_0_layer, 161,
  SPLIT_TYPE, 0x0, NULL,
  split, forward_split,
  &_model_2_Split_output_0_chain,
  NULL, &_model_2_Split_output_0_layer, AI_STATIC, 
  .outer_elems = 2304, 
  .outer_elems_stride = 16, 
)


AI_STATIC_CONST ai_i8 _model_2_m_0_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -123, -123, -122, -122, -121, -120, -120, -119, -118, -117, -115, -114, -113, -111, -109, -107, -105, -103, -101, -98, -95, -92, -88, -85, -81, -77, -72, -67, -62, -57, -51, -46, -40, -33, -27, -21, -14, -7, 0, 6, 13, 20, 26, 32, 39, 45, 50, 56, 61, 66, 71, 76, 80, 84, 87, 91, 94, 97, 100, 102, 104, 106, 108, 110, 112, 113, 114, 116, 117, 118, 119, 119, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_2_m_0_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_2_m_0_cv1_act_Sigmoid_output_0_nl_params_data, _model_2_m_0_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_m_0_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_m_0_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_m_0_cv1_act_Sigmoid_output_0_layer, 169,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_2_m_0_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_2_m_0_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_2_m_0_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_m_0_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_2_m_0_cv1_conv_Conv_output_0_output, &_model_2_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_m_0_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_m_0_cv1_act_Mul_output_0_layer, 172,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_2_m_0_cv1_act_Mul_output_0_chain,
  NULL, &_model_2_m_0_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_2_m_0_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -125, -125, -125, -124, -124, -123, -123, -122, -121, -120, -119, -118, -116, -115, -113, -111, -109, -106, -103, -100, -97, -93, -89, -84, -80, -74, -68, -62, -55, -48, -41, -33, -25, -17, -9, 0, 8, 16, 24, 32, 40, 47, 54, 61, 67, 73, 79, 83, 88, 92, 96, 99, 102, 105, 108, 110, 112, 114, 115, 117, 118, 119, 120, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_2_m_0_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_2_m_0_cv2_act_Sigmoid_output_0_nl_params_data, _model_2_m_0_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_m_0_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_m_0_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_m_0_cv2_act_Sigmoid_output_0_layer, 178,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_2_m_0_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_2_m_0_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_2_m_0_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_m_0_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_2_m_0_cv2_conv_Conv_output_0_output, &_model_2_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_m_0_cv2_act_Mul_output_0_layer, 181,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_2_m_0_cv2_act_Mul_output_0_chain,
  NULL, &_model_2_m_0_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_m_0_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_2_Split_output_0_output1, &_model_2_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_m_0_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_m_0_Add_output_0_layer, 184,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_2_m_0_Add_output_0_chain,
  NULL, &_model_2_m_0_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_2_Split_output_0_output0, &_model_2_Split_output_0_output1, &_model_2_m_0_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_Concat_output_0_layer, 187,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_2_Concat_output_0_chain,
  NULL, &_model_2_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_2_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -125, -125, -124, -124, -123, -123, -122, -121, -120, -119, -117, -116, -114, -112, -110, -107, -104, -101, -97, -93, -89, -84, -78, -72, -66, -59, -52, -44, -36, -27, -18, -10, 0, 9, 17, 26, 35, 43, 51, 58, 65, 71, 77, 83, 88, 92, 96, 100, 103, 106, 109, 111, 113, 115, 116, 118, 119, 120, 121, 122, 122, 123, 123, 124, 124, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_2_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_2_cv2_act_Sigmoid_output_0_nl_params_data, _model_2_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_cv2_act_Sigmoid_output_0_layer, 193,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_2_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_2_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_2_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_2_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_2_cv2_conv_Conv_output_0_output, &_model_2_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_2_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_2_cv2_act_Mul_output_0_layer, 196,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_2_cv2_act_Mul_output_0_chain,
  NULL, &_model_2_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_3_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -125, -125, -125, -125, -124, -124, -123, -123, -122, -122, -121, -120, -119, -118, -117, -116, -115, -113, -112, -110, -108, -106, -104, -101, -99, -96, -93, -89, -85, -82, -77, -73, -68, -63, -58, -52, -46, -40, -34, -27, -21, -14, -7, 0, 6, 13, 20, 26, 33, 39, 45, 51, 57, 62, 67, 72, 76, 81, 84, 88, 92, 95, 98, 100, 103, 105, 107, 109, 111, 112, 114, 115, 116, 117, 118, 119, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_3_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_3_act_Sigmoid_output_0_nl_params_data, _model_3_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_3_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_3_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_3_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_3_act_Sigmoid_output_0_layer, 202,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_3_act_Sigmoid_output_0_chain,
  NULL, &_model_3_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_3_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_3_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_3_conv_Conv_output_0_output, &_model_3_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_3_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_3_act_Mul_output_0_layer, 205,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_3_act_Mul_output_0_chain,
  NULL, &_model_3_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_4_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -123, -123, -122, -122, -121, -120, -120, -119, -118, -116, -115, -114, -112, -110, -109, -107, -104, -102, -99, -96, -93, -90, -86, -82, -78, -74, -69, -64, -58, -53, -47, -41, -34, -28, -21, -14, -7, 0, 6, 13, 20, 27, 33, 40, 46, 52, 57, 63, 68, 73, 77, 81, 85, 89, 92, 95, 98, 101, 103, 106, 108, 109, 111, 113, 114, 115, 117, 118, 119, 119, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_4_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_4_cv1_act_Sigmoid_output_0_nl_params_data, _model_4_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_cv1_act_Sigmoid_output_0_layer, 211,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_4_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_4_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_4_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_cv1_conv_Conv_output_0_output, &_model_4_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_cv1_act_Mul_output_0_layer, 214,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_4_cv1_act_Mul_output_0_chain,
  NULL, &_model_4_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_Split_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_Split_output_0_output0, &_model_4_Split_output_0_output1),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_Split_output_0_num_or_size_splits),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_Split_output_0_layer, 217,
  SPLIT_TYPE, 0x0, NULL,
  split, forward_split,
  &_model_4_Split_output_0_chain,
  NULL, &_model_4_Split_output_0_layer, AI_STATIC, 
  .outer_elems = 576, 
  .outer_elems_stride = 32, 
)


AI_STATIC_CONST ai_i8 _model_4_m_0_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -123, -123, -122, -122, -121, -120, -119, -118, -117, -116, -115, -113, -112, -110, -108, -106, -104, -101, -99, -96, -93, -89, -86, -82, -77, -73, -68, -63, -58, -52, -46, -40, -34, -27, -21, -14, -7, 0, 6, 13, 20, 26, 33, 39, 45, 51, 57, 62, 67, 72, 76, 81, 85, 88, 92, 95, 98, 100, 103, 105, 107, 109, 111, 112, 114, 115, 116, 117, 118, 119, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_4_m_0_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_4_m_0_cv1_act_Sigmoid_output_0_nl_params_data, _model_4_m_0_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_0_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_0_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_0_cv1_act_Sigmoid_output_0_layer, 225,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_4_m_0_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_4_m_0_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_4_m_0_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_0_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_m_0_cv1_conv_Conv_output_0_output, &_model_4_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_0_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_0_cv1_act_Mul_output_0_layer, 228,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_4_m_0_cv1_act_Mul_output_0_chain,
  NULL, &_model_4_m_0_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_4_m_0_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -123, -123, -122, -122, -121, -120, -119, -119, -118, -116, -115, -114, -112, -111, -109, -107, -105, -103, -100, -97, -95, -91, -88, -84, -80, -76, -72, -67, -62, -57, -51, -45, -39, -33, -27, -20, -14, -7, 0, 6, 13, 19, 26, 32, 38, 44, 50, 56, 61, 66, 71, 75, 79, 83, 87, 90, 94, 96, 99, 102, 104, 106, 108, 110, 111, 113, 114, 115, 117, 118, 118, 119, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_4_m_0_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_4_m_0_cv2_act_Sigmoid_output_0_nl_params_data, _model_4_m_0_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_0_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_0_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_0_cv2_act_Sigmoid_output_0_layer, 234,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_4_m_0_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_4_m_0_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_4_m_0_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_0_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_m_0_cv2_conv_Conv_output_0_output, &_model_4_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_0_cv2_act_Mul_output_0_layer, 237,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_4_m_0_cv2_act_Mul_output_0_chain,
  NULL, &_model_4_m_0_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_0_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_Split_output_0_output1, &_model_4_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_0_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_0_Add_output_0_layer, 240,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_4_m_0_Add_output_0_chain,
  NULL, &_model_4_m_0_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_4_m_1_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -125, -125, -125, -125, -124, -124, -123, -123, -122, -122, -121, -121, -120, -119, -118, -117, -116, -115, -113, -112, -110, -109, -107, -105, -103, -100, -98, -95, -92, -89, -85, -81, -78, -73, -69, -64, -59, -54, -49, -43, -38, -32, -26, -19, -13, -7, 0, 6, 12, 18, 25, 31, 37, 42, 48, 53, 58, 63, 68, 72, 77, 80, 84, 88, 91, 94, 97, 99, 102, 104, 106, 108, 109, 111, 112, 114, 115, 116, 117, 118, 119, 120, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_4_m_1_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_4_m_1_cv1_act_Sigmoid_output_0_nl_params_data, _model_4_m_1_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_1_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_1_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_1_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_1_cv1_act_Sigmoid_output_0_layer, 246,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_4_m_1_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_4_m_1_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_4_m_1_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_1_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_m_1_cv1_conv_Conv_output_0_output, &_model_4_m_1_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_1_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_1_cv1_act_Mul_output_0_layer, 249,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_4_m_1_cv1_act_Mul_output_0_chain,
  NULL, &_model_4_m_1_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_4_m_1_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -124, -123, -123, -122, -122, -121, -120, -119, -118, -118, -116, -115, -114, -113, -111, -109, -108, -106, -103, -101, -98, -96, -93, -89, -86, -82, -78, -74, -70, -65, -60, -55, -50, -44, -38, -32, -26, -20, -13, -7, 0, 6, 12, 19, 25, 31, 37, 43, 49, 54, 59, 64, 69, 73, 77, 81, 85, 88, 92, 95, 97, 100, 102, 105, 107, 108, 110, 112, 113, 114, 115, 117, 117, 118, 119, 120, 121, 121, 122, 122, 123, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_4_m_1_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_4_m_1_cv2_act_Sigmoid_output_0_nl_params_data, _model_4_m_1_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_1_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_1_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_1_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_1_cv2_act_Sigmoid_output_0_layer, 255,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_4_m_1_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_4_m_1_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_4_m_1_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_1_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_m_1_cv2_conv_Conv_output_0_output, &_model_4_m_1_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_1_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_1_cv2_act_Mul_output_0_layer, 258,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_4_m_1_cv2_act_Mul_output_0_chain,
  NULL, &_model_4_m_1_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_m_1_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_m_0_Add_output_0_output, &_model_4_m_1_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_m_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_m_1_Add_output_0_layer, 261,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_4_m_1_Add_output_0_chain,
  NULL, &_model_4_m_1_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 4, &_model_4_Split_output_0_output0, &_model_4_Split_output_0_output1, &_model_4_m_0_Add_output_0_output, &_model_4_m_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_Concat_output_0_layer, 264,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_4_Concat_output_0_chain,
  NULL, &_model_4_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_4_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -124, -123, -123, -122, -122, -121, -120, -119, -119, -118, -117, -115, -114, -113, -111, -110, -108, -106, -104, -102, -99, -97, -94, -91, -88, -84, -81, -77, -73, -68, -64, -59, -54, -48, -43, -37, -31, -25, -19, -13, -7, 0, 6, 12, 18, 24, 30, 36, 42, 47, 53, 58, 63, 67, 72, 76, 80, 83, 87, 90, 93, 96, 98, 101, 103, 105, 107, 109, 110, 112, 113, 114, 116, 117, 118, 118, 119, 120, 121, 121, 122, 122, 123, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_4_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_4_cv2_act_Sigmoid_output_0_nl_params_data, _model_4_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_cv2_act_Sigmoid_output_0_layer, 270,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_4_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_4_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_4_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_4_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_4_cv2_conv_Conv_output_0_output, &_model_4_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_4_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_4_cv2_act_Mul_output_0_layer, 273,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_4_cv2_act_Mul_output_0_chain,
  NULL, &_model_4_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_5_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -122, -122, -121, -121, -120, -120, -119, -119, -118, -117, -116, -115, -114, -113, -112, -111, -110, -109, -107, -106, -104, -103, -101, -99, -97, -95, -92, -90, -88, -85, -82, -79, -76, -73, -70, -66, -62, -59, -55, -51, -47, -42, -38, -34, -29, -24, -20, -15, -10, -5, 0, 4, 9, 14, 19, 23, 28, 33, 37, 41, 46, 50, 54, 58, 62, 65, 69, 72, 75, 78, 81, 84, 87, 89, 91, 94, 96, 98, 100, 102, 103, 105, 106, 108, 109, 110, 111, 112, 114, 114, 115, 116, 117, 118, 118, 119, 119, 120, 121, 121, 121, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_5_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_5_act_Sigmoid_output_0_nl_params_data, _model_5_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_5_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_5_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_5_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_5_act_Sigmoid_output_0_layer, 279,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_5_act_Sigmoid_output_0_chain,
  NULL, &_model_5_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_5_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_5_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_5_conv_Conv_output_0_output, &_model_5_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_5_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_5_act_Mul_output_0_layer, 282,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_5_act_Mul_output_0_chain,
  NULL, &_model_5_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_6_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -122, -122, -122, -121, -121, -120, -119, -119, -118, -117, -117, -116, -115, -114, -113, -112, -111, -109, -108, -107, -105, -104, -102, -100, -98, -96, -94, -92, -89, -87, -84, -81, -78, -75, -72, -69, -65, -62, -58, -54, -50, -46, -42, -38, -33, -29, -24, -19, -15, -10, -5, 0, 4, 9, 14, 18, 23, 28, 32, 37, 41, 45, 49, 53, 57, 61, 64, 68, 71, 74, 77, 80, 83, 86, 88, 91, 93, 95, 97, 99, 101, 103, 104, 106, 107, 108, 110, 111, 112, 113, 114, 115, 116, 116, 117, 118, 118, 119, 120, 120, 121, 121, 121, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_6_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_6_cv1_act_Sigmoid_output_0_nl_params_data, _model_6_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_cv1_act_Sigmoid_output_0_layer, 288,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_6_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_6_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_6_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_cv1_conv_Conv_output_0_output, &_model_6_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_cv1_act_Mul_output_0_layer, 291,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_6_cv1_act_Mul_output_0_chain,
  NULL, &_model_6_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_Split_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_Split_output_0_output0, &_model_6_Split_output_0_output1),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_Split_output_0_num_or_size_splits),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_Split_output_0_layer, 294,
  SPLIT_TYPE, 0x0, NULL,
  split, forward_split,
  &_model_6_Split_output_0_chain,
  NULL, &_model_6_Split_output_0_layer, AI_STATIC, 
  .outer_elems = 144, 
  .outer_elems_stride = 64, 
)


AI_STATIC_CONST ai_i8 _model_6_m_0_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -122, -122, -122, -121, -121, -120, -120, -119, -118, -118, -117, -116, -115, -114, -113, -112, -111, -110, -109, -108, -106, -105, -103, -102, -100, -98, -96, -94, -92, -89, -87, -85, -82, -79, -76, -73, -70, -67, -63, -60, -56, -52, -48, -44, -40, -36, -32, -28, -23, -19, -14, -10, -5, 0, 4, 9, 13, 18, 22, 27, 31, 35, 39, 43, 47, 51, 55, 59, 62, 66, 69, 72, 75, 78, 81, 84, 86, 88, 91, 93, 95, 97, 99, 101, 102, 104, 105, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 117, 118, 119, 119, 120, 120, 121, 121, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_6_m_0_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_6_m_0_cv1_act_Sigmoid_output_0_nl_params_data, _model_6_m_0_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_0_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_0_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_0_cv1_act_Sigmoid_output_0_layer, 302,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_6_m_0_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_6_m_0_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_6_m_0_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_0_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_m_0_cv1_conv_Conv_output_0_output, &_model_6_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_0_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_0_cv1_act_Mul_output_0_layer, 305,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_6_m_0_cv1_act_Mul_output_0_chain,
  NULL, &_model_6_m_0_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_6_m_0_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -123, -123, -122, -122, -122, -121, -121, -120, -120, -120, -119, -119, -118, -117, -117, -116, -116, -115, -114, -113, -112, -112, -111, -110, -109, -108, -106, -105, -104, -103, -101, -100, -99, -97, -95, -94, -92, -90, -88, -86, -84, -82, -80, -77, -75, -72, -70, -67, -65, -62, -59, -56, -53, -50, -46, -43, -40, -36, -33, -30, -26, -22, -19, -15, -11, -8, -4, 0, 3, 7, 11, 14, 18, 22, 25, 29, 32, 36, 39, 42, 46, 49, 52, 55, 58, 61, 64, 66, 69, 72, 74, 77, 79, 81, 83, 85, 87, 89, 91, 93, 95, 96, 98, 99, 101, 102, 103, 105, 106, 107, 108, 109, 110, 111, 112, 113, 113, 114, 115, 115, 116, 117, 117, 118, 118, 119, 119, 120, 120, 120, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_6_m_0_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_6_m_0_cv2_act_Sigmoid_output_0_nl_params_data, _model_6_m_0_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_0_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_0_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_0_cv2_act_Sigmoid_output_0_layer, 311,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_6_m_0_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_6_m_0_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_6_m_0_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_0_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_m_0_cv2_conv_Conv_output_0_output, &_model_6_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_0_cv2_act_Mul_output_0_layer, 314,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_6_m_0_cv2_act_Mul_output_0_chain,
  NULL, &_model_6_m_0_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_0_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_Split_output_0_output1, &_model_6_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_0_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_0_Add_output_0_layer, 317,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_6_m_0_Add_output_0_chain,
  NULL, &_model_6_m_0_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_6_m_1_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -119, -119, -118, -118, -117, -116, -116, -115, -114, -114, -113, -112, -111, -110, -109, -108, -107, -106, -105, -103, -102, -101, -99, -98, -96, -95, -93, -91, -89, -87, -85, -83, -81, -79, -77, -74, -72, -69, -67, -64, -61, -58, -55, -52, -49, -46, -43, -39, -36, -33, -29, -26, -22, -19, -15, -11, -8, -4, 0, 3, 7, 11, 14, 18, 21, 25, 28, 32, 35, 39, 42, 45, 48, 51, 55, 57, 60, 63, 66, 69, 71, 74, 76, 78, 80, 83, 85, 87, 89, 91, 92, 94, 96, 97, 99, 100, 101, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 114, 115, 116, 116, 117, 117, 118, 119, 119, 119, 120, 120, 121, 121, 121, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_6_m_1_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_6_m_1_cv1_act_Sigmoid_output_0_nl_params_data, _model_6_m_1_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_1_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_1_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_1_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_1_cv1_act_Sigmoid_output_0_layer, 323,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_6_m_1_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_6_m_1_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_6_m_1_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_1_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_m_1_cv1_conv_Conv_output_0_output, &_model_6_m_1_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_1_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_1_cv1_act_Mul_output_0_layer, 326,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_6_m_1_cv1_act_Mul_output_0_chain,
  NULL, &_model_6_m_1_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_6_m_1_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -123, -122, -122, -121, -121, -121, -120, -120, -119, -118, -118, -117, -116, -116, -115, -114, -113, -112, -111, -110, -109, -108, -107, -106, -104, -103, -101, -100, -98, -96, -94, -92, -90, -88, -86, -84, -81, -79, -76, -73, -70, -67, -64, -61, -58, -55, -51, -48, -44, -40, -37, -33, -29, -25, -21, -17, -13, -9, -5, 0, 4, 8, 12, 16, 20, 24, 28, 32, 36, 40, 43, 47, 50, 54, 57, 60, 64, 67, 70, 72, 75, 78, 80, 83, 85, 87, 89, 91, 93, 95, 97, 99, 100, 102, 103, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 116, 117, 118, 118, 119, 119, 120, 120, 121, 121, 121, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_6_m_1_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_6_m_1_cv2_act_Sigmoid_output_0_nl_params_data, _model_6_m_1_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_1_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_1_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_1_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_1_cv2_act_Sigmoid_output_0_layer, 332,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_6_m_1_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_6_m_1_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_6_m_1_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_1_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_m_1_cv2_conv_Conv_output_0_output, &_model_6_m_1_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_1_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_1_cv2_act_Mul_output_0_layer, 335,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_6_m_1_cv2_act_Mul_output_0_chain,
  NULL, &_model_6_m_1_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_m_1_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_m_0_Add_output_0_output, &_model_6_m_1_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_m_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_m_1_Add_output_0_layer, 338,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_6_m_1_Add_output_0_chain,
  NULL, &_model_6_m_1_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 4, &_model_6_Split_output_0_output0, &_model_6_Split_output_0_output1, &_model_6_m_0_Add_output_0_output, &_model_6_m_1_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_Concat_output_0_layer, 341,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_6_Concat_output_0_chain,
  NULL, &_model_6_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_6_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -123, -122, -122, -122, -121, -121, -120, -120, -119, -119, -118, -117, -117, -116, -115, -115, -114, -113, -112, -111, -110, -109, -108, -106, -105, -104, -102, -101, -99, -97, -96, -94, -92, -90, -88, -85, -83, -81, -78, -75, -73, -70, -67, -64, -61, -58, -54, -51, -47, -44, -40, -36, -33, -29, -25, -21, -17, -13, -9, -5, 0, 4, 8, 12, 16, 20, 24, 28, 32, 35, 39, 43, 46, 50, 53, 57, 60, 63, 66, 69, 72, 74, 77, 80, 82, 84, 87, 89, 91, 93, 95, 96, 98, 100, 101, 103, 104, 105, 107, 108, 109, 110, 111, 112, 113, 114, 114, 115, 116, 117, 117, 118, 118, 119, 119, 120, 120, 121, 121, 121, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_6_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_6_cv2_act_Sigmoid_output_0_nl_params_data, _model_6_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_cv2_act_Sigmoid_output_0_layer, 347,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_6_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_6_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_6_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_6_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_6_cv2_conv_Conv_output_0_output, &_model_6_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_6_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_6_cv2_act_Mul_output_0_layer, 350,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_6_cv2_act_Mul_output_0_chain,
  NULL, &_model_6_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_7_act_Sigmoid_output_0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -120, -119, -119, -118, -118, -117, -117, -116, -115, -115, -114, -114, -113, -112, -111, -111, -110, -109, -108, -107, -106, -105, -104, -103, -102, -100, -99, -98, -97, -95, -94, -92, -91, -89, -87, -85, -84, -82, -80, -78, -76, -74, -71, -69, -67, -65, -62, -60, -57, -54, -52, -49, -46, -44, -41, -38, -35, -32, -29, -26, -23, -19, -16, -13, -10, -7, -4, 0, 3, 6, 9, 12, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46, 48, 51, 54, 56, 59, 61, 64, 66, 68, 71, 73, 75, 77, 79, 81, 83, 85, 86, 88, 90, 91, 93, 94, 96, 97, 98, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 111, 112, 113, 113, 114, 115, 115, 116, 116, 117, 117, 118, 118, 119, 119, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_7_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_7_act_Sigmoid_output_0_nl_params_data, _model_7_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_7_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_7_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_7_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_7_act_Sigmoid_output_0_layer, 356,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_7_act_Sigmoid_output_0_chain,
  NULL, &_model_7_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_7_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_7_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_7_conv_Conv_output_0_output, &_model_7_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_7_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_7_act_Mul_output_0_layer, 359,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_7_act_Mul_output_0_chain,
  NULL, &_model_7_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_8_cv1_act_Sigmoid_output_0_nl_params_data[] = { -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -122, -122, -122, -122, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -117, -117, -117, -116, -116, -115, -115, -114, -113, -113, -112, -112, -111, -110, -109, -109, -108, -107, -106, -105, -104, -103, -103, -101, -100, -99, -98, -97, -96, -95, -93, -92, -91, -89, -88, -86, -85, -83, -82, -80, -78, -77, -75, -73, -71, -69, -67, -65, -63, -61, -59, -57, -54, -52, -50, -47, -45, -43, -40, -38, -35, -33, -30, -27, -25, -22, -19, -17, -14, -11, -8, -6, -3, 0, 3, 5, 8, 11, 14, 16, 19, 22, 24, 27, 30, 32, 35, 37, 40, 42, 45, 47, 49, 52, 54, 56, 59, 61, 63, 65, 67, 69, 71, 73, 74, 76, 78, 80, 81, 83, 85, 86, 88, 89, 90, 92, 93, 94, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 108, 109, 110, 110, 111, 112, 112, 113, 114, 114, 115, 115, 116, 116, 117, 117, 118, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_8_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_8_cv1_act_Sigmoid_output_0_nl_params_data, _model_8_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_cv1_act_Sigmoid_output_0_layer, 365,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_8_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_8_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_8_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_8_cv1_conv_Conv_output_0_output, &_model_8_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_cv1_act_Mul_output_0_layer, 368,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_8_cv1_act_Mul_output_0_chain,
  NULL, &_model_8_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_Split_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_8_Split_output_0_output0, &_model_8_Split_output_0_output1),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_Split_output_0_num_or_size_splits),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_Split_output_0_layer, 371,
  SPLIT_TYPE, 0x0, NULL,
  split, forward_split,
  &_model_8_Split_output_0_chain,
  NULL, &_model_8_Split_output_0_layer, AI_STATIC, 
  .outer_elems = 36, 
  .outer_elems_stride = 64, 
)


AI_STATIC_CONST ai_i8 _model_8_m_0_cv1_act_Sigmoid_output_0_nl_params_data[] = { -122, -122, -122, -122, -122, -121, -121, -121, -121, -120, -120, -120, -120, -120, -119, -119, -119, -118, -118, -118, -118, -117, -117, -117, -116, -116, -116, -115, -115, -114, -114, -114, -113, -113, -112, -112, -111, -111, -111, -110, -109, -109, -108, -108, -107, -107, -106, -105, -105, -104, -103, -103, -102, -101, -101, -100, -99, -98, -97, -97, -96, -95, -94, -93, -92, -91, -90, -89, -88, -87, -86, -85, -84, -83, -82, -81, -79, -78, -77, -76, -74, -73, -72, -70, -69, -68, -66, -65, -63, -62, -60, -59, -57, -55, -54, -52, -51, -49, -47, -45, -44, -42, -40, -38, -37, -35, -33, -31, -29, -27, -26, -24, -22, -20, -18, -16, -14, -12, -10, -8, -6, -4, -2, 0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 21, 23, 25, 27, 29, 31, 33, 35, 37, 38, 40, 42, 44, 46, 47, 49, 51, 53, 54, 56, 57, 59, 61, 62, 64, 65, 67, 68, 70, 71, 73, 74, 75, 77, 78, 79, 80, 82, 83, 84, 85, 86, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 99, 100, 101, 102, 103, 103, 104, 105, 106, 106, 107, 108, 108, 109, 110, 110, 111, 111, 112, 113, 113, 114, 114, 115, 115, 116, 116, 116, 117, 117, 118, 118, 118, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_8_m_0_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_8_m_0_cv1_act_Sigmoid_output_0_nl_params_data, _model_8_m_0_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_m_0_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_m_0_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_m_0_cv1_act_Sigmoid_output_0_layer, 379,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_8_m_0_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_8_m_0_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_8_m_0_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_m_0_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_8_m_0_cv1_conv_Conv_output_0_output, &_model_8_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_m_0_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_m_0_cv1_act_Mul_output_0_layer, 382,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_8_m_0_cv1_act_Mul_output_0_chain,
  NULL, &_model_8_m_0_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_8_m_0_cv2_act_Sigmoid_output_0_nl_params_data[] = { -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -122, -122, -122, -122, -122, -121, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -118, -117, -117, -117, -116, -116, -115, -115, -114, -114, -113, -113, -112, -112, -111, -110, -110, -109, -108, -108, -107, -106, -106, -105, -104, -103, -102, -101, -100, -99, -99, -98, -96, -95, -94, -93, -92, -91, -90, -88, -87, -86, -84, -83, -82, -80, -79, -77, -76, -74, -72, -71, -69, -67, -66, -64, -62, -60, -58, -56, -54, -52, -50, -48, -46, -44, -42, -40, -37, -35, -33, -31, -28, -26, -24, -21, -19, -17, -14, -12, -9, -7, -5, -2, 0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24, 26, 29, 31, 33, 36, 38, 40, 42, 44, 46, 49, 51, 53, 55, 57, 59, 60, 62, 64, 66, 68, 69, 71, 73, 74, 76, 78, 79, 81, 82, 83, 85, 86, 88, 89, 90, 91, 92, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 103, 104, 105, 106, 107, 107, 108, 109, 109, 110, 111, 111, 112, 113, 113, 114, 114, 115, 115, 116, 116, 116, 117, 117, 118, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_8_m_0_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_8_m_0_cv2_act_Sigmoid_output_0_nl_params_data, _model_8_m_0_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_m_0_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_m_0_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_m_0_cv2_act_Sigmoid_output_0_layer, 388,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_8_m_0_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_8_m_0_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_8_m_0_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_m_0_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_8_m_0_cv2_conv_Conv_output_0_output, &_model_8_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_m_0_cv2_act_Mul_output_0_layer, 391,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_8_m_0_cv2_act_Mul_output_0_chain,
  NULL, &_model_8_m_0_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_m_0_Add_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_8_Split_output_0_output1, &_model_8_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_m_0_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_m_0_Add_output_0_layer, 394,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_8_m_0_Add_output_0_chain,
  NULL, &_model_8_m_0_Add_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_8_Split_output_0_output0, &_model_8_Split_output_0_output1, &_model_8_m_0_Add_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_Concat_output_0_layer, 397,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_8_Concat_output_0_chain,
  NULL, &_model_8_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_8_cv2_act_Sigmoid_output_0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -122, -122, -122, -122, -121, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -118, -117, -117, -116, -116, -115, -115, -114, -114, -113, -113, -112, -111, -111, -110, -109, -109, -108, -107, -106, -106, -105, -104, -103, -102, -101, -100, -99, -98, -97, -96, -95, -93, -92, -91, -90, -88, -87, -86, -84, -83, -81, -79, -78, -76, -75, -73, -71, -69, -67, -65, -64, -62, -60, -57, -55, -53, -51, -49, -47, -44, -42, -40, -37, -35, -33, -30, -28, -25, -23, -20, -18, -15, -13, -10, -7, -5, -2, 0, 3, 6, 8, 11, 13, 16, 18, 21, 24, 26, 29, 31, 33, 36, 38, 41, 43, 45, 47, 50, 52, 54, 56, 58, 60, 62, 64, 66, 68, 70, 72, 74, 75, 77, 79, 80, 82, 83, 85, 86, 88, 89, 90, 92, 93, 94, 95, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 106, 107, 108, 109, 109, 110, 111, 112, 112, 113, 113, 114, 115, 115, 116, 116, 117, 117, 117, 118, 118, 119, 119, 119, 120, 120, 121, 121, 121, 121, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_8_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_8_cv2_act_Sigmoid_output_0_nl_params_data, _model_8_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_cv2_act_Sigmoid_output_0_layer, 403,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_8_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_8_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_8_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_8_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_8_cv2_conv_Conv_output_0_output, &_model_8_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_8_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_8_cv2_act_Mul_output_0_layer, 406,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_8_cv2_act_Mul_output_0_chain,
  NULL, &_model_8_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_9_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 4, &_model_9_cv1_conv_Conv_output_0_output, &_model_9_m_MaxPool_output_0_output, &_model_9_m_1_MaxPool_output_0_output, &_model_9_m_2_MaxPool_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_9_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_9_Concat_output_0_layer, 421,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_9_Concat_output_0_chain,
  NULL, &_model_9_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_9_cv2_act_Sigmoid_output_0_nl_params_data[] = { -123, -123, -122, -122, -122, -122, -122, -121, -121, -121, -121, -121, -120, -120, -120, -120, -119, -119, -119, -119, -118, -118, -118, -117, -117, -117, -116, -116, -116, -115, -115, -114, -114, -114, -113, -113, -112, -112, -111, -111, -110, -110, -109, -109, -108, -107, -107, -106, -106, -105, -104, -104, -103, -102, -101, -101, -100, -99, -98, -97, -96, -95, -95, -94, -93, -92, -91, -90, -89, -88, -86, -85, -84, -83, -82, -81, -79, -78, -77, -75, -74, -73, -71, -70, -69, -67, -66, -64, -62, -61, -59, -58, -56, -54, -53, -51, -49, -48, -46, -44, -42, -40, -38, -37, -35, -33, -31, -29, -27, -25, -23, -21, -19, -17, -15, -13, -11, -9, -7, -5, -3, -1, 1, 3, 5, 7, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 35, 37, 39, 41, 43, 45, 47, 48, 50, 52, 54, 55, 57, 59, 60, 62, 64, 65, 67, 68, 70, 71, 73, 74, 75, 77, 78, 79, 81, 82, 83, 84, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 102, 103, 104, 105, 105, 106, 107, 108, 108, 109, 109, 110, 111, 111, 112, 112, 113, 113, 114, 114, 115, 115, 116, 116, 117, 117, 117, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_9_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_9_cv2_act_Sigmoid_output_0_nl_params_data, _model_9_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_9_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_9_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_9_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_9_cv2_act_Sigmoid_output_0_layer, 427,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_9_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_9_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_9_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_9_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_9_cv2_conv_Conv_output_0_output, &_model_9_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_9_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_9_cv2_act_Mul_output_0_layer, 430,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_9_cv2_act_Mul_output_0_chain,
  NULL, &_model_9_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float _model_10_Resize_output_0_scales_data[] = { 2.0, 2.0, 1.0, 1.0 };
AI_ARRAY_OBJ_DECLARE(
    _model_10_Resize_output_0_scales, AI_ARRAY_FORMAT_FLOAT,
    _model_10_Resize_output_0_scales_data, _model_10_Resize_output_0_scales_data, 4, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_10_Resize_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_9_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_10_Resize_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_10_Resize_output_0_layer, 433,
  UPSAMPLE_TYPE, 0x0, NULL,
  upsample, forward_upsample_nearest,
  &_model_10_Resize_output_0_chain,
  NULL, &_model_10_Resize_output_0_layer, AI_STATIC, 
  .scales = &_model_10_Resize_output_0_scales, 
  .center = false, 
  .mode = AI_UPSAMPLE_NEAREST, 
  .nearest_mode = AI_ROUND_FLOOR, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_11_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_10_Resize_output_0_output, &_model_6_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_11_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_11_Concat_output_0_layer, 436,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_11_Concat_output_0_chain,
  NULL, &_model_11_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_12_cv1_act_Sigmoid_output_0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -123, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -120, -119, -119, -118, -118, -117, -117, -116, -115, -115, -114, -113, -113, -112, -111, -110, -109, -108, -107, -106, -105, -104, -103, -102, -101, -99, -98, -97, -95, -94, -92, -91, -89, -87, -85, -84, -82, -80, -77, -75, -73, -71, -69, -66, -64, -61, -59, -56, -53, -50, -48, -45, -42, -39, -36, -33, -30, -26, -23, -20, -17, -14, -10, -7, -4, 0, 3, 6, 10, 13, 16, 19, 23, 26, 29, 32, 35, 38, 41, 44, 47, 50, 52, 55, 58, 60, 63, 65, 68, 70, 72, 75, 77, 79, 81, 83, 85, 86, 88, 90, 91, 93, 95, 96, 97, 99, 100, 101, 102, 104, 105, 106, 107, 108, 109, 109, 110, 111, 112, 113, 113, 114, 115, 115, 116, 116, 117, 117, 118, 118, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_12_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_12_cv1_act_Sigmoid_output_0_nl_params_data, _model_12_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_cv1_act_Sigmoid_output_0_layer, 442,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_12_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_12_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_12_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_12_cv1_conv_Conv_output_0_output, &_model_12_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_cv1_act_Mul_output_0_layer, 445,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_12_cv1_act_Mul_output_0_chain,
  NULL, &_model_12_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_Split_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_12_Split_output_0_output0, &_model_12_Split_output_0_output1),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_Split_output_0_num_or_size_splits),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_Split_output_0_layer, 448,
  SPLIT_TYPE, 0x0, NULL,
  split, forward_split,
  &_model_12_Split_output_0_chain,
  NULL, &_model_12_Split_output_0_layer, AI_STATIC, 
  .outer_elems = 144, 
  .outer_elems_stride = 64, 
)


AI_STATIC_CONST ai_i8 _model_12_m_0_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -119, -119, -118, -118, -117, -116, -116, -115, -114, -113, -112, -112, -111, -110, -109, -108, -106, -105, -104, -103, -101, -100, -98, -97, -95, -93, -91, -89, -87, -85, -83, -81, -79, -76, -74, -71, -69, -66, -63, -60, -57, -54, -51, -47, -44, -41, -37, -34, -30, -27, -23, -19, -16, -12, -8, -4, 0, 3, 7, 11, 15, 18, 22, 26, 29, 33, 36, 40, 43, 47, 50, 53, 56, 59, 62, 65, 68, 70, 73, 75, 78, 80, 82, 84, 87, 89, 90, 92, 94, 96, 97, 99, 100, 102, 103, 104, 105, 107, 108, 109, 110, 111, 112, 112, 113, 114, 115, 115, 116, 117, 117, 118, 118, 119, 119, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_12_m_0_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_12_m_0_cv1_act_Sigmoid_output_0_nl_params_data, _model_12_m_0_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_m_0_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_m_0_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_m_0_cv1_act_Sigmoid_output_0_layer, 456,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_12_m_0_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_12_m_0_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_12_m_0_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_m_0_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_12_m_0_cv1_conv_Conv_output_0_output, &_model_12_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_m_0_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_m_0_cv1_act_Mul_output_0_layer, 459,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_12_m_0_cv1_act_Mul_output_0_chain,
  NULL, &_model_12_m_0_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_12_m_0_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -119, -119, -118, -118, -117, -117, -116, -115, -115, -114, -113, -112, -111, -111, -110, -109, -108, -107, -105, -104, -103, -102, -100, -99, -98, -96, -94, -93, -91, -89, -87, -85, -83, -81, -79, -77, -75, -72, -70, -67, -65, -62, -59, -56, -54, -51, -48, -45, -41, -38, -35, -32, -28, -25, -21, -18, -14, -11, -7, -4, 0, 3, 7, 10, 14, 17, 21, 24, 28, 31, 34, 38, 41, 44, 47, 50, 53, 56, 59, 61, 64, 67, 69, 72, 74, 76, 79, 81, 83, 85, 87, 89, 91, 92, 94, 95, 97, 98, 100, 101, 102, 104, 105, 106, 107, 108, 109, 110, 111, 112, 112, 113, 114, 115, 115, 116, 117, 117, 118, 118, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_12_m_0_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_12_m_0_cv2_act_Sigmoid_output_0_nl_params_data, _model_12_m_0_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_m_0_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_m_0_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_m_0_cv2_act_Sigmoid_output_0_layer, 465,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_12_m_0_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_12_m_0_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_12_m_0_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_m_0_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_12_m_0_cv2_conv_Conv_output_0_output, &_model_12_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_m_0_cv2_act_Mul_output_0_layer, 468,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_12_m_0_cv2_act_Mul_output_0_chain,
  NULL, &_model_12_m_0_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_12_Split_output_0_output0, &_model_12_Split_output_0_output1, &_model_12_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_Concat_output_0_layer, 471,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_12_Concat_output_0_chain,
  NULL, &_model_12_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_12_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -119, -119, -118, -117, -117, -116, -115, -115, -114, -113, -112, -111, -110, -109, -108, -107, -105, -104, -103, -101, -100, -98, -97, -95, -93, -91, -89, -87, -85, -83, -80, -78, -76, -73, -70, -67, -65, -62, -59, -55, -52, -49, -45, -42, -38, -35, -31, -27, -24, -20, -16, -12, -8, -4, 0, 3, 7, 11, 15, 19, 23, 27, 30, 34, 37, 41, 44, 48, 51, 54, 58, 61, 64, 66, 69, 72, 75, 77, 80, 82, 84, 86, 88, 90, 92, 94, 96, 97, 99, 100, 102, 103, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 114, 115, 116, 116, 117, 118, 118, 119, 119, 120, 120, 120, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_12_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_12_cv2_act_Sigmoid_output_0_nl_params_data, _model_12_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_cv2_act_Sigmoid_output_0_layer, 477,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_12_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_12_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_12_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_12_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_12_cv2_conv_Conv_output_0_output, &_model_12_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_12_cv2_act_Mul_output_0_layer, 480,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_12_cv2_act_Mul_output_0_chain,
  NULL, &_model_12_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_float _model_13_Resize_output_0_scales_data[] = { 2.0, 2.0, 1.0, 1.0 };
AI_ARRAY_OBJ_DECLARE(
    _model_13_Resize_output_0_scales, AI_ARRAY_FORMAT_FLOAT,
    _model_13_Resize_output_0_scales_data, _model_13_Resize_output_0_scales_data, 4, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_13_Resize_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_12_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_13_Resize_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_13_Resize_output_0_layer, 483,
  UPSAMPLE_TYPE, 0x0, NULL,
  upsample, forward_upsample_nearest,
  &_model_13_Resize_output_0_chain,
  NULL, &_model_13_Resize_output_0_layer, AI_STATIC, 
  .scales = &_model_13_Resize_output_0_scales, 
  .center = false, 
  .mode = AI_UPSAMPLE_NEAREST, 
  .nearest_mode = AI_ROUND_FLOOR, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_14_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_13_Resize_output_0_output, &_model_4_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_14_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_14_Concat_output_0_layer, 486,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_14_Concat_output_0_chain,
  NULL, &_model_14_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_15_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -123, -122, -122, -122, -121, -121, -120, -120, -119, -118, -118, -117, -116, -116, -115, -114, -113, -112, -111, -110, -109, -107, -106, -105, -103, -102, -100, -98, -97, -95, -93, -91, -88, -86, -84, -81, -78, -76, -73, -70, -67, -63, -60, -57, -53, -50, -46, -42, -38, -34, -30, -26, -22, -18, -13, -9, -5, 0, 4, 8, 12, 17, 21, 25, 29, 33, 37, 41, 45, 49, 52, 56, 59, 63, 66, 69, 72, 75, 77, 80, 83, 85, 87, 90, 92, 94, 96, 97, 99, 101, 102, 104, 105, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 116, 117, 118, 118, 119, 119, 120, 120, 121, 121, 121, 122, 122, 122, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_15_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_15_cv1_act_Sigmoid_output_0_nl_params_data, _model_15_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_cv1_act_Sigmoid_output_0_layer, 492,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_15_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_15_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_15_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_15_cv1_conv_Conv_output_0_output, &_model_15_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_cv1_act_Mul_output_0_layer, 495,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_15_cv1_act_Mul_output_0_chain,
  NULL, &_model_15_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_Split_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_15_Split_output_0_output0, &_model_15_Split_output_0_output1),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_Split_output_0_num_or_size_splits),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_Split_output_0_layer, 498,
  SPLIT_TYPE, 0x0, NULL,
  split, forward_split,
  &_model_15_Split_output_0_chain,
  NULL, &_model_15_Split_output_0_layer, AI_STATIC, 
  .outer_elems = 576, 
  .outer_elems_stride = 32, 
)


AI_STATIC_CONST ai_i8 _model_15_m_0_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -124, -123, -123, -122, -122, -121, -121, -120, -119, -118, -117, -116, -115, -114, -113, -111, -110, -108, -106, -104, -102, -100, -97, -95, -92, -89, -86, -82, -79, -75, -70, -66, -62, -57, -52, -47, -41, -36, -30, -24, -19, -13, -7, 0, 6, 12, 18, 23, 29, 35, 40, 46, 51, 56, 61, 65, 69, 74, 78, 81, 85, 88, 91, 94, 96, 99, 101, 103, 105, 107, 109, 110, 112, 113, 114, 115, 116, 117, 118, 119, 120, 120, 121, 121, 122, 122, 123, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_15_m_0_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_15_m_0_cv1_act_Sigmoid_output_0_nl_params_data, _model_15_m_0_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_m_0_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_m_0_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_m_0_cv1_act_Sigmoid_output_0_layer, 506,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_15_m_0_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_15_m_0_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_15_m_0_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_m_0_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_15_m_0_cv1_conv_Conv_output_0_output, &_model_15_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_m_0_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_m_0_cv1_act_Mul_output_0_layer, 509,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_15_m_0_cv1_act_Mul_output_0_chain,
  NULL, &_model_15_m_0_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_15_m_0_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -124, -124, -124, -123, -123, -123, -122, -122, -121, -121, -120, -119, -119, -118, -117, -116, -116, -115, -113, -112, -111, -110, -108, -107, -105, -104, -102, -100, -98, -96, -93, -91, -88, -85, -82, -79, -76, -73, -69, -65, -62, -58, -53, -49, -45, -40, -35, -31, -26, -21, -16, -11, -6, 0, 5, 10, 15, 20, 25, 30, 34, 39, 44, 48, 52, 57, 61, 64, 68, 72, 75, 78, 81, 84, 87, 90, 92, 95, 97, 99, 101, 103, 104, 106, 107, 109, 110, 111, 112, 114, 115, 115, 116, 117, 118, 119, 119, 120, 120, 121, 121, 122, 122, 122, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_15_m_0_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_15_m_0_cv2_act_Sigmoid_output_0_nl_params_data, _model_15_m_0_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_m_0_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_m_0_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_m_0_cv2_act_Sigmoid_output_0_layer, 515,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_15_m_0_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_15_m_0_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_15_m_0_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_m_0_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_15_m_0_cv2_conv_Conv_output_0_output, &_model_15_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_m_0_cv2_act_Mul_output_0_layer, 518,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_15_m_0_cv2_act_Mul_output_0_chain,
  NULL, &_model_15_m_0_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_15_Split_output_0_output0, &_model_15_Split_output_0_output1, &_model_15_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_Concat_output_0_layer, 521,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_15_Concat_output_0_chain,
  NULL, &_model_15_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_15_cv2_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -124, -124, -124, -123, -123, -122, -122, -121, -121, -120, -119, -119, -118, -117, -116, -115, -114, -113, -111, -110, -109, -107, -105, -103, -101, -99, -97, -95, -92, -89, -86, -83, -80, -77, -73, -69, -65, -61, -57, -52, -48, -43, -38, -33, -28, -22, -17, -11, -6, 0, 5, 10, 16, 21, 27, 32, 37, 42, 47, 51, 56, 60, 64, 68, 72, 76, 79, 82, 85, 88, 91, 94, 96, 98, 100, 102, 104, 106, 108, 109, 110, 112, 113, 114, 115, 116, 117, 118, 118, 119, 120, 120, 121, 121, 122, 122, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_15_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_15_cv2_act_Sigmoid_output_0_nl_params_data, _model_15_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_cv2_act_Sigmoid_output_0_layer, 527,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_15_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_15_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_15_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_15_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_15_cv2_conv_Conv_output_0_output, &_model_15_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_15_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_15_cv2_act_Mul_output_0_layer, 530,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_15_cv2_act_Mul_output_0_chain,
  NULL, &_model_15_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_16_act_Sigmoid_output_0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -119, -119, -118, -118, -117, -116, -116, -115, -114, -114, -113, -112, -111, -110, -109, -108, -107, -106, -105, -103, -102, -101, -99, -98, -96, -95, -93, -91, -89, -87, -85, -83, -81, -79, -77, -74, -72, -69, -67, -64, -61, -58, -55, -52, -49, -46, -43, -39, -36, -33, -29, -26, -22, -19, -15, -11, -8, -4, 0, 3, 7, 10, 14, 18, 21, 25, 28, 32, 35, 39, 42, 45, 48, 51, 54, 57, 60, 63, 66, 68, 71, 73, 76, 78, 80, 82, 84, 86, 88, 90, 92, 94, 95, 97, 98, 100, 101, 102, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 113, 114, 115, 115, 116, 117, 117, 118, 118, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_16_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_16_act_Sigmoid_output_0_nl_params_data, _model_16_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_16_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_16_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_16_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_16_act_Sigmoid_output_0_layer, 542,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_16_act_Sigmoid_output_0_chain,
  NULL, &_model_16_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_16_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_16_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_16_conv_Conv_output_0_output, &_model_16_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_16_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_16_act_Mul_output_0_layer, 551,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_16_act_Mul_output_0_chain,
  NULL, &_model_16_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_17_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_16_act_Mul_output_0_output, &_model_12_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_17_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_17_Concat_output_0_layer, 560,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_17_Concat_output_0_chain,
  NULL, &_model_17_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_18_cv1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -123, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -119, -119, -118, -118, -117, -117, -116, -116, -115, -114, -114, -113, -112, -111, -110, -109, -108, -107, -106, -105, -104, -103, -102, -100, -99, -98, -96, -95, -93, -91, -90, -88, -86, -84, -82, -80, -78, -76, -73, -71, -69, -66, -63, -61, -58, -55, -52, -50, -47, -44, -40, -37, -34, -31, -28, -24, -21, -18, -14, -11, -7, -4, 0, 3, 7, 10, 13, 17, 20, 24, 27, 30, 33, 37, 40, 43, 46, 49, 52, 55, 57, 60, 63, 65, 68, 70, 73, 75, 77, 79, 81, 83, 85, 87, 89, 91, 92, 94, 95, 97, 98, 100, 101, 102, 103, 105, 106, 107, 108, 109, 110, 110, 111, 112, 113, 114, 114, 115, 115, 116, 117, 117, 118, 118, 119, 119, 119, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_18_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_18_cv1_act_Sigmoid_output_0_nl_params_data, _model_18_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_cv1_act_Sigmoid_output_0_layer, 578,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_18_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_18_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_18_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_18_cv1_conv_Conv_output_0_output, &_model_18_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_cv1_act_Mul_output_0_layer, 587,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_18_cv1_act_Mul_output_0_chain,
  NULL, &_model_18_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_Split_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_18_Split_output_0_output0, &_model_18_Split_output_0_output1),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_Split_output_0_num_or_size_splits),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_Split_output_0_layer, 596,
  SPLIT_TYPE, 0x0, NULL,
  split, forward_split,
  &_model_18_Split_output_0_chain,
  NULL, &_model_18_Split_output_0_layer, AI_STATIC, 
  .outer_elems = 144, 
  .outer_elems_stride = 64, 
)


AI_STATIC_CONST ai_i8 _model_18_m_0_cv1_act_Sigmoid_output_0_nl_params_data[] = { -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -122, -122, -122, -122, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -117, -117, -116, -116, -115, -115, -114, -114, -113, -112, -112, -111, -110, -110, -109, -108, -107, -106, -105, -104, -103, -102, -101, -100, -99, -98, -97, -95, -94, -93, -91, -90, -89, -87, -86, -84, -82, -81, -79, -77, -75, -73, -71, -69, -67, -65, -63, -61, -59, -56, -54, -52, -49, -47, -44, -42, -39, -36, -34, -31, -28, -26, -23, -20, -17, -15, -12, -9, -6, -3, 0, 3, 5, 8, 11, 14, 17, 20, 22, 25, 28, 30, 33, 36, 38, 41, 44, 46, 49, 51, 53, 56, 58, 60, 62, 65, 67, 69, 71, 73, 74, 76, 78, 80, 82, 83, 85, 86, 88, 89, 91, 92, 94, 95, 96, 97, 98, 100, 101, 102, 103, 104, 105, 106, 106, 107, 108, 109, 110, 110, 111, 112, 112, 113, 114, 114, 115, 115, 116, 116, 117, 117, 118, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_18_m_0_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_18_m_0_cv1_act_Sigmoid_output_0_nl_params_data, _model_18_m_0_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_m_0_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_m_0_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_m_0_cv1_act_Sigmoid_output_0_layer, 610,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_18_m_0_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_18_m_0_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_18_m_0_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_m_0_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_18_m_0_cv1_conv_Conv_output_0_output, &_model_18_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_m_0_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_m_0_cv1_act_Mul_output_0_layer, 613,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_18_m_0_cv1_act_Mul_output_0_chain,
  NULL, &_model_18_m_0_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_18_m_0_cv2_act_Sigmoid_output_0_nl_params_data[] = { -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -122, -122, -122, -122, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -118, -117, -117, -116, -116, -115, -115, -114, -113, -113, -112, -112, -111, -110, -109, -109, -108, -107, -106, -105, -104, -104, -103, -102, -101, -99, -98, -97, -96, -95, -94, -92, -91, -89, -88, -87, -85, -83, -82, -80, -78, -77, -75, -73, -71, -69, -67, -65, -63, -61, -59, -57, -55, -52, -50, -48, -45, -43, -40, -38, -35, -33, -30, -27, -25, -22, -19, -17, -14, -11, -9, -6, -3, 0, 2, 5, 8, 11, 13, 16, 19, 22, 24, 27, 29, 32, 35, 37, 40, 42, 45, 47, 49, 52, 54, 56, 58, 61, 63, 65, 67, 69, 71, 73, 74, 76, 78, 80, 81, 83, 84, 86, 87, 89, 90, 92, 93, 94, 95, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 107, 108, 109, 110, 110, 111, 112, 112, 113, 113, 114, 115, 115, 116, 116, 116, 117, 117, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_18_m_0_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_18_m_0_cv2_act_Sigmoid_output_0_nl_params_data, _model_18_m_0_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_m_0_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_m_0_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_m_0_cv2_act_Sigmoid_output_0_layer, 619,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_18_m_0_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_18_m_0_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_18_m_0_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_m_0_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_18_m_0_cv2_conv_Conv_output_0_output, &_model_18_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_m_0_cv2_act_Mul_output_0_layer, 622,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_18_m_0_cv2_act_Mul_output_0_chain,
  NULL, &_model_18_m_0_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_18_Split_output_0_output0, &_model_18_Split_output_0_output1, &_model_18_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_Concat_output_0_layer, 625,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_18_Concat_output_0_chain,
  NULL, &_model_18_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_18_cv2_act_Sigmoid_output_0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -123, -123, -123, -122, -122, -122, -122, -121, -121, -120, -120, -120, -119, -119, -118, -118, -117, -117, -116, -115, -115, -114, -113, -113, -112, -111, -110, -109, -108, -107, -106, -105, -104, -103, -101, -100, -99, -97, -96, -94, -93, -91, -89, -87, -86, -84, -82, -80, -77, -75, -73, -71, -68, -66, -63, -60, -58, -55, -52, -49, -46, -43, -40, -37, -34, -31, -27, -24, -21, -17, -14, -11, -7, -4, 0, 3, 6, 10, 13, 17, 20, 23, 27, 30, 33, 36, 39, 42, 45, 48, 51, 54, 57, 60, 62, 65, 67, 70, 72, 74, 77, 79, 81, 83, 85, 87, 88, 90, 92, 93, 95, 96, 98, 99, 100, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 112, 113, 114, 114, 115, 116, 116, 117, 117, 118, 118, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_18_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_18_cv2_act_Sigmoid_output_0_nl_params_data, _model_18_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_cv2_act_Sigmoid_output_0_layer, 631,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_18_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_18_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_18_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_18_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_18_cv2_conv_Conv_output_0_output, &_model_18_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_18_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_18_cv2_act_Mul_output_0_layer, 634,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_18_cv2_act_Mul_output_0_chain,
  NULL, &_model_18_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_19_act_Sigmoid_output_0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -122, -122, -122, -122, -122, -121, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -118, -117, -117, -117, -116, -116, -115, -115, -114, -114, -113, -113, -112, -112, -111, -111, -110, -110, -109, -108, -108, -107, -106, -105, -105, -104, -103, -102, -101, -100, -99, -99, -98, -97, -95, -94, -93, -92, -91, -90, -89, -87, -86, -85, -84, -82, -81, -79, -78, -76, -75, -73, -72, -70, -68, -67, -65, -63, -61, -60, -58, -56, -54, -52, -50, -48, -46, -44, -42, -39, -37, -35, -33, -31, -28, -26, -24, -22, -19, -17, -15, -12, -10, -8, -5, -3, 0, 2, 4, 7, 9, 11, 14, 16, 19, 21, 23, 26, 28, 30, 32, 35, 37, 39, 41, 43, 46, 48, 50, 52, 54, 56, 58, 60, 62, 63, 65, 67, 69, 71, 72, 74, 76, 77, 79, 80, 82, 83, 85, 86, 87, 89, 90, 91, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 108, 109, 110, 111, 111, 112, 113, 113, 114, 115, 115, 116, 116, 117, 117, 118, 118, 119, 119, 120, 120, 121, 121, 121, 122, 122, 122, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 126, 126, 126, 126, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_19_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_19_act_Sigmoid_output_0_nl_params_data, _model_19_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_19_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_19_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_19_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_19_act_Sigmoid_output_0_layer, 646,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_19_act_Sigmoid_output_0_chain,
  NULL, &_model_19_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_19_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_19_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_19_conv_Conv_output_0_output, &_model_19_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_19_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_19_act_Mul_output_0_layer, 655,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_19_act_Mul_output_0_chain,
  NULL, &_model_19_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_20_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_19_act_Mul_output_0_output, &_model_9_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_20_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_20_Concat_output_0_layer, 664,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_20_Concat_output_0_chain,
  NULL, &_model_20_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_21_cv1_act_Sigmoid_output_0_nl_params_data[] = { -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -123, -122, -122, -122, -122, -122, -122, -121, -121, -121, -121, -120, -120, -120, -120, -119, -119, -119, -118, -118, -118, -117, -117, -117, -116, -116, -116, -115, -115, -114, -114, -114, -113, -113, -112, -112, -111, -111, -110, -109, -109, -108, -108, -107, -106, -106, -105, -104, -103, -103, -102, -101, -100, -99, -99, -98, -97, -96, -95, -94, -93, -92, -91, -90, -89, -88, -86, -85, -84, -83, -82, -80, -79, -78, -76, -75, -73, -72, -70, -69, -67, -66, -64, -63, -61, -59, -58, -56, -54, -52, -50, -49, -47, -45, -43, -41, -39, -37, -35, -33, -31, -29, -27, -25, -23, -21, -19, -16, -14, -12, -10, -8, -6, -3, -1, 1, 3, 5, 8, 10, 12, 14, 16, 18, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 47, 49, 51, 53, 55, 57, 59, 60, 62, 64, 66, 67, 69, 71, 72, 74, 75, 77, 78, 80, 81, 83, 84, 85, 87, 88, 89, 90, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 108, 109, 110, 111, 111, 112, 113, 113, 114, 115, 115, 116, 116, 117, 117, 118, 118, 119, 119, 120, 120, 121, 121, 122, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 125, 126, 126, 126, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_21_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_21_cv1_act_Sigmoid_output_0_nl_params_data, _model_21_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_cv1_act_Sigmoid_output_0_layer, 682,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_21_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_21_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_21_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_21_cv1_conv_Conv_output_0_output, &_model_21_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_cv1_act_Mul_output_0_layer, 691,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_21_cv1_act_Mul_output_0_chain,
  NULL, &_model_21_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_Split_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_21_Split_output_0_output0, &_model_21_Split_output_0_output1),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_Split_output_0_num_or_size_splits),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_Split_output_0_layer, 700,
  SPLIT_TYPE, 0x0, NULL,
  split, forward_split,
  &_model_21_Split_output_0_chain,
  NULL, &_model_21_Split_output_0_layer, AI_STATIC, 
  .outer_elems = 36, 
  .outer_elems_stride = 64, 
)


AI_STATIC_CONST ai_i8 _model_21_m_0_cv1_act_Sigmoid_output_0_nl_params_data[] = { -117, -117, -117, -116, -116, -116, -116, -115, -115, -115, -114, -114, -114, -113, -113, -113, -112, -112, -112, -111, -111, -110, -110, -110, -109, -109, -108, -108, -107, -107, -106, -106, -105, -105, -104, -104, -103, -103, -102, -101, -101, -100, -100, -99, -98, -98, -97, -96, -96, -95, -94, -93, -93, -92, -91, -90, -89, -89, -88, -87, -86, -85, -84, -83, -82, -81, -80, -79, -78, -77, -76, -75, -74, -73, -72, -71, -70, -69, -68, -66, -65, -64, -63, -62, -60, -59, -58, -57, -55, -54, -53, -51, -50, -48, -47, -46, -44, -43, -41, -40, -38, -37, -35, -34, -32, -31, -29, -28, -26, -25, -23, -21, -20, -18, -17, -15, -13, -12, -10, -8, -7, -5, -4, -2, 0, 1, 3, 5, 6, 8, 10, 11, 13, 15, 16, 18, 20, 21, 23, 24, 26, 28, 29, 31, 32, 34, 36, 37, 39, 40, 42, 43, 45, 46, 48, 49, 51, 52, 54, 55, 56, 58, 59, 61, 62, 63, 65, 66, 67, 69, 70, 71, 72, 74, 75, 76, 77, 78, 79, 80, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 95, 96, 97, 98, 99, 100, 100, 101, 102, 103, 104, 104, 105, 106, 106, 107, 108, 108, 109, 110, 110, 111, 111, 112, 113, 113, 114, 114, 115, 115, 116, 116, 117, 117, 118, 118, 119, 119, 119, 120, 120, 121, 121, 121, 122, 122, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 126, 126, 126, 126, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_21_m_0_cv1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_21_m_0_cv1_act_Sigmoid_output_0_nl_params_data, _model_21_m_0_cv1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_m_0_cv1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_m_0_cv1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_m_0_cv1_act_Sigmoid_output_0_layer, 714,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_21_m_0_cv1_act_Sigmoid_output_0_chain,
  NULL, &_model_21_m_0_cv1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_21_m_0_cv1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_m_0_cv1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_21_m_0_cv1_conv_Conv_output_0_output, &_model_21_m_0_cv1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_m_0_cv1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_m_0_cv1_act_Mul_output_0_layer, 717,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_21_m_0_cv1_act_Mul_output_0_chain,
  NULL, &_model_21_m_0_cv1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_21_m_0_cv2_act_Sigmoid_output_0_nl_params_data[] = { -106, -106, -106, -105, -105, -104, -104, -103, -103, -102, -102, -101, -101, -100, -100, -99, -99, -98, -98, -97, -97, -96, -95, -95, -94, -94, -93, -92, -92, -91, -90, -90, -89, -88, -88, -87, -86, -85, -85, -84, -83, -82, -81, -81, -80, -79, -78, -77, -76, -76, -75, -74, -73, -72, -71, -70, -69, -68, -67, -66, -65, -64, -63, -62, -61, -60, -59, -58, -57, -56, -55, -53, -52, -51, -50, -49, -48, -47, -45, -44, -43, -42, -40, -39, -38, -37, -35, -34, -33, -32, -30, -29, -28, -26, -25, -24, -22, -21, -20, -18, -17, -16, -14, -13, -12, -10, -9, -7, -6, -5, -3, -2, 0, 1, 2, 4, 5, 7, 8, 9, 11, 12, 14, 15, 16, 18, 19, 21, 22, 23, 25, 26, 28, 29, 30, 32, 33, 34, 36, 37, 38, 40, 41, 42, 44, 45, 46, 48, 49, 50, 51, 53, 54, 55, 56, 57, 59, 60, 61, 62, 63, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 79, 80, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 91, 92, 93, 94, 95, 96, 96, 97, 98, 99, 99, 100, 101, 102, 102, 103, 104, 104, 105, 106, 106, 107, 108, 108, 109, 109, 110, 110, 111, 112, 112, 113, 113, 114, 114, 115, 115, 116, 116, 117, 117, 118, 118, 119, 119, 119, 120, 120, 121, 121, 121, 122, 122, 123, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 126, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_21_m_0_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_21_m_0_cv2_act_Sigmoid_output_0_nl_params_data, _model_21_m_0_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_m_0_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_m_0_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_m_0_cv2_act_Sigmoid_output_0_layer, 723,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_21_m_0_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_21_m_0_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_21_m_0_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_m_0_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_21_m_0_cv2_conv_Conv_output_0_output, &_model_21_m_0_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_m_0_cv2_act_Mul_output_0_layer, 726,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_21_m_0_cv2_act_Mul_output_0_chain,
  NULL, &_model_21_m_0_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_21_Split_output_0_output0, &_model_21_Split_output_0_output1, &_model_21_m_0_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_Concat_output_0_layer, 729,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_21_Concat_output_0_chain,
  NULL, &_model_21_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)


AI_STATIC_CONST ai_i8 _model_21_cv2_act_Sigmoid_output_0_nl_params_data[] = { -109, -109, -108, -108, -107, -107, -106, -106, -105, -105, -104, -104, -103, -103, -102, -101, -101, -100, -99, -99, -98, -97, -97, -96, -95, -95, -94, -93, -92, -91, -91, -90, -89, -88, -87, -86, -85, -84, -84, -83, -82, -81, -80, -79, -78, -76, -75, -74, -73, -72, -71, -70, -69, -67, -66, -65, -64, -62, -61, -60, -59, -57, -56, -55, -53, -52, -50, -49, -48, -46, -45, -43, -42, -40, -39, -37, -36, -34, -33, -31, -29, -28, -26, -25, -23, -21, -20, -18, -17, -15, -13, -12, -10, -8, -7, -5, -3, -2, 0, 2, 3, 5, 7, 8, 10, 12, 13, 15, 17, 18, 20, 22, 23, 25, 27, 28, 30, 31, 33, 35, 36, 38, 39, 41, 42, 44, 45, 47, 48, 50, 51, 53, 54, 55, 57, 58, 59, 61, 62, 63, 65, 66, 67, 68, 70, 71, 72, 73, 74, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 92, 93, 94, 95, 96, 97, 97, 98, 99, 100, 100, 101, 102, 102, 103, 104, 104, 105, 105, 106, 107, 107, 108, 108, 109, 109, 110, 110, 111, 111, 112, 112, 113, 113, 114, 114, 115, 115, 115, 116, 116, 116, 117, 117, 118, 118, 118, 119, 119, 119, 119, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_21_cv2_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_21_cv2_act_Sigmoid_output_0_nl_params_data, _model_21_cv2_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_cv2_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_cv2_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_cv2_act_Sigmoid_output_0_layer, 735,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_21_cv2_act_Sigmoid_output_0_chain,
  NULL, &_model_21_cv2_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_21_cv2_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_21_cv2_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_21_cv2_conv_Conv_output_0_output, &_model_21_cv2_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_21_cv2_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_21_cv2_act_Mul_output_0_layer, 738,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_21_cv2_act_Mul_output_0_chain,
  NULL, &_model_21_cv2_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_nl_params_data[] = { -120, -120, -120, -120, -119, -119, -119, -119, -118, -118, -118, -118, -117, -117, -117, -116, -116, -116, -116, -115, -115, -114, -114, -114, -113, -113, -112, -112, -112, -111, -111, -110, -110, -109, -109, -108, -108, -107, -107, -106, -106, -105, -104, -104, -103, -102, -102, -101, -100, -100, -99, -98, -97, -97, -96, -95, -94, -93, -93, -92, -91, -90, -89, -88, -87, -86, -85, -84, -83, -82, -81, -80, -79, -77, -76, -75, -74, -73, -71, -70, -69, -67, -66, -65, -63, -62, -61, -59, -58, -56, -55, -53, -52, -50, -49, -47, -45, -44, -42, -40, -39, -37, -35, -34, -32, -30, -29, -27, -25, -23, -21, -20, -18, -16, -14, -12, -11, -9, -7, -5, -3, -1, 1, 2, 4, 6, 8, 10, 12, 14, 15, 17, 19, 21, 23, 25, 26, 28, 30, 32, 33, 35, 37, 39, 40, 42, 44, 45, 47, 49, 50, 52, 53, 55, 57, 58, 60, 61, 63, 64, 65, 67, 68, 70, 71, 72, 74, 75, 76, 77, 79, 80, 81, 82, 83, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 97, 98, 99, 100, 101, 102, 102, 103, 104, 105, 105, 106, 107, 107, 108, 109, 109, 110, 110, 111, 112, 112, 113, 113, 114, 114, 115, 115, 116, 116, 117, 117, 117, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_nl_params_data, _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_layer, 747,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output, &_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_2_cv2_2_0_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_0_act_Mul_output_0_layer, 753,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv2_2_cv2_2_0_act_Mul_output_0_chain,
  NULL, &_model_22_cv2_2_cv2_2_0_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_nl_params_data[] = { -116, -115, -115, -115, -114, -114, -114, -113, -113, -113, -112, -112, -111, -111, -111, -110, -110, -109, -109, -108, -108, -107, -107, -106, -106, -105, -105, -104, -104, -103, -102, -102, -101, -101, -100, -99, -99, -98, -97, -96, -96, -95, -94, -93, -93, -92, -91, -90, -89, -88, -87, -86, -86, -85, -84, -83, -82, -81, -80, -79, -77, -76, -75, -74, -73, -72, -71, -69, -68, -67, -66, -65, -63, -62, -61, -59, -58, -57, -55, -54, -52, -51, -50, -48, -47, -45, -44, -42, -41, -39, -37, -36, -34, -33, -31, -30, -28, -26, -25, -23, -21, -20, -18, -16, -15, -13, -11, -9, -8, -6, -4, -3, -1, 1, 3, 4, 6, 8, 9, 11, 13, 15, 16, 18, 20, 21, 23, 25, 26, 28, 30, 31, 33, 35, 36, 38, 40, 41, 43, 44, 46, 47, 49, 50, 52, 53, 55, 56, 58, 59, 60, 62, 63, 65, 66, 67, 68, 70, 71, 72, 73, 75, 76, 77, 78, 79, 80, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 93, 94, 95, 96, 97, 98, 99, 99, 100, 101, 102, 102, 103, 104, 104, 105, 106, 106, 107, 108, 108, 109, 109, 110, 111, 111, 112, 112, 113, 113, 114, 114, 115, 115, 115, 116, 116, 117, 117, 117, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_nl_params_data, _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_layer, 765,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output, &_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_2_cv2_2_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_2_cv2_2_1_act_Mul_output_0_layer, 771,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv2_2_cv2_2_1_act_Mul_output_0_chain,
  NULL, &_model_22_cv2_2_cv2_2_1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_nl_params_data[] = { -97, -96, -95, -95, -94, -93, -93, -92, -91, -91, -90, -89, -88, -88, -87, -86, -85, -84, -84, -83, -82, -81, -80, -79, -78, -78, -77, -76, -75, -74, -73, -72, -71, -70, -69, -68, -67, -66, -65, -64, -62, -61, -60, -59, -58, -57, -56, -55, -53, -52, -51, -50, -48, -47, -46, -45, -43, -42, -41, -40, -38, -37, -36, -34, -33, -32, -30, -29, -27, -26, -25, -23, -22, -20, -19, -18, -16, -15, -13, -12, -10, -9, -8, -6, -5, -3, -2, 0, 1, 3, 4, 6, 7, 8, 10, 11, 13, 14, 16, 17, 19, 20, 21, 23, 24, 26, 27, 29, 30, 31, 33, 34, 35, 37, 38, 40, 41, 42, 44, 45, 46, 47, 49, 50, 51, 53, 54, 55, 56, 57, 59, 60, 61, 62, 63, 64, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 85, 86, 87, 88, 89, 90, 91, 91, 92, 93, 94, 94, 95, 96, 97, 97, 98, 99, 99, 100, 101, 101, 102, 102, 103, 104, 104, 105, 105, 106, 106, 107, 107, 108, 109, 109, 110, 110, 110, 111, 111, 112, 112, 113, 113, 114, 114, 114, 115, 115, 115, 116, 116, 117, 117, 117, 118, 118, 118, 119, 119, 119, 119, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_nl_params_data, _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_layer, 748,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output, &_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_0_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_0_act_Mul_output_0_layer, 754,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv3_2_cv3_2_0_act_Mul_output_0_chain,
  NULL, &_model_22_cv3_2_cv3_2_0_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_nl_params_data[] = { -112, -112, -111, -111, -111, -110, -110, -110, -109, -109, -109, -108, -108, -107, -107, -107, -106, -106, -105, -105, -105, -104, -104, -103, -103, -102, -102, -101, -101, -100, -100, -99, -99, -98, -98, -97, -96, -96, -95, -95, -94, -93, -93, -92, -91, -91, -90, -89, -89, -88, -87, -87, -86, -85, -84, -84, -83, -82, -81, -80, -80, -79, -78, -77, -76, -75, -74, -74, -73, -72, -71, -70, -69, -68, -67, -66, -65, -64, -63, -62, -61, -60, -59, -58, -57, -56, -54, -53, -52, -51, -50, -49, -48, -46, -45, -44, -43, -41, -40, -39, -38, -37, -35, -34, -33, -31, -30, -29, -27, -26, -25, -23, -22, -21, -19, -18, -17, -15, -14, -13, -11, -10, -8, -7, -6, -4, -3, -1, 0, 2, 3, 4, 6, 7, 9, 10, 12, 13, 14, 16, 17, 19, 20, 21, 23, 24, 26, 27, 29, 30, 31, 33, 34, 35, 37, 38, 40, 41, 42, 44, 45, 46, 48, 49, 50, 52, 53, 54, 55, 57, 58, 59, 60, 62, 63, 64, 65, 67, 68, 69, 70, 71, 72, 73, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 96, 97, 98, 99, 100, 101, 101, 102, 103, 104, 105, 105, 106, 107, 108, 108, 109, 110, 110, 111, 112, 112, 113, 114, 114, 115, 115, 116, 117, 117, 118, 118, 119, 119, 120, 120, 121, 121, 122, 122, 123, 123, 124, 124, 125, 125, 126, 126, 126, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_nl_params_data, _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_layer, 766,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output, &_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_1_act_Mul_output_0_layer, 772,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv3_2_cv3_2_1_act_Mul_output_0_chain,
  NULL, &_model_22_cv3_2_cv3_2_1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_2_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_22_cv3_2_cv3_2_2_Conv_output_0_weights, &_model_22_cv3_2_cv3_2_2_Conv_output_0_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_2_cv3_2_2_Conv_output_0_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_2_cv3_2_2_Conv_output_0_layer, 778,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSSA,
  &_model_22_cv3_2_cv3_2_2_Conv_output_0_chain,
  NULL, &_model_22_cv3_2_cv3_2_2_Conv_output_0_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)


AI_STATIC_CONST ai_i8 _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -122, -122, -122, -122, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -117, -117, -117, -116, -116, -115, -115, -114, -113, -113, -112, -111, -111, -110, -109, -109, -108, -107, -106, -105, -104, -103, -102, -101, -100, -99, -98, -97, -96, -95, -93, -92, -91, -89, -88, -86, -85, -83, -82, -80, -78, -77, -75, -73, -71, -69, -67, -65, -63, -61, -59, -57, -54, -52, -50, -47, -45, -43, -40, -38, -35, -32, -30, -27, -25, -22, -19, -17, -14, -11, -8, -6, -3, 0, 3, 5, 8, 11, 14, 16, 19, 22, 24, 27, 30, 32, 35, 37, 40, 42, 45, 47, 50, 52, 54, 56, 59, 61, 63, 65, 67, 69, 71, 73, 75, 76, 78, 80, 82, 83, 85, 86, 88, 89, 91, 92, 93, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 109, 110, 111, 111, 112, 113, 113, 114, 114, 115, 115, 116, 116, 117, 117, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_nl_params_data, _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_layer, 647,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output, &_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_1_cv2_1_0_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_0_act_Mul_output_0_layer, 656,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv2_1_cv2_1_0_act_Mul_output_0_chain,
  NULL, &_model_22_cv2_1_cv2_1_0_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_nl_params_data[] = { -125, -125, -125, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -122, -122, -122, -122, -121, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -117, -117, -117, -116, -116, -115, -115, -114, -113, -113, -112, -112, -111, -110, -110, -109, -108, -107, -106, -106, -105, -104, -103, -102, -101, -100, -99, -98, -97, -95, -94, -93, -92, -90, -89, -88, -86, -85, -83, -82, -80, -78, -77, -75, -73, -71, -69, -68, -66, -64, -62, -60, -57, -55, -53, -51, -49, -46, -44, -41, -39, -37, -34, -32, -29, -27, -24, -21, -19, -16, -14, -11, -8, -6, -3, 0, 2, 5, 8, 10, 13, 16, 18, 21, 24, 26, 29, 31, 34, 36, 39, 41, 43, 46, 48, 50, 53, 55, 57, 59, 61, 63, 65, 67, 69, 71, 73, 74, 76, 78, 80, 81, 83, 84, 86, 87, 89, 90, 91, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 108, 109, 110, 110, 111, 112, 112, 113, 113, 114, 115, 115, 116, 116, 116, 117, 117, 118, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_nl_params_data, _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_layer, 674,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output, &_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_1_cv2_1_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_1_cv2_1_1_act_Mul_output_0_layer, 683,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv2_1_cv2_1_1_act_Mul_output_0_chain,
  NULL, &_model_22_cv2_1_cv2_1_1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_nl_params_data[] = { -121, -121, -121, -121, -120, -120, -120, -120, -119, -119, -119, -118, -118, -118, -117, -117, -117, -116, -116, -116, -115, -115, -115, -114, -114, -113, -113, -112, -112, -111, -111, -110, -110, -109, -109, -108, -107, -107, -106, -106, -105, -104, -104, -103, -102, -101, -101, -100, -99, -98, -97, -96, -95, -94, -94, -93, -92, -91, -89, -88, -87, -86, -85, -84, -83, -82, -80, -79, -78, -76, -75, -74, -72, -71, -69, -68, -66, -65, -63, -62, -60, -59, -57, -55, -54, -52, -50, -48, -47, -45, -43, -41, -39, -37, -36, -34, -32, -30, -28, -26, -24, -22, -20, -18, -16, -14, -12, -10, -7, -5, -3, -1, 1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 43, 45, 46, 48, 50, 52, 53, 55, 57, 59, 60, 62, 63, 65, 67, 68, 70, 71, 72, 74, 75, 77, 78, 79, 81, 82, 83, 84, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 100, 101, 102, 103, 104, 104, 105, 106, 106, 107, 108, 108, 109, 110, 110, 111, 111, 112, 112, 113, 113, 114, 114, 115, 115, 116, 116, 117, 117, 117, 118, 118, 118, 119, 119, 119, 120, 120, 120, 121, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_nl_params_data, _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_layer, 648,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output, &_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_0_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_0_act_Mul_output_0_layer, 657,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv3_1_cv3_1_0_act_Mul_output_0_chain,
  NULL, &_model_22_cv3_1_cv3_1_0_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_nl_params_data[] = { -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -124, -124, -123, -123, -123, -123, -123, -122, -122, -122, -122, -122, -121, -121, -121, -121, -120, -120, -120, -119, -119, -119, -118, -118, -118, -117, -117, -116, -116, -116, -115, -115, -114, -114, -113, -113, -112, -112, -111, -110, -110, -109, -108, -108, -107, -106, -106, -105, -104, -103, -102, -101, -100, -99, -99, -98, -96, -95, -94, -93, -92, -91, -90, -88, -87, -86, -84, -83, -82, -80, -79, -77, -76, -74, -72, -71, -69, -67, -66, -64, -62, -60, -58, -56, -54, -52, -50, -48, -46, -44, -42, -39, -37, -35, -33, -30, -28, -26, -23, -21, -19, -16, -14, -11, -9, -6, -4, -2, 1, 3, 6, 8, 11, 13, 16, 18, 21, 23, 25, 28, 30, 33, 35, 37, 39, 42, 44, 46, 48, 51, 53, 55, 57, 59, 61, 63, 65, 67, 69, 70, 72, 74, 76, 78, 79, 81, 82, 84, 85, 87, 88, 90, 91, 93, 94, 95, 96, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 111, 112, 113, 114, 114, 115, 116, 116, 117, 118, 118, 119, 119, 120, 120, 121, 121, 122, 122, 123, 123, 124, 124, 124, 125, 125, 125, 126, 126, 126, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_nl_params_data, _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_layer, 675,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output, &_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_1_act_Mul_output_0_layer, 684,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv3_1_cv3_1_1_act_Mul_output_0_chain,
  NULL, &_model_22_cv3_1_cv3_1_1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_2_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_22_cv3_1_cv3_1_2_Conv_output_0_weights, &_model_22_cv3_1_cv3_1_2_Conv_output_0_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_1_cv3_1_2_Conv_output_0_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_1_cv3_1_2_Conv_output_0_layer, 693,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSSA,
  &_model_22_cv3_1_cv3_1_2_Conv_output_0_chain,
  NULL, &_model_22_cv3_1_cv3_1_2_Conv_output_0_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)


AI_STATIC_CONST ai_i8 _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -125, -125, -125, -124, -124, -124, -123, -123, -122, -122, -121, -120, -119, -119, -118, -117, -116, -115, -113, -112, -110, -109, -107, -105, -103, -101, -98, -95, -93, -90, -86, -83, -79, -75, -71, -67, -62, -57, -52, -47, -42, -36, -31, -25, -19, -13, -7, 0, 6, 12, 18, 24, 30, 35, 41, 46, 51, 56, 61, 66, 70, 74, 78, 82, 85, 89, 92, 94, 97, 100, 102, 104, 106, 108, 109, 111, 112, 114, 115, 116, 117, 118, 118, 119, 120, 121, 121, 122, 122, 123, 123, 123, 124, 124, 124, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_nl_params_data, _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_layer, 543,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output, &_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_0_cv2_0_0_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_0_act_Mul_output_0_layer, 552,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv2_0_cv2_0_0_act_Mul_output_0_chain,
  NULL, &_model_22_cv2_0_cv2_0_0_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -125, -125, -125, -124, -124, -124, -124, -124, -123, -123, -123, -123, -122, -122, -122, -121, -121, -121, -120, -120, -119, -119, -119, -118, -118, -117, -117, -116, -115, -115, -114, -113, -113, -112, -111, -110, -110, -109, -108, -107, -106, -105, -104, -102, -101, -100, -99, -97, -96, -94, -93, -91, -90, -88, -86, -84, -83, -81, -79, -77, -74, -72, -70, -68, -65, -63, -60, -58, -55, -52, -50, -47, -44, -41, -38, -35, -32, -29, -26, -23, -20, -16, -13, -10, -7, -3, 0, 3, 6, 10, 13, 16, 19, 22, 25, 29, 32, 35, 38, 41, 44, 46, 49, 52, 55, 57, 60, 62, 65, 67, 70, 72, 74, 76, 78, 80, 82, 84, 86, 88, 89, 91, 92, 94, 95, 97, 98, 99, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 112, 113, 114, 114, 115, 116, 116, 117, 117, 118, 118, 119, 119, 119, 120, 120, 121, 121, 121, 122, 122, 122, 122, 123, 123, 123, 123, 124, 124, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_nl_params_data, _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_layer, 570,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output, &_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv2_0_cv2_0_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv2_0_cv2_0_1_act_Mul_output_0_layer, 579,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv2_0_cv2_0_1_act_Mul_output_0_chain,
  NULL, &_model_22_cv2_0_cv2_0_1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Concat_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_22_cv2_0_cv2_0_2_Conv_output_0_output0, &_model_22_cv2_1_cv2_1_2_Conv_output_0_output0, &_model_22_cv2_2_cv2_2_2_Conv_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Concat_output_0_layer, 789,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_22_Concat_output_0_chain,
  NULL, &_model_22_Concat_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_HEIGHT, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_dfl_Reshape_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Concat_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_Reshape_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_dfl_Reshape_output_0_to_chlast_layer, 795,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_model_22_dfl_Reshape_output_0_to_chlast_chain,
  NULL, &_model_22_dfl_Reshape_output_0_to_chlast_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_dfl_Transpose_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_Reshape_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_Transpose_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_dfl_Transpose_output_0_layer, 801,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_model_22_dfl_Transpose_output_0_chain,
  NULL, &_model_22_dfl_Transpose_output_0_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_22_dfl_conv_Conv_output_0_weights, &_model_22_dfl_conv_Conv_output_0_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_conv_Conv_output_0_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  _model_22_dfl_conv_Conv_output_0_layer, 807,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSSA,
  &_model_22_dfl_conv_Conv_output_0_chain,
  NULL, &_model_22_dfl_conv_Conv_output_0_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_dfl_Reshape_1_output_0_to_chlast_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_Reshape_1_output_0_to_chlast_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_dfl_Reshape_1_output_0_to_chlast_layer, 810,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_model_22_dfl_Reshape_1_output_0_to_chlast_chain,
  NULL, &_model_22_dfl_Reshape_1_output_0_to_chlast_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_u8 _model_22_Slice_1_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_Slice_1_output_0_axes, AI_ARRAY_FORMAT_U8,
    _model_22_Slice_1_output_0_axes_data, _model_22_Slice_1_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 _model_22_Slice_1_output_0_starts_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_Slice_1_output_0_starts, AI_ARRAY_FORMAT_S16,
    _model_22_Slice_1_output_0_starts_data, _model_22_Slice_1_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 _model_22_Slice_1_output_0_ends_data[] = { 4 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_Slice_1_output_0_ends, AI_ARRAY_FORMAT_S16,
    _model_22_Slice_1_output_0_ends_data, _model_22_Slice_1_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Slice_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_Reshape_1_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Slice_1_output_0_layer, 814,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_model_22_Slice_1_output_0_chain,
  NULL, &_model_22_Slice_1_output_0_layer, AI_STATIC, 
  .axes = &_model_22_Slice_1_output_0_axes, 
  .starts = &_model_22_Slice_1_output_0_starts, 
  .ends = &_model_22_Slice_1_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Add_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_Constant_13_output_0_DequantizeLinear_Output_const, &_model_22_Slice_1_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Add_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Add_1_output_0_layer, 820,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_Add_1_output_0_chain,
  NULL, &_model_22_Add_1_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)


AI_STATIC_CONST ai_u8 _model_22_Slice_output_0_axes_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_Slice_output_0_axes, AI_ARRAY_FORMAT_U8,
    _model_22_Slice_output_0_axes_data, _model_22_Slice_output_0_axes_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 _model_22_Slice_output_0_starts_data[] = { 0 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_Slice_output_0_starts, AI_ARRAY_FORMAT_S16,
    _model_22_Slice_output_0_starts_data, _model_22_Slice_output_0_starts_data, 1, AI_STATIC_CONST)

AI_STATIC_CONST ai_i16 _model_22_Slice_output_0_ends_data[] = { 2 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_Slice_output_0_ends, AI_ARRAY_FORMAT_S16,
    _model_22_Slice_output_0_ends_data, _model_22_Slice_output_0_ends_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Slice_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_dfl_Reshape_1_output_0_to_chlast_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Slice_output_0_layer, 813,
  SLICE_TYPE, 0x0, NULL,
  slice, forward_slice,
  &_model_22_Slice_output_0_chain,
  NULL, &_model_22_Slice_output_0_layer, AI_STATIC, 
  .axes = &_model_22_Slice_output_0_axes, 
  .starts = &_model_22_Slice_output_0_starts, 
  .ends = &_model_22_Slice_output_0_ends, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Sub_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_Constant_12_output_0_DequantizeLinear_Output_const, &_model_22_Slice_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Sub_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Sub_output_0_layer, 819,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_Sub_output_0_chain,
  NULL, &_model_22_Sub_output_0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Sub_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_Add_1_output_0_output, &_model_22_Sub_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Sub_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Sub_1_output_0_layer, 826,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_Sub_1_output_0_chain,
  NULL, &_model_22_Sub_1_output_0_layer, AI_STATIC, 
  .operation = ai_sub_f32, 
  .buffer_operation = ai_sub_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Add_2_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_Sub_output_0_output, &_model_22_Add_1_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Add_2_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Add_2_output_0_layer, 825,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_Add_2_output_0_chain,
  NULL, &_model_22_Add_2_output_0_layer, AI_STATIC, 
  .operation = ai_sum_f32, 
  .buffer_operation = ai_sum_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Div_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_Add_2_output_0_output, &_model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Div_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Div_1_output_0_layer, 831,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_Div_1_output_0_chain,
  NULL, &_model_22_Div_1_output_0_layer, AI_STATIC, 
  .operation = ai_div_f32, 
  .buffer_operation = ai_div_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Concat_2_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_Div_1_output_0_output, &_model_22_Sub_1_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Concat_2_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Concat_2_output_0_layer, 834,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_22_Concat_2_output_0_chain,
  NULL, &_model_22_Concat_2_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_CHANNEL, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Mul_2_output_0_QuantizeLinear_Input_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_Concat_2_output_0_output, &_model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Mul_2_output_0_QuantizeLinear_Input_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Mul_2_output_0_QuantizeLinear_Input_layer, 837,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_Mul_2_output_0_QuantizeLinear_Input_chain,
  NULL, &_model_22_Mul_2_output_0_QuantizeLinear_Input_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Mul_2_output_0_QuantizeLinear_Input_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_layer, 1,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &_model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_chain,
  NULL, &_model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)


AI_STATIC_CONST ai_i8 _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -127, -126, -126, -126, -126, -126, -126, -126, -125, -125, -125, -125, -124, -124, -124, -123, -123, -123, -122, -122, -121, -121, -120, -119, -119, -118, -117, -116, -115, -114, -113, -112, -111, -110, -108, -107, -105, -103, -102, -100, -97, -95, -93, -90, -88, -85, -82, -79, -76, -72, -69, -65, -61, -57, -53, -49, -44, -40, -35, -30, -26, -21, -16, -11, -6, 0, 5, 10, 15, 20, 25, 29, 34, 39, 43, 48, 52, 56, 60, 64, 68, 71, 75, 78, 81, 84, 87, 89, 92, 94, 97, 99, 101, 102, 104, 106, 107, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 118, 119, 120, 120, 121, 121, 122, 122, 122, 123, 123, 123, 124, 124, 124, 124, 125, 125, 125, 125, 125, 125, 125, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_nl_params_data, _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_layer, 544,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output, &_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_0_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_0_act_Mul_output_0_layer, 553,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv3_0_cv3_0_0_act_Mul_output_0_chain,
  NULL, &_model_22_cv3_0_cv3_0_0_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)


AI_STATIC_CONST ai_i8 _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -126, -126, -126, -125, -125, -124, -123, -122, -121, -119, -117, -115, -112, -109, -106, -101, -96, -90, -84, -76, -68, -58, -48, -37, -25, -13, -1, 12, 24, 36, 47, 57, 67, 75, 83, 89, 95, 100, 105, 108, 111, 114, 116, 118, 120, 121, 122, 123, 124, 124, 125, 125, 125, 126, 126, 126, 126, 126, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_nl_params_data, _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_layer, 571,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_chain,
  NULL, &_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_layer, AI_STATIC, 
  .nl_params = &_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_nl_params, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_act_Mul_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &_model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output, &_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_1_act_Mul_output_0_layer, 580,
  ELTWISE_INTEGER_TYPE, 0x0, NULL,
  eltwise_integer, forward_eltwise_integer_INT8,
  &_model_22_cv3_0_cv3_0_1_act_Mul_output_0_chain,
  NULL, &_model_22_cv3_0_cv3_0_1_act_Mul_output_0_layer, AI_STATIC, 
  .operation = ai_mul_f32, 
  .buffer_operation = ai_mul_buffer_INT8, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_1_act_Mul_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_2_Conv_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_22_cv3_0_cv3_0_2_Conv_output_0_weights, &_model_22_cv3_0_cv3_0_2_Conv_output_0_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_cv3_0_cv3_0_2_Conv_output_0_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  _model_22_cv3_0_cv3_0_2_Conv_output_0_layer, 589,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSSA,
  &_model_22_cv3_0_cv3_0_2_Conv_output_0_chain,
  NULL, &_model_22_cv3_0_cv3_0_2_Conv_output_0_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Concat_1_output_0_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &_model_22_cv3_0_cv3_0_2_Conv_output_0_output0, &_model_22_cv3_1_cv3_1_2_Conv_output_0_output0, &_model_22_cv3_2_cv3_2_2_Conv_output_0_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Concat_1_output_0_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Concat_1_output_0_layer, 790,
  CONCAT_TYPE, 0x0, NULL,
  concat, forward_concat,
  &_model_22_Concat_1_output_0_chain,
  NULL, &_model_22_Concat_1_output_0_layer, AI_STATIC, 
  .axis = AI_SHAPE_HEIGHT, 
)


AI_STATIC_CONST ai_i8 _model_22_Sigmoid_output_0_QuantizeLinear_Input_nl_params_data[] = { -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -128, -127, -127, -127, -127, -127, -126, -126, -126, -125, -124, -123, -122, -121, -119, -117, -114, -111, -107, -103, -97, -90, -82, -73, -62, -49, -35, -20, -4, 13, 31, 49, 66, 82, 97, 111, 124 };
AI_ARRAY_OBJ_DECLARE(
    _model_22_Sigmoid_output_0_QuantizeLinear_Input_nl_params, AI_ARRAY_FORMAT_S8,
    _model_22_Sigmoid_output_0_QuantizeLinear_Input_nl_params_data, _model_22_Sigmoid_output_0_QuantizeLinear_Input_nl_params_data, 256, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  _model_22_Sigmoid_output_0_QuantizeLinear_Input_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Concat_1_output_0_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &_model_22_Sigmoid_output_0_QuantizeLinear_Input_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  _model_22_Sigmoid_output_0_QuantizeLinear_Input_layer, 796,
  NL_TYPE, 0x0, NULL,
  nl, forward_nl_integer,
  &_model_22_Sigmoid_output_0_QuantizeLinear_Input_chain,
  NULL, &_model_22_Sigmoid_output_0_QuantizeLinear_Input_layer, AI_STATIC, 
  .nl_params = &_model_22_Sigmoid_output_0_QuantizeLinear_Input_nl_params, 
)
/**  Hybrid layers declarations section  *************************************/
void forward_lite_nl_integer__model_0_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _model_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73728);
  _STAI_NETWORK_EVENT_NODE_START_CB(137, 1, { _model_0_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_0_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(137, 1, { _model_0_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_0_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _model_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_0_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_0_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(140, 2, { _model_0_conv_Conv_output_0_output.data->data,_model_0_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_0_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(140, 1, { _model_0_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 76544);
  _model_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76544);
  _model_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(146, 1, { _model_1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(146, 1, { _model_1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 76544);
  _model_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 76544);
  _model_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _model_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _STAI_NETWORK_EVENT_NODE_START_CB(149, 2, { _model_1_conv_Conv_output_0_output.data->data,_model_1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(149, 1, { _model_1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_2_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_2_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_2_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_2_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_2_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(155, 1, { _model_2_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_2_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(155, 1, { _model_2_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_2_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_2_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_2_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_2_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_2_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _model_2_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _STAI_NETWORK_EVENT_NODE_START_CB(158, 2, { _model_2_cv1_conv_Conv_output_0_output.data->data,_model_2_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_2_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(158, 1, { _model_2_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_split__model_2_Split_output_0(_stai_network_context* net_ctx)
{
  _model_2_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_Split_output_0_num_or_size_splits_array.data = AI_PTR(net_ctx->_weights[0] + 5424);
  _model_2_Split_output_0_num_or_size_splits_array.data_start = AI_PTR(net_ctx->_weights[0] + 5424);
  _model_2_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 18432);
  _model_2_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 18432);
  _model_2_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_2_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(161, 1, { _model_2_cv1_act_Mul_output_0_output.data->data});
  forward_split(&_model_2_Split_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(161, 2, { _model_2_Split_output_0_output0.data->data,_model_2_Split_output_0_output1.data->data});
}
void forward_lite_nl_integer__model_2_m_0_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_2_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 38416);
  _model_2_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 38416);
  _model_2_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 56848);
  _model_2_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 56848);
  _STAI_NETWORK_EVENT_NODE_START_CB(169, 1, { _model_2_m_0_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_2_m_0_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(169, 1, { _model_2_m_0_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_2_m_0_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_2_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 38416);
  _model_2_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 38416);
  _model_2_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 56848);
  _model_2_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 56848);
  _model_2_m_0_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 75280);
  _model_2_m_0_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 75280);
  _STAI_NETWORK_EVENT_NODE_START_CB(172, 2, { _model_2_m_0_cv1_conv_Conv_output_0_output.data->data,_model_2_m_0_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_2_m_0_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(172, 1, { _model_2_m_0_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_2_m_0_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_2_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 38416);
  _model_2_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 38416);
  _model_2_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 56848);
  _model_2_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 56848);
  _STAI_NETWORK_EVENT_NODE_START_CB(178, 1, { _model_2_m_0_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_2_m_0_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(178, 1, { _model_2_m_0_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_2_m_0_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_2_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 38416);
  _model_2_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 38416);
  _model_2_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 56848);
  _model_2_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 56848);
  _model_2_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 75280);
  _model_2_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 75280);
  _STAI_NETWORK_EVENT_NODE_START_CB(181, 2, { _model_2_m_0_cv2_conv_Conv_output_0_output.data->data,_model_2_m_0_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_2_m_0_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(181, 1, { _model_2_m_0_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_2_m_0_Add_output_0(_stai_network_context* net_ctx)
{
  _model_2_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 18432);
  _model_2_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 18432);
  _model_2_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 75280);
  _model_2_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 75280);
  _model_2_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _STAI_NETWORK_EVENT_NODE_START_CB(184, 2, { _model_2_Split_output_0_output1.data->data,_model_2_m_0_cv2_act_Mul_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_2_m_0_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(184, 1, { _model_2_m_0_Add_output_0_output.data->data});
}
void forward_lite_concat__model_2_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_2_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 0);
  _model_2_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 0);
  _model_2_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 18432);
  _model_2_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 18432);
  _model_2_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_2_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _STAI_NETWORK_EVENT_NODE_START_CB(187, 3, { _model_2_Split_output_0_output0.data->data,_model_2_Split_output_0_output1.data->data,_model_2_m_0_Add_output_0_output.data->data});
  forward_concat(&_model_2_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(187, 1, { _model_2_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_2_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_2_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_2_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_2_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _STAI_NETWORK_EVENT_NODE_START_CB(193, 1, { _model_2_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_2_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(193, 1, { _model_2_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_2_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_2_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_2_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_2_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_2_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_2_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73728);
  _STAI_NETWORK_EVENT_NODE_START_CB(196, 2, { _model_2_cv2_conv_Conv_output_0_output.data->data,_model_2_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_2_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(196, 1, { _model_2_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_3_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_3_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 43008);
  _model_3_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 43008);
  _model_3_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  _model_3_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _STAI_NETWORK_EVENT_NODE_START_CB(202, 1, { _model_3_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_3_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(202, 1, { _model_3_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_3_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_3_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 43008);
  _model_3_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 43008);
  _model_3_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 61440);
  _model_3_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 61440);
  _model_3_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 79872);
  _model_3_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 79872);
  _STAI_NETWORK_EVENT_NODE_START_CB(205, 2, { _model_3_conv_Conv_output_0_output.data->data,_model_3_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_3_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(205, 1, { _model_3_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_4_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_4_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 37312);
  _model_4_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 37312);
  _model_4_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55744);
  _model_4_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55744);
  _STAI_NETWORK_EVENT_NODE_START_CB(211, 1, { _model_4_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_4_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(211, 1, { _model_4_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_4_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_4_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 37312);
  _model_4_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 37312);
  _model_4_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55744);
  _model_4_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55744);
  _model_4_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 74176);
  _model_4_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 74176);
  _STAI_NETWORK_EVENT_NODE_START_CB(214, 2, { _model_4_cv1_conv_Conv_output_0_output.data->data,_model_4_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_4_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(214, 1, { _model_4_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_split__model_4_Split_output_0(_stai_network_context* net_ctx)
{
  _model_4_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 74176);
  _model_4_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 74176);
  _model_4_Split_output_0_num_or_size_splits_array.data = AI_PTR(net_ctx->_weights[0] + 12980);
  _model_4_Split_output_0_num_or_size_splits_array.data_start = AI_PTR(net_ctx->_weights[0] + 12980);
  _model_4_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  _model_4_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _model_4_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_4_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _STAI_NETWORK_EVENT_NODE_START_CB(217, 1, { _model_4_cv1_act_Mul_output_0_output.data->data});
  forward_split(&_model_4_Split_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(217, 2, { _model_4_Split_output_0_output0.data->data,_model_4_Split_output_0_output1.data->data});
}
void forward_lite_nl_integer__model_4_m_0_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 71520);
  _model_4_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 71520);
  _model_4_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _STAI_NETWORK_EVENT_NODE_START_CB(225, 1, { _model_4_m_0_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_4_m_0_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(225, 1, { _model_4_m_0_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_4_m_0_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 71520);
  _model_4_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 71520);
  _model_4_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_0_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 80736);
  _STAI_NETWORK_EVENT_NODE_START_CB(228, 2, { _model_4_m_0_cv1_conv_Conv_output_0_output.data->data,_model_4_m_0_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_4_m_0_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(228, 1, { _model_4_m_0_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_4_m_0_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 71520);
  _model_4_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 71520);
  _model_4_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _STAI_NETWORK_EVENT_NODE_START_CB(234, 1, { _model_4_m_0_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_4_m_0_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(234, 1, { _model_4_m_0_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_4_m_0_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 71520);
  _model_4_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 71520);
  _model_4_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 80736);
  _STAI_NETWORK_EVENT_NODE_START_CB(237, 2, { _model_4_m_0_cv2_conv_Conv_output_0_output.data->data,_model_4_m_0_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_4_m_0_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(237, 1, { _model_4_m_0_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_4_m_0_Add_output_0(_stai_network_context* net_ctx)
{
  _model_4_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  _model_4_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _model_4_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _STAI_NETWORK_EVENT_NODE_START_CB(240, 2, { _model_4_Split_output_0_output1.data->data,_model_4_m_0_cv2_act_Mul_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_4_m_0_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(240, 1, { _model_4_m_0_Add_output_0_output.data->data});
}
void forward_lite_nl_integer__model_4_m_1_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_1_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_1_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_1_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_m_1_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(246, 1, { _model_4_m_1_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_4_m_1_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(246, 1, { _model_4_m_1_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_4_m_1_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_1_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_1_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_1_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_m_1_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_m_1_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 89952);
  _model_4_m_1_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 89952);
  _STAI_NETWORK_EVENT_NODE_START_CB(249, 2, { _model_4_m_1_cv1_conv_Conv_output_0_output.data->data,_model_4_m_1_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_4_m_1_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(249, 1, { _model_4_m_1_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_4_m_1_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_1_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_1_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_1_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_m_1_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(255, 1, { _model_4_m_1_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_4_m_1_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(255, 1, { _model_4_m_1_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_4_m_1_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_1_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_1_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 80736);
  _model_4_m_1_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_m_1_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_m_1_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 89952);
  _model_4_m_1_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 89952);
  _STAI_NETWORK_EVENT_NODE_START_CB(258, 2, { _model_4_m_1_cv2_conv_Conv_output_0_output.data->data,_model_4_m_1_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_4_m_1_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(258, 1, { _model_4_m_1_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_4_m_1_Add_output_0(_stai_network_context* net_ctx)
{
  _model_4_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_1_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 89952);
  _model_4_m_1_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 89952);
  _model_4_m_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_m_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(261, 2, { _model_4_m_0_Add_output_0_output.data->data,_model_4_m_1_cv2_act_Mul_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_4_m_1_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(261, 1, { _model_4_m_1_Add_output_0_output.data->data});
}
void forward_lite_concat__model_4_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_4_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_4_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 36864);
  _model_4_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 46080);
  _model_4_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 46080);
  _model_4_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_m_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_m_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_4_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_4_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73728);
  _STAI_NETWORK_EVENT_NODE_START_CB(264, 4, { _model_4_Split_output_0_output0.data->data,_model_4_Split_output_0_output1.data->data,_model_4_m_0_Add_output_0_output.data->data,_model_4_m_1_Add_output_0_output.data->data});
  forward_concat(&_model_4_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(264, 1, { _model_4_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_4_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_4_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_4_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_4_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _STAI_NETWORK_EVENT_NODE_START_CB(270, 1, { _model_4_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_4_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(270, 1, { _model_4_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_4_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_4_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_4_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_4_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_4_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_4_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73728);
  _STAI_NETWORK_EVENT_NODE_START_CB(273, 2, { _model_4_cv2_conv_Conv_output_0_output.data->data,_model_4_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_4_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(273, 1, { _model_4_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_5_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_5_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 62464);
  _model_5_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 62464);
  _model_5_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_5_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_NETWORK_EVENT_NODE_START_CB(279, 1, { _model_5_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_5_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(279, 1, { _model_5_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_5_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_5_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 62464);
  _model_5_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 62464);
  _model_5_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_5_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_5_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_5_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _STAI_NETWORK_EVENT_NODE_START_CB(282, 2, { _model_5_conv_Conv_output_0_output.data->data,_model_5_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_5_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(282, 1, { _model_5_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_6_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_6_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 56192);
  _model_6_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 56192);
  _model_6_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_NETWORK_EVENT_NODE_START_CB(288, 1, { _model_6_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_6_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(288, 1, { _model_6_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_6_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_6_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 56192);
  _model_6_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 56192);
  _model_6_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_6_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _STAI_NETWORK_EVENT_NODE_START_CB(291, 2, { _model_6_cv1_conv_Conv_output_0_output.data->data,_model_6_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_6_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(291, 1, { _model_6_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_split__model_6_Split_output_0(_stai_network_context* net_ctx)
{
  _model_6_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_6_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_6_Split_output_0_num_or_size_splits_array.data = AI_PTR(net_ctx->_weights[0] + 47672);
  _model_6_Split_output_0_num_or_size_splits_array.data_start = AI_PTR(net_ctx->_weights[0] + 47672);
  _model_6_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 59904);
  _model_6_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 59904);
  _model_6_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_6_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _STAI_NETWORK_EVENT_NODE_START_CB(294, 1, { _model_6_cv1_act_Mul_output_0_output.data->data});
  forward_split(&_model_6_Split_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(294, 2, { _model_6_Split_output_0_output0.data->data,_model_6_Split_output_0_output1.data->data});
}
void forward_lite_nl_integer__model_6_m_0_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 98880);
  _model_6_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 98880);
  _model_6_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(302, 1, { _model_6_m_0_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_6_m_0_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(302, 1, { _model_6_m_0_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_6_m_0_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 98880);
  _model_6_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 98880);
  _model_6_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_0_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _STAI_NETWORK_EVENT_NODE_START_CB(305, 2, { _model_6_m_0_cv1_conv_Conv_output_0_output.data->data,_model_6_m_0_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_6_m_0_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(305, 1, { _model_6_m_0_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_6_m_0_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 98432);
  _model_6_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 98432);
  _model_6_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(311, 1, { _model_6_m_0_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_6_m_0_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(311, 1, { _model_6_m_0_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_6_m_0_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 98432);
  _model_6_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 98432);
  _model_6_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _STAI_NETWORK_EVENT_NODE_START_CB(314, 2, { _model_6_m_0_cv2_conv_Conv_output_0_output.data->data,_model_6_m_0_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_6_m_0_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(314, 1, { _model_6_m_0_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_6_m_0_Add_output_0(_stai_network_context* net_ctx)
{
  _model_6_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 59904);
  _model_6_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 59904);
  _model_6_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(317, 2, { _model_6_Split_output_0_output1.data->data,_model_6_m_0_cv2_act_Mul_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_6_m_0_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(317, 1, { _model_6_m_0_Add_output_0_output.data->data});
}
void forward_lite_nl_integer__model_6_m_1_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_1_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_m_1_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_NETWORK_EVENT_NODE_START_CB(323, 1, { _model_6_m_1_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_6_m_1_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(323, 1, { _model_6_m_1_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_6_m_1_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_1_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_m_1_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_m_1_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_6_m_1_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _STAI_NETWORK_EVENT_NODE_START_CB(326, 2, { _model_6_m_1_cv1_conv_Conv_output_0_output.data->data,_model_6_m_1_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_6_m_1_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(326, 1, { _model_6_m_1_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_6_m_1_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_1_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_m_1_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_NETWORK_EVENT_NODE_START_CB(332, 1, { _model_6_m_1_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_6_m_1_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(332, 1, { _model_6_m_1_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_6_m_1_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_1_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_m_1_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_m_1_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_6_m_1_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _STAI_NETWORK_EVENT_NODE_START_CB(335, 2, { _model_6_m_1_cv2_conv_Conv_output_0_output.data->data,_model_6_m_1_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_6_m_1_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(335, 1, { _model_6_m_1_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_6_m_1_Add_output_0(_stai_network_context* net_ctx)
{
  _model_6_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_1_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_6_m_1_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_6_m_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _STAI_NETWORK_EVENT_NODE_START_CB(338, 2, { _model_6_m_0_Add_output_0_output.data->data,_model_6_m_1_cv2_act_Mul_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_6_m_1_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(338, 1, { _model_6_m_1_Add_output_0_output.data->data});
}
void forward_lite_concat__model_6_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_6_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_6_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 55296);
  _model_6_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 59904);
  _model_6_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 59904);
  _model_6_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_m_1_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_m_1_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_6_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_NETWORK_EVENT_NODE_START_CB(341, 4, { _model_6_Split_output_0_output0.data->data,_model_6_Split_output_0_output1.data->data,_model_6_m_0_Add_output_0_output.data->data,_model_6_m_1_Add_output_0_output.data->data});
  forward_concat(&_model_6_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(341, 1, { _model_6_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_6_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_6_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_6_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_6_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(347, 1, { _model_6_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_6_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(347, 1, { _model_6_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_6_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_6_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_6_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_6_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_6_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _STAI_NETWORK_EVENT_NODE_START_CB(350, 2, { _model_6_cv2_conv_Conv_output_0_output.data->data,_model_6_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_6_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(350, 1, { _model_6_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_7_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_7_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 113920);
  _model_7_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 113920);
  _model_7_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_7_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(356, 1, { _model_7_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_7_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(356, 1, { _model_7_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_7_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_7_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 113920);
  _model_7_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 113920);
  _model_7_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_7_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_7_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_7_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _STAI_NETWORK_EVENT_NODE_START_CB(359, 2, { _model_7_conv_Conv_output_0_output.data->data,_model_7_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_7_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(359, 1, { _model_7_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_8_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_8_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_8_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_8_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_8_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(365, 1, { _model_8_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_8_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(365, 1, { _model_8_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_8_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_8_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_8_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_8_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_8_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_8_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _STAI_NETWORK_EVENT_NODE_START_CB(368, 2, { _model_8_cv1_conv_Conv_output_0_output.data->data,_model_8_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_8_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(368, 1, { _model_8_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_split__model_8_Split_output_0(_stai_network_context* net_ctx)
{
  _model_8_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_Split_output_0_num_or_size_splits_array.data = AI_PTR(net_ctx->_weights[0] + 134972);
  _model_8_Split_output_0_num_or_size_splits_array.data_start = AI_PTR(net_ctx->_weights[0] + 134972);
  _model_8_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 65664);
  _model_8_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 65664);
  _model_8_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_8_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _STAI_NETWORK_EVENT_NODE_START_CB(371, 1, { _model_8_cv1_act_Mul_output_0_output.data->data});
  forward_split(&_model_8_Split_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(371, 2, { _model_8_Split_output_0_output0.data->data,_model_8_Split_output_0_output1.data->data});
}
void forward_lite_nl_integer__model_8_m_0_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_8_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 68864);
  _model_8_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 68864);
  _model_8_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _STAI_NETWORK_EVENT_NODE_START_CB(379, 1, { _model_8_m_0_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_8_m_0_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(379, 1, { _model_8_m_0_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_8_m_0_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_8_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 68864);
  _model_8_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 68864);
  _model_8_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_m_0_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 70016);
  _model_8_m_0_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 70016);
  _STAI_NETWORK_EVENT_NODE_START_CB(382, 2, { _model_8_m_0_cv1_conv_Conv_output_0_output.data->data,_model_8_m_0_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_8_m_0_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(382, 1, { _model_8_m_0_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_8_m_0_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_8_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 68864);
  _model_8_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 68864);
  _model_8_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _STAI_NETWORK_EVENT_NODE_START_CB(388, 1, { _model_8_m_0_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_8_m_0_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(388, 1, { _model_8_m_0_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_8_m_0_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_8_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 68864);
  _model_8_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 68864);
  _model_8_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 70016);
  _model_8_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 70016);
  _STAI_NETWORK_EVENT_NODE_START_CB(391, 2, { _model_8_m_0_cv2_conv_Conv_output_0_output.data->data,_model_8_m_0_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_8_m_0_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(391, 1, { _model_8_m_0_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_8_m_0_Add_output_0(_stai_network_context* net_ctx)
{
  _model_8_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 65664);
  _model_8_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 65664);
  _model_8_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 70016);
  _model_8_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 70016);
  _model_8_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _STAI_NETWORK_EVENT_NODE_START_CB(394, 2, { _model_8_Split_output_0_output1.data->data,_model_8_m_0_cv2_act_Mul_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_8_m_0_Add_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(394, 1, { _model_8_m_0_Add_output_0_output.data->data});
}
void forward_lite_concat__model_8_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_8_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_8_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 64512);
  _model_8_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 65664);
  _model_8_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 65664);
  _model_8_m_0_Add_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_m_0_Add_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 67968);
  _model_8_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 67968);
  _STAI_NETWORK_EVENT_NODE_START_CB(397, 3, { _model_8_Split_output_0_output0.data->data,_model_8_Split_output_0_output1.data->data,_model_8_m_0_Add_output_0_output.data->data});
  forward_concat(&_model_8_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(397, 1, { _model_8_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_8_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_8_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 71424);
  _model_8_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 71424);
  _model_8_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _STAI_NETWORK_EVENT_NODE_START_CB(403, 1, { _model_8_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_8_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(403, 1, { _model_8_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_8_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_8_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 71424);
  _model_8_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 71424);
  _model_8_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_8_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69120);
  _model_8_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69120);
  _STAI_NETWORK_EVENT_NODE_START_CB(406, 2, { _model_8_cv2_conv_Conv_output_0_output.data->data,_model_8_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_8_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(406, 1, { _model_8_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_concat__model_9_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_9_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 67392);
  _model_9_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 67392);
  _model_9_m_MaxPool_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 68544);
  _model_9_m_MaxPool_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 68544);
  _model_9_m_1_MaxPool_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 69696);
  _model_9_m_1_MaxPool_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 69696);
  _model_9_m_2_MaxPool_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 70848);
  _model_9_m_2_MaxPool_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 70848);
  _model_9_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_9_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _STAI_NETWORK_EVENT_NODE_START_CB(421, 4, { _model_9_cv1_conv_Conv_output_0_output.data->data,_model_9_m_MaxPool_output_0_output.data->data,_model_9_m_1_MaxPool_output_0_output.data->data,_model_9_m_2_MaxPool_output_0_output.data->data});
  forward_concat(&_model_9_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(421, 1, { _model_9_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_9_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_9_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 67968);
  _model_9_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 67968);
  _model_9_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 70272);
  _model_9_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 70272);
  _STAI_NETWORK_EVENT_NODE_START_CB(427, 1, { _model_9_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_9_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(427, 1, { _model_9_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_9_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_9_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 67968);
  _model_9_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 67968);
  _model_9_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 70272);
  _model_9_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 70272);
  _model_9_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_9_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _STAI_NETWORK_EVENT_NODE_START_CB(430, 2, { _model_9_cv2_conv_Conv_output_0_output.data->data,_model_9_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_9_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(430, 1, { _model_9_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_upsample_nearest__model_10_Resize_output_0(_stai_network_context* net_ctx)
{
  _model_9_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_9_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_10_Resize_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_10_Resize_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _STAI_NETWORK_EVENT_NODE_START_CB(433, 1, { _model_9_cv2_act_Mul_output_0_output.data->data});
  forward_upsample_nearest(&_model_10_Resize_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(433, 1, { _model_10_Resize_output_0_output.data->data});
}
void forward_lite_concat__model_11_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_10_Resize_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_10_Resize_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_6_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_6_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_11_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_11_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(436, 2, { _model_10_Resize_output_0_output.data->data,_model_6_cv2_act_Mul_output_0_output.data->data});
  forward_concat(&_model_11_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(436, 1, { _model_11_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_12_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_12_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_12_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_12_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _STAI_NETWORK_EVENT_NODE_START_CB(442, 1, { _model_12_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_12_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(442, 1, { _model_12_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_12_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_12_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_12_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_12_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_12_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(445, 2, { _model_12_cv1_conv_Conv_output_0_output.data->data,_model_12_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_12_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(445, 1, { _model_12_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_split__model_12_Split_output_0(_stai_network_context* net_ctx)
{
  _model_12_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_12_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_12_Split_output_0_num_or_size_splits_array.data = AI_PTR(net_ctx->_weights[0] + 179136);
  _model_12_Split_output_0_num_or_size_splits_array.data_start = AI_PTR(net_ctx->_weights[0] + 179136);
  _model_12_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_12_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_12_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_12_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _STAI_NETWORK_EVENT_NODE_START_CB(448, 1, { _model_12_cv1_act_Mul_output_0_output.data->data});
  forward_split(&_model_12_Split_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(448, 2, { _model_12_Split_output_0_output0.data->data,_model_12_Split_output_0_output1.data->data});
}
void forward_lite_nl_integer__model_12_m_0_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_12_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_12_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_12_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _STAI_NETWORK_EVENT_NODE_START_CB(456, 1, { _model_12_m_0_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_12_m_0_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(456, 1, { _model_12_m_0_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_12_m_0_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_12_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_12_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_12_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_m_0_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_12_m_0_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(459, 2, { _model_12_m_0_cv1_conv_Conv_output_0_output.data->data,_model_12_m_0_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_12_m_0_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(459, 1, { _model_12_m_0_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_12_m_0_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_12_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_12_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_12_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _STAI_NETWORK_EVENT_NODE_START_CB(465, 1, { _model_12_m_0_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_12_m_0_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(465, 1, { _model_12_m_0_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_12_m_0_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_12_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_12_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_12_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_12_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(468, 2, { _model_12_m_0_cv2_conv_Conv_output_0_output.data->data,_model_12_m_0_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_12_m_0_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(468, 1, { _model_12_m_0_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_concat__model_12_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_12_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_12_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 66816);
  _model_12_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_12_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 92160);
  _model_12_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_12_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_12_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_12_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(471, 3, { _model_12_Split_output_0_output0.data->data,_model_12_Split_output_0_output1.data->data,_model_12_m_0_cv2_act_Mul_output_0_output.data->data});
  forward_concat(&_model_12_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(471, 1, { _model_12_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_12_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_12_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_12_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(477, 1, { _model_12_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_12_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(477, 1, { _model_12_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_12_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_12_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_12_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_12_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_12_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_12_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _STAI_NETWORK_EVENT_NODE_START_CB(480, 2, { _model_12_cv2_conv_Conv_output_0_output.data->data,_model_12_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_12_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(480, 1, { _model_12_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_upsample_nearest__model_13_Resize_output_0(_stai_network_context* net_ctx)
{
  _model_12_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_12_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_13_Resize_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_13_Resize_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _STAI_NETWORK_EVENT_NODE_START_CB(483, 1, { _model_12_cv2_act_Mul_output_0_output.data->data});
  forward_upsample_nearest(&_model_13_Resize_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(483, 1, { _model_13_Resize_output_0_output.data->data});
}
void forward_lite_concat__model_14_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_13_Resize_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_13_Resize_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_4_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_4_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 73728);
  _model_14_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_14_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _STAI_NETWORK_EVENT_NODE_START_CB(486, 2, { _model_13_Resize_output_0_output.data->data,_model_4_cv2_act_Mul_output_0_output.data->data});
  forward_concat(&_model_14_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(486, 1, { _model_14_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_15_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_15_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 72128);
  _model_15_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 72128);
  _model_15_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _STAI_NETWORK_EVENT_NODE_START_CB(492, 1, { _model_15_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_15_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(492, 1, { _model_15_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_15_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_15_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 72128);
  _model_15_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 72128);
  _model_15_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _STAI_NETWORK_EVENT_NODE_START_CB(495, 2, { _model_15_cv1_conv_Conv_output_0_output.data->data,_model_15_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_15_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(495, 1, { _model_15_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_split__model_15_Split_output_0(_stai_network_context* net_ctx)
{
  _model_15_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_Split_output_0_num_or_size_splits_array.data = AI_PTR(net_ctx->_weights[0] + 207428);
  _model_15_Split_output_0_num_or_size_splits_array.data_start = AI_PTR(net_ctx->_weights[0] + 207428);
  _model_15_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 80640);
  _model_15_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 80640);
  _model_15_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 71424);
  _model_15_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 71424);
  _STAI_NETWORK_EVENT_NODE_START_CB(498, 1, { _model_15_cv1_act_Mul_output_0_output.data->data});
  forward_split(&_model_15_Split_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(498, 2, { _model_15_Split_output_0_output0.data->data,_model_15_Split_output_0_output1.data->data});
}
void forward_lite_nl_integer__model_15_m_0_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_15_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _STAI_NETWORK_EVENT_NODE_START_CB(506, 1, { _model_15_m_0_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_15_m_0_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(506, 1, { _model_15_m_0_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_15_m_0_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_15_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_m_0_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_15_m_0_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(509, 2, { _model_15_m_0_cv1_conv_Conv_output_0_output.data->data,_model_15_m_0_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_15_m_0_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(509, 1, { _model_15_m_0_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_15_m_0_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_15_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 109088);
  _model_15_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 109088);
  _model_15_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _STAI_NETWORK_EVENT_NODE_START_CB(515, 1, { _model_15_m_0_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_15_m_0_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(515, 1, { _model_15_m_0_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_15_m_0_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_15_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 109088);
  _model_15_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 109088);
  _model_15_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 140544);
  _model_15_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 140544);
  _STAI_NETWORK_EVENT_NODE_START_CB(518, 2, { _model_15_m_0_cv2_conv_Conv_output_0_output.data->data,_model_15_m_0_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_15_m_0_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(518, 1, { _model_15_m_0_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_concat__model_15_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_15_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 71424);
  _model_15_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 71424);
  _model_15_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 80640);
  _model_15_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 80640);
  _model_15_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 140544);
  _model_15_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 140544);
  _model_15_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 149760);
  _model_15_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 149760);
  _STAI_NETWORK_EVENT_NODE_START_CB(521, 3, { _model_15_Split_output_0_output0.data->data,_model_15_Split_output_0_output1.data->data,_model_15_m_0_cv2_act_Mul_output_0_output.data->data});
  forward_concat(&_model_15_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(521, 1, { _model_15_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_15_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_15_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _STAI_NETWORK_EVENT_NODE_START_CB(527, 1, { _model_15_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_15_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(527, 1, { _model_15_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_15_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_15_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_15_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_15_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 149760);
  _model_15_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 149760);
  _STAI_NETWORK_EVENT_NODE_START_CB(530, 2, { _model_15_cv2_conv_Conv_output_0_output.data->data,_model_15_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_15_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(530, 1, { _model_15_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_16_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_16_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_16_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_16_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_16_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _STAI_NETWORK_EVENT_NODE_START_CB(542, 1, { _model_16_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_16_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(542, 1, { _model_16_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_16_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_16_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_16_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_16_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_16_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_16_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_16_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(551, 2, { _model_16_conv_Conv_output_0_output.data->data,_model_16_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_16_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(551, 1, { _model_16_act_Mul_output_0_output.data->data});
}
void forward_lite_concat__model_17_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_16_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_16_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_12_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_12_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_17_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 131328);
  _model_17_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 131328);
  _STAI_NETWORK_EVENT_NODE_START_CB(560, 2, { _model_16_act_Mul_output_0_output.data->data,_model_12_cv2_act_Mul_output_0_output.data->data});
  forward_concat(&_model_17_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(560, 1, { _model_17_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_18_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_18_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_18_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_18_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_18_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(578, 1, { _model_18_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_18_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(578, 1, { _model_18_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_18_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_18_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_18_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_18_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_18_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_18_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_18_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _STAI_NETWORK_EVENT_NODE_START_CB(587, 2, { _model_18_cv1_conv_Conv_output_0_output.data->data,_model_18_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_18_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(587, 1, { _model_18_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_split__model_18_Split_output_0(_stai_network_context* net_ctx)
{
  _model_18_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_18_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_18_Split_output_0_num_or_size_splits_array.data = AI_PTR(net_ctx->_weights[0] + 229576);
  _model_18_Split_output_0_num_or_size_splits_array.data_start = AI_PTR(net_ctx->_weights[0] + 229576);
  _model_18_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_18_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_18_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_18_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _STAI_NETWORK_EVENT_NODE_START_CB(596, 1, { _model_18_cv1_act_Mul_output_0_output.data->data});
  forward_split(&_model_18_Split_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(596, 2, { _model_18_Split_output_0_output0.data->data,_model_18_Split_output_0_output1.data->data});
}
void forward_lite_nl_integer__model_18_m_0_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_18_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 121280);
  _model_18_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 121280);
  _model_18_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(610, 1, { _model_18_m_0_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_18_m_0_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(610, 1, { _model_18_m_0_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_18_m_0_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_18_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 121280);
  _model_18_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 121280);
  _model_18_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_m_0_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_18_m_0_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(613, 2, { _model_18_m_0_cv1_conv_Conv_output_0_output.data->data,_model_18_m_0_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_18_m_0_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(613, 1, { _model_18_m_0_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_18_m_0_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_18_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 123776);
  _model_18_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 123776);
  _model_18_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(619, 1, { _model_18_m_0_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_18_m_0_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(619, 1, { _model_18_m_0_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_18_m_0_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_18_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 123776);
  _model_18_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 123776);
  _model_18_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_18_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(622, 2, { _model_18_m_0_cv2_conv_Conv_output_0_output.data->data,_model_18_m_0_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_18_m_0_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(622, 1, { _model_18_m_0_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_concat__model_18_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_18_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_18_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 96768);
  _model_18_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_18_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 103680);
  _model_18_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_18_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_18_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 117504);
  _model_18_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 117504);
  _STAI_NETWORK_EVENT_NODE_START_CB(625, 3, { _model_18_Split_output_0_output0.data->data,_model_18_Split_output_0_output1.data->data,_model_18_m_0_cv2_act_Mul_output_0_output.data->data});
  forward_concat(&_model_18_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(625, 1, { _model_18_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_18_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_18_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 117504);
  _model_18_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 117504);
  _STAI_NETWORK_EVENT_NODE_START_CB(631, 1, { _model_18_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_18_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(631, 1, { _model_18_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_18_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_18_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_18_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 117504);
  _model_18_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 117504);
  _model_18_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 126720);
  _model_18_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 126720);
  _STAI_NETWORK_EVENT_NODE_START_CB(634, 2, { _model_18_cv2_conv_Conv_output_0_output.data->data,_model_18_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_18_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(634, 1, { _model_18_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_19_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_19_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_19_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_19_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_19_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(646, 1, { _model_19_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_19_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(646, 1, { _model_19_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_19_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_19_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_19_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_19_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_19_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_19_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_19_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _STAI_NETWORK_EVENT_NODE_START_CB(655, 2, { _model_19_conv_Conv_output_0_output.data->data,_model_19_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_19_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(655, 1, { _model_19_act_Mul_output_0_output.data->data});
}
void forward_lite_concat__model_20_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_19_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_19_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_9_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_9_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_20_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_20_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(664, 2, { _model_19_act_Mul_output_0_output.data->data,_model_9_cv2_act_Mul_output_0_output.data->data});
  forward_concat(&_model_20_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(664, 1, { _model_20_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_21_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_21_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_21_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 89856);
  _STAI_NETWORK_EVENT_NODE_START_CB(682, 1, { _model_21_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_21_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(682, 1, { _model_21_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_21_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_21_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_21_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_21_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_21_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(691, 2, { _model_21_cv1_conv_Conv_output_0_output.data->data,_model_21_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_21_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(691, 1, { _model_21_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_split__model_21_Split_output_0(_stai_network_context* net_ctx)
{
  _model_21_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_21_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_21_Split_output_0_num_or_size_splits_array.data = AI_PTR(net_ctx->_weights[0] + 300236);
  _model_21_Split_output_0_num_or_size_splits_array.data_start = AI_PTR(net_ctx->_weights[0] + 300236);
  _model_21_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 91008);
  _model_21_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 91008);
  _model_21_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_21_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 89856);
  _STAI_NETWORK_EVENT_NODE_START_CB(700, 1, { _model_21_cv1_act_Mul_output_0_output.data->data});
  forward_split(&_model_21_Split_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(700, 2, { _model_21_Split_output_0_output0.data->data,_model_21_Split_output_0_output1.data->data});
}
void forward_lite_nl_integer__model_21_m_0_cv1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_21_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115008);
  _model_21_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115008);
  _model_21_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _STAI_NETWORK_EVENT_NODE_START_CB(714, 1, { _model_21_m_0_cv1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_21_m_0_cv1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(714, 1, { _model_21_m_0_cv1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_21_m_0_cv1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_21_m_0_cv1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 115008);
  _model_21_m_0_cv1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 115008);
  _model_21_m_0_cv1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_m_0_cv1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_m_0_cv1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_21_m_0_cv1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102528);
  _STAI_NETWORK_EVENT_NODE_START_CB(717, 2, { _model_21_m_0_cv1_conv_Conv_output_0_output.data->data,_model_21_m_0_cv1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_21_m_0_cv1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(717, 1, { _model_21_m_0_cv1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_21_m_0_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_21_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_21_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102528);
  _STAI_NETWORK_EVENT_NODE_START_CB(723, 1, { _model_21_m_0_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_21_m_0_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(723, 1, { _model_21_m_0_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_21_m_0_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_21_m_0_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_m_0_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_m_0_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_21_m_0_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_21_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_21_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(726, 2, { _model_21_m_0_cv2_conv_Conv_output_0_output.data->data,_model_21_m_0_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_21_m_0_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(726, 1, { _model_21_m_0_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_concat__model_21_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_21_Split_output_0_output0_array.data = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_21_Split_output_0_output0_array.data_start = AI_PTR(net_ctx->_activations[0] + 89856);
  _model_21_Split_output_0_output1_array.data = AI_PTR(net_ctx->_activations[0] + 91008);
  _model_21_Split_output_0_output1_array.data_start = AI_PTR(net_ctx->_activations[0] + 91008);
  _model_21_m_0_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_21_m_0_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_21_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 109440);
  _model_21_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 109440);
  _STAI_NETWORK_EVENT_NODE_START_CB(729, 3, { _model_21_Split_output_0_output0.data->data,_model_21_Split_output_0_output1.data->data,_model_21_m_0_cv2_act_Mul_output_0_output.data->data});
  forward_concat(&_model_21_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(729, 1, { _model_21_Concat_output_0_output.data->data});
}
void forward_lite_nl_integer__model_21_cv2_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_21_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_21_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_21_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _STAI_NETWORK_EVENT_NODE_START_CB(735, 1, { _model_21_cv2_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_21_cv2_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(735, 1, { _model_21_cv2_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_21_cv2_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_21_cv2_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_21_cv2_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_21_cv2_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_cv2_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_21_cv2_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_21_cv2_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(738, 2, { _model_21_cv2_conv_Conv_output_0_output.data->data,_model_21_cv2_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_21_cv2_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(738, 1, { _model_21_cv2_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _STAI_NETWORK_EVENT_NODE_START_CB(747, 1, { _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(747, 1, { _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv2_2_cv2_2_0_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_22_cv2_2_cv2_2_0_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_2_cv2_2_0_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(753, 2, { _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_output.data->data,_model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv2_2_cv2_2_0_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(753, 1, { _model_22_cv2_2_cv2_2_0_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _STAI_NETWORK_EVENT_NODE_START_CB(765, 1, { _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(765, 1, { _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv2_2_cv2_2_1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_22_cv2_2_cv2_2_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_2_cv2_2_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(771, 2, { _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_output.data->data,_model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv2_2_cv2_2_1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(771, 1, { _model_22_cv2_2_cv2_2_1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102528);
  _STAI_NETWORK_EVENT_NODE_START_CB(748, 1, { _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(748, 1, { _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv3_2_cv3_2_0_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_22_cv3_2_cv3_2_0_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_cv3_2_cv3_2_0_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(754, 2, { _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_output.data->data,_model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv3_2_cv3_2_0_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(754, 1, { _model_22_cv3_2_cv3_2_0_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _STAI_NETWORK_EVENT_NODE_START_CB(766, 1, { _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(766, 1, { _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv3_2_cv3_2_1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_22_cv3_2_cv3_2_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102528);
  _STAI_NETWORK_EVENT_NODE_START_CB(772, 2, { _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_output.data->data,_model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv3_2_cv3_2_1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(772, 1, { _model_22_cv3_2_cv3_2_1_act_Mul_output_0_output.data->data});
}
void forward_lite_conv2d_integer_SSSA__model_22_cv3_2_cv3_2_2_Conv_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_2_cv3_2_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_22_cv3_2_cv3_2_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102528);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 431824);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 431824);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 431856);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 431856);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101504);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101504);
  _STAI_NETWORK_EVENT_NODE_START_CB(778, 1, { _model_22_cv3_2_cv3_2_1_act_Mul_output_0_output.data->data});
  forward_conv2d_integer_SSSA(&_model_22_cv3_2_cv3_2_2_Conv_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(778, 1, { _model_22_cv3_2_cv3_2_2_Conv_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(647, 1, { _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(647, 1, { _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv2_1_cv2_1_0_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_1_cv2_1_0_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 135936);
  _model_22_cv2_1_cv2_1_0_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 135936);
  _STAI_NETWORK_EVENT_NODE_START_CB(656, 2, { _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_output.data->data,_model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv2_1_cv2_1_0_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(656, 1, { _model_22_cv2_1_cv2_1_0_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _STAI_NETWORK_EVENT_NODE_START_CB(674, 1, { _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(674, 1, { _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv2_1_cv2_1_1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_1_cv2_1_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 135936);
  _model_22_cv2_1_cv2_1_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 135936);
  _STAI_NETWORK_EVENT_NODE_START_CB(683, 2, { _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_output.data->data,_model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv2_1_cv2_1_1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(683, 1, { _model_22_cv2_1_cv2_1_1_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 129984);
  _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 129984);
  _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _STAI_NETWORK_EVENT_NODE_START_CB(648, 1, { _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(648, 1, { _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv3_1_cv3_1_0_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 129984);
  _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 129984);
  _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_22_cv3_1_cv3_1_0_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 134592);
  _model_22_cv3_1_cv3_1_0_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 134592);
  _STAI_NETWORK_EVENT_NODE_START_CB(657, 2, { _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_output.data->data,_model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv3_1_cv3_1_0_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(657, 1, { _model_22_cv3_1_cv3_1_0_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 135104);
  _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 135104);
  _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _STAI_NETWORK_EVENT_NODE_START_CB(675, 1, { _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(675, 1, { _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv3_1_cv3_1_1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 135104);
  _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 135104);
  _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 122112);
  _model_22_cv3_1_cv3_1_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 126720);
  _model_22_cv3_1_cv3_1_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 126720);
  _STAI_NETWORK_EVENT_NODE_START_CB(684, 2, { _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_output.data->data,_model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv3_1_cv3_1_1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(684, 1, { _model_22_cv3_1_cv3_1_1_act_Mul_output_0_output.data->data});
}
void forward_lite_conv2d_integer_SSSA__model_22_cv3_1_cv3_1_2_Conv_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_1_cv3_1_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 126720);
  _model_22_cv3_1_cv3_1_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 126720);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 538356);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 538356);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 538388);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 538388);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101540);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101540);
  _STAI_NETWORK_EVENT_NODE_START_CB(693, 1, { _model_22_cv3_1_cv3_1_1_act_Mul_output_0_output.data->data});
  forward_conv2d_integer_SSSA(&_model_22_cv3_1_cv3_1_2_Conv_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(693, 1, { _model_22_cv3_1_cv3_1_2_Conv_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 175360);
  _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 175360);
  _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 212224);
  _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 212224);
  _STAI_NETWORK_EVENT_NODE_START_CB(543, 1, { _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(543, 1, { _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv2_0_cv2_0_0_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 175360);
  _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 175360);
  _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 212224);
  _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 212224);
  _model_22_cv2_0_cv2_0_0_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 175360);
  _model_22_cv2_0_cv2_0_0_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 175360);
  _STAI_NETWORK_EVENT_NODE_START_CB(552, 2, { _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_output.data->data,_model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv2_0_cv2_0_0_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(552, 1, { _model_22_cv2_0_cv2_0_0_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 212224);
  _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 212224);
  _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _STAI_NETWORK_EVENT_NODE_START_CB(570, 1, { _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(570, 1, { _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv2_0_cv2_0_1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 212224);
  _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 212224);
  _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_0_cv2_0_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 212224);
  _model_22_cv2_0_cv2_0_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 212224);
  _STAI_NETWORK_EVENT_NODE_START_CB(579, 2, { _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_output.data->data,_model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv2_0_cv2_0_1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(579, 1, { _model_22_cv2_0_cv2_0_1_act_Mul_output_0_output.data->data});
}
void forward_lite_concat__model_22_Concat_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv2_0_cv2_0_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_0_cv2_0_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_cv2_1_cv2_1_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_1_cv2_1_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112896);
  _model_22_cv2_2_cv2_2_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_22_cv2_2_cv2_2_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 110592);
  _model_22_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 205056);
  _model_22_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 205056);
  _STAI_NETWORK_EVENT_NODE_START_CB(789, 3, { _model_22_cv2_0_cv2_0_2_Conv_output_0_output0.data->data,_model_22_cv2_1_cv2_1_2_Conv_output_0_output0.data->data,_model_22_cv2_2_cv2_2_2_Conv_output_0_output0.data->data});
  forward_concat(&_model_22_Concat_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(789, 1, { _model_22_Concat_output_0_output.data->data});
}
void forward_lite_transpose__model_22_dfl_Reshape_output_0_to_chlast(_stai_network_context* net_ctx)
{
  _model_22_Concat_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 205056);
  _model_22_Concat_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 205056);
  _model_22_dfl_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 253440);
  _model_22_dfl_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 253440);
  _STAI_NETWORK_EVENT_NODE_START_CB(795, 1, { _model_22_Concat_output_0_output.data->data});
  forward_transpose(&_model_22_dfl_Reshape_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(795, 1, { _model_22_dfl_Reshape_output_0_to_chlast_output.data->data});
}
void forward_lite_transpose__model_22_dfl_Transpose_output_0(_stai_network_context* net_ctx)
{
  _model_22_dfl_Reshape_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 253440);
  _model_22_dfl_Reshape_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 253440);
  _model_22_dfl_Transpose_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_dfl_Transpose_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _STAI_NETWORK_EVENT_NODE_START_CB(801, 1, { _model_22_dfl_Reshape_output_0_to_chlast_output0.data->data});
  forward_transpose(&_model_22_dfl_Transpose_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(801, 1, { _model_22_dfl_Transpose_output_0_output.data->data});
}
void forward_lite_conv2d_integer_SSSA__model_22_dfl_conv_Conv_output_0(_stai_network_context* net_ctx)
{
  _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output_array.data = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 168192);
  _model_22_dfl_conv_Conv_output_0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 598552);
  _model_22_dfl_conv_Conv_output_0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 598552);
  _model_22_dfl_conv_Conv_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 598568);
  _model_22_dfl_conv_Conv_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 598568);
  _model_22_dfl_conv_Conv_output_0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_dfl_conv_Conv_output_0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_dfl_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_dfl_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(807, 1, { _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_output.data->data});
  forward_conv2d_integer_SSSA(&_model_22_dfl_conv_Conv_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(807, 1, { _model_22_dfl_conv_Conv_output_0_output.data->data});
}
void forward_lite_transpose__model_22_dfl_Reshape_1_output_0_to_chlast(_stai_network_context* net_ctx)
{
  _model_22_dfl_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_dfl_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_dfl_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_dfl_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _STAI_NETWORK_EVENT_NODE_START_CB(810, 1, { _model_22_dfl_conv_Conv_output_0_output.data->data});
  forward_transpose(&_model_22_dfl_Reshape_1_output_0_to_chlast_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(810, 1, { _model_22_dfl_Reshape_1_output_0_to_chlast_output.data->data});
}
void forward_lite_slice__model_22_Slice_1_output_0(_stai_network_context* net_ctx)
{
  _model_22_dfl_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_dfl_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101684);
  _STAI_NETWORK_EVENT_NODE_START_CB(814, 1, { _model_22_dfl_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_slice(&_model_22_Slice_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(814, 1, { _model_22_Slice_1_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_Add_1_output_0(_stai_network_context* net_ctx)
{
  _model_22_Constant_13_output_0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 2272);
  _model_22_Constant_13_output_0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 2272);
  _model_22_Slice_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Slice_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Add_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_Add_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(820, 2, { _model_22_Constant_13_output_0_DequantizeLinear_Output_const.data->data,_model_22_Slice_1_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_Add_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(820, 1, { _model_22_Add_1_output_0_output.data->data});
}
void forward_lite_slice__model_22_Slice_output_0(_stai_network_context* net_ctx)
{
  _model_22_dfl_Reshape_1_output_0_to_chlast_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_dfl_Reshape_1_output_0_to_chlast_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101684);
  _STAI_NETWORK_EVENT_NODE_START_CB(813, 1, { _model_22_dfl_Reshape_1_output_0_to_chlast_output0.data->data});
  forward_slice(&_model_22_Slice_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(813, 1, { _model_22_Slice_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_Sub_output_0(_stai_network_context* net_ctx)
{
  _model_22_Constant_12_output_0_DequantizeLinear_Output_const_array.data = AI_PTR(net_ctx->_weights[0] + 760);
  _model_22_Constant_12_output_0_DequantizeLinear_Output_const_array.data_start = AI_PTR(net_ctx->_weights[0] + 760);
  _model_22_Slice_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Slice_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Sub_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 109800);
  _model_22_Sub_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 109800);
  _STAI_NETWORK_EVENT_NODE_START_CB(819, 2, { _model_22_Constant_12_output_0_DequantizeLinear_Output_const.data->data,_model_22_Slice_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_Sub_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(819, 1, { _model_22_Sub_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_Sub_1_output_0(_stai_network_context* net_ctx)
{
  _model_22_Add_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_Add_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_Sub_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 109800);
  _model_22_Sub_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 109800);
  _model_22_Sub_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Sub_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101684);
  _STAI_NETWORK_EVENT_NODE_START_CB(826, 2, { _model_22_Add_1_output_0_output.data->data,_model_22_Sub_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_Sub_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(826, 1, { _model_22_Sub_1_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_Add_2_output_0(_stai_network_context* net_ctx)
{
  _model_22_Sub_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 109800);
  _model_22_Sub_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 109800);
  _model_22_Add_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_Add_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_Add_2_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_Add_2_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _STAI_NETWORK_EVENT_NODE_START_CB(825, 2, { _model_22_Sub_output_0_output.data->data,_model_22_Add_1_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_Add_2_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(825, 1, { _model_22_Add_2_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_Div_1_output_0(_stai_network_context* net_ctx)
{
  _model_22_Add_2_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_Add_2_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 756);
  _model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 756);
  _model_22_Div_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_Div_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _STAI_NETWORK_EVENT_NODE_START_CB(831, 2, { _model_22_Add_2_output_0_output.data->data,_model_22_Constant_14_output_0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&_model_22_Div_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(831, 1, { _model_22_Div_1_output_0_output.data->data});
}
void forward_lite_concat__model_22_Concat_2_output_0(_stai_network_context* net_ctx)
{
  _model_22_Div_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_Div_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 108288);
  _model_22_Sub_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Sub_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_Concat_2_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 109800);
  _model_22_Concat_2_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 109800);
  _STAI_NETWORK_EVENT_NODE_START_CB(834, 2, { _model_22_Div_1_output_0_output.data->data,_model_22_Sub_1_output_0_output.data->data});
  forward_concat(&_model_22_Concat_2_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(834, 1, { _model_22_Concat_2_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_Mul_2_output_0_QuantizeLinear_Input(_stai_network_context* net_ctx)
{
  _model_22_Concat_2_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 109800);
  _model_22_Concat_2_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 109800);
  _model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D_array.data = AI_PTR(net_ctx->_weights[0] + 0);
  _model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D_array.data_start = AI_PTR(net_ctx->_weights[0] + 0);
  _model_22_Mul_2_output_0_QuantizeLinear_Input_output_array.data = AI_PTR(net_ctx->_activations[0] + 112824);
  _model_22_Mul_2_output_0_QuantizeLinear_Input_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112824);
  _STAI_NETWORK_EVENT_NODE_START_CB(837, 2, { _model_22_Concat_2_output_0_output.data->data,_model_22_Constant_15_output_0_DequantizeLinear_Output_const_3D.data->data});
  forward_eltwise_integer_INT8(&_model_22_Mul_2_output_0_QuantizeLinear_Input_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(837, 1, { _model_22_Mul_2_output_0_QuantizeLinear_Input_output.data->data});
}
void forward_lite_transpose__model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0(_stai_network_context* net_ctx)
{
  _model_22_Mul_2_output_0_QuantizeLinear_Input_output_array.data = AI_PTR(net_ctx->_activations[0] + 112824);
  _model_22_Mul_2_output_0_QuantizeLinear_Input_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 112824);
  _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output_array.data = AI_PTR(net_ctx->_outputs[0] + 0);
  _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output_array.data_start = AI_PTR(net_ctx->_outputs[0] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(1, 1, { _model_22_Mul_2_output_0_QuantizeLinear_Input_output.data->data});
  forward_transpose(&_model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(1, 1, { _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 139664);
  _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 139664);
  _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _STAI_NETWORK_EVENT_NODE_START_CB(544, 1, { _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(544, 1, { _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv3_0_cv3_0_0_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 139664);
  _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 139664);
  _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_cv3_0_cv3_0_0_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 158096);
  _model_22_cv3_0_cv3_0_0_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 158096);
  _STAI_NETWORK_EVENT_NODE_START_CB(553, 2, { _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_output.data->data,_model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv3_0_cv3_0_0_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(553, 1, { _model_22_cv3_0_cv3_0_0_act_Mul_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 139664);
  _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 139664);
  _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _STAI_NETWORK_EVENT_NODE_START_CB(571, 1, { _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output.data->data});
  forward_nl_integer(&_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(571, 1, { _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output.data->data});
}
void forward_lite_eltwise_integer_INT8__model_22_cv3_0_cv3_0_1_act_Mul_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 139664);
  _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 139664);
  _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 111312);
  _model_22_cv3_0_cv3_0_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 158096);
  _model_22_cv3_0_cv3_0_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 158096);
  _STAI_NETWORK_EVENT_NODE_START_CB(580, 2, { _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_output.data->data,_model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0_output.data->data});
  forward_eltwise_integer_INT8(&_model_22_cv3_0_cv3_0_1_act_Mul_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(580, 1, { _model_22_cv3_0_cv3_0_1_act_Mul_output_0_output.data->data});
}
void forward_lite_conv2d_integer_SSSA__model_22_cv3_0_cv3_0_2_Conv_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_0_cv3_0_1_act_Mul_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 158096);
  _model_22_cv3_0_cv3_0_1_act_Mul_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 158096);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_weights_array.data = AI_PTR(net_ctx->_weights[0] + 617260);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_weights_array.data_start = AI_PTR(net_ctx->_weights[0] + 617260);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_bias_array.data = AI_PTR(net_ctx->_weights[0] + 617292);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_bias_array.data_start = AI_PTR(net_ctx->_weights[0] + 617292);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_scratch0_array.data = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_scratch0_array.data_start = AI_PTR(net_ctx->_activations[0] + 101376);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101684);
  _STAI_NETWORK_EVENT_NODE_START_CB(589, 1, { _model_22_cv3_0_cv3_0_1_act_Mul_output_0_output.data->data});
  forward_conv2d_integer_SSSA(&_model_22_cv3_0_cv3_0_2_Conv_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(589, 1, { _model_22_cv3_0_cv3_0_2_Conv_output_0_output.data->data});
}
void forward_lite_concat__model_22_Concat_1_output_0(_stai_network_context* net_ctx)
{
  _model_22_cv3_0_cv3_0_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_cv3_0_cv3_0_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101684);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101540);
  _model_22_cv3_1_cv3_1_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101540);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 101504);
  _model_22_cv3_2_cv3_2_2_Conv_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 101504);
  _model_22_Concat_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102260);
  _model_22_Concat_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102260);
  _STAI_NETWORK_EVENT_NODE_START_CB(790, 3, { _model_22_cv3_0_cv3_0_2_Conv_output_0_output0.data->data,_model_22_cv3_1_cv3_1_2_Conv_output_0_output0.data->data,_model_22_cv3_2_cv3_2_2_Conv_output_0_output0.data->data});
  forward_concat(&_model_22_Concat_1_output_0_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(790, 1, { _model_22_Concat_1_output_0_output.data->data});
}
void forward_lite_nl_integer__model_22_Sigmoid_output_0_QuantizeLinear_Input(_stai_network_context* net_ctx)
{
  _model_22_Concat_1_output_0_output_array.data = AI_PTR(net_ctx->_activations[0] + 102260);
  _model_22_Concat_1_output_0_output_array.data_start = AI_PTR(net_ctx->_activations[0] + 102260);
  _model_22_Sigmoid_output_0_QuantizeLinear_Input_output_array.data = AI_PTR(net_ctx->_outputs[1] + 0);
  _model_22_Sigmoid_output_0_QuantizeLinear_Input_output_array.data_start = AI_PTR(net_ctx->_outputs[1] + 0);
  _STAI_NETWORK_EVENT_NODE_START_CB(796, 1, { _model_22_Concat_1_output_0_output.data->data});
  forward_nl_integer(&_model_22_Sigmoid_output_0_QuantizeLinear_Input_layer);
  _STAI_NETWORK_EVENT_NODE_STOP_CB(796, 1, { _model_22_Sigmoid_output_0_QuantizeLinear_Input_output.data->data});
}

/*****************************************************************************/


static const ai_u16 _model_0_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 192;
static const ai_u16 _model_0_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 192;
static const ai_u16 _model_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 1;
static const ai_u16 _model_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 8;
static const ai_u16 _model_0_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_0_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_0_conv_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _model_0_conv_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i32 _model_0_conv_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _model_0_conv_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_i8 _model_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -128;
static const ai_i8 _model_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -5;
static const ai_float _model_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.003921568859368563f;
static const ai_float _model_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.7047433853149414f;
static const ai_float _model_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.288398414850235f, 0.3156549036502838f, 0.39518624544143677f, 0.04081791639328003f, 0.25941744446754456f, 0.11971151828765869f, 0.07991745322942734f, 0.11062818020582199f);
static const ai_layer_format_type _model_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_0_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 96;
static const ai_u16 _model_0_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 96;



static const ai_u16 _model_1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 96;
static const ai_u16 _model_1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 96;
static const ai_u16 _model_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 8;
static const ai_u16 _model_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_1_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_1_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_1_conv_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _model_1_conv_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i32 _model_1_conv_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _model_1_conv_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_i8 _model_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -127;
static const ai_i8 _model_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 48;
static const ai_float _model_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.3665452003479004f;
static const ai_float _model_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.24923212826251984f;
static const ai_float _model_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002258595312014222f, 0.0018338676309213042f, 0.00106597482226789f, 0.0006753256893716753f, 0.0005264161736704409f, 0.0021601933985948563f, 0.0011357837356626987f, 0.0008908999152481556f, 0.0020941307302564383f, 0.0022850881796330214f, 0.0009374777437187731f, 0.0021477823611348867f, 0.0012884190073236823f, 0.0006773003260605037f, 0.0012907764175906777f, 0.002242048503831029f);
static const ai_layer_format_type _model_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 48;
static const ai_u16 _model_1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 48;



static const ai_u16 _model_2_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 48;
static const ai_u16 _model_2_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 48;
static const ai_u16 _model_2_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_2_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_2_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_2_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_i8 _model_2_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -124;
static const ai_i8 _model_2_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 43;
static const ai_float _model_2_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.07827641069889069f;
static const ai_float _model_2_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.2619330883026123f;
static const ai_float _model_2_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0065629854798316956f, 0.009743725880980492f, 0.01657387986779213f, 0.006286585703492165f, 0.012394089251756668f, 0.006627670023590326f, 0.012316820211708546f, 0.0064757950603961945f, 0.006389137357473373f, 0.012772724963724613f, 0.004248654469847679f, 0.004234867170453072f, 0.016088323667645454f, 0.003912473097443581f, 0.003859536023810506f, 0.006466038059443235f);
static const ai_layer_format_type _model_2_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 48;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 48;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 8;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 8;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i32 _model_2_m_0_cv1_conv_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _model_2_m_0_cv1_conv_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_i8 _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -125;
static const ai_i8 _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 5;
static const ai_float _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.08771698921918869f;
static const ai_float _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.10575424879789352f;
static const ai_float _model_2_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002320650964975357f, 0.004827298689633608f, 0.006028399337083101f, 0.0037968168035149574f, 0.004329901188611984f, 0.002194878412410617f, 0.0038613297510892153f, 0.004637247417122126f);
static const ai_layer_format_type _model_2_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 48;
static const ai_u16 _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 48;



static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 48;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 48;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 8;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 8;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i32 _model_2_m_0_cv2_conv_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _model_2_m_0_cv2_conv_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_i8 _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -123;
static const ai_i8 _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 31;
static const ai_float _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.051501937210559845f;
static const ai_float _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.13174638152122498f;
static const ai_float _model_2_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0036863270215690136f, 0.0037174599710851908f, 0.005787063855677843f, 0.004845174960792065f, 0.005963884759694338f, 0.003823615377768874f, 0.003262750105932355f, 0.0033556343987584114f);
static const ai_layer_format_type _model_2_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 48;
static const ai_u16 _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 48;





static const ai_u16 _model_2_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 48;
static const ai_u16 _model_2_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 48;
static const ai_u16 _model_2_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_2_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_2_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 24;
static const ai_u16 _model_2_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_i8 _model_2_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_2_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 22;
static const ai_float _model_2_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.08880893141031265f;
static const ai_float _model_2_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.14203006029129028f;
static const ai_float _model_2_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0033802965190261602f, 0.0036545537877827883f, 0.003946192096918821f, 0.0039446125738322735f, 0.004552314057946205f, 0.005587073974311352f, 0.004402956925332546f, 0.002730719279497862f, 0.0038020943757146597f, 0.004035233054310083f, 0.0052182674407958984f, 0.003474285826086998f, 0.005173076409846544f, 0.00431462749838829f, 0.003753580851480365f, 0.0036314798053354025f);
static const ai_layer_format_type _model_2_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u16 _model_3_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 48;
static const ai_u16 _model_3_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 48;
static const ai_u16 _model_3_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_3_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_3_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_3_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_3_conv_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _model_3_conv_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i32 _model_3_conv_Conv_output_0_l_pad_W_0_const_s32 = 1;
static const ai_i32 _model_3_conv_Conv_output_0_l_pad_H_0_const_s32 = 1;
static const ai_i8 _model_3_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -123;
static const ai_i8 _model_3_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -6;
static const ai_float _model_3_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.05937632918357849f;
static const ai_float _model_3_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.10724584013223648f;
static const ai_float _model_3_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002005607122555375f, 0.00204551056958735f, 0.0029787567909806967f, 0.003437693929299712f, 0.0029729849193245173f, 0.002523173112422228f, 0.0028802487067878246f, 0.002026484813541174f, 0.0026471924502402544f, 0.002069932408630848f, 0.0016208725282922387f, 0.0015864864690229297f, 0.0019593455363065004f, 0.0013616285286843777f, 0.0014842184027656913f, 0.0031172435265034437f, 0.002376717748120427f, 0.0015255599282681942f, 0.0022728920448571444f, 0.001955346204340458f, 0.002484625903889537f, 0.00231296569108963f, 0.001803309889510274f, 0.004324648529291153f, 0.002997924108058214f, 0.0016857566079124808f, 0.0024268014822155237f, 0.0018321777461096644f, 0.001604180783033371f, 0.0016782906604930758f, 0.0014883986441418529f, 0.0015452771913260221f);
static const ai_layer_format_type _model_3_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_3_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_3_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;



static const ai_u16 _model_4_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 24;
static const ai_u16 _model_4_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 24;
static const ai_u16 _model_4_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_4_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_4_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_4_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_4_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -123;
static const ai_i8 _model_4_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -11;
static const ai_float _model_4_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.05681946128606796f;
static const ai_float _model_4_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.10862993448972702f;
static const ai_float _model_4_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.006551219150424004f, 0.005925013218075037f, 0.006024752277880907f, 0.004804936703294516f, 0.004762043245136738f, 0.005555120296776295f, 0.00536789745092392f, 0.004164091777056456f, 0.0049115139991045f, 0.005011631175875664f, 0.005429841578006744f, 0.005229798145592213f, 0.0037308158352971077f, 0.0042687226086854935f, 0.003694169456139207f, 0.006688497960567474f, 0.0026664319448173046f, 0.0032199015840888023f, 0.002039114246144891f, 0.003265060717239976f, 0.002040693536400795f, 0.0024415708612650633f, 0.00196173507720232f, 0.0023821229115128517f, 0.0030889371410012245f, 0.002392492024227977f, 0.0020266720093786716f, 0.0020834614988416433f, 0.001711475197225809f, 0.0024634927976876497f, 0.0027924850583076477f, 0.0027831813786178827f);
static const ai_layer_format_type _model_4_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_i8 _model_4_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-123);
static const ai_i16 _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i32 _model_4_m_0_cv1_conv_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _model_4_m_0_cv1_conv_Conv_output_0_l_pad_H_0_const_s32 = 0;
static const ai_i8 _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -123;
static const ai_i8 _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -3;
static const ai_float _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.05976736173033714f;
static const ai_float _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.10748530179262161f;
static const ai_float _model_4_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0028312939684838057f, 0.0032702554017305374f, 0.0030339686200022697f, 0.0042344266548752785f, 0.003788482630625367f, 0.0020973526407033205f, 0.0035744113847613335f, 0.0029900697991251945f, 0.003080617869272828f, 0.004449563100934029f, 0.004170744679868221f, 0.0022630849853157997f, 0.0058384849689900875f, 0.003963499329984188f, 0.0033538939896970987f, 0.003559697885066271f);
static const ai_layer_format_type _model_4_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;



static const ai_i8 _model_4_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-123);
static const ai_i16 _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i32 _model_4_m_0_cv2_conv_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _model_4_m_0_cv2_conv_Conv_output_0_l_pad_H_0_const_s32 = 0;
static const ai_i8 _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -123;
static const ai_i8 _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 16;
static const ai_float _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.05574588105082512f;
static const ai_float _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.10500090569257736f;
static const ai_float _model_4_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0023017488420009613f, 0.003084487747400999f, 0.003497605212032795f, 0.0022916027810424566f, 0.002328047761693597f, 0.003225061809644103f, 0.0038964617997407913f, 0.002556362422183156f, 0.002529562683776021f, 0.0024676029570400715f, 0.003197338664904237f, 0.0034440213348716497f, 0.0026535948272794485f, 0.003149216528981924f, 0.002158800372853875f, 0.002594918245449662f);
static const ai_layer_format_type _model_4_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;




static const ai_i8 _model_4_m_1_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-117);
static const ai_i16 _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i32 _model_4_m_1_cv1_conv_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _model_4_m_1_cv1_conv_Conv_output_0_l_pad_H_0_const_s32 = 0;
static const ai_i8 _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -117;
static const ai_i8 _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 39;
static const ai_float _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.04983390122652054f;
static const ai_float _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.09995905309915543f;
static const ai_float _model_4_m_1_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0017412519082427025f, 0.0017757208552211523f, 0.0025117085315287113f, 0.0017044213600456715f, 0.002287955256178975f, 0.0015700262738391757f, 0.00210738112218678f, 0.002165229059755802f, 0.0019094020826742053f, 0.0016794519033282995f, 0.0027843350544571877f, 0.00199342449195683f, 0.002006181515753269f, 0.0011437312932685018f, 0.00208051479421556f, 0.002650202950462699f);
static const ai_layer_format_type _model_4_m_1_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;



static const ai_i8 _model_4_m_1_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-120);
static const ai_i16 _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i32 _model_4_m_1_cv2_conv_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _model_4_m_1_cv2_conv_Conv_output_0_l_pad_H_0_const_s32 = 0;
static const ai_i8 _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -120;
static const ai_i8 _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -20;
static const ai_float _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03565911203622818f;
static const ai_float _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.10154405236244202f;
static const ai_float _model_4_m_1_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003641970921307802f, 0.0047011347487568855f, 0.004240780603140593f, 0.0033758755307644606f, 0.0034251788165420294f, 0.00430231261998415f, 0.006225136108696461f, 0.0049452949315309525f, 0.004637620411813259f, 0.0039573549292981625f, 0.005056906957179308f, 0.0037355110980570316f, 0.004182678647339344f, 0.005858876276761293f, 0.00446632644161582f, 0.0038706110790371895f);
static const ai_layer_format_type _model_4_m_1_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;





static const ai_u16 _model_4_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 24;
static const ai_u16 _model_4_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 24;
static const ai_u16 _model_4_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_4_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_4_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_4_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_4_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -116;
static const ai_i8 _model_4_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 0;
static const ai_float _model_4_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.06870301067829132f;
static const ai_float _model_4_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.0986211895942688f;
static const ai_float _model_4_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.00279068760573864f, 0.0027281094808131456f, 0.0024540023878216743f, 0.0030724648386240005f, 0.0020326010417193174f, 0.0013239483814686537f, 0.0021867859177291393f, 0.003307164879515767f, 0.003021687502041459f, 0.0016250546323135495f, 0.0030310831498354673f, 0.004487915895879269f, 0.002616369863972068f, 0.0018369312165305018f, 0.0023360135965049267f, 0.0025781637523323298f, 0.0021686749532818794f, 0.002735715825110674f, 0.002920707920566201f, 0.003623519791290164f, 0.0028685592114925385f, 0.002795108361169696f, 0.002780180424451828f, 0.0026212192606180906f, 0.001977568259462714f, 0.0034104925580322742f, 0.002842732472345233f, 0.0017796726897358894f, 0.001521368627436459f, 0.0020045714918524027f, 0.004103916697204113f, 0.0028698795940726995f);
static const ai_layer_format_type _model_4_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_i8 _model_5_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-122);
static const ai_i16 _model_5_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_5_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_5_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_5_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_5_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_5_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_5_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_5_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_5_conv_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _model_5_conv_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _model_5_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_5_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 0;
static const ai_float _model_5_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.05003619194030762f;
static const ai_float _model_5_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.07586664706468582f;
static const ai_float _model_5_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0013137537753209472f, 0.0012227037223055959f, 0.0015365041326731443f, 0.0013515729224309325f, 0.0017183581367135048f, 0.0013286154717206955f, 0.0012657549232244492f, 0.001687746262177825f, 0.001661695190705359f, 0.0009781374828889966f, 0.0011985207675024867f, 0.0011595010291785002f, 0.0016353807877749205f, 0.0012024205643683672f, 0.0008444233098998666f, 0.0008461892139166594f, 0.0011240170570090413f, 0.001272747409529984f, 0.0012012544320896268f, 0.0015484163304790854f, 0.00130213494412601f, 0.0013361801393330097f, 0.0007797855068929493f, 0.0013818653533235192f, 0.0008018990629352629f, 0.0013353191316127777f, 0.0011987839825451374f, 0.0012688474962487817f, 0.0008869300363585353f, 0.00110074772965163f, 0.0014553548535332084f, 0.001351199927739799f, 0.0014752787537872791f, 0.0009082553442567587f, 0.0016404069028794765f, 0.0012819289695471525f, 0.0009825291344895959f, 0.0013529104180634022f, 0.001121465116739273f, 0.0013643287820741534f, 0.0010368444491177797f, 0.0009844237938523293f, 0.0016755169490352273f, 0.001544320723041892f, 0.0011742741335183382f, 0.0009289101581089199f, 0.0012901220470666885f, 0.0007757892599329352f, 0.0015418370021507144f, 0.001022900571115315f, 0.001278865383937955f, 0.0009189237607643008f, 0.001560342381708324f, 0.001532885362394154f, 0.0005908989696763456f, 0.001531453337520361f, 0.0009979879250749946f, 0.0019909765105694532f, 0.001291944645345211f, 0.0010439809411764145f, 0.0011866256827488542f, 0.0010081261862069368f, 0.0010600273963063955f, 0.0009749454911798239f);
static const ai_layer_format_type _model_5_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_5_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_5_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;



static const ai_u16 _model_6_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 12;
static const ai_u16 _model_6_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 12;
static const ai_u16 _model_6_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_6_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_6_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_6_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_6_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -121;
static const ai_i8 _model_6_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -16;
static const ai_float _model_6_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03880762308835983f;
static const ai_float _model_6_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.07475388795137405f;
static const ai_float _model_6_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0036897386889904737f, 0.006509716156870127f, 0.003753740107640624f, 0.005062083713710308f, 0.0047873943112790585f, 0.003065147902816534f, 0.003345848061144352f, 0.0035113319754600525f, 0.0031576582696288824f, 0.0027170516550540924f, 0.0030534854158759117f, 0.002650867449119687f, 0.0036074351519346237f, 0.0038749792147427797f, 0.0027456243988126516f, 0.0036333599127829075f, 0.004069217015057802f, 0.003538464894518256f, 0.003405369818210602f, 0.004004352726042271f, 0.003284446196630597f, 0.003389643505215645f, 0.002925700042396784f, 0.003958764486014843f, 0.003977836109697819f, 0.004628496710211039f, 0.0038714248221367598f, 0.0035970257595181465f, 0.005072489380836487f, 0.004199231043457985f, 0.0029101958498358727f, 0.004752192180603743f, 0.002068504923954606f, 0.0020585148595273495f, 0.001971648307517171f, 0.0024311854504048824f, 0.0034076981246471405f, 0.0023327581584453583f, 0.0017209277721121907f, 0.0016207383014261723f, 0.002810622565448284f, 0.00162960821762681f, 0.0024696211330592632f, 0.0031592221930623055f, 0.002380347577854991f, 0.0017847948474809527f, 0.0023950047325342894f, 0.0033347075805068016f, 0.0036330765578895807f, 0.0022048393730074167f, 0.0015258572530001402f, 0.0025348379276692867f, 0.002099324483424425f, 0.0028956239111721516f, 0.0019502198556438088f, 0.00179976352956146f, 0.0027437633834779263f, 0.002104138256981969f, 0.0016478323377668858f, 0.002914097160100937f, 0.003218762343749404f, 0.0018193635623902082f, 0.001696523278951645f, 0.002717892872169614f);
static const ai_layer_format_type _model_6_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_i8 _model_6_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-122);
static const ai_i16 _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -6;
static const ai_float _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.04313071444630623f;
static const ai_float _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.07193232327699661f;
static const ai_float _model_6_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0023761135526001453f, 0.002247747266665101f, 0.0024128269869834185f, 0.002489147474989295f, 0.0019841964822262526f, 0.0021133427508175373f, 0.0014261089963838458f, 0.0022102519869804382f, 0.002001113025471568f, 0.0028500789776444435f, 0.0030322237871587276f, 0.001479400903917849f, 0.0025456275325268507f, 0.0020360215567052364f, 0.00254428549669683f, 0.002116657793521881f, 0.0013921590289101005f, 0.0023952198680490255f, 0.0023428986314684153f, 0.001915155677124858f, 0.0023506360594183207f, 0.0026863967068493366f, 0.002813860075548291f, 0.0028319985140115023f, 0.0029884823597967625f, 0.003491449635475874f, 0.002109585329890251f, 0.0029453688766807318f, 0.0025023408234119415f, 0.0019224639981985092f, 0.0031052730046212673f, 0.002561788773164153f);
static const ai_layer_format_type _model_6_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;



static const ai_i8 _model_6_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-121);
static const ai_i16 _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -121;
static const ai_i8 _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 7;
static const ai_float _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03852131962776184f;
static const ai_float _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.05817737057805061f;
static const ai_float _model_6_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0014873535837978125f, 0.001952707883901894f, 0.001820830861106515f, 0.001436069724150002f, 0.002184564247727394f, 0.0021730195730924606f, 0.0013323863968253136f, 0.0016423542983829975f, 0.0015644074883311987f, 0.0019225854193791747f, 0.001787329907529056f, 0.0023973791394382715f, 0.0020468810107558966f, 0.001230360008776188f, 0.0014994905795902014f, 0.00204296363517642f, 0.0010968574788421392f, 0.0019299626583233476f, 0.0016859984025359154f, 0.0015073302201926708f, 0.0018062388990074396f, 0.0016207877779379487f, 0.0013040899066254497f, 0.001749515999108553f, 0.001645944663323462f, 0.0016526230610907078f, 0.0018917362904176116f, 0.0016217809170484543f, 0.0017660725861787796f, 0.0016447692178189754f, 0.0019934126175940037f, 0.0020933912601321936f);
static const ai_layer_format_type _model_6_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;




static const ai_i8 _model_6_m_1_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-110);
static const ai_i16 _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -110;
static const ai_i8 _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 11;
static const ai_float _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.030677510425448418f;
static const ai_float _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.057439856231212616f;
static const ai_float _model_6_m_1_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0013719267444685102f, 0.001697832951322198f, 0.0013724432792514563f, 0.0015892357332631946f, 0.0014771909918636084f, 0.0017427456332370639f, 0.0011741527123376727f, 0.0025390710216015577f, 0.0011780032655224204f, 0.001655822852626443f, 0.0011912871850654483f, 0.0015805145958438516f, 0.0014007157878950238f, 0.0010590361198410392f, 0.0013480742927640676f, 0.0011110204504802823f, 0.0017159214476123452f, 0.0017196486005559564f, 0.0012627607211470604f, 0.0014431346207857132f, 0.001255980459973216f, 0.0014976718230172992f, 0.0013121026568114758f, 0.0011987198377028108f, 0.0013492893194779754f, 0.001771717332303524f, 0.0015378267271444201f, 0.0013952064327895641f, 0.0010430703405290842f, 0.001099982182495296f, 0.0014864831464365125f, 0.0013034334406256676f);
static const ai_layer_format_type _model_6_m_1_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;



static const ai_i8 _model_6_m_1_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-118);
static const ai_i16 _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -118;
static const ai_i8 _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 11;
static const ai_float _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.027188777923583984f;
static const ai_float _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.06489749997854233f;
static const ai_float _model_6_m_1_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0018905356992036104f, 0.002752636093646288f, 0.0024708483833819628f, 0.0024986539501696825f, 0.0030992513056844473f, 0.0020709314849227667f, 0.0024438167456537485f, 0.002918725134804845f, 0.001924091367982328f, 0.0031243832781910896f, 0.002684759208932519f, 0.0020965146832168102f, 0.0019058414036408067f, 0.0021352125331759453f, 0.0023031707387417555f, 0.004588397219777107f, 0.00200462038628757f, 0.002736339345574379f, 0.002266703872010112f, 0.0022050959523767233f, 0.0026359804905951023f, 0.0021771646570414305f, 0.002131673041731119f, 0.002414318732917309f, 0.002031042007729411f, 0.0020830153953284025f, 0.0023333486169576645f, 0.003326469799503684f, 0.002356362296268344f, 0.002224317518994212f, 0.00278625194914639f, 0.002587077906355262f);
static const ai_layer_format_type _model_6_m_1_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;





static const ai_u16 _model_6_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 12;
static const ai_u16 _model_6_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 12;
static const ai_u16 _model_6_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_6_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_6_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 _model_6_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_6_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -110;
static const ai_i8 _model_6_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 1;
static const ai_float _model_6_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.04531298577785492f;
static const ai_float _model_6_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.06418560445308685f;
static const ai_float _model_6_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0014932550257071853f, 0.002041310304775834f, 0.0014433501055464149f, 0.0021597258746623993f, 0.0017737032612785697f, 0.002356145763769746f, 0.002146258717402816f, 0.0021618600003421307f, 0.0015146111836656928f, 0.0021232215221971273f, 0.0019825350027531385f, 0.0017421310767531395f, 0.0015698678325861692f, 0.0023120888508856297f, 0.0018109529046341777f, 0.001987512456253171f, 0.0012215846218168736f, 0.0017481825780123472f, 0.00287427706643939f, 0.0023730150423943996f, 0.002227834891527891f, 0.0024707401171326637f, 0.002357587916776538f, 0.0021041736472398043f, 0.002257724292576313f, 0.0016438968013972044f, 0.0026789905969053507f, 0.001239356235601008f, 0.002243608236312866f, 0.0020913451444357634f, 0.0020604231394827366f, 0.0018334140768274665f, 0.0021587687078863382f, 0.0020625197794288397f, 0.0016623497940599918f, 0.0023976112715899944f, 0.0021882089786231518f, 0.001778828096576035f, 0.001574686262756586f, 0.0018774450290948153f, 0.0018794393399730325f, 0.0019751160871237516f, 0.002291689394041896f, 0.0013461305061355233f, 0.002350826049223542f, 0.0026303238701075315f, 0.0022057753521949053f, 0.0021715648472309113f, 0.0036936854012310505f, 0.002127616200596094f, 0.0036395585630089045f, 0.0017574846278876066f, 0.0016085996758192778f, 0.002423759549856186f, 0.0031509953550994396f, 0.0023630408104509115f, 0.0016784045146778226f, 0.0025309117045253515f, 0.0014018454821780324f, 0.0023485361598432064f, 0.0024089713115245104f, 0.003310641273856163f, 0.0016710689524188638f, 0.0019468616228550673f);
static const ai_layer_format_type _model_6_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_i8 _model_7_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-119);
static const ai_i16 _model_7_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_7_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_7_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_7_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_7_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_7_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_7_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_7_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_7_conv_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _model_7_conv_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _model_7_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -119;
static const ai_i8 _model_7_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -13;
static const ai_float _model_7_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.032705314457416534f;
static const ai_float _model_7_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.05027378350496292f;
static const ai_float _model_7_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0006076930440030992f, 0.0007301793084479868f, 0.00105028145480901f, 0.0006584920920431614f, 0.0008791270665824413f, 0.0007870578556321561f, 0.0006781638949178159f, 0.0007268617046065629f, 0.0005681435577571392f, 0.0008136192336678505f, 0.0007488912669941783f, 0.0007530070724897087f, 0.0009292763425037265f, 0.0008655467536300421f, 0.0007615843205712736f, 0.0006785056320950389f, 0.0007525389082729816f, 0.0007074717432260513f, 0.0004956689081154764f, 0.0007124640978872776f, 0.0006676107295788825f, 0.0006165269878692925f, 0.000661821395624429f, 0.0006825971067883074f, 0.0019489515107125044f, 0.0007894043228588998f, 0.0006389464833773673f, 0.0006688270950689912f, 0.0006718759541399777f, 0.0006235918262973428f, 0.0009832276264205575f, 0.0009009924833662808f, 0.000840475142467767f, 0.0008815787150524557f, 0.0008702710038051009f, 0.0007044543162919581f, 0.0007103133830241859f, 0.0006548333331011236f, 0.0005864906124770641f, 0.0006836394895799458f, 0.0006982753984630108f, 0.0006461461307480931f, 0.000708750041667372f, 0.001111537916585803f, 0.0006728422013111413f, 0.0012427743058651686f, 0.0007841201149858534f, 0.0008711014525033534f, 0.0007005221559666097f, 0.0008819766226224601f, 0.0011193641694262624f, 0.0009332535555586219f, 0.0006276504718698561f, 0.0008577305707149208f, 0.0007114043110050261f, 0.0007024770020507276f, 0.001038463320583105f, 0.000728288316167891f, 0.0006103445193730295f, 0.0006793320644646883f, 0.0008122522849589586f, 0.0008306038798764348f, 0.0009207337279804051f, 0.0008311300771310925f);
static const ai_layer_format_type _model_7_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_7_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_7_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;



static const ai_u16 _model_8_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_8_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_8_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_8_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_8_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_8_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_8_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -118;
static const ai_i8 _model_8_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -11;
static const ai_float _model_8_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.02866680547595024f;
static const ai_float _model_8_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.04314478114247322f;
static const ai_float _model_8_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0029721639584749937f, 0.00316555961035192f, 0.002640813821926713f, 0.0025219572708010674f, 0.0018950777594000101f, 0.002770209452137351f, 0.002765327924862504f, 0.0027501217555254698f, 0.0024530733935534954f, 0.0028651603497564793f, 0.002786131575703621f, 0.00452033057808876f, 0.0029959927778691053f, 0.0029537235386669636f, 0.0028201835229992867f, 0.0032517563086003065f, 0.0029524543788284063f, 0.002653052331879735f, 0.0028165196999907494f, 0.003556681564077735f, 0.0027113130781799555f, 0.002717498689889908f, 0.00355098070576787f, 0.0035030152648687363f, 0.0022295676171779633f, 0.002282590139657259f, 0.002980717457830906f, 0.0027196179144084454f, 0.0024104109033942223f, 0.0028135208413004875f, 0.0023541252594441175f, 0.00204969453625381f, 0.002070361515507102f, 0.0021692970767617226f, 0.0024102788884192705f, 0.002685778308659792f, 0.002104146871715784f, 0.003151837969198823f, 0.0021457879338413477f, 0.002597391838207841f, 0.002080122008919716f, 0.002702115336433053f, 0.0024184167850762606f, 0.0022765109315514565f, 0.0016332699451595545f, 0.0018370545003563166f, 0.0036643280182033777f, 0.0019106740364804864f, 0.0016241158591583371f, 0.0011847332352772355f, 0.002392875263467431f, 0.002714472822844982f, 0.0030288167763501406f, 0.002053303411230445f, 0.0015897834673523903f, 0.0021708840504288673f, 0.0022111611906439066f, 0.0023120883852243423f, 0.0027302114758640528f, 0.0022058815229684114f, 0.003050359897315502f, 0.0021798957604914904f, 0.002453868044540286f, 0.0028452349361032248f);
static const ai_layer_format_type _model_8_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_i8 _model_8_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-117);
static const ai_i16 _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 6;

static const ai_u16 _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 8;
static const ai_u16 _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 8;
static const ai_u16 _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -117;
static const ai_i8 _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -4;
static const ai_float _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.024431629106402397f;
static const ai_float _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.030549263581633568f;
static const ai_float _model_8_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0023127265740185976f, 0.0017184193711727858f, 0.0017540479311719537f, 0.0019179850351065397f, 0.0015773705672472715f, 0.0016098226187750697f, 0.001470134244300425f, 0.002152641536667943f, 0.001424834132194519f, 0.001387356431223452f, 0.001844399026595056f, 0.0019387571373954415f, 0.0020769676193594933f, 0.0011691719992086291f, 0.001393358688801527f, 0.0016825919738039374f, 0.0014598421985283494f, 0.002110386034473777f, 0.001535845804028213f, 0.0017093948554247618f, 0.0016300948336720467f, 0.002176042180508375f, 0.002039361512288451f, 0.001106374547816813f, 0.0018323808908462524f, 0.0015649874694645405f, 0.0009619970223866403f, 0.0019966948311775923f, 0.0015071672387421131f, 0.0014078418025746942f, 0.0012354350183159113f, 0.0016442763153463602f);
static const ai_layer_format_type _model_8_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;



static const ai_i8 _model_8_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-111);
static const ai_i16 _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 6;

static const ai_u16 _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 8;
static const ai_u16 _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 8;
static const ai_u16 _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -111;
static const ai_i8 _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -11;
static const ai_float _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.01654183864593506f;
static const ai_float _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.037787217646837234f;
static const ai_float _model_8_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0020333901047706604f, 0.0010497135808691382f, 0.0018696441547945142f, 0.0017111568013206124f, 0.0016681988490745425f, 0.0026096953079104424f, 0.001154470955953002f, 0.0016493399161845446f, 0.0018961286405101418f, 0.0015998478047549725f, 0.002331712981685996f, 0.001151209813542664f, 0.002485117642208934f, 0.0016037877649068832f, 0.0011812049197033048f, 0.0012269499711692333f, 0.0018492944072932005f, 0.0019203020492568612f, 0.0015718915965408087f, 0.0026553734205663204f, 0.002061442704871297f, 0.0022255696821957827f, 0.0015647989930585027f, 0.0018153056735172868f, 0.0015124647179618478f, 0.0018534634727984667f, 0.0018553902627900243f, 0.0013463601935654879f, 0.001497784280218184f, 0.0020729012321680784f, 0.0018904140451923013f, 0.0016719696577638388f);
static const ai_layer_format_type _model_8_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;





static const ai_u16 _model_8_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_8_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_8_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_8_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_8_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _model_8_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_8_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -109;
static const ai_i8 _model_8_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 5;
static const ai_float _model_8_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.029858563095331192f;
static const ai_float _model_8_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.04047897085547447f;
static const ai_float _model_8_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0018514052499085665f, 0.0018986623035743833f, 0.0023124930448830128f, 0.002210903214290738f, 0.0026915164198726416f, 0.0014461440732702613f, 0.0023784611839801073f, 0.001432162942364812f, 0.0016777621349319816f, 0.0023913367185741663f, 0.001905204844661057f, 0.0017189979553222656f, 0.0017479099333286285f, 0.00210253382101655f, 0.0016321006696671247f, 0.0015494304243475199f, 0.0021088356152176857f, 0.0016618272056803107f, 0.0017948249587789178f, 0.001837115385569632f, 0.0019695693626999855f, 0.002215894404798746f, 0.001772464020177722f, 0.0018180448096245527f, 0.0019020338077098131f, 0.001962072914466262f, 0.0012970917159691453f, 0.0019260405097156763f, 0.001761564752086997f, 0.0018303439719602466f, 0.0018705448601394892f, 0.0027376683428883553f, 0.002252495614811778f, 0.002520258305594325f, 0.002077279146760702f, 0.001573891961015761f, 0.0016174562042579055f, 0.0017907717265188694f, 0.0023565238807350397f, 0.0014853929169476032f, 0.0023392136208713055f, 0.0023544637951999903f, 0.0020895032212138176f, 0.0020553760696202517f, 0.0018907312769442797f, 0.0013637484516948462f, 0.0022443686611950397f, 0.0019359076395630836f, 0.002398970304057002f, 0.0019854933489114046f, 0.0013218438252806664f, 0.002119984943419695f, 0.0020423021633177996f, 0.0018885749159380794f, 0.0018205096712335944f, 0.0019960692152380943f, 0.0022329422645270824f, 0.0016029804246500134f, 0.0019184130942448974f, 0.0015323178377002478f, 0.002429899061098695f, 0.001219028141349554f, 0.0013986935373395681f, 0.0024068483617156744f);
static const ai_layer_format_type _model_8_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_u16 _model_9_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_9_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_9_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_9_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_9_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_9_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_9_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -114;
static const ai_i8 _model_9_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -16;
static const ai_float _model_9_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.02038503624498844f;
static const ai_float _model_9_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.040920440107584f;
static const ai_float _model_9_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0024780877865850925f, 0.0030070506036281586f, 0.001688069081865251f, 0.0019533138256520033f, 0.0020518542733043432f, 0.0022384864278137684f, 0.003013710491359234f, 0.0022301794961094856f, 0.0023184646852314472f, 0.0016606246354058385f, 0.001981226494535804f, 0.002724976744502783f, 0.00294695096090436f, 0.0019202097319066525f, 0.001958294538781047f, 0.002345402492210269f, 0.0026937329676002264f, 0.0018672309815883636f, 0.003204975975677371f, 0.002612550975754857f, 0.0026428471319377422f, 0.0030716138426214457f, 0.0026045353151857853f, 0.00203903135843575f, 0.0027766134589910507f, 0.0023772967979311943f, 0.002115947660058737f, 0.0016814616974443197f, 0.002142720390111208f, 0.002349965739995241f, 0.0017106736777350307f, 0.0026309434324502945f);
static const ai_layer_format_type _model_9_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_u16 _model_9_m_MaxPool_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_9_m_MaxPool_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_9_m_MaxPool_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_9_m_MaxPool_output_0_l_pool_size_1_const_u16 = 5;
static const ai_u16 _model_9_m_MaxPool_output_0_l_pool_size_0_const_u16 = 5;
static const ai_u16 _model_9_m_MaxPool_output_0_l_legacy_pool_pad_1_const_u16 = 2;
static const ai_u16 _model_9_m_MaxPool_output_0_l_legacy_pool_pad_0_const_u16 = 2;
static const ai_u16 _model_9_m_MaxPool_output_0_l_pool_stride_1_const_u16 = 1;
static const ai_u16 _model_9_m_MaxPool_output_0_l_pool_stride_0_const_u16 = 1;
static const ai_u16 _model_9_m_MaxPool_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_9_m_MaxPool_output_0_t_out_0_shape_h_const_u16 = 6;
static const ai_i8 _model_9_m_MaxPool_output_0_t_in_0_fmt_zero_const_s8 = -16;
static const ai_i8 _model_9_m_MaxPool_output_0_t_out_0_fmt_zero_const_s8 = -16;

static const ai_u16 _model_9_m_1_MaxPool_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_9_m_1_MaxPool_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_9_m_1_MaxPool_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_9_m_1_MaxPool_output_0_l_pool_size_1_const_u16 = 5;
static const ai_u16 _model_9_m_1_MaxPool_output_0_l_pool_size_0_const_u16 = 5;
static const ai_u16 _model_9_m_1_MaxPool_output_0_l_legacy_pool_pad_1_const_u16 = 2;
static const ai_u16 _model_9_m_1_MaxPool_output_0_l_legacy_pool_pad_0_const_u16 = 2;
static const ai_u16 _model_9_m_1_MaxPool_output_0_l_pool_stride_1_const_u16 = 1;
static const ai_u16 _model_9_m_1_MaxPool_output_0_l_pool_stride_0_const_u16 = 1;
static const ai_u16 _model_9_m_1_MaxPool_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_9_m_1_MaxPool_output_0_t_out_0_shape_h_const_u16 = 6;
static const ai_i8 _model_9_m_1_MaxPool_output_0_t_in_0_fmt_zero_const_s8 = -16;
static const ai_i8 _model_9_m_1_MaxPool_output_0_t_out_0_fmt_zero_const_s8 = -16;

static const ai_u16 _model_9_m_2_MaxPool_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_9_m_2_MaxPool_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_9_m_2_MaxPool_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_9_m_2_MaxPool_output_0_l_pool_size_1_const_u16 = 5;
static const ai_u16 _model_9_m_2_MaxPool_output_0_l_pool_size_0_const_u16 = 5;
static const ai_u16 _model_9_m_2_MaxPool_output_0_l_legacy_pool_pad_1_const_u16 = 2;
static const ai_u16 _model_9_m_2_MaxPool_output_0_l_legacy_pool_pad_0_const_u16 = 2;
static const ai_u16 _model_9_m_2_MaxPool_output_0_l_pool_stride_1_const_u16 = 1;
static const ai_u16 _model_9_m_2_MaxPool_output_0_l_pool_stride_0_const_u16 = 1;
static const ai_u16 _model_9_m_2_MaxPool_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_9_m_2_MaxPool_output_0_t_out_0_shape_h_const_u16 = 6;
static const ai_i8 _model_9_m_2_MaxPool_output_0_t_in_0_fmt_zero_const_s8 = -16;
static const ai_i8 _model_9_m_2_MaxPool_output_0_t_out_0_fmt_zero_const_s8 = -16;


static const ai_u16 _model_9_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_9_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_9_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_9_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_9_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 _model_9_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_9_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -16;
static const ai_i8 _model_9_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -6;
static const ai_float _model_9_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.040920440107584f;
static const ai_float _model_9_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03179952874779701f;
static const ai_float _model_9_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0015651392750442028f, 0.0007467122050002217f, 0.0018069365760311484f, 0.0013475202722474933f, 0.0009533936972729862f, 0.0012089329538866878f, 0.0009169320692308247f, 0.0015274924226105213f, 0.0014216259587556124f, 0.0016132041346281767f, 0.0011412326712161303f, 0.001294967019930482f, 0.0011179170105606318f, 0.0011977936374023557f, 0.0008473375928588212f, 0.0013594472547993064f, 0.0013543365057557821f, 0.000965428538620472f, 0.0024013349320739508f, 0.0011347016552463174f, 0.0016822017496451735f, 0.0011668514925986528f, 0.0014186807675287127f, 0.000805346411652863f, 0.0010639637475833297f, 0.0010489760898053646f, 0.0013396081048995256f, 0.0009194774902425706f, 0.0012381832348182797f, 0.0010674184886738658f, 0.0009034127579070628f, 0.0012832119828090072f, 0.0009838605765253305f, 0.0008356987382285297f, 0.0020056196954101324f, 0.0009018384735099971f, 0.0011169691570103168f, 0.0010303743183612823f, 0.0007918999763205647f, 0.0009487945935688913f, 0.0008800354553386569f, 0.0008764167432673275f, 0.0010723304003477097f, 0.001286248560063541f, 0.0011766251409426332f, 0.0012923565227538347f, 0.0009311916655860841f, 0.0017915095668286085f, 0.0012136525474488735f, 0.0012281365925446153f, 0.0011002247920259833f, 0.0008747039246372879f, 0.001221938175149262f, 0.0010760811856016517f, 0.0017870544688776135f, 0.0014497794909402728f, 0.0009939114097505808f, 0.001427201903425157f, 0.0008584196330048144f, 0.0012182762147858739f, 0.0012716819765046239f, 0.0007875083829276264f, 0.0011346754617989063f, 0.0017144293524324894f);
static const ai_layer_format_type _model_9_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _model_12_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 12;
static const ai_u16 _model_12_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 12;
static const ai_u16 _model_12_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_12_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_12_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 _model_12_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_12_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -119;
static const ai_i8 _model_12_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -10;
static const ai_float _model_12_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.032705314457416534f;
static const ai_float _model_12_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.05183536186814308f;
static const ai_float _model_12_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0017864485271275043f, 0.0029900597874075174f, 0.0035569414030760527f, 0.0028299724217504263f, 0.0024842943530529737f, 0.00293962424620986f, 0.00227062008343637f, 0.002793450141325593f, 0.0025869435630738735f, 0.0026611308567225933f, 0.002763281809166074f, 0.002716704038903117f, 0.0025630767922848463f, 0.0026543333660811186f, 0.003776236204430461f, 0.002067903056740761f, 0.00217771646566689f, 0.0023202586453408003f, 0.002657043980434537f, 0.002351981122046709f, 0.00293678417801857f, 0.0019574849866330624f, 0.0026996349915862083f, 0.0025536988396197557f, 0.0026661595329642296f, 0.003459701081737876f, 0.002253171056509018f, 0.0021945207845419645f, 0.0019628230948001146f, 0.001913408050313592f, 0.0016301028663292527f, 0.0018048847559839487f, 0.0017597622936591506f, 0.0019527466502040625f, 0.0028803986497223377f, 0.002493988024070859f, 0.0025953452568501234f, 0.0019359225407242775f, 0.0017516087973490357f, 0.002198418602347374f, 0.0033752562012523413f, 0.001961003988981247f, 0.002100311918184161f, 0.0027466444298624992f, 0.0027033076621592045f, 0.0021398868411779404f, 0.002854905091226101f, 0.002996833762153983f, 0.0028910187538713217f, 0.0023375805467367172f, 0.002652950119227171f, 0.0021443702280521393f, 0.001964686904102564f, 0.002671861555427313f, 0.0022475074511021376f, 0.00267933402210474f, 0.002628287998959422f, 0.002908626338467002f, 0.003121795365586877f, 0.0027726958505809307f, 0.003169383853673935f, 0.0029605920426547527f, 0.002658604411408305f, 0.0029184871818870306f);
static const ai_layer_format_type _model_12_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_i8 _model_12_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-118);
static const ai_i16 _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -118;
static const ai_i8 _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -6;
static const ai_float _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.02900150790810585f;
static const ai_float _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.0595129132270813f;
static const ai_float _model_12_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001660486334003508f, 0.00207913969643414f, 0.001771246548742056f, 0.0016164807602763176f, 0.0020895497873425484f, 0.0026646123733371496f, 0.00218298495747149f, 0.0025959706399589777f, 0.0033033795189112425f, 0.002560547785833478f, 0.002765942830592394f, 0.0019578339997678995f, 0.0026297764852643013f, 0.002074325690045953f, 0.0022485621739178896f, 0.0014523027930408716f, 0.0018736753845587373f, 0.0020538135431706905f, 0.0012986816000193357f, 0.0014807308325544f, 0.0017528317403048277f, 0.001959337620064616f, 0.0017026470741257071f, 0.0018925236072391272f, 0.0017711991677060723f, 0.0025881733745336533f, 0.00169361662119627f, 0.001672674436122179f, 0.0020334601867944f, 0.002138720592483878f, 0.001687868614681065f, 0.0023266649805009365f);
static const ai_layer_format_type _model_12_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;



static const ai_i8 _model_12_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-119);
static const ai_i16 _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -119;
static const ai_i8 _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 12;
static const ai_float _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03217185288667679f;
static const ai_float _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.055566348135471344f;
static const ai_float _model_12_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001422073575668037f, 0.0013706170720979571f, 0.001690531149506569f, 0.0018277581548318267f, 0.0013616853393614292f, 0.0018089388031512499f, 0.002249229233711958f, 0.0011119635310024023f, 0.0017248280346393585f, 0.001649231999181211f, 0.0015294344630092382f, 0.0016348829958587885f, 0.0015875316457822919f, 0.0018720568623393774f, 0.0016325674951076508f, 0.0017981427954509854f, 0.0020651505328714848f, 0.002067952649667859f, 0.0018846787279471755f, 0.0023741289041936398f, 0.002217584755271673f, 0.0019090514397248626f, 0.0012068799696862698f, 0.0014542851131409407f, 0.001447222544811666f, 0.0010072228033095598f, 0.0016837978037074208f, 0.0020383268129080534f, 0.0018885420868173242f, 0.0014898244990035892f, 0.0012036889092996716f, 0.0018520134035497904f);
static const ai_layer_format_type _model_12_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;




static const ai_u16 _model_12_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 12;
static const ai_u16 _model_12_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 12;
static const ai_u16 _model_12_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_12_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_12_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _model_12_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_12_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -118;
static const ai_i8 _model_12_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -10;
static const ai_float _model_12_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.02900150790810585f;
static const ai_float _model_12_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.061407145112752914f;
static const ai_float _model_12_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002166252350434661f, 0.002347744768485427f, 0.005777270998805761f, 0.0027521925512701273f, 0.0026767863892018795f, 0.0023637658450752497f, 0.002585663227364421f, 0.0030700708739459515f, 0.003912350162863731f, 0.002695083152502775f, 0.004355371464043856f, 0.0016813745023682714f, 0.0030971518717706203f, 0.002168173436075449f, 0.0033963299356400967f, 0.0024037736002355814f, 0.002758727176114917f, 0.002541317604482174f, 0.00305655668489635f, 0.00359157077036798f, 0.0024792803451418877f, 0.0034831154625862837f, 0.0032984826248139143f, 0.004407258238643408f, 0.0033056975807994604f, 0.0026662456803023815f, 0.003964679781347513f, 0.0019892991986125708f, 0.0024051794316619635f, 0.004224179312586784f, 0.003252945374697447f, 0.0026104256976395845f, 0.0035779299214482307f, 0.0022109011188149452f, 0.0031743415165692568f, 0.0033849100582301617f, 0.003746296279132366f, 0.0020660117734223604f, 0.0019679456017911434f, 0.002831706777215004f, 0.0035137287341058254f, 0.005280033219605684f, 0.0026293250266462564f, 0.0037177486810833216f, 0.00232282979413867f, 0.003493034280836582f, 0.00402536615729332f, 0.0029671185184270144f, 0.0022135432809591293f, 0.002732051070779562f, 0.0025179805234074593f, 0.003349945181980729f, 0.0036523311864584684f, 0.0021566699724644423f, 0.0021551514510065317f, 0.003887907136231661f, 0.0033860988914966583f, 0.0023436907213181257f, 0.002600383944809437f, 0.0026559210382401943f, 0.00400636438280344f, 0.0027853131759911776f, 0.0016352564562112093f, 0.0033331436570733786f);
static const ai_layer_format_type _model_12_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;





static const ai_u16 _model_15_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 24;
static const ai_u16 _model_15_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 24;
static const ai_u16 _model_15_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_15_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_15_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _model_15_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_15_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_15_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 13;
static const ai_float _model_15_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.05003619194030762f;
static const ai_float _model_15_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.06764547526836395f;
static const ai_float _model_15_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0020649393554776907f, 0.0033237310126423836f, 0.004859141539782286f, 0.004250861704349518f, 0.002284958492964506f, 0.0025762126315385103f, 0.0030527585186064243f, 0.002421754412353039f, 0.003300782758742571f, 0.0020972159691154957f, 0.0028445280622690916f, 0.0028690460603684187f, 0.00475328741595149f, 0.0016895791050046682f, 0.0037765749730169773f, 0.0020997184328734875f, 0.0016697089886292815f, 0.002012861194089055f, 0.0021913358941674232f, 0.0020553411450237036f, 0.0023392243310809135f, 0.0029110275208950043f, 0.0029756685253232718f, 0.0024686772376298904f, 0.002526313764974475f, 0.002020107116550207f, 0.0037284560967236757f, 0.0019086155807599425f, 0.0019411257235333323f, 0.0026120212860405445f, 0.0017767209792509675f, 0.002002368215471506f);
static const ai_layer_format_type _model_15_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_i8 _model_15_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-119);
static const ai_i16 _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i32 _model_15_m_0_cv1_conv_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _model_15_m_0_cv1_conv_Conv_output_0_l_pad_H_0_const_s32 = 0;
static const ai_i8 _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -119;
static const ai_i8 _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 0;
static const ai_float _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.0314263254404068f;
static const ai_float _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.09491623193025589f;
static const ai_float _model_15_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.004380779340863228f, 0.003800478531047702f, 0.00219477410428226f, 0.0020343235228210688f, 0.004017396830022335f, 0.0035024431999772787f, 0.0020951246842741966f, 0.002054487355053425f, 0.003137950785458088f, 0.0021605335641652346f, 0.002273844787850976f, 0.002364897634834051f, 0.005662046372890472f, 0.0025293135549873114f, 0.003520488739013672f, 0.0028577952180057764f);
static const ai_layer_format_type _model_15_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;



static const ai_i8 _model_15_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-122);
static const ai_i16 _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 16;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_i32 _model_15_m_0_cv2_conv_Conv_output_0_l_pad_W_0_const_s32 = 0;
static const ai_i32 _model_15_m_0_cv2_conv_Conv_output_0_l_pad_H_0_const_s32 = 0;
static const ai_i8 _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -22;
static const ai_float _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.0484776571393013f;
static const ai_float _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.08017320185899734f;
static const ai_float _model_15_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0019456112058833241f, 0.002620697021484375f, 0.0037695509381592274f, 0.0016432969132438302f, 0.002385783242061734f, 0.0018872437067329884f, 0.004359100945293903f, 0.0023931495379656553f, 0.0027569965459406376f, 0.0025694509968161583f, 0.0022922344505786896f, 0.0020511720795184374f, 0.0038745179772377014f, 0.003140742890536785f, 0.001826771884225309f, 0.002506101969629526f);
static const ai_layer_format_type _model_15_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;




static const ai_u16 _model_15_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 24;
static const ai_u16 _model_15_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 24;
static const ai_u16 _model_15_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_15_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_15_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 48;
static const ai_u16 _model_15_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_15_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_15_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -5;
static const ai_float _model_15_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.047851625829935074f;
static const ai_float _model_15_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.08608002960681915f;
static const ai_float _model_15_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0039631980471313f, 0.002760560717433691f, 0.003883877769112587f, 0.0026288179215043783f, 0.0022317487746477127f, 0.0059597003273665905f, 0.0032137306407094f, 0.003136275103315711f, 0.003233093535527587f, 0.0044657341204583645f, 0.0031736891251057386f, 0.003588138148188591f, 0.0037296165246516466f, 0.004016552120447159f, 0.003944321535527706f, 0.0036314844619482756f, 0.0041121747344732285f, 0.0034399949945509434f, 0.0032933379989117384f, 0.004484101198613644f, 0.004268892575055361f, 0.00388555065728724f, 0.00399797223508358f, 0.00339002488180995f, 0.003794213756918907f, 0.0032240054570138454f, 0.002728110644966364f, 0.0038831839337944984f, 0.003091102698817849f, 0.0030432280618697405f, 0.003888614010065794f, 0.004181627184152603f);
static const ai_layer_format_type _model_15_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_i8 _model_16_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-122);
static const ai_i16 _model_16_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_16_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_16_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_16_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_16_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_16_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_16_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_16_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_16_conv_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _model_16_conv_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _model_16_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_16_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -33;
static const ai_float _model_16_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.045634496957063675f;
static const ai_float _model_16_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.05743343010544777f;
static const ai_float _model_16_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0009669764549471438f, 0.0011577279074117541f, 0.0007409892859868705f, 0.0009454652899876237f, 0.0011968484614044428f, 0.0013404093915596604f, 0.0009656920447014272f, 0.0015639184275642037f, 0.0013098042691126466f, 0.0007321684388443828f, 0.0014061485417187214f, 0.0010064408415928483f, 0.0014347535325214267f, 0.001166917965747416f, 0.0010123145766556263f, 0.0015848270850256085f, 0.0011749902041628957f, 0.0017539041582494974f, 0.0013596522621810436f, 0.001213784795254469f, 0.0013577200006693602f, 0.001124901114962995f, 0.001044329721480608f, 0.001244463142938912f, 0.0009609581902623177f, 0.0011560794664546847f, 0.00123414711561054f, 0.0016026705270633101f, 0.0011316179297864437f, 0.0012617771280929446f, 0.0016419190214946866f, 0.0012402746360749006f);
static const ai_layer_format_type _model_16_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_16_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_16_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;




static const ai_u16 _model_18_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 12;
static const ai_u16 _model_18_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 12;
static const ai_u16 _model_18_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_18_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_18_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _model_18_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_18_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -121;
static const ai_i8 _model_18_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -5;
static const ai_float _model_18_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03713902831077576f;
static const ai_float _model_18_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.05417117476463318f;
static const ai_float _model_18_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002759339986369014f, 0.003468668321147561f, 0.00403913389891386f, 0.0031781289726495743f, 0.0028896124567836523f, 0.002489711157977581f, 0.002841654233634472f, 0.003639068454504013f, 0.0030566940549761057f, 0.005207240581512451f, 0.003044277196750045f, 0.002318976679816842f, 0.004176865331828594f, 0.0037672589533030987f, 0.0037336288951337337f, 0.002973425667732954f, 0.0034618761856108904f, 0.004044559318572283f, 0.00283827050589025f, 0.00348208867944777f, 0.002670822897925973f, 0.0024182219058275223f, 0.0039581903256475925f, 0.0045043365098536015f, 0.0031743720173835754f, 0.004190544597804546f, 0.003296840935945511f, 0.004575855564326048f, 0.002524352166801691f, 0.004159349482506514f, 0.0033782406244426966f, 0.00277931266464293f, 0.0035533795598894358f, 0.002769356593489647f, 0.0030498537234961987f, 0.00418872619047761f, 0.00326648005284369f, 0.001835470087826252f, 0.0023692806717008352f, 0.002415209775790572f, 0.0028316201642155647f, 0.003235350828617811f, 0.0024296350311487913f, 0.003354678861796856f, 0.002733054105192423f, 0.002125659491866827f, 0.0036663240753114223f, 0.0040539707988500595f, 0.0030914023518562317f, 0.0020301116164773703f, 0.004597081802785397f, 0.0028011880349367857f, 0.002633023075759411f, 0.0021649571135640144f, 0.0028215220663696527f, 0.003557326039299369f, 0.0026256160344928503f, 0.0025070083793252707f, 0.003077572677284479f, 0.002643106272444129f, 0.0042525967583060265f, 0.0038348971866071224f, 0.002999462652951479f, 0.0024317775387316942f);
static const ai_layer_format_type _model_18_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_i8 _model_18_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-118);
static const ai_i16 _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -118;
static const ai_i8 _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -19;
static const ai_float _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.02921578288078308f;
static const ai_float _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.04476507008075714f;
static const ai_float _model_18_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0015313590411096811f, 0.001392359146848321f, 0.001250302535481751f, 0.0012931372039020061f, 0.0011925454018637538f, 0.0014879712834954262f, 0.0018501576269045472f, 0.0017424651887267828f, 0.001500345068052411f, 0.0010259251575917006f, 0.0014578272821381688f, 0.0012291813036426902f, 0.0010532769374549389f, 0.001010914915241301f, 0.0014670250238850713f, 0.0012474863324314356f, 0.0011321821948513389f, 0.0011945543810725212f, 0.0009945607744157314f, 0.0016160181257873774f, 0.0014898499939590693f, 0.0019913401920348406f, 0.0012340114917606115f, 0.0019313580123707652f, 0.0011853970354422927f, 0.0010166012216359377f, 0.002115488052368164f, 0.0012133194832131267f, 0.0011599574936553836f, 0.0010128702269867063f, 0.0015036850236356258f, 0.0016174918273463845f);
static const ai_layer_format_type _model_18_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;



static const ai_i8 _model_18_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-118);
static const ai_i16 _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -118;
static const ai_i8 _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -22;
static const ai_float _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.02672426402568817f;
static const ai_float _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.04318301007151604f;
static const ai_float _model_18_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0013086660765111446f, 0.00109863409306854f, 0.001152932527475059f, 0.0010733673116192222f, 0.0010354113765060902f, 0.0014884527772665024f, 0.001523564918898046f, 0.0018981276080012321f, 0.001036857021972537f, 0.0012398306280374527f, 0.00118204765021801f, 0.00109673326369375f, 0.001447508460842073f, 0.0007459713378921151f, 0.0010049488628283143f, 0.0011042276164516807f, 0.0009213283774442971f, 0.0015647263498976827f, 0.0013705609599128366f, 0.0010762568563222885f, 0.0013814367121085525f, 0.0013741868315264583f, 0.0010486006503924727f, 0.0014503381680697203f, 0.0015602681087329984f, 0.0014682244509458542f, 0.0007635722868144512f, 0.0010717901168391109f, 0.0011590163921937346f, 0.0009099281742237508f, 0.0013140887022018433f, 0.0009749069577082992f);
static const ai_layer_format_type _model_18_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;




static const ai_u16 _model_18_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 12;
static const ai_u16 _model_18_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 12;
static const ai_u16 _model_18_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_18_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_18_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _model_18_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_18_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -118;
static const ai_i8 _model_18_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -14;
static const ai_float _model_18_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.02921578288078308f;
static const ai_float _model_18_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.053707871586084366f;
static const ai_float _model_18_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.003803613828495145f, 0.0028500165790319443f, 0.00333462399430573f, 0.003572158981114626f, 0.003286694409325719f, 0.002776143839582801f, 0.003496336517855525f, 0.004351318348199129f, 0.0033948703203350306f, 0.004547671880573034f, 0.0026815179735422134f, 0.005167232360690832f, 0.0027573949191719294f, 0.003422407666221261f, 0.0038988578598946333f, 0.0031780642457306385f, 0.0024150388780981302f, 0.004228540230542421f, 0.004201093222945929f, 0.003228540066629648f, 0.0039177280850708485f, 0.003966616000980139f, 0.0027498933486640453f, 0.0036914246156811714f, 0.0023462262470275164f, 0.0024300513323396444f, 0.0030016738455742598f, 0.0034036983270198107f, 0.003142783883959055f, 0.002664315514266491f, 0.00546101201325655f, 0.003126654075458646f, 0.00340123544447124f, 0.0039089457131922245f, 0.0056699113920331f, 0.004031975753605366f, 0.003281722543761134f, 0.004480994306504726f, 0.0017648902721703053f, 0.0030471058562397957f, 0.0027355097699910402f, 0.003981829155236483f, 0.0033712389413267374f, 0.002978807780891657f, 0.0033051881473511457f, 0.003442785469815135f, 0.004399847239255905f, 0.004166214726865292f, 0.004765285644680262f, 0.0025688628666102886f, 0.004145221784710884f, 0.002894342876970768f, 0.0031693778000772f, 0.003568327985703945f, 0.0024653684813529253f, 0.0035497257485985756f, 0.0027462507132440805f, 0.0036420130636543036f, 0.004967977758497f, 0.0033616607543081045f, 0.003936707973480225f, 0.0019027718808501959f, 0.0026156932581216097f, 0.0025860306341201067f);
static const ai_layer_format_type _model_18_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_i8 _model_19_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-119);
static const ai_i16 _model_19_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_19_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_19_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_19_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_19_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_19_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_19_conv_Conv_output_0_t_weight_0_shape_w_const_u16 = 3;
static const ai_u16 _model_19_conv_Conv_output_0_t_weight_0_shape_h_const_u16 = 3;
static const ai_u16 _model_19_conv_Conv_output_0_l_stride_1_const_u16 = 2;
static const ai_u16 _model_19_conv_Conv_output_0_l_stride_0_const_u16 = 2;
static const ai_i8 _model_19_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -119;
static const ai_i8 _model_19_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 19;
static const ai_float _model_19_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03080553002655506f;
static const ai_float _model_19_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03670447692275047f;
static const ai_float _model_19_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0008470809552818537f, 0.000994974048808217f, 0.000511922175064683f, 0.0008847899152897298f, 0.0006565614021383226f, 0.0015203106449916959f, 0.0006121865590102971f, 0.0006678980425931513f, 0.0006796868983656168f, 0.0005508303875103593f, 0.0005521323182620108f, 0.0009797889506444335f, 0.001028891303576529f, 0.0007020959164947271f, 0.0006801877170801163f, 0.0007031566346995533f, 0.0007998048677109182f, 0.001121576759032905f, 0.000735015026293695f, 0.0009064266341738403f, 0.0006518481532111764f, 0.0008436954231001437f, 0.001652453443966806f, 0.0008378693019039929f, 0.0005817036144435406f, 0.0012367692543193698f, 0.0009750503813847899f, 0.0015902122249826789f, 0.000671758723910898f, 0.0006483377073891461f, 0.0007562101818621159f, 0.001644634990952909f, 0.0008157022530212998f, 0.0013472853461280465f, 0.001925294054672122f, 0.001430272008292377f, 0.0009552933042868972f, 0.0009582185884937644f, 0.000681921374052763f, 0.0006552065024152398f, 0.0005478257080540061f, 0.0005796494078822434f, 0.0005032469052821398f, 0.000600136409047991f, 0.0007424944778904319f, 0.0009584280196577311f, 0.0008366939728148282f, 0.0006211824947968125f, 0.0008372302399948239f, 0.00043238262878730893f, 0.0005618027062155306f, 0.0006435637478716671f, 0.0005667961086146533f, 0.000500957656186074f, 0.0007463821093551815f, 0.0008182398742064834f, 0.0008707771194167435f, 0.0007484935340471566f, 0.0012877249391749501f, 0.0014284804929047823f, 0.0004726604383904487f, 0.0009492154349572957f, 0.0016735512763261795f, 0.0007597816293127835f);
static const ai_layer_format_type _model_19_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_19_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_19_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;




static const ai_u16 _model_21_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_21_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_21_cv1_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_21_cv1_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_21_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 128;
static const ai_u16 _model_21_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_21_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -112;
static const ai_i8 _model_21_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 21;
static const ai_float _model_21_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.017500974237918854f;
static const ai_float _model_21_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03340889886021614f;
static const ai_float _model_21_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0019908330868929625f, 0.003050887258723378f, 0.002648161491379142f, 0.004391458351165056f, 0.004018045961856842f, 0.0021357976365834475f, 0.0009728840086609125f, 0.0013362508034333587f, 0.0017021532403305173f, 0.002054942073300481f, 0.0029056533239781857f, 0.0021735415793955326f, 0.0025120817590504885f, 0.0023281287867575884f, 0.0023676592390984297f, 0.0017844843678176403f, 0.0030341926030814648f, 0.001973121426999569f, 0.006612272467464209f, 0.0016248298343271017f, 0.0025629322044551373f, 0.0012747752480208874f, 0.0031092462595552206f, 0.0014836974442005157f, 0.0020032296888530254f, 0.001806052285246551f, 0.002202636795118451f, 0.00107602309435606f, 0.0037420070730149746f, 0.0026490362361073494f, 0.00235452177003026f, 0.0018847560277208686f, 0.003039209172129631f, 0.003758420003578067f, 0.004212620202451944f, 0.0020453711040318012f, 0.0018558806041255593f, 0.0011309281690046191f, 0.0024181792978197336f, 0.0030884782318025827f, 0.0017195474356412888f, 0.0019521848298609257f, 0.001809688750654459f, 0.00222240062430501f, 0.0012064831098541617f, 0.002141342032700777f, 0.0017716194270178676f, 0.0027184062637388706f, 0.0016841398319229484f, 0.0018247115658596158f, 0.0019576274789869785f, 0.0015278870705515146f, 0.0014083142159506679f, 0.00288726226426661f, 0.00117688428144902f, 0.002209577476605773f, 0.0011569823836907744f, 0.0016547988634556532f, 0.001393548445776105f, 0.0017048836452886462f, 0.001794707146473229f, 0.0016096584731712937f, 0.0024341947864741087f, 0.0021339203231036663f);
static const ai_layer_format_type _model_21_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_i8 _model_21_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-109);
static const ai_i16 _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 6;

static const ai_u16 _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 8;
static const ai_u16 _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 8;
static const ai_u16 _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -109;
static const ai_i8 _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -1;
static const ai_float _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.014591030776500702f;
static const ai_float _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.02495408244431019f;
static const ai_float _model_21_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0034448341466486454f, 0.0014576903777197003f, 0.0006831060163676739f, 0.000855189748108387f, 0.0008406918495893478f, 0.0011073013301938772f, 0.0004871147102676332f, 0.0007549448637291789f, 0.001911395345814526f, 0.0014252950204536319f, 0.0009666091063991189f, 0.0004327849892433733f, 0.0008216117275878787f, 0.0005912478663958609f, 0.0011193563695997f, 0.001316100126132369f, 0.0007494849269278347f, 0.001056540641002357f, 0.0008482849807478487f, 0.0007274643867276609f, 0.0010689885821193457f, 0.0005942292627878487f, 0.000787211349233985f, 0.0014484432758763433f, 0.0009689699509181082f, 0.0006339190877042711f, 0.0007943834643810987f, 0.0011175415711477399f, 0.001154660596512258f, 0.0014222684549167752f, 0.000572957273107022f, 0.0010355319827795029f);
static const ai_layer_format_type _model_21_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;



static const ai_i8 _model_21_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-107);
static const ai_i16 _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 6;

static const ai_u16 _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 8;
static const ai_u16 _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 8;
static const ai_u16 _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -107;
static const ai_i8 _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -11;
static const ai_float _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.013126016594469547f;
static const ai_float _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.020877396687865257f;
static const ai_float _model_21_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0009713588515296578f, 0.0010374747216701508f, 0.0009094567503780127f, 0.001050164457410574f, 0.0012407312169671059f, 0.0010767739731818438f, 0.0011805157409980893f, 0.0009474166436120868f, 0.0010269884951412678f, 0.0005695678992196918f, 0.0013309590285643935f, 0.0005132393562234938f, 0.0008296962478198111f, 0.0007407546509057283f, 0.0010058266343548894f, 0.0008148886845447123f, 0.0010871521662920713f, 0.0018680241191759706f, 0.0021752321626991034f, 0.00139160780236125f, 0.00081749411765486f, 0.0009144741925410926f, 0.0006353456410579383f, 0.0015167162055149674f, 0.0007719580316916108f, 0.0009303626138716936f, 0.0010755264665931463f, 0.002784266835078597f, 0.003124936716631055f, 0.0012366711162030697f, 0.0008971622446551919f, 0.0024283479433506727f);
static const ai_layer_format_type _model_21_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;




static const ai_u16 _model_21_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_21_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_21_cv2_conv_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_21_cv2_conv_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_21_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 96;
static const ai_u16 _model_21_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_21_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -109;
static const ai_i8 _model_21_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -29;
static const ai_float _model_21_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.014591030776500702f;
static const ai_float _model_21_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.025828802958130836f;
static const ai_float _model_21_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.008159374818205833f, 0.0039763967506587505f, 0.0059679290279746056f, 0.003984067589044571f, 0.004659148398786783f, 0.006840006448328495f, 0.001828854321502149f, 0.004361474886536598f, 0.0023336466401815414f, 0.008413564413785934f, 0.006643128581345081f, 0.0031810905784368515f, 0.0038037628401070833f, 0.0055174026638269424f, 0.0042291199788451195f, 0.0027138404548168182f, 0.012414026074111462f, 0.003923616837710142f, 0.00299264257773757f, 0.0024100106675177813f, 0.006221575662493706f, 0.008393415249884129f, 0.004629508126527071f, 0.004170312080532312f, 0.001434340258128941f, 0.009103698655962944f, 0.004209446720778942f, 0.0033843405544757843f, 0.003289966844022274f, 0.0036906765308231115f, 0.004766525700688362f, 0.007914649322628975f, 0.0017342782812193036f, 0.006017172243446112f, 0.0035110602620989084f, 0.004043427295982838f, 0.006740671582520008f, 0.008594072423875332f, 0.005492832977324724f, 0.004466974176466465f, 0.005186932627111673f, 0.0056914896704256535f, 0.004632188007235527f, 0.0032850676216185093f, 0.0046784584410488605f, 0.004697445780038834f, 0.00331776961684227f, 0.009182821959257126f, 0.004663816187530756f, 0.007158143911510706f, 0.005350437015295029f, 0.0021521160379052162f, 0.0032458212226629257f, 0.006983092054724693f, 0.003522527404129505f, 0.002852155128493905f, 0.006405065767467022f, 0.004047736059874296f, 0.0027816682122647762f, 0.0032620204146951437f, 0.0023721898905932903f, 0.0029089869931340218f, 0.0022403853945434093f, 0.0017296446021646261f);
static const ai_layer_format_type _model_21_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;



static const ai_i8 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-111);
static const ai_i16 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 6;

static const ai_u16 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 8;
static const ai_u16 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 8;
static const ai_u16 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -111;
static const ai_i8 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -5;
static const ai_float _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.01666065864264965f;
static const ai_float _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.02846970595419407f;
static const ai_float _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0010658734245225787f, 0.0011035638162866235f, 0.0010946036782115698f, 0.0012314236955717206f, 0.0009591499692760408f, 0.0012322557158768177f, 0.0011065880535170436f, 0.0016560702351853251f, 0.0010065599344670773f, 0.0012172539718449116f, 0.0010931241558864713f, 0.0006215011235326529f, 0.0011948405299335718f, 0.0014578326372429729f, 0.001268020598217845f, 0.0011545257875695825f, 0.0010097784688696265f, 0.0008975382661446929f, 0.0007388450321741402f, 0.0012185616651549935f, 0.0011265729553997517f, 0.0008628188516013324f, 0.0008997176773846149f, 0.001021257950924337f, 0.0011705002980306745f, 0.0011134230298921466f, 0.0009216693579219282f, 0.0010626696748659015f, 0.0009162794449366629f, 0.0011054198257625103f, 0.001301744137890637f, 0.0009316290961578488f, 0.0014198333956301212f, 0.0009401675197295845f, 0.0008454626076854765f, 0.0012924488401040435f, 0.0009212690638378263f, 0.0011158486595377326f, 0.0007779307197779417f, 0.0013822800246998668f, 0.0011647220235317945f, 0.0014884592965245247f, 0.0012475029798224568f, 0.0011013008188456297f, 0.0010587780270725489f, 0.0009514750563539565f, 0.0008524474105797708f, 0.0009979548631235957f, 0.0008607606869190931f, 0.0008219079463742673f, 0.0006784690776839852f, 0.0012185616651549935f, 0.000832555815577507f, 0.0010748874628916383f, 0.0012242193333804607f, 0.0006856814143247902f, 0.0010582112008705735f, 0.0009394644876010716f, 0.0009209115523844957f, 0.0011182124726474285f, 0.0008796695619821548f, 0.0014060887042433023f, 0.001434069941751659f, 0.000980691402219236f);
static const ai_layer_format_type _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;



static const ai_i8 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-110);
static const ai_i16 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 6;

static const ai_u16 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 8;
static const ai_u16 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 8;
static const ai_u16 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -110;
static const ai_i8 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -14;
static const ai_float _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.015541751869022846f;
static const ai_float _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.026427119970321655f;
static const ai_float _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.001000117277726531f, 0.001341724069789052f, 0.0011276804143562913f, 0.0013787716161459684f, 0.0007925411919131875f, 0.0013054703595116735f, 0.0008984724408946931f, 0.0009192597353830934f, 0.0011425067204982042f, 0.001432256307452917f, 0.0012858869740739465f, 0.0010491712018847466f, 0.0014973202487453818f, 0.0009574302821420133f, 0.0009660267387516797f, 0.001201963284984231f, 0.0011242878390476108f, 0.0012966408394277096f, 0.0009406704339198768f, 0.0011826654663309455f, 0.0010903023649007082f, 0.001193181611597538f, 0.0009580569458194077f, 0.001141368760727346f, 0.0011790954740718007f, 0.0010476565221324563f, 0.0010967120761051774f, 0.0009595042211003602f, 0.0009446041658520699f, 0.0012224548263475299f, 0.0009399331756867468f, 0.000779688183683902f, 0.0008999738493002951f, 0.0011669740779325366f, 0.001159421051852405f, 0.0007808114751242101f, 0.0009830187773332f, 0.0012541133910417557f, 0.001137199462391436f, 0.0011779122287407517f, 0.0012378894025459886f, 0.00118266639765352f, 0.0008416186319664121f, 0.0011862703831866384f, 0.0009051642846316099f, 0.001266183564439416f, 0.0010278266854584217f, 0.0014009568840265274f, 0.0009796133963391185f, 0.0015733015025034547f, 0.0011649680091068149f, 0.001177518512122333f, 0.0009542209445498884f, 0.0013146912679076195f, 0.0012680395739153028f, 0.000875223835464567f, 0.0013193096965551376f, 0.001056062988936901f, 0.0011911374749615788f, 0.0012238348135724664f, 0.0012709215516224504f, 0.001231851987540722f, 0.001085031544789672f, 0.0010112744057551026f);
static const ai_layer_format_type _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;



static const ai_u16 _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_shape_w_const_u16 = 6;
static const ai_u16 _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_shape_h_const_u16 = 6;
static const ai_u16 _model_22_cv2_2_cv2_2_2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_22_cv2_2_cv2_2_2_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_fmt_zero_const_s8 = -110;
static const ai_i8 _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_fmt_zero_const_s8 = -128;
static const ai_float _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.015319498255848885f;
static const ai_float _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.011250725947320461f;
static const ai_float _model_22_cv2_2_cv2_2_2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0005243256455287337f, 0.0005257673910818994f, 0.0005281703779473901f, 0.000516636180691421f, 0.0005315344897098839f, 0.0005267285741865635f, 0.0005200002924539149f, 0.000532495672814548f, 0.0005224032793194056f, 0.0005243256455287337f, 0.0005339374765753746f, 0.000520961475558579f, 0.0005243256455287337f, 0.0005310539272613823f, 0.0005228838417679071f, 0.0005151943769305944f, 0.0005267285741865635f, 0.0005281703779473901f, 0.0005262480117380619f, 0.0005175973637960851f, 0.0005137526313774288f, 0.0005224032793194056f, 0.0005267285741865635f, 0.0005286509403958917f, 0.0005315344897098839f, 0.0005296121235005558f, 0.0005281703779473901f, 0.0005204809131100774f, 0.0005243256455287337f, 0.0005310539272613823f, 0.000532495672814548f, 0.0005243256455287337f, 0.0005329762934707105f, 0.0005276897572912276f, 0.0005262480117380619f, 0.000532495672814548f, 0.0005339374765753746f, 0.0005248062079772353f, 0.0005286509403958917f, 0.0005320151103660464f, 0.0005310539272613823f, 0.0005320151103660464f, 0.0005108690820634365f, 0.0005310539272613823f, 0.0005296121235005558f, 0.0005185585469007492f, 0.0005099078989587724f, 0.0005305733066052198f, 0.0005334568559192121f, 0.0005060631665401161f, 0.0005320151103660464f, 0.0005329762934707105f, 0.0005305733066052198f, 0.000520961475558579f, 0.0005281703779473901f, 0.0005300927441567183f, 0.0005262480117380619f, 0.0005147138144820929f, 0.0005329762934707105f, 0.0005300927441567183f, 0.0005228838417679071f, 0.0005329762934707105f, 0.000499815447255969f, 0.0005334568559192121f);
static const ai_layer_format_type _model_22_cv2_2_cv2_2_2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_i8 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-111);
static const ai_i16 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 6;

static const ai_u16 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 8;
static const ai_u16 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 8;
static const ai_u16 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -111;
static const ai_i8 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -39;
static const ai_float _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.01666065864264965f;
static const ai_float _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.02233886532485485f;
static const ai_float _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0020626906771212816f, 0.0007914919406175613f, 0.0005280909826979041f, 0.0009882794693112373f, 0.0005499343969859183f, 0.0040968190878629684f, 0.0005738674663007259f, 0.0006779264658689499f, 0.0007507296977564692f, 0.000572201912291348f, 0.0012465305626392365f, 0.0010460314806550741f, 0.0009307455038651824f, 0.00045015281648375094f, 0.0007226681336760521f, 0.00036546651972457767f, 0.00031303599826060236f, 0.0006930723902769387f, 0.0008985391468741f, 0.0013266828609630466f, 0.0010047274408861995f, 0.0005476633668877184f, 0.00039408437442034483f, 0.00046535616274923086f, 0.0005518039688467979f, 0.0009632788132876158f, 0.0008196670678444207f, 0.00048539662384428084f, 0.0015811491757631302f, 0.0005382464732974768f, 0.000659991754218936f, 0.0010156095959246159f);
static const ai_layer_format_type _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;



static const ai_i8 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-110);
static const ai_i16 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 6;

static const ai_u16 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 8;
static const ai_u16 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 8;
static const ai_u16 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -110;
static const ai_i8 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 7;
static const ai_float _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.015259467996656895f;
static const ai_float _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.020661186426877975f;
static const ai_float _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002950686030089855f, 0.0011504326248541474f, 0.0060006859712302685f, 0.0011221440508961678f, 0.0012002907460555434f, 0.0019341976149007678f, 0.0010907583637163043f, 0.0013939609052613378f, 0.0023794276639819145f, 0.0012163794599473476f, 0.0007691954378969967f, 0.0013277544640004635f, 0.0013390193926170468f, 0.000685439445078373f, 0.0016638058004900813f, 0.0019855170976370573f, 0.0030412599444389343f, 0.007940917275846004f, 0.0028701412957161665f, 0.0013363473117351532f, 0.0009642814402468503f, 0.015712833032011986f, 0.0022225251886993647f, 0.0008334987214766443f, 0.001105836476199329f, 0.0014570615021511912f, 0.0018451509531587362f, 0.0022518339101225138f, 0.001187699381262064f, 0.004690586589276791f, 0.0014106468297541142f, 0.0028047331143170595f);
static const ai_layer_format_type _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 6;
static const ai_u16 _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 6;




static const ai_i8 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-119);
static const ai_i16 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -119;
static const ai_i8 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -4;
static const ai_float _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03080553002655506f;
static const ai_float _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.04314032196998596f;
static const ai_float _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0011733346618711948f, 0.0011358889751136303f, 0.0011660088784992695f, 0.0010504625970497727f, 0.0010105868568643928f, 0.0010200194083154202f, 0.0013728865887969732f, 0.0007039430201984942f, 0.0012032432714477181f, 0.001580573501996696f, 0.0012097839498892426f, 0.000925807747989893f, 0.0012545897625386715f, 0.0009632949368096888f, 0.001302863471210003f, 0.0009234092431142926f, 0.0008804030949249864f, 0.0016113871242851019f, 0.001262835692614317f, 0.0010625910945236683f, 0.0014512038324028254f, 0.0010737895499914885f, 0.0011230524396523833f, 0.0013771087396889925f, 0.0010204511927440763f, 0.0008475075010210276f, 0.00112436106428504f, 0.0014004663098603487f, 0.0010959948413074017f, 0.001216444419696927f, 0.0009657786577008665f, 0.0015249120770022273f, 0.001955102663487196f, 0.0012312713079154491f, 0.001238019671291113f, 0.0013669509207829833f, 0.0011982497526332736f, 0.0017274939455091953f, 0.0015933362301439047f, 0.001218029879964888f, 0.0012967834481969476f, 0.0011253843549638987f, 0.001159854931756854f, 0.0010233096545562148f, 0.0017974656075239182f, 0.0009725647396408021f, 0.0010333845857530832f, 0.0008229969535022974f, 0.0010755948023870587f, 0.0017127561150118709f, 0.0013656161027029157f, 0.0009187443647533655f, 0.0018325357232242823f, 0.0011012237519025803f, 0.0013295700773596764f, 0.0009158003958873451f, 0.000977254007011652f, 0.0015656573232263327f, 0.0010286602191627026f, 0.001604656339623034f, 0.0007536805933341384f, 0.0014935859944671392f, 0.0010052802972495556f, 0.001121804933063686f);
static const ai_layer_format_type _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;



static const ai_i8 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-116);
static const ai_i16 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -116;
static const ai_i8 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -23;
static const ai_float _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.023212028667330742f;
static const ai_float _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.04184595122933388f;
static const ai_float _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0011921877739951015f, 0.0022369245998561382f, 0.0007911167340353131f, 0.0009376259986311197f, 0.0010277003748342395f, 0.0017742372583597898f, 0.0006615424063056707f, 0.0009025873732753098f, 0.001027724239975214f, 0.001299960189498961f, 0.0010700785787776113f, 0.0008667029323987663f, 0.0010570264421403408f, 0.0008072816417552531f, 0.0017762144561856985f, 0.0010974528267979622f, 0.0009588821558281779f, 0.000976668088696897f, 0.0013166177086532116f, 0.0006936461431905627f, 0.0009208396077156067f, 0.001090546604245901f, 0.001136177103035152f, 0.0012529788073152304f, 0.0009112581610679626f, 0.0010002930648624897f, 0.0011749102268368006f, 0.0010955689940601587f, 0.0009390137856826186f, 0.0006636764155700803f, 0.0012384157162159681f, 0.0009652735898271203f, 0.0011548101902008057f, 0.0018477389821782708f, 0.0014262730255723f, 0.0013575770426541567f, 0.0011085404548794031f, 0.000891354400664568f, 0.0012224985985085368f, 0.001509842462837696f, 0.0009128760430030525f, 0.0010569695150479674f, 0.0009205096284858882f, 0.0004479314375203103f, 0.0013390003005042672f, 0.0012134552234783769f, 0.0016238163225352764f, 0.0010094000026583672f, 0.0014477679505944252f, 0.001369247678667307f, 0.0008862484828568995f, 0.0011856398778036237f, 0.0012953438563272357f, 0.0006724525592289865f, 0.0008461016113869846f, 0.0010208954336121678f, 0.0008521172567270696f, 0.0018914121901616454f, 0.0010761755984276533f, 0.0010312655940651894f, 0.0009196079336106777f, 0.0013325284235179424f, 0.0011455052299425006f, 0.0013494868762791157f);
static const ai_layer_format_type _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;



static const ai_u16 _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_shape_w_const_u16 = 12;
static const ai_u16 _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_shape_h_const_u16 = 12;
static const ai_u16 _model_22_cv2_1_cv2_1_2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_22_cv2_1_cv2_1_2_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_fmt_zero_const_s8 = -117;
static const ai_i8 _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_fmt_zero_const_s8 = -110;
static const ai_float _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.025640754029154778f;
static const ai_float _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.0409509614109993f;
static const ai_float _model_22_cv2_1_cv2_1_2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0014898345107212663f, 0.002327986527234316f, 0.0024817760568112135f, 0.003143070265650749f, 0.002418337855488062f, 0.0021011473145335913f, 0.0013485405361279845f, 0.0008434385526925325f, 0.0008112389477901161f, 0.0006377453100867569f, 0.0008905365830287337f, 0.0007487619877792895f, 0.0007511649746447802f, 0.0006925327470526099f, 0.0008093165815807879f, 0.0009510911186225712f, 0.000903993146494031f, 0.002574049634858966f, 0.0033083937596529722f, 0.002543291775509715f, 0.00288547296077013f, 0.001798374461941421f, 0.0013696865644305944f, 0.0012360820546746254f, 0.0009015901596285403f, 0.0006858044653199613f, 0.0008011464960873127f, 0.000659852521494031f, 0.0008117195102386177f, 0.0008290208061225712f, 0.0008852500468492508f, 0.0008751576533541083f, 0.0011784110683947802f, 0.003160371445119381f, 0.0020703894551843405f, 0.001808947417885065f, 0.0022357129491865635f, 0.0014023667899891734f, 0.001276451745070517f, 0.0012139747850596905f, 0.0010111650917679071f, 0.0007655827212147415f, 0.0007862481288611889f, 0.0008328655385412276f, 0.0008924589492380619f, 0.0008333461591973901f, 0.0008645846392028034f, 0.0009583000210113823f, 0.002043476328253746f, 0.0023702785838395357f, 0.003329539904370904f, 0.0021818866953253746f, 0.0027778204530477524f, 0.0021761194802820683f, 0.0015580785693600774f, 0.0008208507788367569f, 0.001087098615244031f, 0.0007170429453253746f, 0.0006680225487798452f, 0.0007968212012201548f, 0.0007530873408541083f, 0.0011736050946637988f, 0.000932348077185452f, 0.0010899821063503623f);
static const ai_layer_format_type _model_22_cv2_1_cv2_1_2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;

static const ai_i8 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-119);
static const ai_i16 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -119;
static const ai_i8 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -16;
static const ai_float _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.03080553002655506f;
static const ai_float _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03219177573919296f;
static const ai_float _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0007156211067922413f, 0.0008758701151236892f, 0.0007885052473284304f, 0.0005488396272994578f, 0.0010726290056481957f, 0.0008529816404916346f, 0.0006968820816837251f, 0.0006250315927900374f, 0.0005867610452696681f, 0.0010926324175670743f, 0.0010224507423117757f, 0.0007539839716628194f, 0.0007853003335185349f, 0.0008045269059948623f, 0.0007858853787183762f, 0.0005791921867057681f, 0.0005391278536990285f, 0.0010649424511939287f, 0.0012422531144693494f, 0.001063777832314372f, 0.0010363366454839706f, 0.0009517925791442394f, 0.0010851339902728796f, 0.001142665627412498f, 0.0009301876416429877f, 0.0008625349146313965f, 0.0008322905632667243f, 0.000523519585840404f, 0.0006645835819654167f, 0.0009096616413444281f, 0.0009740249952301383f, 0.0006702679092995822f);
static const ai_layer_format_type _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;



static const ai_i8 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-113);
static const ai_i16 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 12;

static const ai_u16 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 14;
static const ai_u16 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 14;
static const ai_u16 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -113;
static const ai_i8 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 34;
static const ai_float _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.018907619640231133f;
static const ai_float _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.03759874030947685f;
static const ai_float _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0012369398027658463f, 0.0012214435264468193f, 0.0021736561320722103f, 0.0016158423386514187f, 0.0010937259066849947f, 0.0009670259896665812f, 0.0009880362777039409f, 0.0014532858040183783f, 0.001438318518921733f, 0.0014447440626099706f, 0.0019700215198099613f, 0.0018460261635482311f, 0.0020091079641133547f, 0.0008935503428801894f, 0.0016606353456154466f, 0.0016664896393194795f, 0.0022013303823769093f, 0.0019453156273812056f, 0.0017011520685628057f, 0.0016946368850767612f, 0.0007228681934066117f, 0.0020038208458572626f, 0.002236173255369067f, 0.0014265625504776835f, 0.005108342971652746f, 0.001537988311611116f, 0.006290985271334648f, 0.0005115146632306278f, 0.0019994317553937435f, 0.0017101714620366693f, 0.0020307847298681736f, 0.001957822823897004f);
static const ai_layer_format_type _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 12;
static const ai_u16 _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 12;




static const ai_i8 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-122);
static const ai_i16 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 6;
static const ai_float _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.045634496957063675f;
static const ai_float _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.09617367386817932f;
static const ai_float _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.002997742500156164f, 0.0024523104075342417f, 0.0017554457299411297f, 0.001934301806613803f, 0.0010111693991348147f, 0.0016007230151444674f, 0.0013025333173573017f, 0.0026206797920167446f, 0.0027107354253530502f, 0.0022521845530718565f, 0.001927076606079936f, 0.0023171904031187296f, 0.00206615193746984f, 0.0014013948384672403f, 0.002291005337610841f, 0.0019253373611718416f, 0.0025598942302167416f, 0.0014618823770433664f, 0.0016198829980567098f, 0.0012814808869734406f, 0.0013584342086687684f, 0.002242147224023938f, 0.001762242871336639f, 0.0020313651766628027f, 0.004392112605273724f, 0.001470095245167613f, 0.0017573353834450245f, 0.0029064370319247246f, 0.002514051040634513f, 0.0015145838260650635f, 0.0027927495539188385f, 0.002031218959018588f, 0.002260910114273429f, 0.0013501326320692897f, 0.0023716764990240335f, 0.0008962707361206412f, 0.0026888709980994463f, 0.003657308639958501f, 0.00076414889190346f, 0.0026664775796234608f, 0.00207223161123693f, 0.0025059485342353582f, 0.002253513550385833f, 0.001527956104837358f, 0.0014926522271707654f, 0.0008311534766107798f, 0.0018973153783008456f, 0.0026931380853056908f, 0.0018315556226298213f, 0.0026887888088822365f, 0.001536506344564259f, 0.0025549193378537893f, 0.0027229415718466043f, 0.0019486021483317018f, 0.00118628004565835f, 0.0008635938866063952f, 0.001514521543867886f, 0.0017802540678530931f, 0.0026487959548830986f, 0.0015637058531865478f, 0.0013923401711508632f, 0.00187292555347085f, 0.0015314922202378511f, 0.002047366928309202f);
static const ai_layer_format_type _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;



static const ai_i8 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-122);
static const ai_i16 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 7;
static const ai_float _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.0468796007335186f;
static const ai_float _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.05101950839161873f;
static const ai_float _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0012348669115453959f, 0.0007978100329637527f, 0.00024006159219425172f, 0.0005588441272266209f, 0.0006703417748212814f, 0.0004726586921606213f, 0.00025724497390910983f, 0.00045237730955705047f, 0.0014480800600722432f, 0.0010930574499070644f, 0.000713139190338552f, 0.0002878736995626241f, 0.00031568753183819354f, 0.0014275970170274377f, 0.0006877120467834175f, 0.0020726812072098255f, 0.0006082307663746178f, 0.0005138522246852517f, 0.00021406907762866467f, 0.001051845494657755f, 0.0003167907416354865f, 0.0017389488639310002f, 0.0016155610792338848f, 0.0003884978359565139f, 0.0004771793610416353f, 0.0005819950602017343f, 0.0019122761441394687f, 0.00040632273885421455f, 0.0001911554136313498f, 0.001080788904801011f, 0.0008890476892702281f, 0.00038132749614305794f, 0.0008954142685979605f, 0.0010668025352060795f, 0.0008113926742225885f, 0.0008351851138286293f, 0.0003175509045831859f, 0.00034832037636078894f, 0.0011461896356195211f, 0.0008336915052495897f, 0.0034457796718925238f, 0.0007830862305127084f, 0.001608681515790522f, 0.0015172598650678992f, 0.0007279181154444814f, 0.00041063138633035123f, 0.0005572643713094294f, 0.0008333573932759464f, 0.000405344704631716f, 0.0010063434019684792f, 0.0006031769444234669f, 0.0001996235514525324f, 0.0015168084064498544f, 0.00036843252019025385f, 0.0011740849586203694f, 0.0004149399173911661f, 0.00044047145638614893f, 0.0008637579157948494f, 0.001177843427285552f, 0.0002239072200609371f, 0.0008193798130378127f, 0.0008520096889697015f, 0.0011190396035090089f, 0.0009681207593530416f);
static const ai_layer_format_type _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;



static const ai_u16 _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_shape_w_const_u16 = 24;
static const ai_u16 _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_shape_h_const_u16 = 24;
static const ai_u16 _model_22_cv2_0_cv2_0_2_Conv_output_0_l_stride_1_const_u16 = 1;
static const ai_u16 _model_22_cv2_0_cv2_0_2_Conv_output_0_l_stride_0_const_u16 = 1;
static const ai_u16 _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_shape_ch_const_u16 = 64;
static const ai_u16 _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_shape_ch_const_u16 = 64;
static const ai_i8 _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_fmt_zero_const_s8 = -117;
static const ai_i8 _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_fmt_zero_const_s8 = -52;
static const ai_float _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.02510436251759529f;
static const ai_float _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.08558892458677292f;
static const ai_float _model_22_cv2_0_cv2_0_2_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.01387949101626873f, 0.013510395772755146f, 0.014087106101214886f, 0.01131889782845974f, 0.013856422156095505f, 0.009519562125205994f, 0.008327694609761238f, 0.008773683570325375f, 0.00980407278984785f, 0.0060746800154447556f, 0.007624107878655195f, 0.007266547530889511f, 0.0048789675347507f, 0.0051519437693059444f, 0.006651390343904495f, 0.007247324101626873f, 0.013941006734967232f, 0.011157418601214886f, 0.010611466132104397f, 0.012187807820737362f, 0.008642962202429771f, 0.008335383608937263f, 0.007800965569913387f, 0.008158526383340359f, 0.005705585703253746f, 0.007078155875205994f, 0.00557101983577013f, 0.004317636601626873f, 0.006928211078047752f, 0.006624476984143257f, 0.004717488773167133f, 0.0023433654569089413f, 0.015840305015444756f, 0.015463520772755146f, 0.015102116391062737f, 0.01504828967154026f, 0.0127030024304986f, 0.011664923280477524f, 0.007920152507722378f, 0.008196973241865635f, 0.0087890625f, 0.006605253554880619f, 0.007008950691670179f, 0.008089320734143257f, 0.004848209675401449f, 0.005847840569913387f, 0.005605622660368681f, 0.002964290091767907f, 0.01522514782845974f, 0.01326433289796114f, 0.014948327094316483f, 0.014571542851626873f, 0.012456938624382019f, 0.012749139219522476f, 0.010003998875617981f, 0.006524513941258192f, 0.009065883234143257f, 0.009642593562602997f, 0.007043553050607443f, 0.0072088767774403095f, 0.0051788571290671825f, 0.005105806980282068f, 0.00467904144898057f, 0.004329170566052198f);
static const ai_layer_format_type _model_22_cv2_0_cv2_0_2_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;




static const ai_u32 _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 48384;
static const ai_float _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_in_0_fmt_scale_const_f32 = 0.08558892458677292f;
static const ai_i8 _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_in_0_fmt_zero_const_s8 = -52;

static const ai_i32 _model_22_dfl_Softmax_output_0_t_in_0_shape_ch_h_w_prod_const_s32 = 48384;

static const ai_u32 _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32 = 48384;
static const ai_float _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_fmt_scale_const_f32 = 0.003921568859368563f;
static const ai_i8 _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_fmt_zero_const_s8 = -128;













static const ai_i8 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-122);
static const ai_i16 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -122;
static const ai_i8 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = 4;
static const ai_float _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.045634496957063675f;
static const ai_float _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.07984063029289246f;
static const ai_float _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0038749200757592916f, 0.002504208590835333f, 0.0019329170463606715f, 0.0019526297692209482f, 0.0016026007942855358f, 0.0012781317345798016f, 0.0010931482538580894f, 0.0011544364970177412f, 0.0011651178356260061f, 0.0014992699725553393f, 0.0013618257362395525f, 0.004170041531324387f, 0.00136451271828264f, 0.0011306292144581676f, 0.0014468443114310503f, 0.001266009290702641f, 0.0006527493242174387f, 0.0009487030911259353f, 0.0019901006016880274f, 0.0017686596838757396f, 0.0024444113951176405f, 0.0016525322571396828f, 0.0027357665821909904f, 0.0016839989693835378f, 0.0024501141160726547f, 0.001978792017325759f, 0.001316552865318954f, 0.0016024383949115872f, 0.0015215264866128564f, 0.001651015249080956f, 0.0010221515549346805f, 0.0007469065021723509f);
static const ai_layer_format_type _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;



static const ai_i8 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8[] = LITE_ARRAY_VALUES(-121);
static const ai_i16 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16 = 8;
static const ai_u32 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32 = 24;

static const ai_u16 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_shape_w_const_u16 = 26;
static const ai_u16 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_shape_h_const_u16 = 26;
static const ai_u16 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16 = 32;
static const ai_u16 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16 = 32;
static const ai_i8 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8 = -121;
static const ai_i8 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8 = -4;
static const ai_float _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32 = 0.039647117257118225f;
static const ai_float _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32 = 0.19499097764492035f;
static const ai_float _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32[] = LITE_ARRAY_VALUES(0.0015291967429220676f, 0.0022428070660680532f, 0.0021252634469419718f, 0.0003981467161793262f, 0.0018628985853865743f, 0.0016665735747665167f, 0.0022334230598062277f, 0.013671725057065487f, 0.0024395904038101435f, 0.0017121783457696438f, 0.0010982248932123184f, 0.0013465469237416983f, 0.0009032641537487507f, 0.00034006877103820443f, 0.0020740011241286993f, 0.0009604501537978649f, 0.0022130811121314764f, 0.00211873441003263f, 0.0023698925506323576f, 0.0026508523151278496f, 0.007944109849631786f, 0.0021916176192462444f, 0.0007489145500585437f, 0.002785483840852976f, 0.002757416106760502f, 0.0018002097494900227f, 0.006684389431029558f, 0.0005578707787208259f, 0.000509982870426029f, 0.0016248334432020783f, 0.0014533898793160915f, 0.0019393946276977658f);
static const ai_layer_format_type _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type = AI_LAYER_FORMAT_CHANNEL_LAST_VALID;
static const ai_u16 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_shape_w_const_u16 = 24;
static const ai_u16 _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_shape_h_const_u16 = 24;





STAI_API_ENTRY
stai_return_code stai_network_run(
  stai_network* network,
  const stai_run_mode mode)
{
   STAI_UNUSED(mode)
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_ACTIVATIONS) != STAI_FLAG_ACTIVATIONS,
        STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_INPUTS) != STAI_FLAG_INPUTS,
                  STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_OUTPUTS) != STAI_FLAG_OUTPUTS,
                  STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)

  _STAI_SET_ERROR(net_ctx, (net_ctx->_flags & STAI_FLAG_WEIGHTS) != STAI_FLAG_WEIGHTS,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)


  /* LITE_KERNEL_SECTION BEGIN _model_0_conv_Conv_output_0 */
  {
      const ai_i8* _model_0_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_inputs[0] + 0);
    const ai_i8* _model_0_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 3784);
    const ai_i32* _model_0_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 3856);
    ai_i8* _model_0_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    ai_i16* _model_0_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 74504);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(134, 1, {(stai_ptr) _model_0_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_0_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_0_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_0_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_0_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_0_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_0_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_0_conv_Conv_output_0_l_stride_1_const_u16, _model_0_conv_Conv_output_0_l_stride_0_const_u16, _model_0_conv_Conv_output_0_l_pad_W_0_const_s32, _model_0_conv_Conv_output_0_l_pad_H_0_const_s32, _model_0_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_0_conv_Conv_output_0_t_out_0_ptr_s8, _model_0_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_0_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 292, _model_0_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(134, 1, {(stai_ptr) _model_0_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_0_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_0_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_0_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_0_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_0_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_0_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_0_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_1_conv_Conv_output_0 */
  {
      const ai_i8* _model_1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 0);
    const ai_i8* _model_1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 3888);
    const ai_i32* _model_1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 5040);
    ai_i8* _model_1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 76544);
    ai_i16* _model_1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 73728);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(143, 1, {(stai_ptr) _model_1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_1_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_1_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_1_conv_Conv_output_0_l_stride_1_const_u16, _model_1_conv_Conv_output_0_l_stride_0_const_u16, _model_1_conv_Conv_output_0_l_pad_W_0_const_s32, _model_1_conv_Conv_output_0_l_pad_H_0_const_s32, _model_1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_1_conv_Conv_output_0_t_out_0_ptr_s8, _model_1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 2816, _model_1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(143, 1, {(stai_ptr) _model_1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_2_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 36864);
    const ai_i8* _model_2_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 5104);
    const ai_i32* _model_2_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 5360);
    ai_i8* _model_2_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 73728);
    ai_i16* _model_2_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 0);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(152, 1, {(stai_ptr) _model_2_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_2_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_2_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_2_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_2_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_2_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_2_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_2_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_2_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_2_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_2_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_2_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_2_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_2_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_2_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_2_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_2_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 224, _model_2_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(152, 1, {(stai_ptr) _model_2_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_2_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_2_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_2_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_Split_output_0 */
  {
    
  forward_lite_split__model_2_Split_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_Split_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_m_0_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 18432);
    const ai_i8* _model_2_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 5428);
    const ai_i32* _model_2_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 6004);
    ai_i8* _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 38416);
    ai_i16* _model_2_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 36864);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(166, 1, {(stai_ptr) _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_2_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_l_pad_W_0_const_s32, _model_2_m_0_cv1_conv_Conv_output_0_l_pad_H_0_const_s32, _model_2_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_2_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_2_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_2_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1552, _model_2_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(166, 1, {(stai_ptr) _model_2_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_2_m_0_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_m_0_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_2_m_0_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_m_0_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_m_0_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_2_m_0_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_m_0_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_m_0_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 75280);
    const ai_i8* _model_2_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 6036);
    const ai_i32* _model_2_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 6612);
    ai_i8* _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 38416);
    ai_i16* _model_2_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 36864);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(175, 1, {(stai_ptr) _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_2_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_l_pad_W_0_const_s32, _model_2_m_0_cv2_conv_Conv_output_0_l_pad_H_0_const_s32, _model_2_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_2_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_2_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_2_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1552, _model_2_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(175, 1, {(stai_ptr) _model_2_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_2_m_0_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_m_0_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_2_m_0_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_m_0_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_m_0_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_2_m_0_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_m_0_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_m_0_Add_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_2_m_0_Add_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_m_0_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_Concat_output_0 */
  {
    
  forward_lite_concat__model_2_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_2_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55296);
    const ai_i8* _model_2_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 6644);
    const ai_i32* _model_2_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 7028);
    ai_i8* _model_2_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 110592);
    ai_i16* _model_2_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 36864);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(190, 1, {(stai_ptr) _model_2_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_2_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_2_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_2_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_2_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_2_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_2_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_2_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_2_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_2_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_2_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_2_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_2_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_2_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_2_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_2_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_2_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 256, _model_2_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(190, 1, {(stai_ptr) _model_2_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_2_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_2_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_2_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_2_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_2_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_3_conv_Conv_output_0 */
  {
      const ai_i8* _model_3_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 73728);
    const ai_i8* _model_3_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 7092);
    const ai_i32* _model_3_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 11700);
    ai_i8* _model_3_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 43008);
    ai_i16* _model_3_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 36864);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(199, 1, {(stai_ptr) _model_3_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_3_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_3_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_3_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_3_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_3_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_3_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_3_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_3_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_3_conv_Conv_output_0_l_stride_1_const_u16, _model_3_conv_Conv_output_0_l_stride_0_const_u16, _model_3_conv_Conv_output_0_l_pad_W_0_const_s32, _model_3_conv_Conv_output_0_l_pad_H_0_const_s32, _model_3_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_3_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_3_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_3_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_3_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_3_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_3_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_3_conv_Conv_output_0_t_out_0_ptr_s8, _model_3_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_3_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6144, _model_3_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(199, 1, {(stai_ptr) _model_3_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_3_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_3_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_3_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_3_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_3_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_3_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_3_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_4_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 79872);
    const ai_i8* _model_4_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 11828);
    const ai_i32* _model_4_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 12852);
    ai_i8* _model_4_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 37312);
    ai_i16* _model_4_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 36864);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(208, 1, {(stai_ptr) _model_4_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_4_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_4_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_4_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_4_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_4_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_4_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_4_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_4_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_4_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_4_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_4_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_4_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_4_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_4_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_4_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_4_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 448, _model_4_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(208, 1, {(stai_ptr) _model_4_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_4_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_4_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_4_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_Split_output_0 */
  {
    
  forward_lite_split__model_4_Split_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_Split_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 46080);
    ai_ptr _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 55296);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(222, 1, {(stai_ptr) _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_4_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(416), (ai_i32)(416), (ai_i32)(16), (ai_i32)(16));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(222, 1, {(stai_ptr) _model_4_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55296);
    const ai_i8* _model_4_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 12984);
    const ai_i32* _model_4_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 15288);
    ai_i8* _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 71520);
    ai_i16* _model_4_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 66112);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(222, 1, {(stai_ptr) _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_4_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_l_pad_W_0_const_s32, _model_4_m_0_cv1_conv_Conv_output_0_l_pad_H_0_const_s32, _model_4_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_4_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_4_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_4_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 5408, _model_4_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(222, 1, {(stai_ptr) _model_4_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_4_m_0_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_4_m_0_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 80736);
    ai_ptr _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 55296);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(231, 1, {(stai_ptr) _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_4_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(416), (ai_i32)(416), (ai_i32)(16), (ai_i32)(16));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(231, 1, {(stai_ptr) _model_4_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 55296);
    const ai_i8* _model_4_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 15352);
    const ai_i32* _model_4_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 17656);
    ai_i8* _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 71520);
    ai_i16* _model_4_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 66112);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(231, 1, {(stai_ptr) _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_4_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_l_pad_W_0_const_s32, _model_4_m_0_cv2_conv_Conv_output_0_l_pad_H_0_const_s32, _model_4_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_4_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_4_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_4_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 5408, _model_4_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(231, 1, {(stai_ptr) _model_4_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_4_m_0_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_4_m_0_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_0_Add_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_4_m_0_Add_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_0_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 55296);
    ai_ptr _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 64512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(243, 1, {(stai_ptr) _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_4_m_1_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(416), (ai_i32)(416), (ai_i32)(16), (ai_i32)(16));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(243, 1, {(stai_ptr) _model_4_m_1_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 64512);
    const ai_i8* _model_4_m_1_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 17720);
    const ai_i32* _model_4_m_1_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 20024);
    ai_i8* _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 80736);
    ai_i16* _model_4_m_1_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 75328);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(243, 1, {(stai_ptr) _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_4_m_1_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_l_pad_W_0_const_s32, _model_4_m_1_cv1_conv_Conv_output_0_l_pad_H_0_const_s32, _model_4_m_1_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_4_m_1_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_4_m_1_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_4_m_1_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 5408, _model_4_m_1_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(243, 1, {(stai_ptr) _model_4_m_1_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_4_m_1_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_4_m_1_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 89952);
    ai_ptr _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 64512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(252, 1, {(stai_ptr) _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_4_m_1_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(416), (ai_i32)(416), (ai_i32)(16), (ai_i32)(16));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(252, 1, {(stai_ptr) _model_4_m_1_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 64512);
    const ai_i8* _model_4_m_1_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 20088);
    const ai_i32* _model_4_m_1_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 22392);
    ai_i8* _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 80736);
    ai_i16* _model_4_m_1_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 75328);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(252, 1, {(stai_ptr) _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_4_m_1_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_l_pad_W_0_const_s32, _model_4_m_1_cv2_conv_Conv_output_0_l_pad_H_0_const_s32, _model_4_m_1_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_4_m_1_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_4_m_1_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_4_m_1_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 5408, _model_4_m_1_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(252, 1, {(stai_ptr) _model_4_m_1_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_4_m_1_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_4_m_1_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_m_1_Add_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_4_m_1_Add_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_m_1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_Concat_output_0 */
  {
    
  forward_lite_concat__model_4_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_4_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 73728);
    const ai_i8* _model_4_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 22456);
    const ai_i32* _model_4_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 24504);
    ai_i8* _model_4_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 110592);
    ai_i16* _model_4_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 55296);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(267, 1, {(stai_ptr) _model_4_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_4_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_4_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_4_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_4_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_4_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_4_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_4_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_4_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_4_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_4_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_4_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_4_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_4_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_4_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_4_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_4_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 576, _model_4_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(267, 1, {(stai_ptr) _model_4_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_4_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_4_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_4_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_4_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_4_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_5_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_5_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 73728);
    ai_ptr _model_5_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 92160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(276, 1, {(stai_ptr) _model_5_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_5_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_5_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_5_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_5_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_5_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(832), (ai_i32)(832), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(276, 1, {(stai_ptr) _model_5_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_5_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_5_conv_Conv_output_0 */
  {
      const ai_i8* _model_5_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 92160);
    const ai_i8* _model_5_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 24632);
    const ai_i32* _model_5_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 43064);
    ai_i8* _model_5_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 62464);
    ai_i16* _model_5_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 55296);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(276, 1, {(stai_ptr) _model_5_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_model_5_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_5_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_5_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_5_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_5_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_5_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_5_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_5_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_5_conv_Conv_output_0_l_stride_1_const_u16, _model_5_conv_Conv_output_0_l_stride_0_const_u16, _model_5_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_5_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_5_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_5_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_5_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_5_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_5_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_5_conv_Conv_output_0_t_out_0_ptr_s8, _model_5_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_5_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 7168, _model_5_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(276, 1, {(stai_ptr) _model_5_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_5_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_5_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_5_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_5_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_5_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_5_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_5_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_6_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    const ai_i8* _model_6_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 43320);
    const ai_i32* _model_6_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 47416);
    ai_i8* _model_6_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 56192);
    ai_i16* _model_6_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 55296);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(285, 1, {(stai_ptr) _model_6_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_6_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_6_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_6_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_6_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_6_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_6_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_6_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_6_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_6_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_6_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_6_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_6_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_6_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_6_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_6_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_6_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 896, _model_6_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(285, 1, {(stai_ptr) _model_6_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_6_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_6_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_6_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_Split_output_0 */
  {
    
  forward_lite_split__model_6_Split_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_Split_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 59904);
    ai_ptr _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 64512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(299, 1, {(stai_ptr) _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_6_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(299, 1, {(stai_ptr) _model_6_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 64512);
    const ai_i8* _model_6_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 47676);
    const ai_i32* _model_6_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 56892);
    ai_i8* _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 98880);
    ai_i16* _model_6_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 92160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(299, 1, {(stai_ptr) _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_6_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_6_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_6_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_6_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_6_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_6_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_6_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(299, 1, {(stai_ptr) _model_6_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_6_m_0_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_6_m_0_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 69120);
    ai_ptr _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 92160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(308, 1, {(stai_ptr) _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_6_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(308, 1, {(stai_ptr) _model_6_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 92160);
    const ai_i8* _model_6_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 57020);
    const ai_i32* _model_6_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 66236);
    ai_i8* _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 98432);
    ai_i16* _model_6_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 64512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(308, 1, {(stai_ptr) _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_6_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_6_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_6_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_6_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_6_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_6_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_6_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(308, 1, {(stai_ptr) _model_6_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_6_m_0_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_6_m_0_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_0_Add_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_6_m_0_Add_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_0_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 64512);
    ai_ptr _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 92160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(320, 1, {(stai_ptr) _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_6_m_1_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(320, 1, {(stai_ptr) _model_6_m_1_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 92160);
    const ai_i8* _model_6_m_1_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 66364);
    const ai_i32* _model_6_m_1_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 75580);
    ai_i8* _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 69120);
    ai_i16* _model_6_m_1_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 98432);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(320, 1, {(stai_ptr) _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_6_m_1_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_6_m_1_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_6_m_1_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_6_m_1_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_6_m_1_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_6_m_1_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_6_m_1_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(320, 1, {(stai_ptr) _model_6_m_1_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_6_m_1_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_6_m_1_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 96768);
    ai_ptr _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 101376);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(329, 1, {(stai_ptr) _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_6_m_1_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(329, 1, {(stai_ptr) _model_6_m_1_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    const ai_i8* _model_6_m_1_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 75708);
    const ai_i32* _model_6_m_1_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 84924);
    ai_i8* _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 69120);
    ai_i16* _model_6_m_1_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 92160);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(329, 1, {(stai_ptr) _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_6_m_1_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_6_m_1_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_6_m_1_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_6_m_1_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_6_m_1_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_6_m_1_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_6_m_1_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(329, 1, {(stai_ptr) _model_6_m_1_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_6_m_1_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_6_m_1_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_m_1_Add_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_6_m_1_Add_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_m_1_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_Concat_output_0 */
  {
    
  forward_lite_concat__model_6_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_6_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 92160);
    const ai_i8* _model_6_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 85052);
    const ai_i32* _model_6_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 93244);
    ai_i8* _model_6_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 110592);
    ai_i16* _model_6_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 64512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(344, 1, {(stai_ptr) _model_6_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_6_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_6_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_6_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_6_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_6_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_6_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_6_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_6_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_6_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_6_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_6_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_6_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_6_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_6_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_6_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_6_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1152, _model_6_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(344, 1, {(stai_ptr) _model_6_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_6_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_6_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_6_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_6_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_6_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_7_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_7_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 92160);
    ai_ptr _model_7_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 101376);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(353, 1, {(stai_ptr) _model_7_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_7_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_7_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_7_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_7_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_7_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(896), (ai_i32)(896), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(353, 1, {(stai_ptr) _model_7_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_7_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_7_conv_Conv_output_0 */
  {
      const ai_i8* _model_7_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    const ai_i8* _model_7_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 93500);
    const ai_i32* _model_7_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 130364);
    ai_i8* _model_7_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 113920);
    ai_i16* _model_7_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 64512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(353, 1, {(stai_ptr) _model_7_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_model_7_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_7_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_7_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_7_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_7_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_7_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_7_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_7_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_7_conv_Conv_output_0_l_stride_1_const_u16, _model_7_conv_Conv_output_0_l_stride_0_const_u16, _model_7_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_7_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_7_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_7_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_7_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_7_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_7_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_7_conv_Conv_output_0_t_out_0_ptr_s8, _model_7_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_7_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 8320, _model_7_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(353, 1, {(stai_ptr) _model_7_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_7_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_7_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_7_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_7_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_7_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_7_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_7_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_8_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 66816);
    const ai_i8* _model_8_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 130620);
    const ai_i32* _model_8_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 134716);
    ai_i8* _model_8_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 69120);
    ai_i16* _model_8_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 64512);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(362, 1, {(stai_ptr) _model_8_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_8_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_8_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_8_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_8_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_8_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_8_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_8_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_8_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_8_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_8_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_8_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_8_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_8_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_8_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_8_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_8_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 896, _model_8_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(362, 1, {(stai_ptr) _model_8_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_8_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_8_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_8_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_Split_output_0 */
  {
    
  forward_lite_split__model_8_Split_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_Split_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 65664);
    ai_ptr _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 66816);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(376, 1, {(stai_ptr) _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_8_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(192), (ai_i32)(256), (ai_i32)(256), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(376, 1, {(stai_ptr) _model_8_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 66816);
    const ai_i8* _model_8_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 134976);
    const ai_i32* _model_8_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 144192);
    ai_i8* _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 68864);
    ai_i16* _model_8_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 101376);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(376, 1, {(stai_ptr) _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_8_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_8_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_8_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_8_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_8_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_8_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_8_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(376, 1, {(stai_ptr) _model_8_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_8_m_0_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_8_m_0_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 70016);
    ai_ptr _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 66816);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(385, 1, {(stai_ptr) _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_8_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(192), (ai_i32)(256), (ai_i32)(256), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(385, 1, {(stai_ptr) _model_8_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 66816);
    const ai_i8* _model_8_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 144320);
    const ai_i32* _model_8_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 153536);
    ai_i8* _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 68864);
    ai_i16* _model_8_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 101376);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(385, 1, {(stai_ptr) _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_8_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_8_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_8_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_8_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_8_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_8_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_8_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(385, 1, {(stai_ptr) _model_8_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_8_m_0_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_8_m_0_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_m_0_Add_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_8_m_0_Add_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_m_0_Add_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_Concat_output_0 */
  {
    
  forward_lite_concat__model_8_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_8_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 67968);
    const ai_i8* _model_8_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 153664);
    const ai_i32* _model_8_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 159808);
    ai_i8* _model_8_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 71424);
    ai_i16* _model_8_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 66816);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(400, 1, {(stai_ptr) _model_8_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_8_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_8_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_8_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_8_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_8_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_8_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_8_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_8_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_8_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_8_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_8_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_8_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_8_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_8_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_8_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_8_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1024, _model_8_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(400, 1, {(stai_ptr) _model_8_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_8_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_8_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_8_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_8_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_8_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_9_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_9_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 69120);
    const ai_i8* _model_9_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 160064);
    const ai_i32* _model_9_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 162112);
    ai_i8* _model_9_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 67392);
    ai_i16* _model_9_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 66816);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(409, 1, {(stai_ptr) _model_9_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_9_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_9_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_9_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_9_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_9_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_9_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_9_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_9_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_9_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_9_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_9_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_9_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_9_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_9_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_9_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_9_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 576, _model_9_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(409, 1, {(stai_ptr) _model_9_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_9_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_9_m_MaxPool_output_0 */
  {
      const ai_i8* _model_9_m_MaxPool_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 67392);
    ai_i8* _model_9_m_MaxPool_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 68544);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(412, 1, {(stai_ptr) _model_9_m_MaxPool_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_maxpool_is8os8_scalepos(_model_9_m_MaxPool_output_0_t_in_0_ptr_const_s8, _model_9_m_MaxPool_output_0_t_out_0_ptr_s8, _model_9_m_MaxPool_output_0_t_in_0_shape_w_const_u16, _model_9_m_MaxPool_output_0_t_in_0_shape_h_const_u16, _model_9_m_MaxPool_output_0_t_in_0_shape_ch_const_u16, _model_9_m_MaxPool_output_0_l_pool_size_1_const_u16, _model_9_m_MaxPool_output_0_l_pool_size_0_const_u16, _model_9_m_MaxPool_output_0_l_legacy_pool_pad_1_const_u16, _model_9_m_MaxPool_output_0_l_legacy_pool_pad_0_const_u16, _model_9_m_MaxPool_output_0_l_pool_stride_1_const_u16, _model_9_m_MaxPool_output_0_l_pool_stride_0_const_u16, _model_9_m_MaxPool_output_0_t_out_0_shape_w_const_u16, _model_9_m_MaxPool_output_0_t_out_0_shape_h_const_u16, 1.0f, _model_9_m_MaxPool_output_0_t_in_0_fmt_zero_const_s8, _model_9_m_MaxPool_output_0_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(412, 1, {(stai_ptr) _model_9_m_MaxPool_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_9_m_MaxPool_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_9_m_1_MaxPool_output_0 */
  {
      const ai_i8* _model_9_m_1_MaxPool_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 68544);
    ai_i8* _model_9_m_1_MaxPool_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 69696);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(415, 1, {(stai_ptr) _model_9_m_1_MaxPool_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_maxpool_is8os8_scalepos(_model_9_m_1_MaxPool_output_0_t_in_0_ptr_const_s8, _model_9_m_1_MaxPool_output_0_t_out_0_ptr_s8, _model_9_m_1_MaxPool_output_0_t_in_0_shape_w_const_u16, _model_9_m_1_MaxPool_output_0_t_in_0_shape_h_const_u16, _model_9_m_1_MaxPool_output_0_t_in_0_shape_ch_const_u16, _model_9_m_1_MaxPool_output_0_l_pool_size_1_const_u16, _model_9_m_1_MaxPool_output_0_l_pool_size_0_const_u16, _model_9_m_1_MaxPool_output_0_l_legacy_pool_pad_1_const_u16, _model_9_m_1_MaxPool_output_0_l_legacy_pool_pad_0_const_u16, _model_9_m_1_MaxPool_output_0_l_pool_stride_1_const_u16, _model_9_m_1_MaxPool_output_0_l_pool_stride_0_const_u16, _model_9_m_1_MaxPool_output_0_t_out_0_shape_w_const_u16, _model_9_m_1_MaxPool_output_0_t_out_0_shape_h_const_u16, 1.0f, _model_9_m_1_MaxPool_output_0_t_in_0_fmt_zero_const_s8, _model_9_m_1_MaxPool_output_0_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(415, 1, {(stai_ptr) _model_9_m_1_MaxPool_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_9_m_1_MaxPool_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_9_m_2_MaxPool_output_0 */
  {
      const ai_i8* _model_9_m_2_MaxPool_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 69696);
    ai_i8* _model_9_m_2_MaxPool_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 70848);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(418, 1, {(stai_ptr) _model_9_m_2_MaxPool_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_maxpool_is8os8_scalepos(_model_9_m_2_MaxPool_output_0_t_in_0_ptr_const_s8, _model_9_m_2_MaxPool_output_0_t_out_0_ptr_s8, _model_9_m_2_MaxPool_output_0_t_in_0_shape_w_const_u16, _model_9_m_2_MaxPool_output_0_t_in_0_shape_h_const_u16, _model_9_m_2_MaxPool_output_0_t_in_0_shape_ch_const_u16, _model_9_m_2_MaxPool_output_0_l_pool_size_1_const_u16, _model_9_m_2_MaxPool_output_0_l_pool_size_0_const_u16, _model_9_m_2_MaxPool_output_0_l_legacy_pool_pad_1_const_u16, _model_9_m_2_MaxPool_output_0_l_legacy_pool_pad_0_const_u16, _model_9_m_2_MaxPool_output_0_l_pool_stride_1_const_u16, _model_9_m_2_MaxPool_output_0_l_pool_stride_0_const_u16, _model_9_m_2_MaxPool_output_0_t_out_0_shape_w_const_u16, _model_9_m_2_MaxPool_output_0_t_out_0_shape_h_const_u16, 1.0f, _model_9_m_2_MaxPool_output_0_t_in_0_fmt_zero_const_s8, _model_9_m_2_MaxPool_output_0_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(418, 1, {(stai_ptr) _model_9_m_2_MaxPool_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_9_m_2_MaxPool_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_9_Concat_output_0 */
  {
    
  forward_lite_concat__model_9_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_9_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_9_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_9_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    const ai_i8* _model_9_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 162240);
    const ai_i32* _model_9_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 170432);
    ai_i8* _model_9_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 67968);
    ai_i16* _model_9_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 66816);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(424, 1, {(stai_ptr) _model_9_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_9_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_9_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_9_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_9_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_9_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_9_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_9_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_9_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_9_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_9_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_9_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_9_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_9_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_9_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_9_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_9_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1152, _model_9_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(424, 1, {(stai_ptr) _model_9_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_9_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_9_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_9_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_9_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_9_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_9_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_9_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_10_Resize_output_0 */
  {
    
  forward_lite_upsample_nearest__model_10_Resize_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_10_Resize_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_11_Concat_output_0 */
  {
    
  forward_lite_concat__model_11_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_11_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_12_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    const ai_i8* _model_12_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 170688);
    const ai_i32* _model_12_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 178880);
    ai_i8* _model_12_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 92160);
    ai_i16* _model_12_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 66816);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(439, 1, {(stai_ptr) _model_12_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_12_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_12_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_12_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_12_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_12_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_12_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_12_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_12_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_12_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_12_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_12_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_12_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_12_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_12_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_12_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_12_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1152, _model_12_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(439, 1, {(stai_ptr) _model_12_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_12_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_12_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_12_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_Split_output_0 */
  {
    
  forward_lite_split__model_12_Split_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_Split_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_m_0_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 92160);
    ai_ptr _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 103680);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(453, 1, {(stai_ptr) _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_12_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(453, 1, {(stai_ptr) _model_12_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_12_m_0_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_12_m_0_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 103680);
    const ai_i8* _model_12_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 179140);
    const ai_i32* _model_12_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 188356);
    ai_i8* _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96768);
    ai_i16* _model_12_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 109952);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(453, 1, {(stai_ptr) _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_12_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_12_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_12_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_12_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_12_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_12_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_12_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(453, 1, {(stai_ptr) _model_12_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_12_m_0_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_m_0_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_12_m_0_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_m_0_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_m_0_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_12_m_0_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_m_0_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_m_0_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 108288);
    ai_ptr _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 112896);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(462, 1, {(stai_ptr) _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_12_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(462, 1, {(stai_ptr) _model_12_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_12_m_0_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_12_m_0_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    const ai_i8* _model_12_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 188484);
    const ai_i32* _model_12_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 197700);
    ai_i8* _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96768);
    ai_i16* _model_12_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 103680);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(462, 1, {(stai_ptr) _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_12_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_12_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_12_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_12_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_12_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_12_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_12_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(462, 1, {(stai_ptr) _model_12_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_12_m_0_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_m_0_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_12_m_0_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_m_0_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_m_0_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_12_m_0_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_m_0_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_Concat_output_0 */
  {
    
  forward_lite_concat__model_12_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_12_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    const ai_i8* _model_12_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 197828);
    const ai_i32* _model_12_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 203972);
    ai_i8* _model_12_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 103680);
    ai_i16* _model_12_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 71424);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(474, 1, {(stai_ptr) _model_12_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_12_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_12_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_12_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_12_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_12_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_12_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_12_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_12_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_12_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_12_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_12_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_12_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_12_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_12_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_12_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_12_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1024, _model_12_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(474, 1, {(stai_ptr) _model_12_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_12_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_12_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_12_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_12_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_12_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_13_Resize_output_0 */
  {
    
  forward_lite_upsample_nearest__model_13_Resize_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_13_Resize_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_14_Concat_output_0 */
  {
    
  forward_lite_concat__model_14_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_14_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_15_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 168192);
    const ai_i8* _model_15_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 204228);
    const ai_i32* _model_15_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 207300);
    ai_i8* _model_15_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 72128);
    ai_i16* _model_15_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 71424);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(489, 1, {(stai_ptr) _model_15_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_15_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_15_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_15_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_15_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_15_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_15_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_15_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_15_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_15_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_15_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_15_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_15_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_15_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_15_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_15_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_15_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 704, _model_15_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(489, 1, {(stai_ptr) _model_15_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_15_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_15_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_15_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_Split_output_0 */
  {
    
  forward_lite_split__model_15_Split_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_Split_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_m_0_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 80640);
    ai_ptr _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 103680);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(503, 1, {(stai_ptr) _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_15_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(416), (ai_i32)(416), (ai_i32)(16), (ai_i32)(16));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(503, 1, {(stai_ptr) _model_15_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_15_m_0_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_15_m_0_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 103680);
    const ai_i8* _model_15_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 207432);
    const ai_i32* _model_15_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 209736);
    ai_i8* _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 131328);
    ai_i16* _model_15_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 114496);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(503, 1, {(stai_ptr) _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_15_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_l_pad_W_0_const_s32, _model_15_m_0_cv1_conv_Conv_output_0_l_pad_H_0_const_s32, _model_15_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_15_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_15_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_15_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 5408, _model_15_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(503, 1, {(stai_ptr) _model_15_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_15_m_0_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_m_0_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_15_m_0_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_m_0_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_m_0_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_15_m_0_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_m_0_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_m_0_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 112896);
    ai_ptr _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 131328);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(512, 1, {(stai_ptr) _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_15_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(416), (ai_i32)(416), (ai_i32)(16), (ai_i32)(16));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(512, 1, {(stai_ptr) _model_15_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_15_m_0_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_15_m_0_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 131328);
    const ai_i8* _model_15_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 209800);
    const ai_i32* _model_15_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 212104);
    ai_i8* _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 109088);
    ai_i16* _model_15_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 103680);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(512, 1, {(stai_ptr) _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_sssa8_ch(_model_15_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_l_pad_W_0_const_s32, _model_15_m_0_cv2_conv_Conv_output_0_l_pad_H_0_const_s32, _model_15_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_15_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_15_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_15_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 5408, _model_15_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(512, 1, {(stai_ptr) _model_15_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_15_m_0_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_m_0_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_15_m_0_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_m_0_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_m_0_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_15_m_0_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_m_0_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_Concat_output_0 */
  {
    
  forward_lite_concat__model_15_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_15_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 149760);
    const ai_i8* _model_15_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 212168);
    const ai_i32* _model_15_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 213704);
    ai_i8* _model_15_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 103680);
    ai_i16* _model_15_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 89856);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(524, 1, {(stai_ptr) _model_15_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_15_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_15_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_15_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_15_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_15_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_15_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_15_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_15_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_15_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_15_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_15_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_15_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_15_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_15_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_15_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_15_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 512, _model_15_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(524, 1, {(stai_ptr) _model_15_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_15_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_15_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_15_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_15_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_15_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_16_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_16_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 149760);
    ai_ptr _model_16_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 168192);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(533, 1, {(stai_ptr) _model_16_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_16_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_16_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_16_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_16_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_16_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(832), (ai_i32)(832), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(533, 1, {(stai_ptr) _model_16_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_16_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_16_conv_Conv_output_0 */
  {
      const ai_i8* _model_16_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 168192);
    const ai_i8* _model_16_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 213832);
    const ai_i32* _model_16_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 223048);
    ai_i8* _model_16_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 96768);
    ai_i16* _model_16_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 103680);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(533, 1, {(stai_ptr) _model_16_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_model_16_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_16_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_16_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_16_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_16_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_16_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_16_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_16_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_16_conv_Conv_output_0_l_stride_1_const_u16, _model_16_conv_Conv_output_0_l_stride_0_const_u16, _model_16_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_16_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_16_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_16_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_16_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_16_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_16_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_16_conv_Conv_output_0_t_out_0_ptr_s8, _model_16_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_16_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 6720, _model_16_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(533, 1, {(stai_ptr) _model_16_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_16_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_16_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_16_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_16_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_16_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_16_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_16_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_17_Concat_output_0 */
  {
    
  forward_lite_concat__model_17_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_17_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_18_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 131328);
    const ai_i8* _model_18_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 223176);
    const ai_i32* _model_18_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 229320);
    ai_i8* _model_18_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 103680);
    ai_i16* _model_18_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 89856);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(569, 1, {(stai_ptr) _model_18_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_18_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_18_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_18_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_18_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_18_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_18_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_18_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_18_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_18_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_18_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_18_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_18_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_18_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_18_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_18_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_18_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1024, _model_18_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(569, 1, {(stai_ptr) _model_18_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_18_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_18_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_18_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_Split_output_0 */
  {
    
  forward_lite_split__model_18_Split_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_Split_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_m_0_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 103680);
    ai_ptr _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 108288);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(607, 1, {(stai_ptr) _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_18_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(607, 1, {(stai_ptr) _model_18_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_18_m_0_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_18_m_0_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 108288);
    const ai_i8* _model_18_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 229580);
    const ai_i32* _model_18_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 238796);
    ai_i8* _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 121280);
    ai_i16* _model_18_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 114560);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(607, 1, {(stai_ptr) _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_18_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_18_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_18_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_18_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_18_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_18_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_18_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(607, 1, {(stai_ptr) _model_18_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_18_m_0_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_m_0_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_18_m_0_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_m_0_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_m_0_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_18_m_0_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_m_0_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_m_0_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 112896);
    ai_ptr _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 117504);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(616, 1, {(stai_ptr) _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_18_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(616, 1, {(stai_ptr) _model_18_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_18_m_0_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_18_m_0_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 117504);
    const ai_i8* _model_18_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 238924);
    const ai_i32* _model_18_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 248140);
    ai_i8* _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 123776);
    ai_i16* _model_18_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 108288);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(616, 1, {(stai_ptr) _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_18_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_18_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_18_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_18_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_18_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_18_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_18_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(616, 1, {(stai_ptr) _model_18_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_18_m_0_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_m_0_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_18_m_0_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_m_0_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_m_0_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_18_m_0_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_m_0_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_Concat_output_0 */
  {
    
  forward_lite_concat__model_18_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_18_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 117504);
    const ai_i8* _model_18_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 248268);
    const ai_i32* _model_18_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 254412);
    ai_i8* _model_18_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 108288);
    ai_i16* _model_18_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 89856);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(628, 1, {(stai_ptr) _model_18_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_18_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_18_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_18_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_18_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_18_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_18_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_18_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_18_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_18_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_18_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_18_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_18_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_18_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_18_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_18_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_18_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1024, _model_18_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(628, 1, {(stai_ptr) _model_18_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_18_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_18_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_18_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_18_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_18_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_19_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_19_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 126720);
    ai_ptr _model_19_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 108288);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(637, 1, {(stai_ptr) _model_19_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_19_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_19_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_19_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_19_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_19_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(896), (ai_i32)(896), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(637, 1, {(stai_ptr) _model_19_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_19_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_19_conv_Conv_output_0 */
  {
      const ai_i8* _model_19_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 108288);
    const ai_i8* _model_19_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 254668);
    const ai_i32* _model_19_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 291532);
    ai_i8* _model_19_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 89856);
    ai_i16* _model_19_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 135936);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(637, 1, {(stai_ptr) _model_19_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_sssa8_ch(_model_19_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_19_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_19_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_19_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_19_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_19_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_19_conv_Conv_output_0_t_weight_0_shape_w_const_u16, _model_19_conv_Conv_output_0_t_weight_0_shape_h_const_u16, _model_19_conv_Conv_output_0_l_stride_1_const_u16, _model_19_conv_Conv_output_0_l_stride_0_const_u16, _model_19_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_19_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_19_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_19_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_19_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_19_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_19_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_19_conv_Conv_output_0_t_out_0_ptr_s8, _model_19_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_19_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 1, 8320, _model_19_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(637, 1, {(stai_ptr) _model_19_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_19_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_19_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_19_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_19_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_19_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_19_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_19_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_20_Concat_output_0 */
  {
    
  forward_lite_concat__model_20_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_20_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_21_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    const ai_i8* _model_21_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 291788);
    const ai_i32* _model_21_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 299980);
    ai_i8* _model_21_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    ai_i16* _model_21_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 89856);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(673, 1, {(stai_ptr) _model_21_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_21_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_21_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_21_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_21_cv1_conv_Conv_output_0_l_stride_1_const_u16, _model_21_cv1_conv_Conv_output_0_l_stride_0_const_u16, _model_21_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_21_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_21_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_21_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_21_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_21_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_21_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_21_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_21_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_21_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_21_cv1_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1152, _model_21_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(673, 1, {(stai_ptr) _model_21_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_21_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_21_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_21_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_Split_output_0 */
  {
    
  forward_lite_split__model_21_Split_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_Split_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_m_0_cv1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 91008);
    ai_ptr _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 101376);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(711, 1, {(stai_ptr) _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_21_m_0_cv1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(192), (ai_i32)(256), (ai_i32)(256), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(711, 1, {(stai_ptr) _model_21_m_0_cv1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_21_m_0_cv1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_21_m_0_cv1_conv_Conv_output_0 */
  {
      const ai_i8* _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    const ai_i8* _model_21_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 300240);
    const ai_i32* _model_21_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 309456);
    ai_i8* _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 115008);
    ai_i16* _model_21_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 108288);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(711, 1, {(stai_ptr) _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_21_m_0_cv1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_21_m_0_cv1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_21_m_0_cv1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_21_m_0_cv1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_21_m_0_cv1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_21_m_0_cv1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8, _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_21_m_0_cv1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(711, 1, {(stai_ptr) _model_21_m_0_cv1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_21_m_0_cv1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_m_0_cv1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_21_m_0_cv1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_m_0_cv1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_m_0_cv1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_21_m_0_cv1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_m_0_cv1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_m_0_cv2_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 102528);
    ai_ptr _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 108288);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(720, 1, {(stai_ptr) _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_21_m_0_cv2_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(192), (ai_i32)(256), (ai_i32)(256), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(720, 1, {(stai_ptr) _model_21_m_0_cv2_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_21_m_0_cv2_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_21_m_0_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 108288);
    const ai_i8* _model_21_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 309584);
    const ai_i32* _model_21_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 318800);
    ai_i8* _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    ai_i16* _model_21_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 110336);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(720, 1, {(stai_ptr) _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_21_m_0_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_21_m_0_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_21_m_0_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_21_m_0_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_21_m_0_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_21_m_0_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8, _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_21_m_0_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(720, 1, {(stai_ptr) _model_21_m_0_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_21_m_0_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_m_0_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_21_m_0_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_m_0_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_m_0_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_21_m_0_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_m_0_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_Concat_output_0 */
  {
    
  forward_lite_concat__model_21_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_cv2_conv_Conv_output_0 */
  {
      const ai_i8* _model_21_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 109440);
    const ai_i8* _model_21_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 318928);
    const ai_i32* _model_21_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 325072);
    ai_i8* _model_21_cv2_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    ai_i16* _model_21_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 101376);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(732, 1, {(stai_ptr) _model_21_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_21_cv2_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_21_cv2_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_21_cv2_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_21_cv2_conv_Conv_output_0_l_stride_1_const_u16, _model_21_cv2_conv_Conv_output_0_l_stride_0_const_u16, _model_21_cv2_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_21_cv2_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_21_cv2_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_21_cv2_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_21_cv2_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_21_cv2_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_21_cv2_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_21_cv2_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_21_cv2_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_21_cv2_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_21_cv2_conv_Conv_output_0_t_out_0_ptr_s8, 1, 1024, _model_21_cv2_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(732, 1, {(stai_ptr) _model_21_cv2_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_21_cv2_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_cv2_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_21_cv2_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_cv2_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_21_cv2_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_21_cv2_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_21_cv2_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 108288);
    ai_ptr _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 110592);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(741, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(512), (ai_i32)(512), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(741, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_0_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 110592);
    const ai_i8* _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 325328);
    const ai_i32* _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 362192);
    ai_i8* _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    ai_i16* _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 114688);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(741, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 8320, _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(741, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_0_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_0_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_0_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_0_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv2_2_cv2_2_0_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_0_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 112896);
    ai_ptr _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 115200);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(759, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(512), (ai_i32)(512), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(759, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_1_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 115200);
    const ai_i8* _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 362448);
    const ai_i32* _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 399312);
    ai_i8* _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    ai_i16* _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 135936);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(759, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 8320, _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(759, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv2_2_cv2_2_1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_2_cv2_2_2_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    const ai_i8* _model_22_cv2_2_cv2_2_2_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 399568);
    const ai_i32* _model_22_cv2_2_cv2_2_2_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 403664);
    ai_i8* _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 110592);
    ai_i16* _model_22_cv2_2_cv2_2_2_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 101376);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(777, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_2_cv2_2_2_Conv_output_0_l_stride_1_const_u16, _model_22_cv2_2_cv2_2_2_Conv_output_0_l_stride_0_const_u16, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_2_cv2_2_2_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_ptr_s8, 1, 896, _model_22_cv2_2_cv2_2_2_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(777, 1, {(stai_ptr) _model_22_cv2_2_cv2_2_2_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_2_cv2_2_2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 108288);
    ai_ptr _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 112896);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(742, 1, {(stai_ptr) _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(512), (ai_i32)(512), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(742, 1, {(stai_ptr) _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_0_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    const ai_i8* _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 403920);
    const ai_i32* _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 422352);
    ai_i8* _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    ai_i16* _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 116992);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(742, 1, {(stai_ptr) _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 7872, _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(742, 1, {(stai_ptr) _model_22_cv3_2_cv3_2_0_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_0_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_0_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_0_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv3_2_cv3_2_0_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_0_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 108288);
    ai_ptr _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 101376);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(760, 1, {(stai_ptr) _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(192), (ai_i32)(256), (ai_i32)(256), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(760, 1, {(stai_ptr) _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_1_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 101376);
    const ai_i8* _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 422480);
    const ai_i32* _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 431696);
    ai_i8* _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 108288);
    ai_i16* _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 112896);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(760, 1, {(stai_ptr) _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(760, 1, {(stai_ptr) _model_22_cv3_2_cv3_2_1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv3_2_cv3_2_1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_2_cv3_2_2_Conv_output_0 */
  {
    
  forward_lite_conv2d_integer_SSSA__model_22_cv3_2_cv3_2_2_Conv_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_2_cv3_2_2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 126720);
    ai_ptr _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 112896);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(638, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(896), (ai_i32)(896), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(638, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_0_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    const ai_i8* _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 431860);
    const ai_i32* _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 468724);
    ai_i8* _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 168192);
    ai_i16* _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 135936);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(638, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 8320, _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(638, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_0_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_0_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_0_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_0_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv2_1_cv2_1_0_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_0_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 135936);
    ai_ptr _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 112896);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(665, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(896), (ai_i32)(896), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(665, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_1_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    const ai_i8* _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 468980);
    const ai_i32* _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 505844);
    ai_i8* _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 168192);
    ai_i16* _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 135936);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(665, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 8320, _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(665, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv2_1_cv2_1_1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_1_cv2_1_2_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 135936);
    const ai_i8* _model_22_cv2_1_cv2_1_2_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 506100);
    const ai_i32* _model_22_cv2_1_cv2_1_2_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 510196);
    ai_i8* _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 112896);
    ai_i16* _model_22_cv2_1_cv2_1_2_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 101540);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(692, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_1_cv2_1_2_Conv_output_0_l_stride_1_const_u16, _model_22_cv2_1_cv2_1_2_Conv_output_0_l_stride_0_const_u16, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_1_cv2_1_2_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_ptr_s8, 1, 896, _model_22_cv2_1_cv2_1_2_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(692, 1, {(stai_ptr) _model_22_cv2_1_cv2_1_2_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_1_cv2_1_2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 126720);
    ai_ptr _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 135936);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(639, 1, {(stai_ptr) _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(896), (ai_i32)(896), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(639, 1, {(stai_ptr) _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_0_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 135936);
    const ai_i8* _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 510452);
    const ai_i32* _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 528884);
    ai_i8* _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 129984);
    ai_i16* _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 122112);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(639, 1, {(stai_ptr) _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 7872, _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(639, 1, {(stai_ptr) _model_22_cv3_1_cv3_1_0_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_0_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_0_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_0_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv3_1_cv3_1_0_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_0_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 134592);
    ai_ptr _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 122112);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(666, 1, {(stai_ptr) _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(384), (ai_i32)(448), (ai_i32)(448), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(666, 1, {(stai_ptr) _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_1_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 122112);
    const ai_i8* _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 529012);
    const ai_i32* _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 538228);
    ai_i8* _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 135104);
    ai_i16* _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 128384);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(666, 1, {(stai_ptr) _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(666, 1, {(stai_ptr) _model_22_cv3_1_cv3_1_1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv3_1_cv3_1_1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_1_cv3_1_2_Conv_output_0 */
  {
    
  forward_lite_conv2d_integer_SSSA__model_22_cv3_1_cv3_1_2_Conv_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_1_cv3_1_2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 149760);
    ai_ptr _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 122112);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(534, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(832), (ai_i32)(832), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(534, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_0_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 122112);
    const ai_i8* _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 538392);
    const ai_i32* _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 556824);
    ai_i8* _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 175360);
    ai_i16* _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 168192);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(534, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 7168, _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(534, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_0_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_0_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_0_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_0_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv2_0_cv2_0_0_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_0_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 175360);
    ai_ptr _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 168960);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(561, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(1536), (ai_i32)(1664), (ai_i32)(1664), (ai_i32)(64), (ai_i32)(64));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(561, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_1_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 168960);
    const ai_i8* _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 557080);
    const ai_i32* _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 593944);
    ai_i8* _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 212224);
    ai_i16* _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 122112);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(561, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 8320, _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(561, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv2_0_cv2_0_1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv2_0_cv2_0_2_Conv_output_0 */
  {
      const ai_i8* _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 212224);
    const ai_i8* _model_22_cv2_0_cv2_0_2_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 594200);
    const ai_i32* _model_22_cv2_0_cv2_0_2_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 598296);
    ai_i8* _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 168192);
    ai_i16* _model_22_cv2_0_cv2_0_2_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 101684);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(588, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_pw_sssa8_ch(_model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv2_0_cv2_0_2_Conv_output_0_l_stride_1_const_u16, _model_22_cv2_0_cv2_0_2_Conv_output_0_l_stride_0_const_u16, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv2_0_cv2_0_2_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_ptr_s8, 1, 896, _model_22_cv2_0_cv2_0_2_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(588, 1, {(stai_ptr) _model_22_cv2_0_cv2_0_2_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv2_0_cv2_0_2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Concat_output_0 */
  {
    
  forward_lite_concat__model_22_Concat_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Concat_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_dfl_Reshape_output_0_to_chlast */
  {
    
  forward_lite_transpose__model_22_dfl_Reshape_output_0_to_chlast(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_dfl_Reshape_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _model_22_dfl_Transpose_output_0 */
  {
    
  forward_lite_transpose__model_22_dfl_Transpose_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_dfl_Transpose_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion */
  {
      const ai_i8* _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 168192);
    ai_float* _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_out_0_ptr_f32 = (ai_float*)(net_ctx->_activations[0] + 216576);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(801, 1, {(stai_ptr) _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_in_0_ptr_const_s8});
    
  forward_lite_node_convert_integer_is8of32(_model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_in_0_ptr_const_s8, _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_out_0_ptr_f32, _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_in_0_fmt_scale_const_f32, _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_in_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(801, 1, {(stai_ptr) _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion_t_out_0_ptr_f32});
  }
  /* LITE_KERNEL_SECTION END _model_22_dfl_Transpose_output_0_0_0__model_22_dfl_Softmax_output_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _model_22_dfl_Softmax_output_0 */
  {
      ai_handle _model_22_dfl_Softmax_output_0_t_out_0_ptr_handle = (ai_handle)(net_ctx->_activations[0] + 216576);
    const ai_handle _model_22_dfl_Softmax_output_0_t_in_0_ptr_const_handle = (ai_handle)(net_ctx->_activations[0] + 216576);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(804, 1, {(stai_ptr) _model_22_dfl_Softmax_output_0_t_in_0_ptr_const_handle});
    
  forward_lite_nl_softmax_if32of32(_model_22_dfl_Softmax_output_0_t_out_0_ptr_handle, _model_22_dfl_Softmax_output_0_t_in_0_ptr_const_handle, _model_22_dfl_Softmax_output_0_t_in_0_shape_ch_h_w_prod_const_s32, 1, 16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(804, 1, {(stai_ptr) _model_22_dfl_Softmax_output_0_t_out_0_ptr_handle});
  }
  /* LITE_KERNEL_SECTION END _model_22_dfl_Softmax_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion */
  {
      const ai_float* _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_in_0_ptr_const_f32 = (ai_float*)(net_ctx->_activations[0] + 216576);
    ai_i8* _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 168192);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(804, 1, {(stai_ptr) _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_in_0_ptr_const_f32});
    
  forward_lite_node_convert_integer_if32os8(_model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_in_0_ptr_const_f32, _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_ptr_s8, _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_shape_h_w_ch_d_prod_const_u32, _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_fmt_scale_const_f32, _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_fmt_zero_const_s8);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(804, 1, {(stai_ptr) _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_dfl_Softmax_output_0_0_0__model_22_dfl_conv_Conv_output_0_conversion */
  /* LITE_KERNEL_SECTION BEGIN _model_22_dfl_conv_Conv_output_0 */
  {
    
  forward_lite_conv2d_integer_SSSA__model_22_dfl_conv_Conv_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_dfl_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_dfl_Reshape_1_output_0_to_chlast */
  {
    
  forward_lite_transpose__model_22_dfl_Reshape_1_output_0_to_chlast(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_dfl_Reshape_1_output_0_to_chlast */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Slice_1_output_0 */
  {
    
  forward_lite_slice__model_22_Slice_1_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Slice_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Add_1_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_Add_1_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Add_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Slice_output_0 */
  {
    
  forward_lite_slice__model_22_Slice_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Slice_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Sub_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_Sub_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Sub_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Sub_1_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_Sub_1_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Sub_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Add_2_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_Add_2_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Add_2_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Div_1_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_Div_1_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Div_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Concat_2_output_0 */
  {
    
  forward_lite_concat__model_22_Concat_2_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Concat_2_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Mul_2_output_0_QuantizeLinear_Input */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_Mul_2_output_0_QuantizeLinear_Input(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Mul_2_output_0_QuantizeLinear_Input */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0 */
  {
    
  forward_lite_transpose__model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Mul_2_output_0_QuantizeLinear_Input_Transpose_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 149760);
    ai_ptr _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 111312);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(535, 1, {(stai_ptr) _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(832), (ai_i32)(832), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(535, 1, {(stai_ptr) _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_0_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 111312);
    const ai_i8* _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 598572);
    const ai_i32* _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 607788);
    ai_i8* _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 139664);
    ai_i16* _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 132944);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(535, 1, {(stai_ptr) _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(535, 1, {(stai_ptr) _model_22_cv3_0_cv3_0_0_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_0_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_0_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_0_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv3_0_cv3_0_0_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_0_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before */
  {
      const ai_ptr _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr = (ai_ptr)(net_ctx->_activations[0] + 158096);
    ai_ptr _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr = (ai_ptr)(net_ctx->_activations[0] + 111312);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(562, 1, {(stai_ptr) _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr});
    
  forward_lite_pad_constant(_model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_in_0_ptr_const_ptr, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr, (ai_handle)(_model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_v_pad_constant_value_const_s8), _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_in_0_fmt_bitsize_const_s16, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_in_0_shape_h_const_u32, (ai_i32)(1), (ai_i32)(768), (ai_i32)(832), (ai_i32)(832), (ai_i32)(32), (ai_i32)(32));
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(562, 1, {(stai_ptr) _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before_t_out_0_ptr_ptr});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_pad_before */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_1_conv_Conv_output_0 */
  {
      const ai_i8* _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_ptr_const_s8 = (ai_i8*)(net_ctx->_activations[0] + 111312);
    const ai_i8* _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_weight_0_ptr_const_s8 = (ai_i8*)(net_ctx->_weights[0] + 607916);
    const ai_i32* _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_weight_1_ptr_const_s32 = (ai_i32*)(net_ctx->_weights[0] + 617132);
    ai_i8* _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_ptr_s8 = (ai_i8*)(net_ctx->_activations[0] + 139664);
    ai_i16* _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_scratch_0_ptr_s16 = (ai_i16*)(net_ctx->_activations[0] + 132944);
  
  _STAI_NETWORK_EVENT_NODE_START_CB(562, 1, {(stai_ptr) _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_ptr_const_s8});
    
  forward_lite_conv2d_deep_3x3_sssa8_ch(_model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_ptr_const_s8, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_shape_w_const_u16, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_shape_h_const_u16, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_shape_ch_const_u16, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_weight_0_ptr_const_s8, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_shape_ch_const_u16, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_weight_1_ptr_const_s32, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_fmt_zero_const_s8, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_fmt_zero_const_s8, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_in_0_fmt_scale_const_f32, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_fmt_scale_const_f32, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_weight_0_fmt_scale_const_f32, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_l_out_ch_format_const_layer_format_type, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_ptr_s8, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_shape_w_const_u16, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_shape_h_const_u16, 1, 6720, _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_scratch_0_ptr_s16);
    
  _STAI_NETWORK_EVENT_NODE_STOP_CB(562, 1, {(stai_ptr) _model_22_cv3_0_cv3_0_1_conv_Conv_output_0_t_out_0_ptr_s8});
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_1_conv_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0 */
  {
    
  forward_lite_nl_integer__model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_1_act_Sigmoid_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_1_act_Mul_output_0 */
  {
    
  forward_lite_eltwise_integer_INT8__model_22_cv3_0_cv3_0_1_act_Mul_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_1_act_Mul_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_cv3_0_cv3_0_2_Conv_output_0 */
  {
    
  forward_lite_conv2d_integer_SSSA__model_22_cv3_0_cv3_0_2_Conv_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_cv3_0_cv3_0_2_Conv_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Concat_1_output_0 */
  {
    
  forward_lite_concat__model_22_Concat_1_output_0(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Concat_1_output_0 */
  /* LITE_KERNEL_SECTION BEGIN _model_22_Sigmoid_output_0_QuantizeLinear_Input */
  {
    
  forward_lite_nl_integer__model_22_Sigmoid_output_0_QuantizeLinear_Input(net_ctx);
  }
  /* LITE_KERNEL_SECTION END _model_22_Sigmoid_output_0_QuantizeLinear_Input */
  return net_ctx->_return_code;
}

/*****************************************************************************/
/*  Getters APIs Section  */
STAI_API_ENTRY
stai_size stai_network_get_context_size()
{
  return (stai_size)STAI_NETWORK_CONTEXT_SIZE;
}

#if defined(HAVE_NETWORK_INFO)
STAI_API_ENTRY
stai_return_code stai_network_get_info(
  stai_network* network,
  stai_network_info* info)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, info==NULL, STAI_ERROR_NETWORK_INVALID_INFO, net_ctx->_return_code)

  // Copy of network info struct
  *info = g_network_info;

  return STAI_SUCCESS;
}
#endif


STAI_API_ENTRY
stai_return_code stai_network_get_activations(
  stai_network* network, stai_ptr* activations, stai_size* n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  _STAI_SET_ERROR(net_ctx, !n_activations, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_activations = STAI_NETWORK_ACTIVATIONS_NUM;
for (stai_size idx=0; activations && (idx<STAI_NETWORK_ACTIVATIONS_NUM); idx++) {
    // get address of the activations buffers
    activations[idx] = net_ctx->_activations[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_weights(
  stai_network* network, stai_ptr* weights, stai_size* n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_weights, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_weights = STAI_NETWORK_WEIGHTS_NUM;
for (stai_size idx=0; weights && (idx<STAI_NETWORK_WEIGHTS_NUM); idx++) {
    // get address of the weights buffers
    weights[idx] = net_ctx->_weights[idx];
  }return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_inputs(
  stai_network* network, stai_ptr* inputs, stai_size* n_inputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_inputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_inputs = STAI_NETWORK_IN_NUM;
  for (stai_size idx=0; inputs && (idx<STAI_NETWORK_IN_NUM); idx++) {
    inputs[idx] = net_ctx->_inputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_outputs(
  stai_network* network, stai_ptr* outputs, stai_size* n_outputs)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_outputs, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  *n_outputs = STAI_NETWORK_OUT_NUM;
  for (stai_size idx=0; outputs && (idx<STAI_NETWORK_OUT_NUM); idx++) {
    outputs[idx] = net_ctx->_outputs[idx];
  }
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_error(
  stai_network* network)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  /* return 1st generated error or STAI_SUCCESS if no errors so far */
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_get_states(
  stai_network* network, stai_ptr* states, stai_size* n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !n_states, STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  /* get the number of internals states (supporting multi-heap also for internal states) */
  *n_states = STAI_NETWORK_STATES_NUM;

  STAI_UNUSED(states)
return net_ctx->_return_code;
}


/*****************************************************************************/
/*  Setters APIs Section  */

STAI_API_ENTRY
stai_return_code stai_network_set_activations(
  stai_network* network,
  const stai_ptr* activations,
  const stai_size n_activations)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _activations_alignment[] = STAI_NETWORK_ACTIVATIONS_ALIGNMENTS;
  STAI_PRINT("  [stai_network_set_activations] network(%p) activations[%d]: %p\n\n", net_ctx, n_activations, activations)
  _STAI_SET_ERROR(net_ctx, !activations,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_activations!=STAI_NETWORK_ACTIVATIONS_NUM,
                  STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_NUM, net_ctx->_return_code)

  for (stai_size idx=0; activations && idx<STAI_NETWORK_ACTIVATIONS_NUM; idx++) {
    STAI_PRINT("  activation[%d]: %p\n", idx, activations[idx])
    _STAI_SET_ERROR(net_ctx, activations[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_ACTIVATIONS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)activations[idx]) & (_activations_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_activations[idx] = activations[idx];
  }
  net_ctx->_inputs[0] = activations[0] + 37640;

  net_ctx->_outputs[0] = activations[0] + 108288;

  net_ctx->_outputs[1] = activations[0] + 101376;
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_weights(
  stai_network* network,
  const stai_ptr* weights,
  const stai_size n_weights)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
const uintptr_t _weights_alignment[] = STAI_NETWORK_WEIGHTS_ALIGNMENTS;
  _STAI_SET_ERROR(net_ctx, !weights,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_weights!=STAI_NETWORK_WEIGHTS_NUM,
                  STAI_ERROR_NETWORK_INVALID_WEIGHTS_NUM, net_ctx->_return_code)
  for (stai_size idx=0; weights && idx<STAI_NETWORK_WEIGHTS_NUM; idx++) {
    STAI_PRINT("  weight[%d]: %p\n", idx, weights[idx])
    _STAI_SET_ERROR(net_ctx, weights[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_WEIGHTS_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)weights[idx]) & (_weights_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_weights[idx] = weights[idx];
  }_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_inputs(
  stai_network* network,
  const stai_ptr* inputs,
  const stai_size n_inputs)
{
  const uintptr_t _inputs_alignment[] = STAI_NETWORK_IN_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !inputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_inputs!=STAI_NETWORK_IN_NUM,
                  STAI_ERROR_NETWORK_INVALID_IN_NUM, net_ctx->_return_code)

  for (stai_size idx=0; inputs && idx<STAI_NETWORK_IN_NUM; idx++) {
    STAI_PRINT("  input[%d]: %p\n", idx, inputs[idx])
    _STAI_SET_ERROR(net_ctx, inputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_IN_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)inputs[idx]) & (_inputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_inputs[idx] = inputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_outputs(
  stai_network* network,
  const stai_ptr* outputs,
  const stai_size n_outputs)
{
  const uintptr_t _outputs_alignment[] = STAI_NETWORK_OUT_ALIGNMENTS;
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  _STAI_SET_ERROR(net_ctx, !outputs,
                  STAI_ERROR_NETWORK_INVALID_API_ARGUMENTS, net_ctx->_return_code)
  _STAI_SET_ERROR(net_ctx, n_outputs!=STAI_NETWORK_OUT_NUM,
                  STAI_ERROR_NETWORK_INVALID_OUT_NUM, net_ctx->_return_code)

  for (stai_size idx=0; outputs && idx<n_outputs; idx++) {
    STAI_PRINT("  output[%d]: %p\n", idx, outputs[idx])
    _STAI_SET_ERROR(net_ctx, outputs[idx]==NULL,
                    STAI_ERROR_NETWORK_INVALID_OUT_PTR, net_ctx->_return_code)
    _STAI_SET_ERROR(net_ctx, ((uintptr_t)outputs[idx]) & (_outputs_alignment[idx]-1),
                    STAI_ERROR_INVALID_BUFFER_ALIGNMENT, net_ctx->_return_code)
    net_ctx->_outputs[idx] = outputs[idx];
  }

  _stai_network_check(net_ctx);
  return net_ctx->_return_code;
}


STAI_API_ENTRY
stai_return_code stai_network_set_states(
  stai_network* network,
  const stai_ptr* states,
  const stai_size n_states)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)

  STAI_UNUSED(states)
  STAI_UNUSED(n_states)
_stai_network_check(net_ctx);
  return net_ctx->_return_code;
}

STAI_API_ENTRY
stai_return_code stai_network_set_callback(
  stai_network* network, const stai_event_cb cb, void* cb_cookie)
{
  _STAI_CONTEXT_ACQUIRE(net_ctx, network)
  STAI_PRINT("  set_callback %p cb %p cookie %p\n", net_ctx, cb, cb_cookie)
  // _STAI_SET_ERROR(net_ctx, cb==NULL, STAI_ERROR_NETWORK_INVALID_CALLBACK, net_ctx->_return_code)
  net_ctx->_callback = cb;
  net_ctx->_callback_cookie = cb_cookie;
  return net_ctx->_return_code;
}

#undef _STAI_SET_ERROR
#undef _STAI_CONTEXT_ALIGNMENT
#undef _STAI_CONTEXT_ACQUIRE
#undef _STAI_NETWORK_EVENT_NODE_START_CB
#undef _STAI_NETWORK_EVENT_NODE_STOP_CB
#undef _STAI_NETWORK_MODEL_SIGNATURE
#undef _STAI_NETWORK_DATETIME
#undef _STAI_NETWORK_COMPILE_DATETIME

