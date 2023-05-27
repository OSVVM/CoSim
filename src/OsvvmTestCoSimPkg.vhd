--
--  File Name:         OssvmTestCoSimPkg.vhd
--  Design Unit Name:  OssvmTestCoSimPkg
--  Revision:          OSVVM MODELS STANDARD VERSION
--
--  Maintainer:        Simon Southwell  email: simon.southwell@gmail.com
--  Contributor(s):
--     Jim Lewis            jim@synthworks.com
--     Simon Southwell      simon.southwell@gmail.com
--
--
--  Description:
--      Defines procedures to support co-simulation
--
--  Revision History:
--    Date      Version    Description
--    05/2023   2023.05    Adding asynchronous, check and try transaction support,
--                         and added address bus responder functionality.
--    04/2023   2023.04    Adding basic stream support
--    01/2023   2023.01    Initial revision
--
--
--  This file is part of OSVVM.
--
--  Copyright (c) 2023 by [OSVVM Authors](../AUTHORS.md)
--
--  Licensed under the Apache License, Version 2.0 (the "License");
--  you may not use this file except in compliance with the License.
--  You may obtain a copy of the License at
--
--      https://www.apache.org/licenses/LICENSE-2.0
--
--  Unless required by applicable law or agreed to in writing, software
--  distributed under the License is distributed on an "AS IS" BASIS,
--  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
--  See the License for the specific language governing permissions and
--  limitations under the License.
--

library ieee ;
  use ieee.std_logic_1164.all ;
  use ieee.numeric_std.all ;
  use ieee.numeric_std_unsigned.all ;

library OSVVM ;
  context OSVVM.OsvvmContext ;
  use osvvm.ScoreboardPkg_slv.all ;

library OSVVM_Common ;
  use OSVVM_Common.AddressBusTransactionPkg.all ;

library osvvm_ethernet ;
    context osvvm_ethernet.xMiiContext ;

library osvvm_cosim ;
  use osvvm_cosim.OsvvmVprocPkg.all ;

package OsvvmTestCoSimPkg is

  -- CoSim specific enumerations
  type CoSimOperationType is (SET_TEST_NAME) ;                            -- For non-standard VPOperation values on VPOp from VTrans

  type BurstType          is (BURST_NORM,       BURST_INCR,               -- Burst sub-operation selection in VPParam from VTrans
                              BURST_RAND,       BURST_INCR_PUSH,
                              BURST_RAND_PUSH,  BURST_INCR_CHECK,
                              BURST_RAND_CHECK, BURST_TRANS,
                              BURST_DATA,       BURST_DATA_CHECK);

  type DirType            is (RX_REC, TX_REC);                            -- Stream bus direction in (overloaded) VPData from VTrans

  ------------------------------------------------------------
  -- Co-simulation procedure to initialise and start user code
  -- for a given node.
  ------------------------------------------------------------

procedure CoSimInit (
  variable NodeNum           : in     integer := 0
  ) ;

  ------------------------------------------------------------
  -- Co-simulation procedure to generate address bus
  -- transactions.
  ------------------------------------------------------------
procedure CoSimTrans (
  signal   ManagerRec        : inout  AddressBusRecType ;
  variable Done              : inout  integer ;
  variable Error             : inout  integer ;
  variable IntReq            : in     integer := 0 ;
  variable NodeNum           : in     integer := 0
  ) ;

  ------------------------------------------------------------
  -- Co-simulation procedure to generate address bus
  -- responses.
  ------------------------------------------------------------
procedure CoSimResp (
  signal   SubordinateRec    : inout  AddressBusRecType ;
  variable Done              : inout  integer ;
  variable Error             : inout  integer ;
  variable NodeNum           : in     integer := 0
  ) ;

  ------------------------------------------------------------
  -- Co-simulation procedure to generate streaming
  -- transactions.
  ------------------------------------------------------------
  procedure CoSimStream (
    signal   TxRec           : inout  StreamRecType ;
    signal   RxRec           : inout  StreamRecType ;
    variable Done            : inout  integer ;
    variable Error           : inout  integer ;
    variable NodeNum         : in     integer := 0
  ) ;

  ------------------------------------------------------------
  -- Co-simulation procedure to dispatch one address bus
  -- transactions
  ------------------------------------------------------------

  procedure CoSimDispatchOneTransaction (
    -- Transaction  interface
    signal   ManagerRec      : inout  AddressBusRecType ;
    constant VPOperation     : in     integer ;
    constant VPAddr          : in     integer ;
    constant VPAddrHi        : in     integer ;
    constant VPAddrWidth     : in     integer ;
    constant VPDataOut       : in     integer ;
    constant VPDataOutHi     : in     integer ;
    constant VPDataWidth     : in     integer ;
    constant VPBurstSize     : in     integer ;
    constant VPTicks         : in     integer ;
    constant VPParam         : in     integer ;
    constant NodeNum         : in     integer
  ) ;

  ------------------------------------------------------------
  -- Co-simulation procedure to dispatch one address bus
  -- transaction repsonse
  ------------------------------------------------------------

  procedure CoSimDispatchOneResponse (
    -- Transaction  interface
    signal   SubordinateRec  : inout  AddressBusRecType ;
    constant VPOperation     : in     integer ;
    constant VPAddr          : in     integer ;
    constant VPAddrHi        : in     integer ;
    constant VPAddrWidth     : in     integer ;
    constant VPDataOut       : in     integer ;
    constant VPDataOutHi     : in     integer ;
    constant VPDataWidth     : in     integer ;
    constant VPBurstSize     : in     integer ;
    constant VPTicks         : in     integer ;
    constant VPParam         : in     integer ;
    constant NodeNum         : in     integer
  ) ;

  ------------------------------------------------------------
  -- Co-simulation procedure to dispatch one stream
  -- transaction
  ------------------------------------------------------------

  procedure CoSimDispatchOneStream (
    -- Transaction  interface
    signal   TxRec           : inout  StreamRecType ;
    signal   RxRec           : inout  StreamRecType ;
    constant VPOperation     : in     integer ;
    constant VPDataOut       : in     integer ;
    constant VPDataOutHi     : in     integer ;
    constant VPDataWidth     : in     integer ;
    variable VPBurstSize     : inout  integer ;
    constant VPTicks         : in     integer ;
    constant VPParam         : in     integer ;
    constant NodeNum         : in     integer
  ) ;

end package OsvvmTestCoSimPkg ;

-- /////////////////////////////////////////////////////////////////////////////////////////
-- /////////////////////////////////////////////////////////////////////////////////////////

package body OsvvmTestCoSimPkg is
  constant ADDR_WIDTH_MAX    : integer := 64 ;
  constant DATA_WIDTH_MAX    : integer := 64 ;

  ------------------------------------------------------------
  -- Co-simulation software initialisation procedure for
  -- a specified node. Must be called, once per node, before
  -- any CoSimTrans calls
  ------------------------------------------------------------

  procedure CoSimInit (
  variable NodeNum           : in     integer := 0
  ) is

  begin
    VInit(NodeNum);
  end procedure CoSimInit ;

  ------------------------------------------------------------
  -- Co-simulation wrapper procedure to send read and write
  -- transactions
  ------------------------------------------------------------
  procedure CoSimTrans (
    -- Transaction  interface
    signal   ManagerRec      : inout  AddressBusRecType ;
    variable Done            : inout  integer ;
    variable Error           : inout  integer ;
    variable IntReq          : in     integer := 0;
    variable NodeNum         : in     integer := 0
    ) is

    variable RdData          : std_logic_vector (ManagerRec.DataFromModel'range) ;

    variable VPData          : integer ;
    variable VPDataHi        : integer ;
    variable VPDataWidth     : integer ;
    variable VPAddr          : integer ;
    variable VPAddrHi        : integer ;
    variable VPAddrWidth     : integer ;
    variable VPOp            : integer ;
    variable VPBurstSize     : integer ;
    variable VPTicks         : integer ;
    variable VPDone          : integer ;
    variable VPError         : integer ;
    variable VPParam         : integer ;
    variable VPStatus        : integer ;
    variable VPCount         : integer ;
    variable UnusedCount     : integer ;

  begin

    -- RdData and Available status won't have persisted from last call, so re-fetch from ManagerRec
    -- which will have persisted (and is not yet updated)
    RdData       := osvvm.TbUtilPkg.MetaTo01(SafeResize(ManagerRec.DataFromModel, RdData'length)) ;
    VPStatus     := 1 when ManagerRec.BoolFromModel else 0 ;
    VPCount      := ManagerRec.IntFromModel ;

    -- Sample the read data from last access, saved in RdData inout port
    if RdData'length > 32 then
      VPData     := to_integer(signed(RdData(31 downto  0))) ;
      VPDataHi   := to_integer(signed(RdData(RdData'length-1 downto 32))) ;
    else
      VPData     := to_integer(signed(RdData(RdData'length-1 downto 0))) ;
      VPDataHi   := 0 ;
    end if;

    -- Call VTrans to generate a new access
    VTrans(NodeNum,   IntReq,      VPStatus,  VPCount, UnusedCount,
           VPData,    VPDataHi,    VPDataWidth,
           VPAddr,    VPAddrHi,    VPAddrWidth,
           VPOp,      VPBurstSize, VPTicks,
           VPDone,    VPError,     VPParam) ;

    Done  := VPDone  ;
    Error := VPError ;

    CoSimDispatchOneTransaction(ManagerRec,
                                VPOp,
                                VPAddr,      VPAddrHi,    VPAddrWidth,
                                VPData,      VPDataHi,    VPDataWidth,
                                VPBurstSize, VPTicks,     VPParam,
                                NodeNum) ;

  end procedure CoSimTrans ;

  ------------------------------------------------------------
  -- Co-simulation procedure to dispatch one transactions
  ------------------------------------------------------------
  procedure CoSimDispatchOneTransaction (
    -- Transaction  interface
    signal   ManagerRec      : inout  AddressBusRecType ;
    constant VPOperation     : in     integer ;
    constant VPAddr          : in     integer ;
    constant VPAddrHi        : in     integer ;
    constant VPAddrWidth     : in     integer ;
    constant VPDataOut       : in     integer ;
    constant VPDataOutHi     : in     integer ;
    constant VPDataWidth     : in     integer ;
    constant VPBurstSize     : in     integer ;
    constant VPTicks         : in     integer ;
    constant VPParam         : in     integer ;
    constant NodeNum         : in     integer
  ) is

    variable RdData          : std_logic_vector (DATA_WIDTH_MAX-1 downto 0) ;
    variable WrData          : std_logic_vector (DATA_WIDTH_MAX-1 downto 0) ;
    variable Address         : std_logic_vector (ADDR_WIDTH_MAX-1 downto 0) ;
    variable WrByteData      : signed (DATA_WIDTH_MAX-1 downto 0) ;
    variable RdDataInt       : integer ;
    variable WrDataInt       : integer ;
    variable TestName        : string(1 to VPBurstSize) ;
    variable Available       : boolean ;
    variable ModelID         : AlertLogIDType ; 

  begin

    -- Convert address and write data to std_logic_vectors
    Address(31 downto  0) := std_logic_vector(to_signed(VPAddr,      32)) ;
    Address(63 downto 32) := std_logic_vector(to_signed(VPAddrHi,    32)) ;

    WrData(31 downto 0 )  := std_logic_vector(to_signed(VPDataOut,   32)) ;
    WrData(63 downto 32)  := std_logic_vector(to_signed(VPDataOutHi, 32)) ;

    if VPOperation < 1024 then
      case AddressBusOperationType'val(VPOperation) is
        when WAIT_FOR_CLOCK =>
          WaitForClock(ManagerRec, VPTicks) ;

        when READ_OP =>
          Read  (ManagerRec, Address(VPAddrWidth-1 downto 0), RdData(VPDataWidth-1 downto 0)) ;

        when READ_CHECK =>
          ReadCheck(ManagerRec, Address(VPAddrWidth-1 downto 0), WrData(VPDataWidth-1 downto 0)) ;

        when WRITE_OP =>
          Write (ManagerRec, Address(VPAddrWidth-1 downto 0), WrData(VPDataWidth-1 downto 0)) ;

        when WRITE_AND_READ =>
          WriteAndRead (ManagerRec, Address(VPAddrWidth-1 downto 0), WrData(VPDataWidth-1 downto 0), RdData(VPDataWidth-1 downto 0)) ;

        when ASYNC_WRITE =>
          WriteAsync (ManagerRec, Address(VPAddrWidth-1 downto 0), WrData(VPDataWidth-1 downto 0)) ;

        when ASYNC_WRITE_AND_READ =>
          WriteAndReadAsync (ManagerRec, Address(VPAddrWidth-1 downto 0), WrData(VPDataWidth-1 downto 0)) ;

        when ASYNC_WRITE_ADDRESS =>
          WriteAddressAsync(ManagerRec, Address(VPAddrWidth-1 downto 0));

        when ASYNC_WRITE_DATA =>
          WriteDataAsync(ManagerRec, Address(VPAddrWidth-1 downto 0), WrData(VPDataWidth-1 downto 0));

        when ASYNC_READ_ADDRESS =>
          ReadAddressAsync(ManagerRec, Address(VPAddrWidth-1 downto 0));

        when READ_DATA =>
          ReadData(ManagerRec, RdData(VPDataWidth-1 downto 0));

        when ASYNC_READ_DATA =>
          TryReadData(ManagerRec, RdData(VPDataWidth-1 downto 0), Available);

        when ASYNC_READ_DATA_CHECK =>
          TryReadCheckData(ManagerRec, WrData(VPDataWidth-1 downto 0), Available);

        when READ_DATA_CHECK =>
          ReadCheckData(ManagerRec, WrData(VPDataWidth-1 downto 0));

        when READ_BURST =>

          -- Select the burst operations based on the VPParam argument passed from the software
          case BurstType'val(VPParam) is

            when BURST_NORM | BURST_TRANS | BURST_DATA =>

              -- Only instigate a read burst transaction if the operation isn't a POP,
              -- as defined by VPParam
              if BurstType'val(VPParam) /= BURST_DATA then
                ReadBurst(ManagerRec, Address(VPAddrWidth-1 downto  0), VPBurstSize) ;
              end if ;

              -- Pop the bytes from the FIFO if not a pure transaction operation, as
              -- defined by VPParam
              if BurstType'val(VPParam) = BURST_NORM or BurstType'val(VPParam) = BURST_DATA then

                -- Pop the bytes from the read fifo and write them to the co-sim receive buffer
                RdData := (others => '0');
                for bidx in 0 to VPBurstSize-1 loop
                  Pop(ManagerRec.ReadBurstFifo, RdData(7 downto 0)) ;
                  RdDataInt := to_integer(unsigned(RdData(7 downto 0))) ;
                  VSetBurstRdByte(NodeNum, bidx, RdDataInt) ;

                end loop;
              end if ;

            when BURST_DATA_CHECK =>

              GetAlertLogID(ManagerRec, ModelID) ; 
              ReadBurst(ManagerRec, Address(VPAddrWidth-1 downto  0), VPBurstSize) ;

              -- Check Burst
              -- Pop the bytes from the read fifo and write them to the co-sim receive buffer
              RdData := (others => '0');
              for bidx in 0 to VPBurstSize-1 loop
                -- Get Byte from read fifo
                Pop(ManagerRec.ReadBurstFifo, RdData(7 downto 0)) ;
                -- Get Byte from co-sim interface
                VGetBurstWrByte(NodeNum, bidx, WrDataInt) ;
                WrByteData := to_signed(WrDataInt, WrByteData'length) ;
                -- Check
                AffirmIfEqual(ModelID, RdData(7 downto 0), std_logic_vector(WrByteData(7 downto 0)),
                  "Burst Read Address: " & to_hxstring(Address(VPAddrWidth-1 downto  0)) & 
                  ", Byte number: " & to_string(bidx) & 
                  ", Byte Data ", 
                  IsLogEnabled(ModelID, INFO) ) ;
              end loop;

            when BURST_INCR =>

              -- The first data value for the increment is in the first byte of the burst write data buffer
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              ReadCheckBurstIncrement(ManagerRec, Address(VPAddrWidth-1 downto  0), std_logic_vector(WrByteData(7 downto 0)), VPBurstSize) ;

            when BURST_RAND =>

              -- The first data value for the random sequence is in the first byte of the burst write data buffer
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              ReadCheckBurstRandom(ManagerRec, Address(VPAddrWidth-1 downto  0), std_logic_vector(WrByteData(7 downto 0)), VPBurstSize) ;

            when BURST_INCR_CHECK =>

              -- The first data value for the increment is in the first byte of the burst write data buffer
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              CheckBurstIncrement(ManagerRec.ReadBurstFifo, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize) ;

            when BURST_RAND_CHECK =>

              -- The first data value for the random sequence is in the first byte of the burst write data buffer
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              CheckBurstRandom(ManagerRec.ReadBurstFifo, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize) ;

            when others =>
                Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneTransaction received unimplemented burst type") ;

          end case;

        when WRITE_BURST | ASYNC_WRITE_BURST =>

          -- Select the burst operations based on the VPParam argument passed from the software
          case BurstType'val(VPParam) is

            when BURST_NORM | BURST_DATA =>

              -- Fetch the bytes from the co-sim send buffer and push to the transaction write fifo
              for bidx in 0 to VPBurstSize-1 loop
                VGetBurstWrByte(NodeNum, bidx, WrDataInt) ;
                WrByteData := to_signed(WrDataInt, WrByteData'length) ;
                Push(ManagerRec.WriteBurstFifo, std_logic_vector(WrByteData(7 downto 0))) ;
              end loop ;

            when BURST_INCR | BURST_INCR_PUSH =>

               -- The first data value for the increment is in the first byte of the burst write data buffer
               VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
               WrByteData := to_signed(WrDataInt, WrByteData'length);
               PushBurstIncrement(ManagerRec.WriteBurstFifo, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize);

            when BURST_RAND | BURST_RAND_PUSH =>

              -- The first data value for the sequence is in the first byte of the burst write data buffer
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length);
              PushBurstRandom(ManagerRec.WriteBurstFifo, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize);

            -- When a pure transaction operation, no data requires fetching from write buffer
            when BURST_TRANS =>
              null;

            when others =>
              Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneTransaction received unimplemented burst type") ;

          end case ;

          -- Only instigate a write burst transaction when not a FIFO push operation, as defined by VPParam
          if BurstType'val(VPParam) /= BURST_INCR_PUSH and BurstType'val(VPParam) /= BURST_RAND_PUSH and
             BurstType'val(VPParam) /= BURST_DATA then

            -- Select blocking or non-blocking operation from  VPOperation
            if AddressBusOperationType'val(VPOperation) = WRITE_BURST then
              WriteBurst(ManagerRec, Address(VPAddrWidth-1 downto  0), VPBurstSize) ;
            else
              WriteBurstAsync(ManagerRec, Address(VPAddrWidth-1 downto  0), VPBurstSize) ;
            end if ;

          end if ;

        when GET_TRANSACTION_COUNT =>
          GetTransactionCount(ManagerRec, RdDataInt) ;

        when GET_WRITE_TRANSACTION_COUNT =>
          GetWriteTransactionCount(ManagerRec, RdDataInt) ;

        when GET_READ_TRANSACTION_COUNT =>
          GetReadTransactionCount(ManagerRec, RdDataInt) ;

        when WAIT_FOR_TRANSACTION =>
          WaitForTransaction(ManagerRec) ;

        when WAIT_FOR_WRITE_TRANSACTION =>
          WaitForWriteTransaction(ManagerRec) ;

        when WAIT_FOR_READ_TRANSACTION =>
          WaitForReadTransaction(ManagerRec) ;

        when others =>
          Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneTransaction received unimplemented transaction") ;

      end case ;

      -- If VPTicks non-zero for transaction operations do wait for clock after the transaction
      -- executed
      if AddressBusOperationType'val(VPOperation) /= WAIT_FOR_CLOCK and VPTicks /= 0 then
        WaitForClock(ManagerRec, VPTicks) ;
      end if ;

    else

      case CoSimOperationType'val(VPOperation - 1024) is

        when SET_TEST_NAME =>

          for bidx in 0 to VPBurstSize-1 loop

            VGetBurstWrByte(NodeNum, bidx, WrDataInt) ;

            if (WrDataInt < 0 or WrDataInt > 255) then
              Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneTransaction SetTestName - bad character value") ;
              return ;
            end if ;

            TestName(bidx+1) := character'val(WrDataInt);
          end loop ;

          SetTestName(TestName(1 to VPBurstSize)) ;

        when others =>
          Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneTransaction received unimplemented transaction") ;
      end case ;

    end if ;

  end procedure CoSimDispatchOneTransaction ;

  ------------------------------------------------------------
  -- Co-simulation wrapper procedure to receive transactions
  -- and send responses
  ------------------------------------------------------------

  procedure CoSimResp (
    signal   SubordinateRec  : inout  AddressBusRecType ;
    variable Done            : inout  integer ;
    variable Error           : inout  integer ;
    variable NodeNum         : in     integer := 0
    ) is

    variable RdData          : std_logic_vector (SubordinateRec.DataFromModel'range) ;
    variable Address         : std_logic_vector (SubordinateRec.Address'range) ;

    variable VPData          : integer ;
    variable VPDataHi        : integer ;
    variable VPDataWidth     : integer ;
    variable VPAddr          : integer ;
    variable VPAddrHi        : integer ;
    variable VPAddrWidth     : integer ;
    variable VPOp            : integer ;
    variable VPBurstSize     : integer ;
    variable VPTicks         : integer ;
    variable VPDone          : integer ;
    variable VPError         : integer ;
    variable VPParam         : integer ;
    variable VPStatus        : integer ;
    variable VPCount         : integer ;
    variable UnusedCount     : integer ;
    variable UnusedIntReq    : integer ;

  begin

    -- RdData and Available status won't have persisted from last call, so re-fetch from ManagerRec
    -- which will have persisted (and is not yet updated)
    RdData       := osvvm.TbUtilPkg.MetaTo01(SafeResize(SubordinateRec.DataFromModel, RdData'length)) ;
    Address      := osvvm.TbUtilPkg.MetaTo01(SafeResize(SubordinateRec.Address, Address'length)) ;
    VPStatus     := 1 when SubordinateRec.BoolFromModel else 0 ;
    VPCount      := SubordinateRec.IntFromModel ;

    -- Sample the read data from last access, saved in RdData inout port
    if RdData'length > 32 then
      VPData     := to_integer(signed(RdData(31 downto  0))) ;
      VPDataHi   := to_integer(signed(RdData(RdData'length-1 downto 32))) ;
    else
      VPData     := to_integer(signed(RdData(31 downto 0))) ;
      VPDataHi   := 0 ;
    end if;

    if Address'length > 32 then
      VPAddr     := to_integer(signed(Address(31 downto  0))) ;
      VPAddrHi   := to_integer(signed(Address(Address'length-1 downto 32))) ;
    else
      VPAddr     := to_integer(signed(Address(31 downto  0))) ;
      VPAddrHi   := 0 ;
    end if ;

    -- Call VTrans to generate a new response operation
    VTrans(NodeNum,      UnusedIntReq,   VPStatus,  VPCount, UnusedCount,
           VPData,       VPDataHi,       VPDataWidth,
           VPAddr,       VPAddrHi,       VPAddrWidth,
           VPOp,         VPBurstSize,    VPTicks,
           VPDone,       VPError,        VPParam) ;

    Done  := VPDone  ;
    Error := VPError ;

    CoSimDispatchOneResponse(SubordinateRec,
                             VPOp,
                             VPAddr,      VPAddrHi, VPAddrWidth,
                             VPData,      VPDataHi, VPDataWidth,
                             VPBurstSize, VPTicks,  VPParam,
                             NodeNum) ;

  end procedure CoSimResp;

  ------------------------------------------------------------
  -- Co-simulation procedure to dispatch one address bus
  -- transaction repsonse
  ------------------------------------------------------------

  procedure CoSimDispatchOneResponse (
    -- Transaction  interface
    signal   SubordinateRec  : inout  AddressBusRecType ;
    constant VPOperation     : in     integer ;
    constant VPAddr          : in     integer ;
    constant VPAddrHi        : in     integer ;
    constant VPAddrWidth     : in     integer ;
    constant VPDataOut       : in     integer ;
    constant VPDataOutHi     : in     integer ;
    constant VPDataWidth     : in     integer ;
    constant VPBurstSize     : in     integer ;
    constant VPTicks         : in     integer ;
    constant VPParam         : in     integer ;
    constant NodeNum         : in     integer
  ) is

    variable RdData          : std_logic_vector (DATA_WIDTH_MAX-1 downto 0) ;
    variable WrData          : std_logic_vector (DATA_WIDTH_MAX-1 downto 0) ;
    variable Address         : std_logic_vector (ADDR_WIDTH_MAX-1 downto 0) ;
    variable RdDataInt       : integer ;
    variable Available       : boolean ;

  begin

    -- Convert address and write data to std_logic_vectors
    Address(31 downto  0) := std_logic_vector(to_signed(VPAddr,      32)) ;
    Address(63 downto 32) := std_logic_vector(to_signed(VPAddrHi,    32)) ;

    WrData(31 downto 0 )  := std_logic_vector(to_signed(VPDataOut,   32)) ;
    WrData(63 downto 32)  := std_logic_vector(to_signed(VPDataOutHi, 32)) ;

    if VPOperation < 1024 then
      case AddressBusOperationType'val(VPOperation) is

        when WAIT_FOR_CLOCK =>
          WaitForClock(SubordinateRec, VPTicks) ;

        when WRITE_OP =>
          GetWrite(SubordinateRec, Address(VPAddrWidth-1 downto 0), RdData(VPDataWidth-1 downto 0)) ;

        when ASYNC_WRITE =>
          TryGetWrite(SubordinateRec, Address(VPAddrWidth-1 downto 0), RdData(VPDataWidth-1 downto 0), Available) ;

        when WRITE_ADDRESS =>
          GetWriteAddress(SubordinateRec, Address(VPAddrWidth-1 downto 0));

        when ASYNC_WRITE_ADDRESS =>
          TryGetWriteAddress(SubordinateRec, Address(VPAddrWidth-1 downto 0), Available) ;

        when WRITE_DATA =>
          GetWriteData(SubordinateRec, Address(VPAddrWidth-1 downto 0), RdData(VPDataWidth-1 downto 0)) ;

        when ASYNC_WRITE_DATA =>
          TryGetWriteData(SubordinateRec, Address(VPAddrWidth-1 downto 0), RdData(VPDataWidth-1 downto 0), Available) ;

        when READ_OP =>
          SendRead(SubordinateRec, Address(VPAddrWidth-1 downto 0), WrData(VPDataWidth-1 downto 0)) ;

        when ASYNC_READ =>
          TrySendRead(SubordinateRec, Address(VPAddrWidth-1 downto 0), WrData(VPDataWidth-1 downto 0), Available) ;

        when READ_ADDRESS =>
          GetReadAddress(SubordinateRec, Address(VPAddrWidth-1 downto 0)) ;

        when ASYNC_READ_ADDRESS =>
          TryGetReadAddress(SubordinateRec, Address(VPAddrWidth-1 downto 0), Available) ;

        when READ_DATA =>
          SendReadData(SubordinateRec, WrData(VPDataWidth-1 downto 0));

        when ASYNC_READ_DATA =>
          SendReadDataAsync(SubordinateRec, WrData(VPDataWidth-1 downto 0));

        when GET_TRANSACTION_COUNT =>
          GetTransactionCount(SubordinateRec, RdDataInt) ;

        when GET_WRITE_TRANSACTION_COUNT =>
          GetWriteTransactionCount(SubordinateRec, RdDataInt) ;

        when GET_READ_TRANSACTION_COUNT =>
          GetReadTransactionCount(SubordinateRec, RdDataInt) ;

        when WAIT_FOR_TRANSACTION =>
          WaitForTransaction(SubordinateRec) ;

        when WAIT_FOR_WRITE_TRANSACTION =>
          WaitForWriteTransaction(SubordinateRec) ;

        when WAIT_FOR_READ_TRANSACTION =>
          WaitForReadTransaction(SubordinateRec) ;

        when others =>
          Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneResponse received unimplemented transaction") ;

      end case ;
    end if ;
  end procedure CoSimDispatchOneResponse ;

  ------------------------------------------------------------
  -- Co-simulation wrapper procedure to send read and write
  -- stream transactions
  ------------------------------------------------------------
  procedure CoSimStream (
    -- Transaction  interface
    signal   TxRec           : inout  StreamRecType ;
    signal   RxRec           : inout  StreamRecType ;
    variable Done            : inout  integer ;
    variable Error           : inout  integer ;
    variable NodeNum         : in     integer := 0
    ) is

    variable VPData            : integer ;
    variable VPDataHi          : integer ;
    variable VPDataWidth       : integer ;
    variable VPOp              : integer ;
    variable VPBurstSize       : integer ;
    variable VPTicks           : integer ;
    variable VPDone            : integer ;
    variable VPError           : integer ;
    variable VPParam           : integer ;
    variable VPStatus          : integer ;
    variable VPCountTx         : integer ;
    variable VPCountRx         : integer ;

    variable UnusedVPAddrLo    : integer ;
    variable UnusedVPAddrHi    : integer ;
    variable UnusedVPAddrWidth : integer ;
    variable Available         : integer  := 0;

    variable RdData            : std_logic_vector (DATA_WIDTH_MAX-1 downto 0) ;
    variable Status            : std_logic_vector (31 downto 0) ;

  begin

    Status     := osvvm.TbUtilPkg.MetaTo01(SafeResize(RxRec.ParamFromModel, Status'length)) ;
    VPStatus   := to_integer(signed(Status)) ;
    VPCountTx  := TxRec.IntFromModel;
    VPCountRx  := RxRec.IntFromModel;

    Available  := 1 when RxRec.BoolFromModel else 0 ;

    RdData     := osvvm.TbUtilPkg.MetaTo01(SafeResize(RxRec.DataFromModel, RdData'length)) ;
    -- Sample the read data from last access, saved in RdData inout port
    if RdData'length > 32 then
      VPData     := to_integer(signed(RdData(31 downto  0))) ;
      VPDataHi   := to_integer(signed(RdData(RdData'length-1 downto 32))) ;
    else
      VPData     := to_integer(signed(RdData(31 downto 0))) ;
      VPDataHi   := 0 ;
    end if;


    -- Call VTrans to generate a new TX access
    VTrans(NodeNum,        Available,      VPStatus, VPCountRx, VPCountTx,
           VPData,         VPDataHi,       VPDataWidth,
           UnusedVPAddrLo, UnusedVPAddrHi, UnusedVPAddrWidth,
           VPOp,           VPBurstSize,    VPTicks,
           VPDone,         VPError,        VPParam) ;

    Done  := VPDone  ;
    Error := VPError ;

    CoSimDispatchOneStream (TxRec, RxRec,
                            VPOp,
                            VPData,      VPDataHi,    VPDataWidth,
                            VPBurstSize, VPTicks,     VPParam,
                            NodeNum) ;


  end procedure CoSimStream ;

  ------------------------------------------------------------
  -- Co-simulation procedure to dispatch one stream transaction
  ------------------------------------------------------------
  procedure CoSimDispatchOneStream (
    -- Transaction  interface
    signal   TxRec           : inout  StreamRecType ;
    signal   RxRec           : inout  StreamRecType ;
    constant VPOperation     : in     integer ;
    constant VPDataOut       : in     integer ;
    constant VPDataOutHi     : in     integer ;
    constant VPDataWidth     : in     integer ;
    variable VPBurstSize     : inout  integer ;
    constant VPTicks         : in     integer ;
    constant VPParam         : in     integer ;
    constant NodeNum         : in     integer
  ) is

    variable RdData          : std_logic_vector (DATA_WIDTH_MAX-1 downto 0) ;
    variable WrData          : std_logic_vector (DATA_WIDTH_MAX-1 downto 0) ;
    variable Param           : std_logic_vector (31 downto 0) ;
    variable WrByteData      : signed (DATA_WIDTH_MAX-1 downto 0) ;
    variable RdDataInt       : integer ;
    variable WrDataInt       : integer ;
    variable TestName        : string(1 to VPBurstSize) ;
    variable Available       : boolean ;
    variable PacketLength    : integer ;

    variable Fifo            : ScoreboardIdType;

  begin

    -- Convert write data to std_logic_vectors
    WrData(31 downto 0 )  := std_logic_vector(to_signed(VPDataOut,   32)) ;
    WrData(63 downto 32)  := std_logic_vector(to_signed(VPDataOutHi, 32)) ;
    Param(31 downto 0)    := std_logic_vector(to_signed(VPParam,     32)) ;

    if VPOperation < 1024 then
      case StreamOperationType'val(VPOperation) is

        when WAIT_FOR_CLOCK =>
          WaitForClock(TxRec, VPTicks) ;

        when GET =>
          Param := (others => '0') ;
          Get  (RxRec, RdData(VPDataWidth-1 downto 0), Param(RxRec.ParamFromModel'length -1 downto 0)) ;

        when TRY_GET =>
          TryGet(RxRec, RdData(VPDataWidth-1 downto 0), Param(RxRec.ParamFromModel'length -1 downto 0), Available) ;

        when CHECK =>
          Check (RxRec, WrData(VPDataWidth-1 downto 0), Param(RxRec.ParamFromModel'length -1 downto 0)) ;

        when TRY_CHECK =>
          TryCheck(RxRec, WrData(VPDataWidth-1 downto 0), Param(RxRec.ParamFromModel'length -1 downto 0), Available) ;

        when SEND =>
          Send (TxRec, WrData(VPDataWidth-1 downto 0), Param(TxRec.ParamToModel'length -1 downto 0)) ;

        when SEND_ASYNC =>
          SendAsync (TxRec, WrData(VPDataWidth-1 downto 0), Param(TxRec.ParamToModel'length -1 downto 0)) ;

        when GET_BURST =>

          -- If not a pure pop type operation, do a get burst transaction
          if BurstType'val(VPParam) /= BURST_DATA then

            PacketLength := VPBurstSize ;
            GetBurst(RxRec, PacketLength, Param(RxRec.ParamToModel'length -1 downto 0)) ;
            AffirmIfEqual(PacketLength, VPBurstSize, "Get burst packet Length") ;

          end if ;

          -- If not a pure get operation, fetch the bytes from the fifo
          if BurstType'val(VPParam) /= BURST_TRANS then

            -- Pop the bytes from the read fifo and write them to the co-sim receive buffer
            RdData := (others => '0');

            for bidx in 0 to VPBurstSize-1 loop
              Pop(RxRec.BurstFifo, RdData(7 downto 0)) ;
              RdDataInt := to_integer(unsigned(RdData(7 downto 0))) ;

              VSetBurstRdByte(NodeNum, bidx, RdDataInt) ;
            end loop ;

          end if ;

        when TRY_GET_BURST =>
          TryGetBurst(RxRec, VPBurstSize, Param(RxRec.ParamToModel'length-1 downto 0), Available);

          if BurstType'val(VPParam) /= BURST_TRANS and Available then

            -- Pop the bytes from the read fifo and write them to the co-sim receive buffer
            RdData := (others => '0');

            for bidx in 0 to VPBurstSize-1 loop
              Pop(RxRec.BurstFifo, RdData(7 downto 0)) ;
              RdDataInt := to_integer(unsigned(RdData(7 downto 0))) ;

              VSetBurstRdByte(NodeNum, bidx, RdDataInt) ;
            end loop ;
          end if ;

        when TRY_CHECK_BURST =>

          case BurstType'val(VPDataOut) is

            when BURST_NORM =>

              for bidx in 0 to VPBurstSize-1 loop

                VGetBurstWrByte(NodeNum, bidx, WrDataInt) ;
                WrByteData := to_signed(WrDataInt, WrByteData'length) ;
                Push(RxRec.BurstFifo, std_logic_vector(WrByteData(7 downto 0))) ;

              end loop ;

              TryCheckBurst(RxRec, VPBurstSize, Param(RxRec.ParamToModel'length-1 downto 0), Available);

            when BURST_TRANS =>
              TryCheckBurst(RxRec, VPBurstSize, Param(RxRec.ParamToModel'length-1 downto 0), Available);

            when BURST_INCR_CHECK =>
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              TryCheckBurstIncrement(RxRec, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize, Param(RxRec.ParamToModel'length-1 downto 0), Available);

            when BURST_RAND_CHECK =>
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              TryCheckBurstRandom(RxRec, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize, Param(RxRec.ParamToModel'length-1 downto 0), Available);

            when others =>
              Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneStream received unimplemented burst type") ;

          end case ;

        when SEND_BURST | SEND_BURST_ASYNC | CHECK_BURST =>

          if StreamOperationType'val(VPOperation) = CHECK_BURST then
            Fifo := RxRec.BurstFifo ;
          else
            Fifo := TxRec.BurstFifo ;
          end if ;

          -- Select the burst operations based on the VPParam argument passed from the software
          case BurstType'val(VPDataOut) is

            when BURST_NORM | BURST_DATA | BURST_TRANS  =>

              -- Fetch the bytes from the co-sim send buffer and push to the transaction write fifo
              if BurstType'val(VPDataOut) /= BURST_TRANS then
                for bidx in 0 to VPBurstSize-1 loop

                  VGetBurstWrByte(NodeNum, bidx, WrDataInt) ;
                  WrByteData := to_signed(WrDataInt, WrByteData'length) ;
                  Push(Fifo, std_logic_vector(WrByteData(7 downto 0))) ;

                end loop ;
              end if ;

              -- Instigate a transaction if not a PUSH operation
              if BurstType'val(VPDataOut) /= BURST_DATA then
                  -- Select blocking, non-blocking or check operation depending on VPOperation
                  if StreamOperationType'val(VPOperation) = SEND_BURST then
                    SendBurst(TxRec, VPBurstSize, Param(TxRec.ParamToModel'length -1 downto 0)) ;
                  elsif StreamOperationType'val(VPOperation) = SEND_BURST_ASYNC then
                    SendBurstAsync(TxRec, VPBurstSize, Param(TxRec.ParamToModel'length -1 downto 0)) ;
                  else
                    CheckBurst(RxRec, VPBurstSize, Param(RxRec.ParamToModel'length -1 downto 0)) ;
                  end if ;
              end if ;

            when BURST_INCR_PUSH =>
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              PushBurstIncrement(Fifo, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize) ;

            when BURST_RAND_PUSH =>
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              PushBurstRandom(Fifo, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize) ;

            when BURST_INCR =>
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              SendBurstIncrement(TxRec, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize, Param(TxRec.ParamToModel'length -1 downto 0)) ;

            when BURST_RAND =>
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              SendBurstRandom(TxRec, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize, Param(TxRec.ParamToModel'length -1 downto 0)) ;

            when BURST_INCR_CHECK =>
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              CheckBurstIncrement(RxRec, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize, Param(TxRec.ParamToModel'length -1 downto 0)) ;

            when BURST_RAND_CHECK =>
              VGetBurstWrByte(NodeNum, 0, WrDataInt) ;
              WrByteData := to_signed(WrDataInt, WrByteData'length) ;
              CheckBurstRandom(RxRec, std_logic_vector(WrByteData(7 downto 0)), VPBurstSize, Param(TxRec.ParamToModel'length -1 downto 0)) ;

            when others =>
              Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneStream received unimplemented burst type") ;

          end case ;

        when WAIT_FOR_TRANSACTION =>
           if DirType'val(VPDataOut) = TX_REC then
             WaitForTransaction(TxRec) ;
           else
             WaitForTransaction(RxRec) ;
           end if ;

        When GET_TRANSACTION_COUNT =>
           if DirType'val(VPDataOut) = TX_REC then
             GetTransactionCount(TxRec, RdDataInt) ;
           else
             GetTransactionCount(RxRec, RdDataInt) ;
           end if ;

        when others =>
          Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneStream received unimplemented transaction") ;

      end case ;

      -- If VPTicks non-zero for transaction operations do wait for clock after the transaction
      -- executed
      if StreamOperationType'val(VPOperation) /= WAIT_FOR_CLOCK and VPTicks /= 0 then
        WaitForClock(TxRec, VPTicks) ;
      end if ;

    else

      case CoSimOperationType'val(VPOperation - 1024) is

        when SET_TEST_NAME =>

          for bidx in 0 to VPBurstSize-1 loop
            VGetBurstWrByte(NodeNum, bidx, WrDataInt) ;
            if (WrDataInt < 0 or WrDataInt > 255) then
              Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneStream SetTestName - bad character value") ;
              return ;
            end if ;
            TestName(bidx+1) := character'val(WrDataInt);
          end loop ;

          SetTestName(TestName(1 to VPBurstSize)) ;

        when others =>
          Alert("CoSim/src/OsvvmTestCoSimPkg: CoSimDispatchOneStream received unimplemented transaction") ;

      end case ;

    end if ;

  end procedure CoSimDispatchOneStream ;

end package body OsvvmTestCoSimPkg ;
