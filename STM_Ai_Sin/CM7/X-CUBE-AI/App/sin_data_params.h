/**
  ******************************************************************************
  * @file    sin_data_params.h
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-04-16T23:02:37+0200
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */

#ifndef SIN_DATA_PARAMS_H
#define SIN_DATA_PARAMS_H

#include "ai_platform.h"

/*
#define AI_SIN_DATA_WEIGHTS_PARAMS \
  (AI_HANDLE_PTR(&ai_sin_data_weights_params[1]))
*/

#define AI_SIN_DATA_CONFIG               (NULL)


#define AI_SIN_DATA_ACTIVATIONS_SIZES \
  { 256, }
#define AI_SIN_DATA_ACTIVATIONS_SIZE     (256)
#define AI_SIN_DATA_ACTIVATIONS_COUNT    (1)
#define AI_SIN_DATA_ACTIVATION_1_SIZE    (256)



#define AI_SIN_DATA_WEIGHTS_SIZES \
  { 4612, }
#define AI_SIN_DATA_WEIGHTS_SIZE         (4612)
#define AI_SIN_DATA_WEIGHTS_COUNT        (1)
#define AI_SIN_DATA_WEIGHT_1_SIZE        (4612)



#define AI_SIN_DATA_ACTIVATIONS_TABLE_GET() \
  (&g_sin_activations_table[1])

extern ai_handle g_sin_activations_table[1 + 2];



#define AI_SIN_DATA_WEIGHTS_TABLE_GET() \
  (&g_sin_weights_table[1])

extern ai_handle g_sin_weights_table[1 + 2];


#endif    /* SIN_DATA_PARAMS_H */
