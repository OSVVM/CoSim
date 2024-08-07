--
--  File Name:         TbAddressBusMemory.vhd
--  Design Unit Name:  TbAddressBusMemory
--  Revision:          OSVVM MODELS STANDARD VERSION
--
--  Maintainer:          Jim Lewis  email: jim@synthworks.com
--  Contributor(s):
--     Jim Lewis        jim@synthworks.com
--     Simon Southwell  simon.southwell@gmail.com
--
--
--  Description:
--      Test Harness for Axi4LiteManager + Axi4LiteMemory
--
--  Developed by:
--        SynthWorks Design Inc.
--        VHDL Training Classes
--        http://www.SynthWorks.com
--
--  Revision History:
--    Date      Version    Description
--    12/2022   2023.01    Updated Entity Name to TbAddressBusMemory
--    12/2020   2020.12    Updated signal and port names
--    01/2020   2020.01    Updated license notice
--    04/2018   2018       Initial revision
--
--
--  This file is part of OSVVM.
--
--  Copyright (c) 2018 - 2022 by SynthWorks Design Inc.
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

library osvvm ;
  context osvvm.OsvvmContext ;

library osvvm_Axi4 ;
  context osvvm_Axi4.Axi4LiteContext ;

entity TbAddressBusMemory is
end entity TbAddressBusMemory ;
architecture TestHarness of TbAddressBusMemory is
  constant AXI_ADDR_WIDTH : integer := 32 ;
  constant AXI_DATA_WIDTH : integer := 32 ;
  constant AXI_STRB_WIDTH : integer := AXI_DATA_WIDTH/8 ;

  constant tperiod_Clk : time := 10 ns ;
  constant tpd         : time := 2 ns ;

  signal Clk         : std_logic ;
  signal nReset      : std_logic ;

  signal ManagerRec, SubordinateRec  : AddressBusRecType(
          Address(AXI_ADDR_WIDTH-1 downto 0),
          DataToModel(AXI_DATA_WIDTH-1 downto 0),
          DataFromModel(AXI_DATA_WIDTH-1 downto 0)
        ) ;

--  -- AXI Manager Functional Interface
  signal   AxiBus1, AxiBus2 : Axi4LiteRecType(
    WriteAddress( Addr (AXI_ADDR_WIDTH-1 downto 0) ),
    WriteData   ( Data (AXI_DATA_WIDTH-1 downto 0),   Strb(AXI_STRB_WIDTH-1 downto 0) ),
    ReadAddress ( Addr (AXI_ADDR_WIDTH-1 downto 0) ),
    ReadData    ( Data (AXI_DATA_WIDTH-1 downto 0) )
  ) ;


  component TestCtrl is
    port (
      -- Global Signal Interface
      Clk                 : In    std_logic ;
      nReset              : In    std_logic ;

      -- Transaction Interfaces
      ManagerRec          : inout AddressBusRecType ;
      SubordinateRec      : inout AddressBusRecType
    ) ;
  end component TestCtrl ;


begin

  -- create Clock
  Osvvm.ClockResetPkg.CreateClock (
    Clk        => Clk,
    Period     => Tperiod_Clk
  )  ;

  -- create nReset
  Osvvm.ClockResetPkg.CreateReset (
    Reset       => nReset,
    ResetActive => '0',
    Clk         => Clk,
    Period      => 7 * tperiod_Clk,
    tpd         => tpd
  ) ;
  
  ------------------------------------------------------------
  Axi4LitePassThru_1 : Axi4LitePassThru 
  ------------------------------------------------------------
  port map (
  -- AXI Manager Interface - Driven By PassThru
    -- AXI Write Address Channel
    mAwAddr       => AxiBus2.WriteAddress.Addr,
    mAwProt       => AxiBus2.WriteAddress.Prot,
    mAwValid      => AxiBus2.WriteAddress.Valid,
    mAwReady      => AxiBus2.WriteAddress.Ready,

    -- AXI Write Data Channel
    mWData        => AxiBus2.WriteData.Data, 
    mWStrb        => AxiBus2.WriteData.Strb, 
    mWValid       => AxiBus2.WriteData.Valid, 
    mWReady       => AxiBus2.WriteData.Ready, 

    -- AXI Write Response Channel
    mBValid       => AxiBus2.WriteResponse.Valid, 
    mBReady       => AxiBus2.WriteResponse.Ready, 
    mBResp        => AxiBus2.WriteResponse.Resp, 
  
    -- AXI Read Address Channel
    mArAddr       => AxiBus2.ReadAddress.Addr,
    mArProt       => AxiBus2.ReadAddress.Prot,
    mArValid      => AxiBus2.ReadAddress.Valid,
    mArReady      => AxiBus2.ReadAddress.Ready,

    -- AXI Read Data Channel
    mRData        => AxiBus2.ReadData.Data, 
    mRResp        => AxiBus2.ReadData.Resp,
    mRValid       => AxiBus2.ReadData.Valid, 
    mRReady       => AxiBus2.ReadData.Ready, 


  -- AXI Subordinate Interface - Driven by DUT
    -- AXI Write Address Channel
    sAwAddr       => AxiBus1.WriteAddress.Addr,
    sAwProt       => AxiBus1.WriteAddress.Prot,
    sAwValid      => AxiBus1.WriteAddress.Valid,
    sAwReady      => AxiBus1.WriteAddress.Ready,

    -- AXI Write Data Channel
    sWData        => AxiBus1.WriteData.Data,  
    sWStrb        => AxiBus1.WriteData.Strb,  
    sWValid       => AxiBus1.WriteData.Valid, 
    sWReady       => AxiBus1.WriteData.Ready, 

    -- AXI Write Response Channel
    sBValid       => AxiBus1.WriteResponse.Valid, 
    sBReady       => AxiBus1.WriteResponse.Ready, 
    sBResp        => AxiBus1.WriteResponse.Resp,  
  
    -- AXI Read Address Channel
    sArAddr       => AxiBus1.ReadAddress.Addr,
    sArProt       => AxiBus1.ReadAddress.Prot,
    sArValid      => AxiBus1.ReadAddress.Valid,
    sArReady      => AxiBus1.ReadAddress.Ready,

    -- AXI Read Data Channel
    sRData        => AxiBus1.ReadData.Data,  
    sRResp        => AxiBus1.ReadData.Resp,
    sRValid       => AxiBus1.ReadData.Valid, 
    sRReady       => AxiBus1.ReadData.Ready 
  ) ;
    
  Memory_1 : Axi4LiteMemory
  port map (
    -- Globals
    Clk         => Clk,
    nReset      => nReset,

    -- AXI Manager Functional Interface
    AxiBus      => AxiBus2,

    -- Testbench Transaction Interface
    TransRec    => SubordinateRec
  ) ;

  Manager_1 : Axi4LiteManager
  port map (
    -- Globals
    Clk         => Clk,
    nReset      => nReset,

    -- AXI Manager Functional Interface
    AxiBus      => AxiBus1,

    -- Testbench Transaction Interface
    TransRec    => ManagerRec
  ) ;


  Monitor_1 : Axi4LiteMonitor
  port map (
    -- Globals
    Clk         => Clk,
    nReset      => nReset,

    -- AXI Manager Functional Interface
    AxiBus      => AxiBus1
  ) ;


  TestCtrl_1 : TestCtrl
  port map (
    -- Globals
    Clk            => Clk,
    nReset         => nReset,

    -- Testbench Transaction Interfaces
    ManagerRec     => ManagerRec,
    SubordinateRec => SubordinateRec
  ) ;

end architecture TestHarness ;