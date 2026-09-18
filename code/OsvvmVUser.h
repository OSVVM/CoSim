// =========================================================================
//
//  File Name:         OsvvmVUser.h
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
//      definitions and prototypes for user side code.
//
//  Revision History:
//    Date      Version    Description
//    09/2026   2026.08    Minor fixes for linux compilation
//    09/2025   2026.01    Added support for Set- & Get- burst mode and model options
//    05/2023   2023.05    Adding support for Async, Try and Check transactions
//                         and address bus repsonder
//    01/2023   2023.01    Initial revision
//
//
//  This file is part of OSVVM.
//
//  Copyright (c) 2023 - 2026 by [OSVVM Authors](../AUTHORS.md)
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

#ifndef _OSVVM_VUSER_H_
#define _OSVVM_VUSER_H_

// -------------------------------------------------------------------------
// INCLUDES
// -------------------------------------------------------------------------

#include <stdint.h>

#include "OsvvmVProc.h"
#include "OsvvmVSchedPli.h"
#include "OsvvmVUserVPrint.h"

// -------------------------------------------------------------------------
// DEFINES AND MACROS
// -------------------------------------------------------------------------

#define DELTA_CYCLE             -1
#define NO_DELTA_CYCLE          0
#define GO_TO_SLEEP             0x7fffffff

#ifdef DISABLE_VUSERMAIN_THREAD
#define SLEEPFOREVER            { return; }
#else
#define SLEEPFOREVER            { while(1) VTick(GO_TO_SLEEP, node); }
#endif

#define MAX_INT_LEVEL           256
#define MIN_INT_LEVEL           1

#define HUNDRED_MILLISECS       1000000
#define FIVESEC_TIMEOUT         (50*HUNDRED_MILLISECS)

// -------------------------------------------------------------------------
// TYPE DEFINITIONS
// -------------------------------------------------------------------------

// Pointer to pthread_create compatible function
typedef void *(*pThreadFunc_t)(void *);

// Pointer to VUserMain function type definition
typedef void (*pVUserMain_t)(int node);

// -------------------------------------------------------------------------
// FUNCTION PROTOTYPES
// -------------------------------------------------------------------------

// VUser function prototypes

// Function to advance simulation by a number of clock ticks
extern int       VTick                          (const uint32_t ticks, const bool done = false, const bool error = false, const uint32_t  node = 0);

// GHDL main support function to wait for simultion to be ready to call
extern void      VWaitForSim                    (const uint32_t node = 0);

// OSVVM support function to set the test name
extern void      VSetTestName                   (const char*    data, const int bytesize, const uint32_t node);

// OSVVM VC Set- and Get- for model options and burst mode
extern void      VSetModelOptions               (const int option, const int optval, const uint32_t node = 0);
extern void      VGetModelOptions               (const int option, int  &optval, const uint32_t node = 0);
extern void      VGetModelOptions               (const int option, bool &optval, const uint32_t node = 0);
extern void      VSetBurstMode                  (const int mode, const uint32_t node = 0);
extern int       VGetBurstMode                  (const uint32_t node = 0);

// Overloaded transaction functions for 32 and 64 bit architecture for byte, half-word, word and double-word
extern uint8_t   VTransUserCommon               (const int op, uint32_t *addr, const uint8_t  data, int* status, const int prot = 0, const uint32_t node = 0);
extern uint16_t  VTransUserCommon               (const int op, uint32_t *addr, const uint16_t data, int* status, const int prot = 0, const uint32_t node = 0);
extern uint32_t  VTransUserCommon               (const int op, uint32_t *addr, const uint32_t data, int* status, const int prot = 0, const uint32_t node = 0);
extern uint8_t   VTransUserCommon               (const int op, uint64_t *addr, const uint8_t  data, int* status, const int prot = 0, const uint32_t node = 0);
extern uint16_t  VTransUserCommon               (const int op, uint64_t *addr, const uint16_t data, int* status, const int prot = 0, const uint32_t node = 0);
extern uint32_t  VTransUserCommon               (const int op, uint64_t *addr, const uint32_t data, int* status, const int prot = 0, const uint32_t node = 0);
extern uint64_t  VTransUserCommon               (const int op, uint64_t *addr, const uint64_t data, int* status, const int prot = 0, const uint32_t node = 0);

// Overloaded stream transaction functions for 32 and 64 bit architecture
extern void      VTransBurstCommon              (const int op, const int param, const uint32_t addr, uint8_t* data, const int bytesize, const int prot = 0, const uint32_t node = 0);
extern void      VTransBurstCommon              (const int op, const int param, const uint64_t addr, uint8_t* data, const int bytesize, const int prot = 0, const uint32_t node = 0);

extern int       VTransGetCount                 (const int op, const uint32_t node = 0);
extern void      VTransTransactionWait          (const int op, const uint32_t node = 0);

// Overloaded stream send/check common transaction functions for byte, half-word, word and double-word
extern uint8_t   VStreamUserCommon              (const int op, const uint8_t   data, const int  param = 0,  const uint32_t node = 0);
extern uint16_t  VStreamUserCommon              (const int op, const uint16_t  data, const int  param = 0,  const uint32_t node = 0);
extern uint32_t  VStreamUserCommon              (const int op, const uint32_t  data, const int  param = 0,  const uint32_t node = 0);
extern uint64_t  VStreamUserCommon              (const int op, const uint64_t  data, const int  param = 0,  const uint32_t node = 0);

// Overloaded stream get common transaction functions for byte, half-word, word and double-word
extern bool      VStreamUserGetCommon           (const int op, uint8_t  *rdata, int *status, const uint8_t  wdata, const int param = 0,  const uint32_t node = 0);
extern bool      VStreamUserGetCommon           (const int op, uint16_t *rdata, int *status, const uint16_t wdata, const int param = 0,  const uint32_t node = 0);
extern bool      VStreamUserGetCommon           (const int op, uint32_t *rdata, int *status, const uint32_t wdata, const int param = 0,  const uint32_t node = 0);
extern bool      VStreamUserGetCommon           (const int op, uint64_t *rdata, int *status, const uint64_t wdata, const int param = 0,  const uint32_t node = 0);

// Stream burst send and get common transaction functions
extern bool      VStreamUserBurstSendCommon     (const int op, const int burst_type, uint8_t* data, const int  bytesize, const int param = 0, const uint32_t node = 0);
extern bool      VStreamUserBurstGetCommon      (const int op, const int param,      uint8_t* data, const int  bytesize, int* status,         const uint32_t node = 0);
extern bool      VStreamUserBurstGetCommon      (const int op, const int param,      uint8_t* data,       int *bytesize, int* status,         const uint32_t node = 0);

extern int       VStreamWaitGetCount            (const int op, const bool txnrx, const uint32_t node = 0);

// User function called from VInit to instigate new user thread
extern int       VUser                          (const int node = 0);

// User interrupt callback registering function
extern void      VRegInterrupt                  (const pVUserInt_t func, const uint32_t node = 0);

// User cycle count callback registering function
extern void      VRegUserValue                  (const pVUser_t func,    const uint32_t node = 0);

#endif
