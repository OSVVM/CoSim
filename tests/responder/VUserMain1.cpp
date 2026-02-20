// ------------------------------------------------------------------------------
//
//  File Name:           VUserMain0.cpp
//  Design Unit Name:    Co-simulation virtual processor test program
//  Revision:            OSVVM MODELS STANDARD VERSION
//
//  Maintainer:          Simon Southwell      email:  simon.southwell@gmail.com
//  Contributor(s):
//     Simon Southwell   simon.southwell@gmail.com
//
//  Description:
//      Co-simulation test transaction source
//
//  Developed by:
//        Simon Southwell
//
//  Revision History:
//    Date      Version    Description
//    05/2023   2022       Initial revision
//
//  This file is part of OSVVM.
//
//  Copyright (c) 2023 by Simon Southwell
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
// ------------------------------------------------------------------------------

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>

// Import VProc user API
#include "OsvvmCosimResp.h"

// I am node 0 context
static int node  = 1;

int barrier = 0;

extern uint32_t testvals[];
extern int      wcount;
extern int      rcount;

// ------------------------------------------------------------------------------
// Routine to check received data and availability flag
// ------------------------------------------------------------------------------

static bool checkdata(uint32_t addr, uint32_t data, int tidx, bool avail, bool expavail, char* funcstr)
{
    uint32_t expaddr;
    uint32_t expdata;
    bool     error = false;

    if (avail != expavail)
    {
        VPrint("***ERROR: Unexpected unavailable status from %s\n", funcstr);
        error = true;
    }
    else
    {
        expaddr = testvals[tidx];
        expdata = testvals[tidx+1];

        if (data != expdata)
        {
            VPrint("***ERROR: data mismatch on %s. Got 0x%08x. Exp 0x%08x\n", funcstr, data, expdata);
            error = true;
        }
        else
        {
            if(addr != expaddr)
            {
                VPrint("***ERROR: address mismatch on %s. Got 0x%08x. Exp 0x%08x\n", funcstr, addr, expaddr);
            }
        }
    }

    return error;
}

// ------------------------------------------------------------------------------
// Routine to release barrier for manager code to advance. A default delay of
// 10 ticks is addeded to allowfor the manager generated transaction to complete,
// but this can be overidden with the delay argument.
// ------------------------------------------------------------------------------

static void releaseBarrier(int delay = 10)
{
    OsvvmCosimResp  sub(node);

    barrier++;

    if (delay > 0)
    {
        sub.tick(delay);
    }
}

// ------------------------------------------------------------------------------
// Main entry point for node 1 virtual processor software
//
// VUserMainX has no calling arguments. If runtime configuration required
// then you'll need to read in a configuration file.
//
// ------------------------------------------------------------------------------

extern "C" void VUserMain1()
{
    VPrint("VUserMain%d()\n", node);

    bool            error = false;
    OsvvmCosimResp  sub(node);

    uint32_t addr;
    uint8_t  data8;
    uint16_t data16;
    uint32_t data32;
    uint32_t expdata;
    uint32_t expaddr;
    bool     avail;
    int      tidx = 0;
    int      totalcount;

    // ------------------------------------
    // Check GetWrite and TryGetWrite

    avail = sub.respTryGetWrite(&addr, &data32);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTryGetWrite\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    avail = sub.respTryGetWrite(&addr, &data32);
    error |= checkdata(addr, data32, tidx, avail, true, "respTryGetWrite"); tidx+=2;

    sub.respGetWrite(&addr, &data32);
    error |= checkdata(addr, data32, tidx, true, true, "respGetWrite"); tidx+=2;

    avail = sub.respTryGetWrite(&addr, &data16);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTryGetWrite\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    avail = sub.respTryGetWrite(&addr, &data16);
    error |= checkdata(addr, (uint32_t)data16, tidx, avail, true, "respTryGetWrite"); tidx+=2;

    sub.respGetWrite(&addr, &data16);
    error |= checkdata(addr, (uint32_t)data16, tidx, true, true, "respGetWrite"); tidx+=2;

    avail = sub.respTryGetWrite(&addr, &data8);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTryGetWrite\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    avail = sub.respTryGetWrite(&addr, &data8);
    error |= checkdata(addr, (uint32_t)data8, tidx, avail, true, "respTryGetWrite"); tidx+=2;

    sub.respGetWrite(&addr, &data8);
    error |= checkdata(addr, (uint32_t)data8, tidx, true, true, "respGetWrite"); tidx+=2;

    // ------------------------------------
    // test respTryGetWriteAddress
    // respGetWriteAddress, TryGetWriteData,
    // GetWriteData

    avail = sub.respTryGetWriteAddress(&addr);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTryGetWrite\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();;

    avail = sub.respTryGetWriteAddress(&addr);
    error |= checkdata(addr, 0, tidx, avail, true, "respTryGetWriteAddress"); tidx+=2;

    avail = sub.respTryGetWriteData(&data32);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTryGetWrite\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    avail = sub.respTryGetWriteData(&data32);
    error |= checkdata(0, data32, tidx, avail, true, "respTryGetWriteData"); tidx+=2;

    // Allow the manager to advance
    releaseBarrier();

    sub.respGetWriteAddress(&addr);
    error |= checkdata(addr, 0, tidx, true, true, "respGetWriteAddress"); tidx+=2;

    avail = sub.respTryGetWriteData(&data16);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTryGetWrite\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    avail = sub.respTryGetWriteData(&data16);
    error |= checkdata(0, (uint32_t)data16, tidx, avail, true, "respTryGetWriteData"); tidx+=2;

    // Allow the manager to advance
    releaseBarrier();

    sub.respGetWriteAddress(&addr);
    error |= checkdata(addr, 0, tidx, true, true, "respGetWriteAddress"); tidx+=2;

    avail = sub.respTryGetWriteData(&data8);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTryGetWrite\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    sub.respGetWriteData(&data8);
    error |= checkdata(0, (uint32_t)data8, tidx, true, true, "respGetWriteData"); tidx+=2;

    // Allow the manager to advance
    releaseBarrier();

    sub.respGetWriteAddress(&addr);
    error |= checkdata(addr, 0, tidx, true, true, "respGetWriteAddress"); tidx+=2;

    sub.respGetWriteData(&data32);
    error |= checkdata(0, data32, tidx, true, true, "respGetWriteData"); tidx+=2;

    sub.respGetWriteAddress(&addr);
    error |= checkdata(addr, 0, tidx, true, true, "respGetWriteAddress"); tidx+=2;

    sub.respGetWriteData(&data16);
    error |= checkdata(0, (uint32_t)data16, tidx, true, true, "respGetWriteData"); tidx+=2;

    sub.respGetWriteAddress(&addr);
    error |= checkdata(addr, 0, tidx, true, true, "respGetWriteAddress"); tidx+=2;

    sub.respGetWriteData(&data8);
    error |= checkdata(0, (uint32_t)data8, tidx, true, true, "respGetWriteData"); tidx+=2;

    // ------------------------------------
    // Test SendRead, TrySendRead

    avail = sub.respTrySendRead(&addr, data32);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTrySendRead\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    data32 = testvals[tidx+1];

    avail = sub.respTrySendRead(&addr, data32);
    error |= checkdata(addr, data32, tidx, avail, true, "respTrySendRead"); tidx+=2;

    avail = sub.respTrySendRead(&addr, data16);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTrySendRead\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    data16 = testvals[tidx+1];

    avail = sub.respTrySendRead(&addr, data16);
    error |= checkdata(addr, data16, tidx, avail, true, "respTrySendRead"); tidx+=2;

    avail = sub.respTrySendRead(&addr, data8);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTrySendRead\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    data8 = testvals[tidx+1];

    avail = sub.respTrySendRead(&addr, data8);
    error |= checkdata(addr, data8, tidx, avail, true, "respTrySendRead"); tidx+=2;

    // Allow the manager to advance
    releaseBarrier();;

    data32 = testvals[tidx+1];

    sub.respSendRead(&addr, data32);
    error |= checkdata(addr, data32, tidx, avail, true, "respTrySendRead"); tidx+=2;

    // Allow the manager to advance
    releaseBarrier();

    data16 = testvals[tidx+1];

    sub.respSendRead(&addr, data16);
    error |= checkdata(addr, data16, tidx, avail, true, "respTrySendRead"); tidx+=2;

    // Allow the manager to advance
    releaseBarrier();

    data8 = testvals[tidx+1];

    sub.respSendRead(&addr, data8);
    error |= checkdata(addr, data8, tidx, avail, true, "respTrySendRead"); tidx+=2;

    // ------------------------------------
    // Test respGetReadAddress, respTryGetReadAddr
    // respSendReadData, respSendReadDataAsync

    avail = sub.respTryGetReadAddress(&addr);

    if (avail)
    {
        VPrint("***ERROR: Unexpected available status from respTryGetReadAddress\n");
        error = true;
    }

    // Allow the manager to advance
    releaseBarrier();

    avail = sub.respTryGetReadAddress(&addr);
    error |= checkdata(addr, 0, tidx, avail, true, "respTryGetReadAddress"); tidx+=2;

    data32 = testvals[tidx+1];
    sub.respSendReadData(data32);
    error |= checkdata(0, data32, tidx, true, true, "respSendReadData"); tidx+=2;

    // Allow the manager to advance
    releaseBarrier();

    sub.respGetReadAddress(&addr);
    error |= checkdata(addr, 0, tidx, avail, true, "respGetReadAddress"); tidx+=2;

    data16 = testvals[tidx+1];
    sub.respSendReadData(data16);
    error |= checkdata(0, data16, tidx, true, true, "respSendReadData"); tidx+=2;

    // Allow the manager to advance
    releaseBarrier();

    sub.respGetReadAddress(&addr);
    error |= checkdata(addr, 0, tidx, avail, true, "respGetReadAddress"); tidx+=2;

    data8 = testvals[tidx+1];
    sub.respSendReadData(data8);
    error |= checkdata(0, data8, tidx, true, true, "respSendReadData"); tidx+=2;

    releaseBarrier();

    avail = sub.respTryGetReadAddress(&addr);
    error |= checkdata(addr, 0, tidx, avail, true, "respTryGetReadAddress"); tidx+=2;

    data32 = testvals[tidx+1];
    sub.respSendReadDataAsync(data32);
    error |= checkdata(0, data32, tidx, true, true, "respSendReadDataAsync"); tidx+=2;

    releaseBarrier();

    avail = sub.respTryGetReadAddress(&addr);
    error |= checkdata(addr, 0, tidx, avail, true, "respTryGetReadAddress"); tidx+=2;

    data16 = testvals[tidx+1];
    sub.respSendReadDataAsync(data16);
    error |= checkdata(0, data16, tidx, true, true, "respSendReadDataAsync"); tidx+=2;

    releaseBarrier();

    avail = sub.respTryGetReadAddress(&addr);
    error |= checkdata(addr, 0, tidx, avail, true, "respTryGetReadAddress"); tidx+=2;

    data8 = testvals[tidx+1];
    sub.respSendReadDataAsync(data8);
    error |= checkdata(0, data8, tidx, true, true, "respSendReadDataAsync"); tidx+=2;

    // ------------------------------------

    releaseBarrier(0);
    sub.respWaitForTransaction();

    avail = sub.respTryGetWrite(&addr, &data32);
    error |= checkdata(addr, data32, tidx, avail, true, "respTryGetWrite"); tidx+=2;

    releaseBarrier(0);
    sub.respWaitForTransaction();

    data32 = testvals[tidx+1];

    avail = sub.respTrySendRead(&addr, data32);
    error |= checkdata(addr, data32, tidx, avail, true, "respTrySendRead"); tidx+=2;

    releaseBarrier(0);
    sub.respWaitForWriteTransaction();

    avail = sub.respTryGetWrite(&addr, &data32);
    error |= checkdata(addr, data32, tidx, avail, true, "respTryGetWrite"); tidx+=2;

    releaseBarrier(0);
    sub.respWaitForReadTransaction();

    data32 = testvals[tidx+1];

    avail = sub.respTrySendRead(&addr, data32);
    error |= checkdata(addr, data32, tidx, avail, true, "respTrySendRead"); tidx+=2;

    // ------------------------------------
    // Test respGetWriteTransactionCount,
    // respGetReadTransactionCount and
    // respGetTransactionCount

    sub.tick(10);

    if (wcount != sub.respGetWriteTransactionCount())
    {
        VPrint("***ERROR: mismatch in write transaction count from respGetWriteTransactionCount. Got %d. Exp %d\n", sub.respGetWriteTransactionCount(), wcount);
        error = true;
    }

    if (rcount != sub.respGetReadTransactionCount())
    {
        VPrint("***ERROR: mismatch in read transaction count from respGetReadTransactionCount. Got %d. Exp %d\n", sub.respGetReadTransactionCount(), rcount);
        error = true;
    }

    totalcount = sub.respGetTransactionCount();

    if (totalcount != (wcount + rcount))
    {
        VPrint("***ERROR: mismatch in  transaction count from respGetTransactionCount. Got %d. Exp %d\n", totalcount, rcount);
        error = true;
    }

    // ------------------------------------
    // End test
    // ------------------------------------

    // Flag to the simulation we're finished, after 10 more iterations
    sub.tick(10, true, error);

    // If ever got this far then sleep forever
    SLEEPFOREVER;
}

