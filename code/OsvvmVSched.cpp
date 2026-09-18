// =========================================================================
//
//  File Name:         OsvvmSched.cpp
//  Design Unit Name:
//  Revision:          OSVVM MODELS STANDARD VERSION
//
//  Maintainer:        Simon Southwell email:  simon.southwell@gmail.com
//  Contributor(s):
//     Simon Southwell      simon.southwell@gmail.com
//
//
//  Description:
//      Simulator co-simulation virtual procedure C interface routines for
//      scheduling access to user processes
//
//  Revision History:
//    Date      Version    Description
//    07/2025   2026.01    Adding VIrqVec CoSim procedure
//                         More generic flag to use VHPI
//    05/2023   2023.05    Adding support for asynchronous transactions
//                         and address bus responder transactions
//    03/2023   2023.04    Adding basic stream support
//    01/2023   2023.01    Initial revision
//
//
//  This file is part of OSVVM.
//
//  Copyright (c) 2023 - 2025 by [OSVVM Authors](../AUTHORS.md)
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

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include "OsvvmVProc.h"
#include "OsvvmVUser.h"
#include "OsvvmVSchedPli.h"

// Pointers to state for each node (up to VP_MAX_NODES)
pSchedState_t ns[VP_MAX_NODES] = { NULL };

#if defined(USE_VHPI)

#include <vhpi_user.h>

#if defined (ALDEC) || defined (ACTIVEHDL)
#include <aldecpli.h>
#endif

// -------------------------------------------------------------------------
// Function setting up table of foreign procedure registration data
// and registering with VHPI/
// -------------------------------------------------------------------------

VPROC_RTN_TYPE reg_foreign_procs() {
    int idx;
    vhpiForeignDataT foreignDataArray[] = {
        {vhpiProcF, (char*)"VProc", (char*)"VInit",           NULL, VInit},
        {vhpiProcF, (char*)"VProc", (char*)"VIrqVec",         NULL, VIrqVec},
        {vhpiProcF, (char*)"VProc", (char*)"VUserValue",      NULL, VUserValue},
        {vhpiProcF, (char*)"VProc", (char*)"VTrans",          NULL, VTrans},
        {vhpiProcF, (char*)"VProc", (char*)"VSetBurstRdByte", NULL, VSetBurstRdByte},
        {vhpiProcF, (char*)"VProc", (char*)"VGetBurstWrByte", NULL, VGetBurstWrByte},
        {(vhpiForeignT) 0}
    };

    for (idx = 0; idx < ((sizeof(foreignDataArray)/sizeof(foreignDataArray[0]))-1); idx++)
    {
        vhpi_register_foreignf(&(foreignDataArray[idx]));
    }
}

// -------------------------------------------------------------------------
// List of user startup functions to call
// -------------------------------------------------------------------------
#ifdef WIN32
__declspec ( dllexport )
#endif

VPROC_RTN_TYPE (*vhpi_startup_routines[])() = {
    reg_foreign_procs,
    0L
};

// -------------------------------------------------------------------------
// getVhpiParams()
//
// Get the parameter values of a foreign procedure using VHPI methods
//
// -------------------------------------------------------------------------

static void getVhpiParams(const struct vhpiCbDataS* cb, int args[], int args_size)
{
    int         idx      = 0;
    vhpiValueT  value;

    vhpiHandleT hParam;
    vhpiHandleT hScope   = cb->obj;
    vhpiHandleT hIter    = vhpi_iterator(vhpiParamDecls, hScope);

    while ((hParam = vhpi_scan(hIter)) && idx < args_size)
    {
        value.format     = vhpiIntVal;
        value.bufSize    = 0;
        value.value.intg = 0;
        vhpi_get_value(hParam, &value);
        args[idx++]      = value.value.intg;
        DebugVPrint("getVhpiParams(): %s = %d\n", vhpi_get_str(vhpiNameP, hParam), value.value.intg);
    }
}

// -------------------------------------------------------------------------
// setVhpiParams()
//
// Set the parameters values of a foreign procedure using VHPI methods
//
// -------------------------------------------------------------------------

static void setVhpiParams(const struct vhpiCbDataS* cb, int args[], int start_of_outputs, int args_size)
{
    int         idx      = 0;
    vhpiValueT  value;

    vhpiHandleT hParam;
    vhpiHandleT hScope   = cb->obj;
    vhpiHandleT hIter    = vhpi_iterator(vhpiParamDecls, hScope);

    while ((hParam = vhpi_scan(hIter)) && idx < args_size)
    {
        if (idx >= start_of_outputs)
        {
            DebugVPrint("setVhpiParams(): %s = %d\n", vhpi_get_str(vhpiNameP, hParam), args[idx]);
            value.format     = vhpiIntVal;
            value.bufSize    = 0;
            value.value.intg = args[idx];
            vhpi_put_value(hParam, &value, vhpiDeposit);
        }
        idx++;
    }
}
#endif

/////////////////////////////////////////////////////////////
// Main routine called whenever $vinit task invoked from
// initial block of VProc module.
//
VPROC_RTN_TYPE VInit (VINIT_PARAMS)
{

#if defined(USE_VHPI)
    int node;
    int args[VINIT_NUM_ARGS];

    setvbuf(stdout, 0, _IONBF, 0);

    getVhpiParams(cb, args, VINIT_NUM_ARGS);
    node = args[0];
#endif

    VPrint("VInit(%ld)\n", (long int)node);

    // Range check node number
    if (node < 0 || node >= VP_MAX_NODES)
    {
        VPrint("***Error: VInit() got out of range node number (%ld)\n", (long int)node);
        exit(VP_USER_ERR);
    }

    DebugVPrint("VInit(): node = %d\n", node);

    // Allocate some space for the node state and update pointer
    ns[node] = (pSchedState_t) malloc(sizeof(SchedState_t));

    // Set up semaphores for this node
    DebugVPrint("VInit(): initialising semaphores for node %d\n", node);

    if (sem_init(&(ns[node]->snd), 0, 0) == -1)
    {
        VPrint("***Error: VInit() failed to initialise semaphore\n");
        exit(1);
    }
    if (sem_init(&(ns[node]->rcv), 0, 0) == -1)
    {
        VPrint("***Error: VInit() failed to initialise semaphore\n");
        exit(1);
    }

    DebugVPrint("VInit(): initialising semaphores for node %d---Done\n", node);

    // Issue a new thread to run the user code
    VUser(node);
}

// -------------------------------------------------------------------------
// VTrans
//
// Main routine called whenever VTrans procedure invoked on
// clock edge of scheduled cycle.
//
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VTrans (VTRANS_PARAMS)
{

    int VPDataOut_int,   VPAddr_int,   VPOp_int, VPTicks_int;
    int VPDataOutHi_int, VPAddrHi_int, VPBurstSize_int, VPDone_int;
    int VPDataWidth_int, VPAddrWidth_int;
    int VPError_int,     VPParam_int;

#if defined(USE_VHPI)
    int  args[VTRANS_NUM_ARGS];
    int  node;
    int  Interrupt;
    int  VPStatus;
    int  VPCount;
    int  VPCountSec;

    getVhpiParams(cb, args, VTRANS_NUM_ARGS);

    int argIdx           = 0;
    node                 = args[argIdx++];
    Interrupt            = args[argIdx++];
    VPStatus             = args[argIdx++];
    VPCount              = args[argIdx++];
    VPCountSec           = args[argIdx++];

    VPDataOut_int        = 0; VPDataOutHi_int = 0;
    VPDataWidth_int      = 0;
    VPAddr_int           = 0; VPAddrHi_int    = 0;
    VPAddrWidth_int      = 0;
    VPOp_int             = 0;
    VPBurstSize_int      = 0;
    VPTicks_int          = 0;
    VPDone_int           = 0;
    VPError_int          = 0;
    VPParam_int          = 0;

    // Sample data inputs and update node receive state
    ns[node]->rcv_buf.data_in         = args[argIdx++];
    ns[node]->rcv_buf.data_in_hi      = args[argIdx++];

    // Skip over data width output
    argIdx               += 1;

    // Sample address and update node receive state
    ns[node]->rcv_buf.addr_in         = args[argIdx++];
    ns[node]->rcv_buf.addr_in_hi      = args[argIdx++];

    ns[node]->rcv_buf.num_burst_bytes = VPCount;

#else
    // Sample data inputs and update node receive state
    if (ns[node]->send_buf.type != trans32_burst)
    {
        ns[node]->rcv_buf.data_in     = *VPData;
        ns[node]->rcv_buf.data_in_hi  = *VPDataHi;
    }

    // Sample Address and update node receive state
    ns[node]->rcv_buf.addr_in         = *VPAddr;
    ns[node]->rcv_buf.addr_in_hi      = *VPAddrHi;

    // Sample burst byte count
    ns[node]->rcv_buf.num_burst_bytes = VPCount;

#endif

    // Sample other inputs and update node receive state
    ns[node]->rcv_buf.interrupt       = Interrupt;
    ns[node]->rcv_buf.status          = VPStatus;
    ns[node]->rcv_buf.count           = VPCount;
    ns[node]->rcv_buf.countsec        = VPCountSec;

    // Send message to VUser with input values
    DebugVPrint("VTrans(): setting rcv[%d] semaphore\n", node);
    sem_post(&(ns[node]->rcv));

    // Wait for a message from VUser process with output data
    DebugVPrint("VTrans(): waiting for snd[%d] semaphore\n", node);
    sem_wait(&(ns[node]->snd));

    // Update outputs of VTrans procedure
    if (ns[node]->send_buf.ticks >= DELTA_CYCLE)
    {
        VPDataOut_int   = ((uint32_t*)ns[node]->send_buf.data)[0];
        VPDataOutHi_int = ((uint32_t*)ns[node]->send_buf.data)[1];
        VPAddr_int      = (uint32_t)((ns[node]->send_buf.addr)       & 0xffffffffULL);
        VPAddrHi_int    = (uint32_t)((ns[node]->send_buf.addr >> 32) & 0xffffffffULL);
        VPOp_int        = ns[node]->send_buf.op;
        VPBurstSize_int = ns[node]->send_buf.num_burst_bytes;
        VPTicks_int     = ns[node]->send_buf.ticks;
        VPDone_int      = ns[node]->send_buf.done;
        VPError_int     = ns[node]->send_buf.error;
        VPParam_int     = ns[node]->send_buf.param;

        switch(ns[node]->send_buf.type)
        {
            case trans32_byte:
                VPAddrWidth_int = 32;
                VPDataWidth_int = 8;
                break;
            case trans32_hword:
                VPAddrWidth_int = 32;
                VPDataWidth_int = 16;
                break;
            case trans32_word:
            case trans32_burst:
                VPAddrWidth_int = 32;
                VPDataWidth_int = 32;
                break;
            case trans64_byte:
            case stream_snd_byte:
            case stream_get_byte:
                VPAddrWidth_int = 64;
                VPDataWidth_int = 8;
                break;
            case trans64_hword:
            case stream_snd_hword:
            case stream_get_hword:
                VPAddrWidth_int = 64;
                VPDataWidth_int = 16;
                break;
            case trans64_word:
            case stream_snd_word:
            case stream_get_word:
                VPAddrWidth_int = 64;
                VPDataWidth_int = 32;
                break;
            case trans64_dword:
            case stream_snd_dword:
            case stream_get_dword:
            case stream_snd_burst:
            case stream_get_burst:
            case trans64_burst:
                VPAddrWidth_int = 64;
                VPDataWidth_int = 64;
                break;

            case trans_idle:
                break;

            default:
                // Unsupported
                break;
        }

        DebugVPrint("VTrans(): VPTicks=%08x\n", VPTicks_int);
    }

    DebugVPrint("VTrans(): returning to simulation from node %d\n\n", node);

    DebugVPrint("===> addr=%08x rnw=%d burst=%d ticks=%d\n", VPAddr_int, VPRw_int, VPBurstSize_int, VPTicks_int);

#if !defined(USE_VHPI)
    // Export outputs over FLI
    *VPData           = VPDataOut_int;
    *VPDataHi         = VPDataOutHi_int;
    *VPDataWidth      = VPDataWidth_int;
    *VPAddr           = VPAddr_int;
    *VPAddrHi         = VPAddrHi_int;
    *VPAddrWidth      = VPAddrWidth_int;
    *VPOp             = VPOp_int;
    *VPBurstSize      = VPBurstSize_int;
    *VPTicks          = VPTicks_int;
    *VPDone           = VPDone_int;
    *VPError          = VPError_int;
    *VPParam          = VPParam_int;
#else
    argIdx            = VTRANS_START_OF_OUTPUTS;
    args[argIdx++]    = VPDataOut_int;
    args[argIdx++]    = VPDataOutHi_int;
    args[argIdx++]    = VPDataWidth_int;
    args[argIdx++]    = VPAddr_int;
    args[argIdx++]    = VPAddrHi_int;
    args[argIdx++]    = VPAddrWidth_int;
    args[argIdx++]    = VPOp_int;
    args[argIdx++]    = VPBurstSize_int;
    args[argIdx++]    = VPTicks_int;
    args[argIdx++]    = VPDone_int;
    args[argIdx++]    = VPError_int;
    args[argIdx++]    = VPParam_int;

    setVhpiParams(cb, args, VTRANS_START_OF_OUTPUTS, VTRANS_NUM_ARGS);
#endif
}

// -------------------------------------------------------------------------
// VSetBurstByte()
//
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VSetBurstRdByte(VSETBURSTRDBYTE_PARAMS)
{
#if defined(USE_VHPI)
    int args[VSETBURSTRDBYTE_NUM_ARGS];

    getVhpiParams(cb, args, VSETBURSTRDBYTE_NUM_ARGS);

    int argIdx           = 0;
    int node             = args[argIdx++];
    int idx              = args[argIdx++];
    int data             = args[argIdx++];
#endif

    ns[node]->rcv_buf.databuf[idx % DATABUF_SIZE] = data;
}

// -------------------------------------------------------------------------
// VGetBurstWrByte()
//
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VGetBurstWrByte(VGETBURSTWRBYTE_PARAMS)
{
#if defined(USE_VHPI)
    int args[VGETBURSTWRBYTE_NUM_ARGS];

    getVhpiParams(cb, args, VGETBURSTWRBYTE_NUM_ARGS);

    int argIdx           = 0;
    int node             = args[argIdx++];
    int idx              = args[argIdx++];

    argIdx            = VGETBURSTWRBYTE_START_OF_OUTPUTS;
    args[argIdx++]    = ns[node]->send_buf.databuf[idx % DATABUF_SIZE];;
    setVhpiParams(cb, args, VGETBURSTWRBYTE_START_OF_OUTPUTS, VGETBURSTWRBYTE_NUM_ARGS);
#else
    *data = ns[node]->send_buf.databuf[idx % DATABUF_SIZE];
#endif
}

// -------------------------------------------------------------------------
// VIrqVec()
///
// Main routine called whenever VIrqVec procedure invoked.
//
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VIrqVec (VIRQVEC_PARAMS)
{
#if defined(USE_VHPI)
    int node, irq;
    int args[VIRQVEC_NUM_ARGS];

    getVhpiParams(cb, args, VIRQVEC_NUM_ARGS);

    int argIdx = 0;
    node = args[argIdx++];
    irq  = args[argIdx++];
#endif

    if (ns[node]->VIntVecCB != NULL)
    {
        (*(ns[node]->VIntVecCB))(irq);
    }
}

// -------------------------------------------------------------------------
// VUserValue()
///
// Main routine called whenever VUserValue procedure invoked.
//
// -------------------------------------------------------------------------

VPROC_RTN_TYPE VUserValue (VUSERVALUE_PARAMS)
{
#if defined(USE_VHPI)
    int node, value, type;
    int args[VUSERVALUE_NUM_ARGS];

    getVhpiParams(cb, args, VUSERVALUE_NUM_ARGS);

    int argIdx = 0;
    node       = args[argIdx++];
    type       = args[argIdx++];
    value      = args[argIdx++];
#endif

    if (ns[node]->VUserCB != NULL)
    {
        (*(ns[node]->VUserCB))(type, value);
    }
}

