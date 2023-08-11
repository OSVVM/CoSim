// =========================================================================
//
//  File Name:         OsvvmVProc.h
//  Design Unit Name:
//  Revision:          OSVVM MODELS STANDARD VERSION
//
//  Maintainer:        Simon Southwell email:  simon.southwell@gmail.com
//  Contributor(s):
//     Simon Southwell      simon.southwell@gmail.com
//
//
//  Description:
//      Internal header for co-simulation virtual procedure definitions
//      and data types
//
//  Revision History:
//    Date      Version    Description
//    05/2023   2023.05    Adding asynchronous transaction support
//    03/2023   2023.04    Adding basic stream support
//    01/2023   2023.01    Initial revision
//
//
//  This file is part of OSVVM.
//
//  Copyright (c) 2023 by [OSVVM Authors](../AUTHORS.md)
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.
//
// =========================================================================

#ifndef _OSVVM_VPROC_H_
#define _OSVVM_VPROC_H_

// -------------------------------------------------------------------------
// INCLUDES
// -------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

# if !defined(ALDEC) || !defined(_WIN32)
#  ifndef __USE_GNU
#  define __USE_GNU
#  include <dlfcn.h>
#  undef __USE_GNU
#  else
#  include <dlfcn.h>
#  endif
# endif

#include <pthread.h>
#include <sched.h>

#include <semaphore.h>

// For file IO
#include <fcntl.h>

// For inode manipulation
#include <unistd.h>

// -------------------------------------------------------------------------
// DEFINES AND MACROS
// -------------------------------------------------------------------------

#ifndef VP_MAX_NODES
#define VP_MAX_NODES            64
#endif

#define VP_EXIT_OK              0
#define VP_QUEUE_ERR            1
#define VP_KEY_ERR              2
#define VP_USER_ERR             3
#define VP_SYSCALL_ERR          4


#define DEFAULT_STR_BUF_SIZE    32
#define DATABUF_SIZE            4096

// -------------------------------------------------------------------------
// TYPEDEFS
// -------------------------------------------------------------------------

typedef enum trans_type_e
{
    trans32_byte  = 0,
    trans32_hword,
    trans32_word,
    trans32_dword,
    trans32_qword,
    trans32_burst,
    trans64_byte,
    trans64_hword,
    trans64_word,
    trans64_dword,
    trans64_qword,
    trans64_burst,

    stream_snd_byte,
    stream_snd_hword,
    stream_snd_word,
    stream_snd_dword,
    stream_snd_qword,
    stream_snd_burst,
    stream_get_byte,
    stream_get_hword,
    stream_get_word,
    stream_get_dword,
    stream_get_qword,
    stream_get_burst,

    trans_idle

} trans_type_e;

typedef enum addr_bus_trans_op_e
{
    NOT_DRIVEN = 0,
    WAIT_FOR_CLOCK,
    WAIT_FOR_TRANSACTION,
    WAIT_FOR_WRITE_TRANSACTION,
    WAIT_FOR_READ_TRANSACTION,
    GET_TRANSACTION_COUNT,
    GET_WRITE_TRANSACTION_COUNT,
    GET_READ_TRANSACTION_COUNT,
    GET_ALERTLOG_ID,
    //SET_USE_RANDOM_DELAYS,
    //GET_USE_RANDOM_DELAYS,
    //SET_DELAYCOV_ID,
    //GET_DELAYCOV_ID,
    SET_BURST_MODE = 13,
    GET_BURST_MODE,
    SET_MODEL_OPTIONS,
    GET_MODEL_OPTIONS,
    INTERRUPT_RETURN,
    WRITE_OP,
    WRITE_ADDRESS,
    WRITE_DATA,
    ASYNC_WRITE,
    ASYNC_WRITE_ADDRESS,
    ASYNC_WRITE_DATA,
    READ_OP,
    READ_ADDRESS,
    READ_DATA,
    READ_CHECK,
    READ_DATA_CHECK,
    ASYNC_READ,
    ASYNC_READ_ADDRESS,
    ASYNC_READ_DATA,
    ASYNC_READ_DATA_CHECK,
    WRITE_AND_READ,
    ASYNC_WRITE_AND_READ,
    WRITE_BURST,
    ASYNC_WRITE_BURST,
    READ_BURST,
    MULTIPLE_DRIVER_DETECT,

    SET_TEST_NAME = 1024
} addr_bus_trans_op_t;

typedef enum stream_operation_e
{
    //NOT_DRIVEN = 0,
    //WAIT_FOR_CLOCK,
    //WAIT_FOR_TRANSACTION,
    STR_GET_TRANSACTION_COUNT = 3,
    //GET_ALERTLOG_ID,
    //SET_USE_RANDOM_DELAYS,
    //GET_USE_RANDOM_DELAYS,
    //SET_DELAYCOV_ID,
    //GET_DELAYCOV_ID,
    //SET_BURST_MODE,
    //GET_BURST_MODE,
    //GOT_BURST,
    //SET_MODEL_OPTIONS,
    //GET_MODEL_OPTIONS,
    SEND = 14,
    SEND_ASYNC,
    SEND_BURST,
    SEND_BURST_ASYNC,
    GET,
    TRY_GET,
    GET_BURST,
    TRY_GET_BURST,
    CHECK,
    TRY_CHECK,
    CHECK_BURST,
    TRY_CHECK_BURST,
    //MULTIPLE_DRIVER_DETECT,

    //SET_TEST_NAME = 1024
} stream_operation_t;

typedef enum burst_write_type_e
{
    BURST_NORM,
    BURST_INCR,
    BURST_RAND,
    BURST_INCR_PUSH,
    BURST_RAND_PUSH,
    BURST_INCR_CHECK,
    BURST_RAND_CHECK,
    BURST_TRANS,
    BURST_DATA,
    BURST_DATA_CHECK,
    BURST_FIFO_CHECK,
} burst_type_t;

typedef struct
{
    addr_bus_trans_op_t op;
    trans_type_e        type;
    uint32_t            prot;
    uint64_t            addr;
    uint8_t             data[16];
    int                 num_burst_bytes;
    uint8_t             databuf[DATABUF_SIZE];
    int                 param;
    int                 ticks;
    int                 done;
    int                 error;
} send_buf_t, *psend_buf_t;

typedef struct
{
    unsigned int        data_in;
    unsigned int        data_in_hi;
    unsigned int        addr_in;
    unsigned int        addr_in_hi;
    int                 num_burst_bytes;
    uint8_t             databuf[DATABUF_SIZE];
    int                 status;
    int                 count;
    int                 countsec;
    unsigned int        interrupt;
} rcv_buf_t, *prcv_buf_t;


// Shared object handle type
typedef void * handle_t;

// Interrupt function pointer type
typedef int  (*pVUserInt_t)      (int);

typedef struct
{
    sem_t               snd;
    sem_t               rcv;
    send_buf_t          send_buf;
    rcv_buf_t           rcv_buf;
    pVUserInt_t         VIntVecCB;
    unsigned int        last_int;
} SchedState_t, *pSchedState_t;

extern pSchedState_t ns[VP_MAX_NODES];

#endif
