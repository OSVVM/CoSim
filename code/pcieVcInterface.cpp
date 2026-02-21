// =========================================================================
//
//  File Name:         pcieVcInterface.cpp
//  Design Unit Name:
//  Revision:          OSVVM MODELS STANDARD VERSION
//
//  Maintainer:        Simon Southwell email:  simon.southwell@gmail.com
//  Contributor(s):
//    Simon Southwell      simon.southwell@gmail.com
//
//  Description:
//    PCIe VC model C++ interface code between bus independent model port
//    and PCIe link ports
//
//  Revision History:
//    Date      Version    Description
//    09/2025   2026.01    Initial Version
//
//  This file is part of OSVVM.
//
//  Copyright (c) 2025 by [OSVVM Authors](../../AUTHORS.md)
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

#include "OsvvmCosim.h"
#include "pcieVcInterface.h"

//-------------------------------------------------------------
// CalcBe()
//
// Returns the 8 bit LBE/FBE TLP header field, based on
// address and byte length.
//
//-------------------------------------------------------------

static inline int CalcBe (const int inaddr, const int byte_len)
{
    int val, endpos;
    int addr = inaddr & 0x3;

    endpos = addr + byte_len;

    // First BE
    val = (0xf << (addr & 0x3)) & 0xf;

    // Only one double word---combine start and end BEs
    if (endpos <= 4)
    {
        val &= 0xf >> (4-endpos);
    // More than one DW
    }
    else
    {
        endpos %= 4;
        endpos += endpos ? 0 : 4;
        val |= (0xf0 >> (4-endpos)) & 0xf0;
    }
    return val;
}

//--------------------------------------------------------------
// CalcWordCount()
//
// Calculate number of words required
//
//-------------------------------------------------------------

static inline int CalcWordCount (const int byte_len, const int be)
{
    int num_words, adj_byte_len;


    int fbe = be & 0xf;
    int lbe = (be >> 4) & 0xf;


    if (lbe == 0)
    {
        num_words = 1;
    }
    else
    {
        adj_byte_len = byte_len + ((fbe == 0xe) ? 1 : (fbe == 0xc) ? 2 : (fbe == 0x8) ? 3 : 0);
        num_words = (adj_byte_len/4) + ((adj_byte_len%4) ? 1 : 0);

        DebugVPrint("===> CalcWordCount: fbe = %x lbe = %x byte_len = %d adj_byte_len = %d num_words = %d\n", fbe, lbe, byte_len, adj_byte_len, num_words);
    }

    return num_words;
}

//-------------------------------------------------------------
// pcieVcInterface::VUserInput()
//
// Singleton re-entrant wrapper for pcieVcInterface object's
// input callback function, with object instance pointer passed
// in with obj_instance.
//
//-------------------------------------------------------------

void pcieVcInterface::VUserInput(pPkt_t pkt, int status, void* obj_instance)
{
    ((pcieVcInterface*)obj_instance)->InputCallback(pkt, status);
}

//-------------------------------------------------------------
// pcieVcInterface::InputCallback()
//
// pcieVcInterface input packet callback method
//
//-------------------------------------------------------------

void pcieVcInterface::InputCallback(pPkt_t pkt, int status)
{
    int idx;

    PktData_t tlp_type = GET_TLP_TYPE(pkt->data);

    // Process DLLPs
    if (pkt->seq == DLLP_SEQ_ID)
    {
        DebugVPrint("---> VUserInput_0 received DLLP\n");
    }
    // Process completions
    else if (tlp_type == TL_CPL || tlp_type == TL_CPLD || tlp_type == TL_CPLLK || tlp_type == TL_CPLDLK)
    {
        DebugVPrint("---> InputCallback received TLP completion,  sequence %d of %d bytes\n", pkt->seq, pkt->ByteCount);

        // Create a new entry in the queue for the completion
        rxbufq.push(CplDataBuf_t());

        // Save the packet status
        rxbufq.back().pkt_status = status;

        // Extract the completion status from the packet.
        rxbufq.back().cpl_status = GET_CPL_STATUS(pkt->data);
        rxbufq.back().tag        = GET_CPL_TAG(pkt->data);
        rxbufq.back().loaddr     = pkt->data[CPL_LOW_ADDR_OFFSET] & 0x7f;

        // Warn if a bad (non-zero) status
        if (rxbufq.back().cpl_status != CPL_SUCCESS)
        {
            VPrint("**WARNING: InputCallback() received packet with status %s at node %d. Discarding.\n",
                    (rxbufq.back().cpl_status == CPL_UNSUPPORTED) ? "UNSUPPORTED" :
                    (rxbufq.back().cpl_status == CPL_CRS)         ? "CRS"         :
                    (rxbufq.back().cpl_status == CPL_ABORT)       ? "ABORT"       :
                                                                    "UNKNOWN", node);
        }
        // If a successful completion with data, extract the TPL payload data
        else if (pkt->ByteCount)
        {
            // Size the buffer for the incoming data, plus offset 
            rxbufq.back().rxbuf.resize(pkt->ByteCount + 4);

            // Get a pointer to the start of the payload data
            pPktData_t payload = GET_TLP_PAYLOAD_PTR(pkt->data);

            // Fetch data and put in the receive buffer
            DebugVPrint("---> ");
            for (idx = 0; idx < pkt->ByteCount; idx++)
            {
                rxbufq.back().rxbuf[idx] = payload[idx];

                DebugVPrint("%02x ", rxbufq.back().rxbuf[idx]);
                if ((idx % 16) == 15)
                {
                    DebugVPrint("\n---> ");
                }
            }

            if ((idx % 16) != 0)
            {
                DebugVPrint("\n");
            }
        }
    }
    // Process requests (mem reads, config space, i/o, message)
    else
    {
        // Byte offset into first payload word of the start of data, defaulting to 0
        uint32_t offset  = 0;
        uint32_t padding = 0;

        // Create a new entry in the queue for the completion
        reqbufq.push(ReqBuf_t());

        // Save the packet status
        reqbufq.back().pkt_status = status;

        // Get settings

        reqbufq.back().type        = tlp_type;
        reqbufq.back().length      = GET_TLP_LENGTH(pkt->data);
        reqbufq.back().tag         = GET_TLP_TAG(pkt->data);
        reqbufq.back().has_digest  = TLP_HAS_DIGEST(pkt->data);
        reqbufq.back().poisoned    = pkt->data[TLP_TD_BYTE_OFFSET] & 0x40;
        reqbufq.back().attr        = (pkt->data[TLP_TD_BYTE_OFFSET] & 0x30) >> 4;
        reqbufq.back().AT          = (pkt->data[TLP_TD_BYTE_OFFSET] & 0x0c) >> 2;
        reqbufq.back().rid         = GET_TLP_RID(pkt->data) ;

        // For transaction requests with an address, fetch this from the buffer
        if (TLP_HAS_ADDR(tlp_type))
        {
            reqbufq.back().addr_bus.addr = GET_TLP_ADDRESS(pkt->data);
        }
        // For configuration accesses, fetch bus/dev/func numbers and register index
        else if (TLP_IS_CFGSPC(tlp_type))
        {
            reqbufq.back().addr_bus.bus.func =  pkt->data[CFG_FUN_OFFSET] & 0xf;
            reqbufq.back().addr_bus.bus.dev  = (pkt->data[CFG_DEV_OFFSET] >> 4) & 0xf;
            reqbufq.back().addr_bus.bus.bus  =  pkt->data[CFG_BUS_OFFSET] & 0xff;
            reqbufq.back().addr_bus.bus.reg  =  pkt->data[CFG_REG_OFFSET] & 0xfc;
        }

        if (TLP_IS_MSG(tlp_type))
        {
            reqbufq.back().be_msg.msgcode = pkt->data[TLP_BE_OFFSET] & 0xff;
        }
        else
        {
            reqbufq.back().be_msg.be.fbe = pkt->data[TLP_BE_OFFSET] & 0xf;
            reqbufq.back().be_msg.be.lbe = (pkt->data[TLP_BE_OFFSET] >> 4) & 0xf;

            if (!TLP_IS_CFGSPC(tlp_type))
            {
                // Calculate offset into first word based on FBE
                offset = (reqbufq.back().be_msg.be.fbe & 0x1) ? 0 :
                         (reqbufq.back().be_msg.be.fbe & 0x2) ? 1 :
                         (reqbufq.back().be_msg.be.fbe & 0x4) ? 2 :
                                                                3;

                // Calculate number padded bytes at end of data
                padding = (reqbufq.back().be_msg.be.lbe == 0x1) ? 3 :
                          (reqbufq.back().be_msg.be.lbe == 0x3) ? 2 :
                          (reqbufq.back().be_msg.be.lbe == 0x7) ? 1 :
                                                                  0;

                // Adjust address for any offset
                reqbufq.back().addr_bus.addr += offset;
            }
        }


        reqbufq.back().byte_length = pkt->ByteCount - offset - padding;

        if (pkt->ByteCount)
        {
            // Size the buffer for the incoming data, plus offset
            reqbufq.back().wrbuf.resize(pkt->ByteCount + 4);

            // Get a pointer to the start of the payload data
            pPktData_t payload = GET_TLP_PAYLOAD_PTR(pkt->data) + offset;

            // Fetch data into buffer
            for (idx = 0; idx < reqbufq.back().byte_length; idx++)
            {
                reqbufq.back().wrbuf[idx] = payload[idx];
            }
        }
    }

    // Once input packet is finished with, the allocated space *must* be freed.
    // All input packets have their own memory space to avoid overwrites with
    // shared buffers.
    DISCARD_PACKET(pkt);
}

//-------------------------------------------------------------
// pcieVcInterface::run()
//
// pcieVcInterface main program.
//
//-------------------------------------------------------------

void pcieVcInterface::run(void)
{
    int                error = 0;
    bool               end   = false;
    int                halt  = 0;

    int                byteidx;
    unsigned           operation;
    unsigned           int_to_model;
    unsigned           option;
    int                pad_offset;

    uint32_t           status;
    uint32_t           be;
    uint32_t           popdata;
    uint32_t           memword;

    uint64_t           rdata;
    uint64_t           wdata;
    uint64_t           wdatawidth;
    uint64_t           rdatawidth;
    uint64_t           word_len;
    uint64_t           remaining_len;
    uint64_t           address;
    uint64_t           addrlo;

    pcie_trans_mode_t  trans_mode;
    bool               rd_lck;
    unsigned           cmplrid;
    unsigned           cmplcid;
    unsigned           cmpltag;
    unsigned           localtag;
    unsigned           enable_auto;

    VRead(ENABLE_AUTO_ADDR, &enable_auto, DELTACYCLE, node);

    if (enable_auto)
    {
        runAutoEp();
    }
    else
    {
        // Initialise PCIe VHost, with input callback function and no user pointer.
        pcie->initialisePcie(pcieVcInterface::VUserInput, this);

        // No internal memory or auto-completion
        pcie->configurePcie(CONFIG_DISABLE_MEM);
        pcie->configurePcie(CONFIG_DISABLE_UR_CPL);

        pcie->getPcieVersionStr(sbuf, STRBUFSIZE);
        VPrint("  %s\n", sbuf);

        DebugVPrint("pcieVcInterface::run: on node %d\n", node);

        // Fetch the model generics
        VRead(LANESADDR,     &link_width,       DELTACYCLE, node);
        VRead(PIPE_ADDR,     &pipe_mode,        DELTACYCLE, node);
        VRead(SCRAMBLE_ADDR, &no_scramble_mode, DELTACYCLE, node);
        VRead(EP_ADDR,       &ep_mode,          DELTACYCLE, node);
        VRead(EN_ECRC_ADDR,  &digest_mode,      DELTACYCLE, node);
        VRead(REQID_ADDR,    &rid,              DELTACYCLE, node);

        // When in PIPE mode, disable codec
        if (pipe_mode)
        {
            pcie->configurePcie(CONFIG_DISABLE_8B10B);
        }
        
        // If configured, disable scrambling
        if (no_scramble_mode)
        {
            pcie->configurePcie(CONFIG_DISABLE_SCRAMBLING);
        }

        // Make sure the link is out of electrical idle
        VWrite(LINK_STATE, 0, DELTACYCLE, node);

        // Use node number as seed
        pcie->pcieSeed(node);

        // Send out idles until reset de-asserted
        do
        {
            pcie->sendIdle();
            VRead(RESET_STATE, &reset_state, CLOCKEDCYCLE, node);
        } while(reset_state);

        // Loop forever, processing commands and driving the PCIe link
        while (!error && !end)
        {
            // Ack transaction
            VWrite(ACKTRANS, 1, DELTACYCLE, node);

            // Check if there is a new transaction (delta)
            VRead(GETNEXTTRANS, &operation, DELTACYCLE, node);

            // Make sure the tag is at a valid value
            if (tag >= MAX_TAG)
            {
                tag = 0;
            }

            switch (operation)
            {

            case GET_MODEL_OPTIONS :

                VRead(GETOPTIONS,    &option,       DELTACYCLE, node);
                switch (option)
                {
                case GETMEMDATA:
                    memword = pcie->readRamWord(mem_addr, LITTLE_END);
                    mem_addr += 4;
                    VWrite(SETINTFROMMODEL, memword, DELTACYCLE, node);
                    break;
                default:
                    VPrint("pcieVcInterface::run : ***ERROR. Unrecognised GET_MODEL_OPTIONS option (%d)\n", option);
                    error++;
                    break;
                }

                break;

            case SET_MODEL_OPTIONS :

                VRead(GETOPTIONS,    &option,       DELTACYCLE, node);
                VRead(GETINTTOMODEL, &int_to_model, DELTACYCLE, node);

                // If a PCIe C model config option, pass straight to model
                if (option < VCOPTIONSTART)
                {
                    pcie->configurePcie(static_cast<config_t>(option), int_to_model);
                }
                else
                {
                    switch(option)
                    {
                    case ENDMODELRUN:
                        end = true;
                        halt = int_to_model;
                        break;

                    case SETCFGSPCOFFSET:
                        cfgspc_offset = int_to_model;
                        break;

                    case SETCFGSPC:
                        if (ep_mode)
                        {
                            pcie->writeConfigSpace(cfgspc_offset, int_to_model);
                        }
                        break;

                    case SETCFGSPCMASK:
                        if (ep_mode)
                        {
                            pcie->writeConfigSpaceMask(cfgspc_offset, int_to_model);
                        }
                        break;

                    case SETMEMENDIANNESS:
                        endian_mode = int_to_model ? LITTLE_END : BIG_END;
                        break;

                    case SETMEMADDRLO:
                        mem_addr = (mem_addr & ~0xffffffffULL) | (uint64_t)((uint32_t)int_to_model & ~0xfffffffc);
                        break;

                    case SETMEMADDRHI:
                        mem_addr = (mem_addr &  0xffffffffULL) | ((uint64_t)((uint32_t)int_to_model) << 32);
                        break;

                    case SETMEMDATA:
                        pcie->writeRamWord(mem_addr, (uint32_t)int_to_model, LITTLE_END);
                        mem_addr += 4;
                        break;

                    default:
                        VPrint("pcieVcInterface::run : ***ERROR. Unrecognised SET_MODEL_OPTIONS option (%d)\n", option);
                        error++;
                    }
                }
                break;

            case WRITE_OP :
            case ASYNC_WRITE_ADDRESS :

                VRead64(GETADDRESS,     &address,    DELTACYCLE, node);
                VRead64(GETDATATOMODEL, &wdata,      DELTACYCLE, node);
                VRead64(GETDATAWIDTH,   &wdatawidth, DELTACYCLE, node);

                trans_mode = (pcie_trans_mode_t)VWrite(GETPARAMS, PARAM_TRANS_MODE, DELTACYCLE, node);

                localtag = VWrite(GETPARAMS, PARAM_REQTAG, DELTACYCLE, node);
                if (localtag >= 0 && localtag < TLP_TAG_AUTO && trans_mode != CPL_TRANS && trans_mode != PART_CPL_TRANS)
                {
                    tag = localtag & (MAX_TAG-1);
                }

                // For completions, the data bytes will start at an offset into the first word, determined
                // by the address low 2 bits
                pad_offset = (trans_mode == CPL_TRANS) ? (address & 0x3) : 0;

                // Place data into a PCIe model byte buffer, padding beginning with 0s if needed
                for (byteidx = -pad_offset; byteidx < int(wdatawidth/8); byteidx++)
                {
                    txdatabuf[byteidx + pad_offset] = (byteidx < 0 ) ? 0 : ((wdata >> (byteidx<<3)) & 0xff);
                }

                switch(trans_mode)
                {
                case MEM_TRANS :
                    // Do a posted memory write (no completion to wait for)
                    pcie->memWrite(address, txdatabuf, wdatawidth/8, tag++, rid, false, digest_mode);
                    break;

                case MSG_TRANS :
                    if (operation != ASYNC_WRITE_ADDRESS)
                    {
                        pcie->message(address, txdatabuf, wdatawidth/8, tag++, rid, false, digest_mode);
                    }
                    else
                    {
                        pcie->message(address, NULL, 0, tag++, rid, false, digest_mode);
                    }
                    break;

                case CFG_SPC_TRANS :
                    if (!ep_mode)
                    {
                        pcie->cfgWrite(address, txdatabuf, wdatawidth/8, tag++, rid, false, digest_mode);

                        // Non-posted transaction, so do a wait for the status completion
                        pcie->waitForCompletion();

                        VWrite64(SETPARAMS, (uint64_t)rxbufq.front().pkt_status | ((uint64_t)PARAM_PKT_STATUS  << 32), DELTACYCLE, node);
                        VWrite64(SETPARAMS, (uint64_t)rxbufq.front().cpl_status | ((uint64_t)PARAM_CMPL_STATUS << 32), DELTACYCLE, node);
                        VWrite64(SETPARAMS, (uint64_t)rxbufq.front().tag        | ((uint64_t)PARAM_CMPL_RX_TAG << 32), DELTACYCLE, node);

                        // Flag any bad status
                        if (rxbufq.front().cpl_status)
                        {
                            VPrint("pcieVcInterface::run : ***ERROR. Received bad status (%d) on WRITE_OP\n", rxbufq.front().cpl_status);
                            error++;
                        }
                    }
                    else
                    {
                        VPrint("pcieVcInterface::run : ***ERROR. Issuing a configuration space write when an endpoint on WRITE_OP\n");
                        error++;
                    }
                    break;

                case IO_TRANS :

                    pcie->ioWrite(address, txdatabuf, wdatawidth/8, tag++, rid, false, digest_mode);

                    // Non-posted transaction, so do a wait for the status completion
                    pcie->waitForCompletion();

                    VWrite64(SETPARAMS, (uint64_t)rxbufq.front().pkt_status | ((uint64_t)PARAM_PKT_STATUS  << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)rxbufq.front().cpl_status | ((uint64_t)PARAM_CMPL_STATUS << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)rxbufq.front().tag        | ((uint64_t)PARAM_CMPL_RX_TAG << 32), DELTACYCLE, node);

                    // Flag any bad status
                    if (rxbufq.front().cpl_status)
                    {
                        VPrint("pcieVcInterface::run : ***WARNING. Received bad status (%d) on WRITE_OP\n", rxbufq.front().cpl_status);
                        //error++;
                    }
                    break;

                case CPL_TRANS :
                case PART_CPL_TRANS :

                    cmpltag       = VWrite(GETPARAMS, PARAM_CMPLRTAG,   DELTACYCLE, node);
                    cmplrid       = VWrite(GETPARAMS, PARAM_CMPLRID,    DELTACYCLE, node);
                    cmplcid       = VWrite(GETPARAMS, PARAM_CMPLCID,    DELTACYCLE, node);
                    rd_lck        = VWrite(GETPARAMS, PARAM_RDLCK,      DELTACYCLE, node);
                    status        = VWrite(GETPARAMS, PARAM_CMPLSTATUS, DELTACYCLE, node);

                    if (operation == ASYNC_WRITE_ADDRESS)
                    {
                        be            = 0;
                        word_len      = 0;
                        remaining_len = 0;
                    }
                    else
                    {

                        be            = CalcBe(address, wdatawidth/8);
                        word_len      = CalcWordCount(wdatawidth/8, be);

                        if (trans_mode == CPL_TRANS)
                        {
                            remaining_len = word_len;
                        }
                        else
                        {
                            remaining_len = VWrite(GETPARAMS, PARAM_CMPLRLEN,   DELTACYCLE, node);
                        }
                    }

                    // Do a completion (effectively posted, so nothing to wait for)
                    pcie->partCompletionLockDelay(address & CMPL_ADDR_MASK, txdatabuf, status, be & 0xf, (be >> 4) & 0xf, remaining_len, word_len,
                                                  cmpltag, cmplcid, cmplrid, rd_lck, digest_mode, false, false);
                    break;

                default :
                    VPrint("pcieVcInterface::run : ***ERROR. Unrecognised transaction mode on WRITE_OP (%d)\n", trans_mode);
                    error++;
                    break;
                }

                // For non-posted transactions, pop the completion from the queue
                //if (trans_mode != MEM_TRANS && trans_mode != MSG_TRANS/* && trans_mode != CPL_TRANS*/)
                if (trans_mode == CFG_SPC_TRANS || trans_mode == IO_TRANS)
                {
                    rxbufq.pop();
                }
                break;

            case READ_OP :
            case ASYNC_READ_ADDRESS :
            case READ_ADDRESS :
            case READ_DATA :
            case ASYNC_READ_DATA :

                VRead64(GETDATAWIDTH, &rdatawidth, DELTACYCLE, node);

                if (operation != READ_DATA && operation != ASYNC_READ_DATA)
                {
                    VRead64(GETADDRESS,   &address,    DELTACYCLE, node);

                    trans_mode = (pcie_trans_mode_t)VWrite(GETPARAMS, PARAM_TRANS_MODE, DELTACYCLE, node);

                    localtag = VWrite(GETPARAMS, PARAM_REQTAG, DELTACYCLE, node);
                    if (localtag >= 0 && localtag < TLP_TAG_AUTO)
                    {
                        tag = localtag & (MAX_TAG-1);
                    }

                    switch(trans_mode)
                    {
                    case MEM_TRANS :
                        rd_lck     = (pcie_trans_mode_t)VWrite(GETPARAMS, PARAM_RDLCK,      DELTACYCLE, node);

                        // Instigate a memory read
                        pcie->memReadLockDigest(address, rdatawidth/8, tag++, rid, rd_lck, digest_mode, false);
                        break;

                    case CFG_SPC_TRANS :
                        if (!ep_mode)
                        {
                            // Instigate a configuration space read
                            pcie->cfgRead(address, rdatawidth/8, tag++, rid, false, digest_mode);
                        }
                        else
                        {
                            VPrint("pcieVcInterface::run : ***ERROR. Issuing a configuration space read when an endpoint on READ_OP\n");
                            error++;
                        }
                        break;

                    case IO_TRANS :
                        // Instigate a configuration space read
                        pcie->ioRead(address, rdatawidth/8, tag++, rid, false, digest_mode);
                        break;

                    default :
                        VPrint("pcieVcInterface::run : ***ERROR. Unrecognised transaction mode on WRITE_OP (%d)\n", trans_mode);
                        error++;
                        break;
                    }
                }

                if (operation != READ_ADDRESS && operation != ASYNC_READ_ADDRESS)
                {
                    // Blocking read, so do a wait for the completion
                    pcie->waitForCompletion();

                    VWrite64((uint64_t)SETPARAMS, (uint64_t)rxbufq.front().cpl_status | ((uint64_t)PARAM_CMPL_STATUS << 32), DELTACYCLE, node);
                    VWrite64((uint64_t)SETPARAMS, (uint64_t)rxbufq.front().pkt_status | ((uint64_t)PARAM_PKT_STATUS  << 32), DELTACYCLE, node);

                    // If a successful completion returned, extract data
                    if (!rxbufq.front().cpl_status && !rxbufq.front().pkt_status)
                    {
                        VWrite64((uint64_t)SETPARAMS, (uint64_t)rxbufq.front().tag    | ((uint64_t)PARAM_CMPL_RX_TAG << 32), DELTACYCLE, node);

                        // Get data
                        addrlo = rxbufq.front().loaddr & 0x3ULL;
                        for (rdata = 0, byteidx = 0; byteidx < (rdatawidth/8); byteidx++)
                        {
                            rdata |= ((uint32_t)rxbufq.front().rxbuf[byteidx+addrlo] & 0xff) << (8 * byteidx);
                        }
                    }
                    else
                    {
                        rdata = 0;
                        VWrite(SETBOOLFROMMODEL, 1, DELTACYCLE, node);
                    }

                    // Update transaction record return data
                    VWrite64(SETDATAFROMMODEL, rdata, DELTACYCLE, node);

                    // Pop the received packet from the queue
                    if (!rxbufq.empty())
                    {
                        rxbufq.pop();
                    };
                }

                break;

            case WRITE_BURST :

                VRead64(GETADDRESS,     &address,    DELTACYCLE, node);
                VRead64(GETDATAWIDTH,   &wdatawidth, DELTACYCLE, node);

                trans_mode = (pcie_trans_mode_t)VWrite(GETPARAMS, PARAM_TRANS_MODE, DELTACYCLE, node);

                localtag = VWrite(GETPARAMS, PARAM_REQTAG, DELTACYCLE, node);
                if (localtag >= 0 && localtag < TLP_TAG_AUTO && trans_mode != CPL_TRANS && trans_mode != PART_CPL_TRANS)
                {
                    tag = localtag & (MAX_TAG-1);
                }

                // For completions, the data bytes will start at an offset into the first word, determined
                // by the address low 2 bits
                pad_offset = (trans_mode == CPL_TRANS || trans_mode == PART_CPL_TRANS) ? (address & BYTE_OFFSET_MASK) : 0;

                for (int pidx = -pad_offset; pidx < (int)wdatawidth; pidx++)
                {
                    if (pidx < 0)
                    {
                        txdatabuf[pidx + pad_offset] = 0;
                    }
                    else
                    {
                      VRead(POPWDATA, &popdata, DELTACYCLE, node);
                      txdatabuf[pidx + pad_offset] = popdata & 0xff;
                    }
                }

                switch(trans_mode)
                {
                case MEM_TRANS :

                    pcie->memWrite(address, txdatabuf, wdatawidth, tag++, rid, false, digest_mode);
                    break;

                case CPL_TRANS :
                case PART_CPL_TRANS :

                    cmpltag       = VWrite(GETPARAMS, PARAM_CMPLRTAG,   DELTACYCLE, node);
                    cmplrid       = VWrite(GETPARAMS, PARAM_CMPLRID,    DELTACYCLE, node);
                    cmplcid       = VWrite(GETPARAMS, PARAM_CMPLCID,    DELTACYCLE, node);
                    rd_lck        = VWrite(GETPARAMS, PARAM_RDLCK,      DELTACYCLE, node);
                    status        = VWrite(GETPARAMS, PARAM_CMPLSTATUS, DELTACYCLE, node);

                    be            = CalcBe(address, wdatawidth);
                    word_len      = CalcWordCount(wdatawidth, be);

                    if (trans_mode == CPL_TRANS)
                    {
                        remaining_len = word_len;
                    }
                    else
                    {
                        remaining_len = VWrite(GETPARAMS, PARAM_CMPLRLEN,   DELTACYCLE, node);
                    }

                    // Do a completion (effectively posted, so nothing to wait for). Align the address to a word
                    // boundary and only use the needed lower 7 bits.
                    pcie->partCompletionLockDelay(address & CMPL_ADDR_MASK, txdatabuf, status, be & 0xf, (be >> 4) & 0xf, remaining_len, word_len,
                                                  cmpltag, cmplcid, cmplrid,
                                                  rd_lck, digest_mode, false, false);
                    break;

                default:
                    break;
                }

                break;

            case READ_BURST :

                VRead64(GETADDRESS,     &address,    DELTACYCLE, node);
                VRead64(GETDATAWIDTH,   &rdatawidth, DELTACYCLE, node);

                trans_mode    = (pcie_trans_mode_t)VWrite(GETPARAMS, PARAM_TRANS_MODE, DELTACYCLE, node);
                rd_lck        = VWrite(GETPARAMS, PARAM_RDLCK, DELTACYCLE, node);

                localtag = VWrite(GETPARAMS, PARAM_REQTAG, DELTACYCLE, node);
                if (localtag >= 0 && localtag < TLP_TAG_AUTO)
                {
                    tag = localtag & (MAX_TAG-1);
                }

                pcie->memReadLockDigest(address, rdatawidth, tag++, rid, rd_lck, digest_mode, false);

                // Blocking read, so do a wait for the completion
                pcie->waitForCompletion();

                VWrite64(SETPARAMS, (uint64_t)rxbufq.front().pkt_status | ((uint64_t)PARAM_PKT_STATUS  << 32), DELTACYCLE, node);
                VWrite64(SETPARAMS, (uint64_t)rxbufq.front().cpl_status | ((uint64_t)PARAM_CMPL_STATUS << 32), DELTACYCLE, node);
                VWrite64(SETPARAMS, (uint64_t)rxbufq.front().tag        | ((uint64_t)PARAM_CMPL_RX_TAG << 32), DELTACYCLE, node);

                // If a successful completion returned, extract data
                if (!rxbufq.front().cpl_status)
                {
                    // Get data
                    addrlo = rxbufq.front().loaddr & 0x3ULL;
                    for (byteidx = 0; byteidx < rdatawidth; byteidx++)
                    {
                        VWrite(PUSHRDATA, rxbufq.front().rxbuf[byteidx+addrlo], DELTACYCLE, node);
                    }
                    rxbufq.pop();
                }
                else
                {
                    rdata = 0;
                    VWrite(SETBOOLFROMMODEL, 1, DELTACYCLE, node);
                }

                break;

            case WAIT_FOR_CLOCK :

                VRead(GETINTTOMODEL, &int_to_model, DELTACYCLE, node);
                pcie->sendIdle(int_to_model);
                break;

            case WAIT_FOR_TRANSACTION:

                pcie->waitForCompletion();
                break;

            case EXTEND_DIRECTIVE_OP:

                VRead(GETOPTIONS,    &option,       DELTACYCLE, node);
                switch(option)
                {
                // Do PHY layer link training initialisation.
                case INITPHY:

                    InitLink(link_width, node);
                    break;

                // Do data link layer flow control initialisation
                case INITDLL:

                    // Initialise flow control
                    pcie->initFc();
                    break;

                default:
                    VPrint("pcieVcInterface::run : ***ERROR. Unrecognised EXTEND_DIRECTIVE_OP option (%d)\n", option);
                    error++;
                    break;
                }

                break;

            case EXTEND_OP :

               VRead(GETOPTIONS,    &option,       DELTACYCLE, node);
               switch (option)
               {
               case WAIT_FOR_TRANS:
                   // Blocking, so wait for something in the request buffer queue
                   while (reqbufq.empty())
                   {
                       SendIdle(1, node);
                   }
                   break;
                case TRY:
                    if (reqbufq.empty())
                    {
                        VWrite(SETBOOLFROMMODEL, 0, DELTACYCLE, node);
                    }
                    else
                    {
                        VWrite(SETBOOLFROMMODEL, 1, DELTACYCLE, node);
                    }
                    break;
                default:
                    VPrint("pcieVcInterface::run : ***ERROR. Unrecognised EXTEND_OP option (%d)\n", option);
                    error++;
                    break;
                }

                if (!reqbufq.empty())
                {
                    uint32_t tlp_type = reqbufq.front().type;

                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().pkt_status | ((uint64_t)PARAM_REQ_PKT_STATUS << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().type       | ((uint64_t)PARAM_REQ_TYPE       << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().tag        | ((uint64_t)PARAM_REQ_TAG        << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().rid        | ((uint64_t)PARAM_REQ_RID        << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().has_digest | ((uint64_t)PARAM_REQ_DIGEST     << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().poisoned   | ((uint64_t)PARAM_REQ_POISONED   << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().attr       | ((uint64_t)PARAM_REQ_ATTR       << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().AT         | ((uint64_t)PARAM_REQ_AT         << 32), DELTACYCLE, node);

                    if (TLP_HAS_ADDR(tlp_type))
                    {
                        VWrite64(SETPARAMS,  (reqbufq.front().addr_bus.addr        & 0xffffffff) | ((uint64_t)PARAM_REQ_ADDR   << 32), DELTACYCLE, node);
                        VWrite64(SETPARAMS, ((reqbufq.front().addr_bus.addr >> 32) & 0xffffffff) | ((uint64_t)PARAM_REQ_ADDRHI << 32), DELTACYCLE, node);
                    }
                    else
                    {
                        VWrite64(SETPARAMS, (uint64_t)reqbufq.front().addr_bus.bus.func | ((uint64_t)PARAM_REQ_CFG_FUNC << 32), DELTACYCLE, node);
                        VWrite64(SETPARAMS, (uint64_t)reqbufq.front().addr_bus.bus.dev  | ((uint64_t)PARAM_REQ_CFG_DEV  << 32), DELTACYCLE, node);
                        VWrite64(SETPARAMS, (uint64_t)reqbufq.front().addr_bus.bus.bus  | ((uint64_t)PARAM_REQ_CFG_BUS  << 32), DELTACYCLE, node);
                        VWrite64(SETPARAMS, (uint64_t)reqbufq.front().addr_bus.bus.reg  | ((uint64_t)PARAM_REQ_CFG_REG  << 32), DELTACYCLE, node);
                    }

                    if (TLP_IS_MSG(tlp_type))
                    {
                        VWrite64(SETPARAMS, (uint64_t)reqbufq.front().be_msg.msgcode     | ((uint64_t)PARAM_REQ_MSG_CODE << 32), DELTACYCLE, node);
                    }
                    else
                    {
                        VWrite64(SETPARAMS, (uint64_t)reqbufq.front().be_msg.be.fbe      | ((uint64_t)PARAM_REQ_FBE << 32), DELTACYCLE, node);
                        VWrite64(SETPARAMS, (uint64_t)reqbufq.front().be_msg.be.lbe      | ((uint64_t)PARAM_REQ_LBE << 32), DELTACYCLE, node);
                    }

                    // If there's write data, push to FIFO
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().length      | ((uint64_t)PARAM_REQ_LENGTH   << 32), DELTACYCLE, node);
                    VWrite64(SETPARAMS, (uint64_t)reqbufq.front().byte_length | ((uint64_t)PARAM_REQ_BYTE_LEN << 32), DELTACYCLE, node);

                    if (TLP_HAS_DATA(tlp_type) && reqbufq.front().byte_length)
                    {
                        for (int byteidx = 0; byteidx < (reqbufq.front().byte_length); byteidx++)
                        {
                            VWrite(PUSHRDATA, reqbufq.front().wrbuf[byteidx], DELTACYCLE, node);
                        }
                    }

                    // Pop transaction from the queue
                    reqbufq.pop();
                }
                break;

            default :
                VPrint("pcieVcInterface::run : ***ERROR. Unrecognised operation (%d)\n", operation);
                error++;
                break;
            }
        }

        if (error)
        {
            VPrint("***Error: pcieVcInterface::run() had an error\n");

            // Send the simulation with an error
            VWrite(PVH_FATAL, error, 0, node);
        }
        else if (end)
        {
            if (halt == FINISHSIM)
            {
                VWrite(PVH_FINISH, 0, 0, node);
            }
            else if (halt == STOPSIM)
            {
                VWrite(PVH_STOP, 0, 0, node);
            }
        }

        // If reached here without stop/finish, send out idles forever
        // to allow simulation to continue
        while (true)
        {
            pcie->sendIdle(10000);
        }
    }
}

//-------------------------------------------------------------
// runAutoEp()
//
// Program for automatic EP model
//
//-------------------------------------------------------------
void pcieVcInterface::runAutoEp()
{
    unsigned reset_state = 0;
    unsigned pipe_mode, link_width, init_phy;

    // Create an API object for this node
    pcieModelClass* pcie = new pcieModelClass(node);

    // Initialise PCIe VHost, with input callback function and no user pointer.
    pcie->initialisePcie(pcieVcInterface::VUserInputAutoEp, &node);

    // Enable internal memory and auto-completion
    pcie->configurePcie(CONFIG_ENABLE_MEM);
    pcie->configurePcie(CONFIG_ENABLE_UR_CPL);

    // Fetch configuration status
    VRead(PIPE_ADDR,    &pipe_mode,  pcieVcInterface::DELTACYCLE, node);
    VRead(LANESADDR,    &link_width, pcieVcInterface::DELTACYCLE, node);
    VRead(INITPHY_ADDR, &init_phy,   pcieVcInterface::DELTACYCLE, node);

    // Set model into PIPE mode, if requested
    if (pipe_mode)
    {
        pcie->configurePcie(CONFIG_DISABLE_SCRAMBLING);
        pcie->configurePcie(CONFIG_DISABLE_8B10B);
    }

    // Make sure the link is out of electrical idle
    VWrite(LINK_STATE, 0, 0, node);

    // Use node number as seed
    pcie->pcieSeed(node);

    // Construct an endpoint PCIe configuration space setting
    ConfigureType0PcieCfg();

    // Wait until reset is inactivated
    do
    {
        pcie->sendIdle();
        VRead(RESET_STATE, &reset_state, 0, node);
    } while(reset_state);


    // If configured to so do, initialise the PHY link for given number of lanes
    if (init_phy)
    {
        InitLink(link_width, node);
    }

    // Initialise DLL flow control
    pcie->initFc();

    // Send out idles forever
    while (true)
    {
        pcie->sendIdle(10000);
    }
}

//-------------------------------------------------------------
// VUserInputAutoEp()
//
// Consumes the unhandled input Packets and display a message
//
//-------------------------------------------------------------

void pcieVcInterface::VUserInputAutoEp(pPkt_t pkt, int status, void* usrptr)
{
    int node = *((int*)usrptr);

    if (pkt->seq == DLLP_SEQ_ID)
    {
        DebugVPrint("---> VUserInputAutoEp received DLLP\n");
    }
    else
    {
        // Get transaction type
        PktData_t type = GET_TLP_TYPE(pkt->data);

        // Get transaction length if a payload type, else set to 0
        int length  = (type & 0x40) ? (GET_TLP_LENGTH(pkt->data)) : 0;

        VPrint("===> VUserInputAutoEp @node%d received %s TLP sequence %d with payload length %d words at cycle %d\n", node,
                     (type & ~0x21) == TL_MRD32  ? "mem read"           :
                     (type & ~0x20) == TL_MWR32  ? "mem_write"          :
                     type           == TL_IORD   ? "i/o read"           :
                     type           == TL_IOWR   ? "i/o write"          :
                     (type & ~0x01) == TL_CFGRD0 ? "config space read"  :
                     (type & ~0x01) == TL_CFGWR0 ? "config space write" :
                     (type & ~0x07) == TL_MSG    ? "message"            :
                     (type & ~0x07) == TL_MSGD   ? "message with data"  :
                     (type & ~0x41) == TL_CPL    ? "completion"         :
                                                   "unknown",
                     pkt->seq, length, pkt->TimeStamp);
    }

    // Once packet is finished with, the allocated space *must* be freed.
    // All input packets have their own memory space to avoid overwrites
    // which shared buffers.
    DISCARD_PACKET(pkt);
}

//-------------------------------------------------------------
// ConfigureType0PcieCfg()
//
// Configure the configuration space buffer for an endpoint
// (type 0) PCIe configuration space with PCI compatible header,
// PCIe capabilities, MSI capabilities and Power management
// capabilities, along with some default values.
//
//-------------------------------------------------------------

void pcieVcInterface::ConfigureType0PcieCfg (void)
{
    unsigned        next_cap_ptr = 0;

    pcieModelClass *pcie         = new pcieModelClass(node);

    // -------------------------------------
    // PPCI compatible header
    // -------------------------------------

    cfg_spc_type0_t     type0;
    cfg_spc_type0_t     type0_mask;
    cfg_spc_pcie_caps_t pcie_caps;
    cfg_spc_pcie_caps_t pcie_caps_mask;
    cfg_spc_msi_caps_t  msi_caps;
    cfg_spc_msi_caps_t  msi_caps_mask;
    pwr_mgmnt_caps_t    pwr_mgmnt_caps;
    pwr_mgmnt_caps_t    pwr_mgmnt_caps_mask;

    // Default to all zeros and read-only
    for (int idx = 0; idx < (CFG_PCI_HDR_SIZE_BYTES/4); idx++)
    {
        type0.words[idx]      = 0x00000000;
        type0_mask.words[idx] = 0xffffffff;
    }

    next_cap_ptr += CFG_PCI_HDR_SIZE_BYTES;

    // Construct PCI compatible structure value, and set writable bits where appropriate
    type0.type0_struct.vendor_id               = 0x14fc;
    type0.type0_struct.device_id               = 0x0002;
    type0.type0_struct.command                 = 0x0006;     type0_mask.type0_struct.command = 0xfab8;
    type0.type0_struct.status                  = 0x0010;
    type0.type0_struct.revision_id             = 0x01;
    type0.type0_struct.prog_if                 = 0x00;       // don't care
    type0.type0_struct.subclass                = 0x80;       // other
    type0.type0_struct.class_code              = 0x02;       // network controller
    type0.type0_struct.cache_line_size         = 0x00;       type0_mask.type0_struct.cache_line_size         = 0x00;
    type0.type0_struct.bar[0]                  = 0x00000008; type0_mask.type0_struct.bar[0]                  = 0x00000fff; // 32-bit, prefetchable, 4K
    type0.type0_struct.bar[1]                  = 0x00000008; type0_mask.type0_struct.bar[1]                  = 0x000003ff; // 32-bit, prefetchable, 1K
    type0.type0_struct.bar[2]                  = 0x00000000;
    type0.type0_struct.bar[3]                  = 0x00000000;
    type0.type0_struct.bar[4]                  = 0x00000000;
    type0.type0_struct.bar[5]                  = 0x00000000;
    type0.type0_struct.expansion_rom_base_addr = 0x00000000; type0_mask.type0_struct.expansion_rom_base_addr = 0x000007fe;
    type0.type0_struct.capabilities_ptr        = next_cap_ptr;

    // Update config space and mask with values
    for (int idx = 0; idx < (CFG_PCI_HDR_SIZE_BYTES/4); idx++)
    {
        pcie->writeConfigSpace     (next_cap_ptr - CFG_PCI_HDR_SIZE_BYTES + idx*4, type0.words[idx]);
        pcie->writeConfigSpaceMask (next_cap_ptr - CFG_PCI_HDR_SIZE_BYTES + idx*4, type0_mask.words[idx]);
    }

    // -------------------------------------
    // PCIe capability
    // -------------------------------------

    // Default to all zeros and read-only
    for (int idx = 0; idx < (CFG_PCIE_CAPS_SIZE_BYTES/4); idx++)
    {
        pcie_caps.words[idx]      = 0x00000000;
        pcie_caps_mask.words[idx] = 0xffffffff;
    }

    next_cap_ptr += CFG_PCIE_CAPS_SIZE_BYTES;

    pcie_caps.pcie_caps_struct.cap_id         = 0x10;
    pcie_caps.pcie_caps_struct.next_cap_ptr   = next_cap_ptr;
    pcie_caps.pcie_caps_struct.device_caps    = 0x00000001;   // max payload = 256 bytes
    pcie_caps.pcie_caps_struct.device_control = 0x2810;       pcie_caps_mask.pcie_caps_struct.device_control = 0x0000;
    pcie_caps.pcie_caps_struct.link_caps      = 0x0003fc12;
    pcie_caps.pcie_caps_struct.link_control   = 0x0000;       pcie_caps_mask.pcie_caps_struct.link_control   = 0xf004;
    pcie_caps.pcie_caps_struct.link_status    = 0x0091;
    pcie_caps.pcie_caps_struct.link_control2  = 0x0002;       pcie_caps_mask.pcie_caps_struct.link_control2  = 0xe06f;

    for (int idx = 0; idx < (CFG_PCIE_CAPS_SIZE_BYTES/4); idx++)
    {
        pcie->writeConfigSpace     (next_cap_ptr - CFG_PCIE_CAPS_SIZE_BYTES + idx*4, pcie_caps.words[idx]);
        pcie->writeConfigSpaceMask (next_cap_ptr - CFG_PCIE_CAPS_SIZE_BYTES + idx*4, pcie_caps_mask.words[idx]);
    }

    // -------------------------------------
    // MSI capability
    // -------------------------------------

    // Default to all zeros and read-only
    for (int idx = 0; idx < (CFG_MSI_CAPS_SIZE_BYTES/4); idx++)
    {
        msi_caps.words[idx]      = 0x00000000;
        msi_caps_mask.words[idx] = 0xffffffff;
    }

    next_cap_ptr += CFG_MSI_CAPS_SIZE_BYTES;

    msi_caps.msi_caps_struct.cap_id           = 0x05;
    msi_caps.msi_caps_struct.next_cap_ptr     = next_cap_ptr;
    msi_caps.msi_caps_struct.mess_control     = 0x0080;     msi_caps_mask.msi_caps_struct.mess_control = 0xff8e;
    msi_caps.msi_caps_struct.mess_addr_lo     = 0x00000000; msi_caps_mask.msi_caps_struct.mess_addr_lo = 0x00000003;
    msi_caps.msi_caps_struct.mess_addr_hi     = 0x00000000; msi_caps_mask.msi_caps_struct.mess_addr_hi = 0x00000000;
    msi_caps.msi_caps_struct.mess_data        = 0x0000;     msi_caps_mask.msi_caps_struct.mess_data    = 0x0000;
    msi_caps.msi_caps_struct.mask             = 0x00000000; msi_caps_mask.msi_caps_struct.mask         = 0x00000000;
    msi_caps.msi_caps_struct.pending          = 0x00000000;

    for (int idx = 0; idx < (CFG_MSI_CAPS_SIZE_BYTES/4); idx++)
    {
        pcie->writeConfigSpace     (next_cap_ptr - CFG_MSI_CAPS_SIZE_BYTES + idx*4, msi_caps.words[idx]);
        pcie->writeConfigSpaceMask (next_cap_ptr - CFG_MSI_CAPS_SIZE_BYTES + idx*4, msi_caps_mask.words[idx]);
    }

    // -------------------------------------
    // Power management capability
    // -------------------------------------

    // Default to all zeros and read-only
    for (int idx = 0; idx < (CFG_PWR_MGMT_CAPS_SIZE_BYTES/4); idx++)
    {
        pwr_mgmnt_caps.words[idx]      = 0x00000000;
        pwr_mgmnt_caps_mask.words[idx] = 0xffffffff;
    }

    next_cap_ptr += CFG_PWR_MGMT_CAPS_SIZE_BYTES;

    pwr_mgmnt_caps.pwr_mgmnt_caps_struct.cap_id                   = 0x01;
    pwr_mgmnt_caps.pwr_mgmnt_caps_struct.next_cap_ptr             = 0x00; // last capability
    pwr_mgmnt_caps.pwr_mgmnt_caps_struct.pwr_mgmnt_caps           = 0x0003;
    pwr_mgmnt_caps.pwr_mgmnt_caps_struct.pwr_mgmnt_control_status = 0x0008;  pwr_mgmnt_caps_mask.pwr_mgmnt_caps_struct.pwr_mgmnt_control_status = 0xe0fc;

    for (int idx = 0; idx < (CFG_PWR_MGMT_CAPS_SIZE_BYTES/4); idx++)
    {
        pcie->writeConfigSpace     (next_cap_ptr - CFG_PWR_MGMT_CAPS_SIZE_BYTES + idx*4, pwr_mgmnt_caps.words[idx]);
        pcie->writeConfigSpaceMask (next_cap_ptr - CFG_PWR_MGMT_CAPS_SIZE_BYTES + idx*4, pwr_mgmnt_caps_mask.words[idx]);
    }
}
