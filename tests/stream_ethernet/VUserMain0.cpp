// ------------------------------------------------------------------------------
//
//  File Name:           VUserMain0.cpp
//  Design Unit Name:    Co-simulation stream Ethernet VC test program
//  Revision:            OSVVM MODELS STANDARD VERSION
//
//  Maintainer:          Simon Southwell      email:  simon.southwell@gmail.com
//  Contributor(s):
//     Simon Southwell   simon.southwell@gmail.com
//
//  Description:
//      Co-simulation burst treaming test using OSVVM Ethernet VC
//
//  Developed by:
//        Simon Southwell
//
//  Revision History:
//    Date      Version    Description
//    03/2023   2023.04    Initial revision
//
//  This file is part of OSVVM.
//
//  Copyright (c) 2023 by [OSVVM Authors](../../AUTHORS.md)
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

// Import OSVVM user API for streams
#include "OsvvmCosimStream.h"

#ifdef _WIN32
#define srandom srand
#define random rand
#endif

#define BUF_SIZE 1024

// I am node 0 context
static int node  = 0;

       uint8_t TestData0[BUF_SIZE];
extern uint8_t TestData1[BUF_SIZE];
static uint8_t RxData[BUF_SIZE];

// ------------------------------------------------------------------------------
// Checkt two data bytes
// ------------------------------------------------------------------------------

bool checkRdata(uint8_t got, uint8_t exp, int idx, int node_num)
{
    bool error = false;

    if (exp != got)
    {
        VPrint("VUserMain%d: ***ERROR*** read 0x%02X, expected 0x%02x at index %d\n", node_num, got, exp, idx);
        error = true;
    }

    return error;
}

// ------------------------------------------------------------------------------
// Main entry point for node 0 virtual processor software
//
// VUserMainX has no calling arguments. If runtime configuration required
// then you'll need to read in a configuration file.
//
// ------------------------------------------------------------------------------

extern "C" void VUserMain0()
{
    VPrint("VUserMain%d()\n", node);

    int      bufidx = 0;
    int      ridx = 0;
    int      bytecount = 0;

    bool                  error = false;
    std::string           test_name("CoSim_ethernet_streams");
    OsvvmCosimStream      txrx(node, test_name);

    // Use node number, inverted, as the random number generator seed.
    srandom(~node);

    // Fill test buffer with random numbers
    for (int idx = 0; idx < BUF_SIZE; idx ++)
    {
        TestData0[bufidx++] = random() & 0xff;
    }

    // reset the buffer index
    bufidx = 0;

    // Send some burst of data over TX stream
    txrx.streamBurstSend(&TestData0[bufidx], 16);  bufidx += 16;
    txrx.streamBurstSend(&TestData0[bufidx], 16);  bufidx += 16;
    txrx.streamBurstSend(&TestData0[bufidx], 256); bufidx += 256;

    // Get some burst data from RX stream
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;

    // Send some burst of data over TX stream
    txrx.streamBurstSend(&TestData0[bufidx], 32);  bufidx += 32;
    txrx.streamBurstSend(&TestData0[bufidx], 64);  bufidx += 64;
    txrx.streamBurstSend(&TestData0[bufidx], 128); bufidx += 128;

    // Get some burst data from RX stream
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;

    // Send some burst of data over TX stream
    txrx.streamBurstSend(&TestData0[bufidx], 256); bufidx += 256;
    txrx.streamBurstSend(&TestData0[bufidx], 256); bufidx += 256;

    // Get some burst data from RX stream
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;
    txrx.streamBurstGet(&RxData[ridx], &bytecount); ridx += bytecount;

    // Check all the received data against that expected
    for (int idx = 0; idx < BUF_SIZE; idx++)
    {
        error |= checkRdata(RxData[idx], TestData1[idx], idx, node);
    }

    // Flag to the simulation we're finished, after 10 more iterations
    txrx.tick(10, true, error);

    // If ever got this far then sleep forever
    SLEEPFOREVER;
}

