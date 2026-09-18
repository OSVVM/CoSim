// =========================================================================
//
//  File Name:         OsvvmVUser.cpp
//  Design Unit Name:
//  Revision:          OSVVM MODELS STANDARD VERSION
//
//  Maintainer:        Simon Southwell email:  simon.southwell@gmail.com
//  Contributor(s):
//     Simon Southwell      simon.southwell@gmail.com
//
//
//  Description:
//      Simulator co-simulation virtual procedure C interface routine
//      definitions for user side code. Top level VP user thread routines.
//      Sets up connection to queue and calls relevant user function for
//      node number
//
//  Revision History:
//    Date      Version    Description
//    09/2025   2026.01    Added support for Set- & Get- burst mode and model options
//    05/2023   2023.05    Adding support for Async, Check and Try functionality
//    04/2023   2023.04    Adding basic stream support
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

// -------------------------------------------------------------------------
// INCLUDES
// -------------------------------------------------------------------------

#include <stdint.h>
#include <errno.h>
#include <unistd.h>
#include <mutex>

#include "OsvvmVProc.h"
#include "OsvvmVUser.h"

#if defined(ALDEC) and defined (_WIN32)

# include <windows.h>

// -------------------------------------------------------------------------
// DEFINES AND MACROS
// -------------------------------------------------------------------------

// Map Linux dynamic loading calls to Windows equivalents

# define dlsym GetProcAddress
# define dlopen(_dll, _args) {LoadLibrary(_dll)}
# define dlerror() ""
# define dlclose FreeLibrary

// -------------------------------------------------------------------------
// TYPEDEFS
// -------------------------------------------------------------------------

typedef HINSTANCE symhdl_t;

#else
typedef void*     symhdl_t;
#endif

#if defined(SIEMENS) || (defined(ALDEC) && !defined(_WIN32))
static symhdl_t hdlvp;
#endif

#if defined(GHDL)
// GHDL, when callable, locks up using mutex pointers/new, so make an array of mutexes for GHDL
static std::mutex  acc_mx[VP_MAX_NODES];
#else
static std::mutex *acc_mx[VP_MAX_NODES];
#endif

// -------------------------------------------------------------------------
// FUNCTION DEFINITIONS
// -------------------------------------------------------------------------

// -------------------------------------------------------------------------
// VInitSendBuf()
//
// Initialise a send_buf_t record
//
// -------------------------------------------------------------------------
static void VInitSendBuf(send_buf_t &sbuf)
{
    sbuf.op              = NOT_DRIVEN;
    sbuf.type            = trans_idle;
    sbuf.addr            = 0;
    sbuf.prot            = 0;
    sbuf.num_burst_bytes = 0;
    sbuf.ticks           = 0;
    sbuf.done            = 0;
    sbuf.error           = 0;
    sbuf.param           = 0;
}

// -------------------------------------------------------------------------
// VWaitOnFirstMessage()
//
// Wait on receive semaphore to indicate simulator is initialised and ready
// to accept transfers from user code.
//
// -------------------------------------------------------------------------
static void VWaitOnFirstMessage(const uint32_t node)
{
    int status;

    // Wait for first message from simulator
    DebugVPrint("VWaitForSim(): waiting for first message semaphore rcv[%d]\n", node);
    if ((status = sem_wait(&(ns[node]->rcv))) == -1)
    {
        printf("***Error: bad sem_post status (%d) on node %d (VUserInit)\n", status, node);
        exit(1);
    }
}

// -------------------------------------------------------------------------
// VUserInit()
//
// New thread initialisation procedure. Synchronises with
// simulation before calling user procedure.
//
// -------------------------------------------------------------------------

static void VUserInit (const int node)
{
    pVUserMain_t VUserMain_func;
    char funcname[DEFAULT_STR_BUF_SIZE];
    char vusersoname[DEFAULT_STR_BUF_SIZE];
    int status;
    symhdl_t hdlvu;

    DebugVPrint("VUserInit(%d)\n", node);

    VWaitOnFirstMessage(node);

    // Get function name of user entry routine
    sprintf(funcname, "%s%d",    "VUserMain", node);

#if !defined(GHDL)
    // Create a new mutex for this node
    acc_mx[node] = new std::mutex;
#endif

#if defined(ACTIVEHDL)
    hdlvu = NULL;
#else
    sprintf(vusersoname, "./VUser.so");
    // Load user shared object to get handle to lookup VUsermain function symbols
    hdlvu = dlopen(vusersoname, RTLD_LAZY | RTLD_GLOBAL);

    if (hdlvu == NULL)
    {
        VPrint("***Error: failed to load VUser.so. %s\n", dlerror());
    }
#endif

    // Get the function pointer for the entry routine
    if ((VUserMain_func = (pVUserMain_t) dlsym(hdlvu, funcname)) == NULL)
    {
        printf("***Error: failed to find user code symbol %s (VUserInit)\n", funcname);
        exit(1);
    }

#if defined(SIEMENS) || (defined(ALDEC) && !defined(_WIN32))
    // Close the VProc.so handle to decrement the count, incremented with the open
    dlclose(hdlvp);
#endif

    DebugVPrint("VUserInit(): got user function (%s) for node %d (%p)\n", funcname, node, VUserMain_func);

    DebugVPrint("VUserInit(): calling user code for node %d\n", node);

    // Call user program
    DebugVPrint("VUserInit(): calling VUserMain%d\n", node);
    VUserMain_func(node);

    while(true);
}

// -------------------------------------------------------------------------
// VUser()
//
// Entry point for new user process. Creates a new thread
// calling VUserInit().
//
// -------------------------------------------------------------------------

int VUser (const int node)
{
    pthread_t thread;
    int status;
    int idx, jdx;

    DebugVPrint("VUser(): node %d\n", node);

    // Interrupt callback initialisation
    ns[node]->VIntVecCB     = NULL;
    ns[node]->VUserCB       = NULL;
    ns[node]->last_int      = 0;

    DebugVPrint("VUser(): initialised interrupt table node %d\n", node);

#if defined (SIEMENS) || (defined(ALDEC) && !defined(_WIN32))
    // Load VProc shared object to make symbols global
    hdlvp = dlopen("./VProc.so", RTLD_LAZY | RTLD_GLOBAL);

    if (hdlvp == NULL)
    {
        VPrint("***Error: failed to load VProc.so. %s\n", dlerror());
    }
#endif


#ifndef DISABLE_VUSERMAIN_THREAD
    // Set off the user code thread
    if (status = pthread_create(&thread, NULL, (pThreadFunc_t)VUserInit, (void *)((long long)node)))
    {
        DebugVPrint("VUser(): pthread_create returned %d\n", status);
        return 1;
    }

    DebugVPrint("VUser(): spawned user thread for node %d\n", node);
#endif
    return 0;
}

// -------------------------------------------------------------------------
// VExch()
//
// Message exchange routine. Handles all messages to and from
// simulation process (apart from initialisation). Each sent
// message has a reply. Interrupt messages require that
// the original IO message reply is waited for again.
//
// -------------------------------------------------------------------------

static void VExch (psend_buf_t psbuf, prcv_buf_t prbuf, const uint32_t node)
{
    // Lock mutex as code is critical if accessed from multiple threads
    // for the same node.
#if defined (GHDL)
    acc_mx[node].lock();
#else
    acc_mx[node]->lock();
#endif

    int status;

    // Send message to simulator
    ns[node]->send_buf = *psbuf;
    DebugVPrint("VExch(): setting snd[%d] semaphore\n", node);

    if ((status = sem_post(&(ns[node]->snd))) == -1)
    {
        printf("***Error: bad sem_post status (%d) on node %d (VExch)\n", status, node);
        exit(1);
    }

    // If this is the last message from the user code
    // unlock/destroy the mutex, as GUI runs seem to
    // hold on to the mutex state which hangs a simulation
    // on subsequent runs.
#if !defined(GHDL)
    if (ns[node]->send_buf.done)
    {
        delete acc_mx[node];
    }
#endif

    // Wait for response message from simulator
    DebugVPrint("VExch(): waiting for rcv[%d] semaphore\n", node);
    sem_wait(&(ns[node]->rcv));

    // Get the pointer to the receive response buffer
    *prbuf = ns[node]->rcv_buf;

    // Call user registered interrupt vector callback if the interrupt vector changes
    if ((prbuf->interrupt != ns[node]->last_int) && ns[node]->VIntVecCB != NULL)
    {
        psbuf->ticks = (*(ns[node]->VIntVecCB))(prbuf->interrupt);
    }

    ns[node]->last_int = prbuf->interrupt;

    // Unlock mutex
#if defined(GHDL)
    acc_mx[node].unlock();
#else
    acc_mx[node]->unlock();
#endif

    DebugVPrint("VExch(): returning to user code from node %d\n", node);
}

// -------------------------------------------------------------------------
// VWaitForSim()
//
// Wait for the simulator to initialise and have sent the first message
//
// -------------------------------------------------------------------------

void VWaitForSim(const uint32_t node)
{
#ifdef DISABLE_VUSERMAIN_THREAD

    int count = 0;

    // Wait until the node's state is initialised (with a time out)
    while(ns[node] == NULL && count < FIVESEC_TIMEOUT)
    {
        usleep(HUNDRED_MILLISECS);
        count++;
    }

    usleep(HUNDRED_MILLISECS);

    // If timed out, generate an error
    if (count == FIVESEC_TIMEOUT)
    {
        VPrint("***ERROR: timed out waiting for simulation\n");
        exit(1);
    }
    else
    {
        // Wait for the first message from the simulator
        VWaitOnFirstMessage(node);
    }
#else
    // When running VUserMain in a thread is not disabled then do nothing
    return;
#endif

}

// -------------------------------------------------------------------------
// VTransUserCommon()
//
// Common 8-bit byte transaction exchange function (32-bit address)
//
// -------------------------------------------------------------------------

uint8_t VTransUserCommon (const int op, uint32_t *addr, const uint8_t data, int* status, const int prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans32_byte;
    sbuf.addr            = *addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;

    *((uint8_t*)sbuf.data) = data & 0xffU;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;
    *addr   = rbuf.addr_in;

    return rbuf.data_in & 0xffU;
}

// -------------------------------------------------------------------------
// VTransUserCommon()
//
// Common 16-bit word transaction exchange function (32-bit address)
//
// -------------------------------------------------------------------------

uint16_t VTransUserCommon (const int op, uint32_t *addr, const uint16_t data,  int* status, int const prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans32_hword;
    sbuf.addr            = *addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;

    *((uint16_t*)sbuf.data) = data & 0xffffU;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;
    *addr   = rbuf.addr_in;

    return rbuf.data_in & 0xffffU;
}

// -------------------------------------------------------------------------
// VTransUserCommon()
//
// Common 32-bit word transaction exchange function (32-bit address)
//
// -------------------------------------------------------------------------

uint32_t VTransUserCommon (const int op, uint32_t *addr, const uint32_t data, int* status,  const int prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans32_word;
    sbuf.addr            = *addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;

    *((uint32_t*)sbuf.data) = data;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;
    *addr   = rbuf.addr_in;

    return rbuf.data_in;
}

// -------------------------------------------------------------------------
// VTransUserCommon()
//
// Common 8-bit word transaction exchange function (64-bit address)
//
// -------------------------------------------------------------------------

uint8_t VTransUserCommon (const int op, uint64_t *addr, const uint8_t data, int* status, const int prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans64_byte;
    sbuf.addr            = *addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;

    *((uint8_t*)sbuf.data) = data & 0xffU;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;
    *addr   = ((uint64_t)rbuf.addr_in_hi << 32) | ((uint64_t)rbuf.addr_in);

    return rbuf.data_in & 0xffU;
}

// -------------------------------------------------------------------------
// VTransUserCommon()
//
// Common 16-bit word transaction exchange function (64-bit address)
//
// -------------------------------------------------------------------------

uint16_t VTransUserCommon (const int op, uint64_t *addr, const uint16_t data, int* status, const int prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans64_hword;
    sbuf.addr            = *addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;

    *((uint16_t*)sbuf.data) = data & 0xffffU;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;
    *addr   = ((uint64_t)rbuf.addr_in_hi << 32) | ((uint64_t)rbuf.addr_in);

    return rbuf.data_in & 0xffffU;
}

// -------------------------------------------------------------------------
// VTransUserCommon()
//
// Common 32-bit word transaction exchange function (64-bit address)
//
// -------------------------------------------------------------------------

uint32_t VTransUserCommon (const int op, uint64_t *addr, const uint32_t data, int* status, const int prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans64_word;
    sbuf.addr            = *addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;

    *((uint32_t*)sbuf.data) = data;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;
    *addr   = ((uint64_t)rbuf.addr_in_hi << 32) | ((uint64_t)rbuf.addr_in);

    return rbuf.data_in;
}

// -------------------------------------------------------------------------
// VTransUserCommon()
//
// Common 64-bit word transaction exchange function (64-bit address)
//
// -------------------------------------------------------------------------

uint64_t VTransUserCommon (const int op, uint64_t *addr, const uint64_t data, int* status, const int prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans64_dword;
    sbuf.addr            = *addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;

    *((uint64_t*)sbuf.data) = data;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;
    *addr   = ((uint64_t)rbuf.addr_in_hi << 32) | ((uint64_t)rbuf.addr_in);

    return (uint64_t)rbuf.data_in | ((uint64_t)rbuf.data_in_hi << 32);
}

// -------------------------------------------------------------------------
// VTransBurstCommon()
//
// Common burst transaction exchange function (32-bit address)
//
// -------------------------------------------------------------------------

void VTransBurstCommon (const int op, const int param, const uint32_t addr, uint8_t* data, const int bytesize, const int prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans32_burst;
    sbuf.addr            = addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;
    sbuf.num_burst_bytes = bytesize % DATABUF_SIZE;

    // Flag when a FIFO fill (or check) operation
    bool is_fill = param == BURST_INCR || param == BURST_INCR_PUSH || param == BURST_INCR_CHECK ||
                   param == BURST_RAND || param == BURST_RAND_PUSH || param == BURST_RAND_CHECK;

    // The number of write bytes is either 1, when a fill/check operation (with first bytes),
    // or none when a pure burst transaction or the same as the bytesize value.
    int num_of_wr_bytes = is_fill ? 1 : (param == BURST_TRANS) ? 0 : sbuf.num_burst_bytes;

    for (int idx = 0; idx < num_of_wr_bytes; idx++)
    {
        sbuf.databuf[idx] = data[idx];
    }

    VExch(&sbuf, &rbuf, node);

    if (op == READ_BURST && param != BURST_TRANS && !is_fill)
    {
        for (int idx = 0; idx < sbuf.num_burst_bytes; idx++)
        {
            data[idx] = rbuf.databuf[idx];
        }
    }

    return;
}

// -------------------------------------------------------------------------
// VTransBurstCommon()
//
// Common burst transaction exchange function (64-bit address)
//
// -------------------------------------------------------------------------

void VTransBurstCommon (const int op, const int param, const uint64_t addr, uint8_t* data, const int bytesize, const int prot, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans64_burst;
    sbuf.addr            = addr;
    sbuf.prot            = prot;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;
    sbuf.num_burst_bytes = bytesize % DATABUF_SIZE;

    // Flag when a FIFO fill (or check) operation
    bool is_fill = param == BURST_INCR || param == BURST_INCR_PUSH || param == BURST_INCR_CHECK ||
                   param == BURST_RAND || param == BURST_RAND_PUSH || param == BURST_RAND_CHECK;

    // The number of write bytes is either 1, when a fill/check operation (with first bytes),
    // or none when a pure burst transaction or the same as the bytesize value.
    int num_of_wr_bytes = is_fill ? 1  : (param == BURST_TRANS) ? 0 : sbuf.num_burst_bytes;

    for (int idx = 0; idx < num_of_wr_bytes; idx++)
    {
        sbuf.databuf[idx] = data[idx];
    }

    VExch(&sbuf, &rbuf, node);

    if (op == READ_BURST && param != BURST_TRANS)
    {
        for (int idx = 0; idx < sbuf.num_burst_bytes; idx++)
        {
            data[idx] = rbuf.databuf[idx];
        }
    }

    return;
}

// -------------------------------------------------------------------------
// VTransGetCount
//
// Function to return various counts
// -------------------------------------------------------------------------

int VTransGetCount (const int op, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.op              = (addr_bus_trans_op_t)op;

    VExch(&sbuf, &rbuf, node);

    return rbuf.count;

}

// -------------------------------------------------------------------------
// VTransTransactionWait
//
// Function wait on transactions
// -------------------------------------------------------------------------

void VTransTransactionWait (const int op, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.op              = (addr_bus_trans_op_t)op;

    VExch(&sbuf, &rbuf, node);

    return;
}

// -------------------------------------------------------------------------
// VStreamUserCommon()
//
// Common 8-bit byte stream send/check transaction exchange function
//
// -------------------------------------------------------------------------

uint8_t VStreamUserCommon (const int op, const uint8_t data, const int param, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_snd_byte;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;

    *((uint8_t*)sbuf.data) = data & 0xffU;

    VExch(&sbuf, &rbuf, node);

    return rbuf.data_in & 0xffU;
}

// -------------------------------------------------------------------------
// VStreamUserGetCommon()
//
// Common 8-bit byte stream get transaction exchange
//
// -------------------------------------------------------------------------

bool VStreamUserGetCommon (int op, uint8_t *rdata, int *status, const uint8_t wdata, const int param, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_get_byte;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;

    *((uint8_t*)sbuf.data) = wdata & 0xffU;

    VExch(&sbuf, &rbuf, node);

    if (op != TRY_CHECK)
    {
        *status = rbuf.status;
        *rdata = rbuf.data_in & 0xffU;
    }

    // Return available status (sent back in unused interrupt field)
    return rbuf.interrupt;
}

// -------------------------------------------------------------------------
// VStreamUserCommon()
//
// Common 16-bit word stream send/check transaction exchange function
//
// -------------------------------------------------------------------------

uint16_t VStreamUserCommon (const int op, const uint16_t data, const int param, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_snd_hword;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;

    *((uint16_t*)sbuf.data) = data & 0xffffU;

    VExch(&sbuf, &rbuf, node);

    return rbuf.data_in & 0xffffU;
}

// -------------------------------------------------------------------------
// VStreamUserGetCommon()
//
// Common 16-bit word stream get transaction get exchange function
//
// -------------------------------------------------------------------------

bool VStreamUserGetCommon (int op, uint16_t *rdata, int *status, const uint16_t wdata, const int param, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_get_hword;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;

    *((uint16_t*)sbuf.data) = wdata & 0xffffU;

    VExch(&sbuf, &rbuf, node);

    if (op != TRY_CHECK)
    {
        *status = rbuf.status;
        *rdata = rbuf.data_in & 0xffffU;
    }

    // Return available status (sent back in unused interrupt field)
    return rbuf.interrupt;
}

// -------------------------------------------------------------------------
// VStreamUserCommon()
//
// Common 32-bit word stream send/check transaction exchange function
//
// -------------------------------------------------------------------------

uint32_t VStreamUserCommon (const int op, const uint32_t data, const int param, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_snd_word;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;

    *((uint32_t*)sbuf.data) = data & 0xffffffffU;

    VExch(&sbuf, &rbuf, node);

    return rbuf.data_in & 0xffffffffU;
}


// -------------------------------------------------------------------------
// VStreamUserGetCommon()
//
// Common 32-bit word stream get transaction exchange function
//
// -------------------------------------------------------------------------

bool VStreamUserGetCommon (int op, uint32_t *rdata, int *status, const uint32_t wdata, const int param, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_get_word;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;

    *((uint32_t*)sbuf.data) = wdata & 0xffffffffU;

    VExch(&sbuf, &rbuf, node);

    if (op != TRY_CHECK)
    {
        *status = rbuf.status;
        *rdata = rbuf.data_in & 0xffffffffU;
    }

    // Return available status (sent back in unused interrupt field)
    return rbuf.interrupt;
}

// -------------------------------------------------------------------------
// VStreamUserCommon()
//
// Common 64-bit word send/check transaction exchange function
//
// -------------------------------------------------------------------------

uint64_t VStreamUserCommon (const int op, const uint64_t data, const int param, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_snd_dword;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.param           = param;

    *((uint64_t*)sbuf.data) = data;

    VExch(&sbuf, &rbuf, node);

    return (uint64_t)rbuf.data_in | ((uint64_t)rbuf.data_in_hi << 32);
}

// -------------------------------------------------------------------------
// VStreamUserGetCommon()
//
// Common 64-bit word stream get transaction exchange function
//
// -------------------------------------------------------------------------

bool VStreamUserGetCommon (int op, uint64_t *rdata, int *status, const uint64_t wdata, const int param,  const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_get_dword;
    sbuf.op              = (addr_bus_trans_op_t)op;

    VExch(&sbuf, &rbuf, node);

    *((uint64_t*)sbuf.data) = wdata;


    if (op != TRY_CHECK)
    {
        *status = rbuf.status;
        *rdata  = (uint64_t)rbuf.data_in | ((uint64_t)rbuf.data_in_hi << 32);
    }

    // Return available status (sent back in unused interrupt field)
    return rbuf.interrupt;
}

// -------------------------------------------------------------------------
// VStreamUserBurstSendCommon()
//
// Common function for Send/Check related stream transactions
// -------------------------------------------------------------------------

bool VStreamUserBurstSendCommon (const int op, const int burst_type, uint8_t* data, const int bytesize, const int param, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type               = stream_snd_burst;
    sbuf.op                 = (addr_bus_trans_op_t)op;
    sbuf.num_burst_bytes    = bytesize % DATABUF_SIZE;
    sbuf.param              = param;
    *((uint32_t*)sbuf.data) = burst_type; // Re-use data field of send buffer for burst sub-operation

    // The number of write bytes is either 1, when a fill/check operation (with first bytes),
    // or none when a pure burst transaction or the same as the bytesize value.
    int num_of_wr_bytes = (burst_type == BURST_TRANS)                         ? 0 :
                          (op == TRY_CHECK_BURST && burst_type != BURST_NORM) ? 1 :
                                                                                sbuf.num_burst_bytes;

    for (int idx = 0; idx < num_of_wr_bytes; idx++)
    {
        sbuf.databuf[idx] = data[idx];
    }

    VExch(&sbuf, &rbuf, node);


    // Return available status (sent back in unused interrupt field)
    return rbuf.interrupt;
}

// -------------------------------------------------------------------------
// VStreamUserBurstGetCommon()
//
// Common function for Get related stream transactions
// -------------------------------------------------------------------------

bool VStreamUserBurstGetCommon (const int op, const int param, uint8_t* data, const int bytesize, int* status, const uint32_t node)
{
    rcv_buf_t    rbuf;
    send_buf_t   sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_get_burst;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.num_burst_bytes = bytesize % DATABUF_SIZE;
    sbuf.param           = param;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;

    // Return data for normal/data transactions, but only if not a try with none available
    if ((param == BURST_NORM || param == BURST_DATA) && !((stream_operation_t)sbuf.op == TRY_GET_BURST && !rbuf.interrupt))
    {
        for (int idx = 0; idx < sbuf.num_burst_bytes; idx++)
        {
            data[idx] = rbuf.databuf[idx];
        }
    }

    // Return available status (sent back in unused interrupt field)
    return rbuf.interrupt;
}

// -------------------------------------------------------------------------
// VStreamUserBurstGetCommon()
//
// Common function for Get related stream transactions
// -------------------------------------------------------------------------

bool VStreamUserBurstGetCommon (const int op, const int param, uint8_t* data, int *bytesize, int* status, const uint32_t node)
{
    rcv_buf_t    rbuf;
    send_buf_t   sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = stream_get_burst;
    sbuf.op              = (addr_bus_trans_op_t)op;
    sbuf.num_burst_bytes = *bytesize % DATABUF_SIZE;
    sbuf.param           = param;

    VExch(&sbuf, &rbuf, node);

    *status = rbuf.status;

    // Return data for normal/data transactions, but only if not a try with none available
    if ((param == BURST_NORM || param == BURST_DATA) && !((stream_operation_t)sbuf.op == TRY_GET_BURST && !rbuf.interrupt))
    {
        for (int idx = 0; idx < rbuf.num_burst_bytes; idx++)
        {
            data[idx] = rbuf.databuf[idx];
        }
        
        *bytesize = rbuf.num_burst_bytes;
    }
    else
    {
        *bytesize = 0;
    }
    

    // Return available status (sent back in unused interrupt field)
    return rbuf.interrupt;
}

// -------------------------------------------------------------------------
// VStreamWaitGetCount()
//
// Common function for transaction wait and get count operation exchange
//
// -------------------------------------------------------------------------

int VStreamWaitGetCount (const int op, const bool txnrx, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.op                = (addr_bus_trans_op_t)op;
    *((uint32_t*)sbuf.data) = txnrx ? 1 : 0;

    VExch(&sbuf, &rbuf, node);

    return txnrx ? rbuf.countsec : rbuf.count;
}

// -------------------------------------------------------------------------
// VTick()
//
// Invokes a tick message exchange
//
// -------------------------------------------------------------------------

int VTick (const uint32_t ticks, const bool done, const bool error, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    // Ensure the tick loop executes at least once to allow donr and/or error to be
    // set with a tick argument of 0.
    int loops = ticks ? ticks : 1;

    for (int idx = 0; idx < loops; idx++)
    {
        // Loop, ticking once, to allow for interrupts to be registered while sleeping
        sbuf.ticks       = ticks ? 1 : 0;

        // Only flag the done and error status on the first tick.
        sbuf.done        = (done  && (idx == 0)) ? 1 : 0;
        sbuf.error       = (error && (idx == 0)) ? 1 : 0;

        // Operation is to wait for clock
        sbuf.op          = WAIT_FOR_CLOCK;

        // Exchange the tick command with simulator code
        VExch(&sbuf, &rbuf, node);
    }

    return 0;
}

// -------------------------------------------------------------------------
// VRegInterrupt()
//
// Registers a user function as an interrupt callback
//
// -------------------------------------------------------------------------

void VRegInterrupt (const pVUserInt_t func, const uint32_t node)
{
    DebugVPrint("VRegInterrupt(): at node %d, registering vector interrupt callback\n", node);

    ns[node]->VIntVecCB = func;
}

// -------------------------------------------------------------------------
// VRegUserValue()
//
// Registers a generic user callback
//
// -------------------------------------------------------------------------

void VRegUserValue (const pVUser_t func, const uint32_t node)
{
    DebugVPrint("VRegUserValue(): at node %d, registering user callback\n", node);

    ns[node]->VUserCB = func;
}

// -------------------------------------------------------------------------
// VSetTestName()
//
// Set the tests name to the string of characters in data
// -------------------------------------------------------------------------

void VSetTestName (const char* data, const int bytesize, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type            = trans_idle;
    sbuf.op              = SET_TEST_NAME;
    sbuf.num_burst_bytes = bytesize % DATABUF_SIZE;

    for (int idx = 0; idx < sbuf.num_burst_bytes; idx++)
    {
        sbuf.databuf[idx] = data[idx];
    }

    VExch(&sbuf, &rbuf, node);

    return;
}

// -------------------------------------------------------------------------
// VSetModelOptions()
//
// OSVVM VC SetModelOptions
// -------------------------------------------------------------------------

void VSetModelOptions (const int option, const int optval, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type           = trans_idle;
    sbuf.op             = SET_MODEL_OPTIONS;
    sbuf.param          = option;

    *((int*)sbuf.data)  = optval;

    VExch(&sbuf, &rbuf, node);
}

// -------------------------------------------------------------------------
// VGetModelOptions()
//
// OSVVM VC GetModelOptions
// -------------------------------------------------------------------------

void VGetModelOptions (const int option, int &optval, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type           = trans_idle;
    sbuf.op             = GET_MODEL_OPTIONS;
    sbuf.param          = option;

    VExch(&sbuf, &rbuf, node);

    // Option value is returned in the count field
    // containing the IntToModel value.
    optval = rbuf.count;
}

void VGetModelOptions (const int option, bool &optval, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type           = trans_idle;
    sbuf.op             = GET_MODEL_OPTIONS;
    sbuf.param          = option;

    VExch(&sbuf, &rbuf, node);

    // Option value is returned in the status field
    // containing the BoolToModel value.
    optval = rbuf.status;
}

// -------------------------------------------------------------------------
// VSetBurstMode()
//
// OSVVM VC SetBurstMode
// -------------------------------------------------------------------------

void VSetBurstMode (const int mode, const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type           = trans_idle;
    sbuf.op             = SET_BURST_MODE;

    *((int*)sbuf.data)  = mode;

    VExch(&sbuf, &rbuf, node);
}

// -------------------------------------------------------------------------
// VGetBurstMode()
//
// OSVVM VC GetBurstMode
// -------------------------------------------------------------------------

int  VGetBurstMode (const uint32_t node)
{
    rcv_buf_t  rbuf;
    send_buf_t sbuf;

    VInitSendBuf(sbuf);

    sbuf.type           = trans_idle;
    sbuf.op             = GET_BURST_MODE;

    VExch(&sbuf, &rbuf, node);

    // mode value is returned in the count field
    // containing the IntToModel value.
    return rbuf.count;
}



