--
--  File Name:         TbAb_InterruptCoSim3.vhd
--  Design Unit Name:  Architecture of TestCtrl
--  Revision:          OSVVM MODELS STANDARD VERSION
--
--  Maintainer:        Simon Southwell  email: simon.southwell@gmail.com
--  Contributor(s):
--     Simon Southwell      simon.southwell@gmail.com
--
--
--  Description:
--      Test interrupt handling done in CoSim interface
--
--  Revision History:
--    Date      Version    Description
--    10/2022   2023.01    Initial revision
--
--
--  This file is part of OSVVM.
--
--  Copyright (c) 2022 by [OSVVM Authors](../../AUTHORS.md)
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

architecture InterruptCoSim3 of TestCtrl is

  signal ManagerSync1, MemorySync1, TestDone : integer_barrier := 1 ;
  signal GlobalIntReq : std_logic_vector(gIntReq'range) ;

begin

  GlobalIntReq <= gIntReq ; 

  ------------------------------------------------------------
  -- ControlProc
  --   Set up AlertLog and wait for end of test
  ------------------------------------------------------------
  ControlProc : process
  begin

    -- Initialization of test
--    SetTestName("TbAb_InterruptCoSim3") ;
    SetLogEnable(PASSED, TRUE) ;    -- Enable PASSED logs
    SetLogEnable(INFO, TRUE) ;    -- Enable INFO logs
    SetLogEnable(GetAlertLogID("Memory_1"), INFO, FALSE) ;

    -- Wait for testbench initialization
    wait for 0 ns ;  wait for 0 ns ;
    TranscriptOpen ;  -- SetTestName done in SW
    SetTranscriptMirror(TRUE) ;

    -- Wait for Design Reset
    wait until nReset = '1' ;
    ClearAlerts ;

    -- Wait for test to finish
    WaitForBarrier(TestDone, 35 ms) ;
    AlertIf(now >= 35 ms, "Test finished due to timeout") ;
    AlertIf(GetAffirmCount < 1, "Test is not Self-Checking");


    TranscriptClose ;
    -- Printing differs in different simulators due to differences in process order execution
    -- AffirmIfTranscriptsMatch(PATH_TO_VALIDATED_RESULTS) ;

    EndOfTestReports ;
    std.env.stop ;
    wait ;
  end process ControlProc ;

  ------------------------------------------------------------
  -- ManagerProc
  --   Generate transactions for AxiManager
  ------------------------------------------------------------
  ManagerProc : process
    variable Data        : std_logic_vector(AXI_DATA_WIDTH-1 downto 0) := (others => '0') ;
    variable Done        : integer := 0 ;
    variable Error       : integer := 0 ;
    variable Node        : integer := 0 ;
    variable Int         : integer := 0 ;
    variable WaitForClockRV : RandomPType ;
  begin
    wait until nReset = '1' ;
    WaitForClock(ManagerRec, 2) ;

    -- Initialise VProc code
    CoSimInit(Node);
    -- Fetch the SetTestName
    CoSimTrans(ManagerRec, Done, Error, Int, Node) ;
    
    OperationLoop : loop
    
      -- 20 % of the time add a no-op cycle with a delay of 1 to 5 clocks
      if WaitForClockRV.DistInt((8, 2)) = 1 then
        WaitForClock(ManagerRec, WaitForClockRV.RandInt(1, 5)) ;
      end if ;
      
      -- Inspect interrupt state and and convert to integer
      Int         := to_integer(signed(gIntReq)) ;

      -- Call co-simulation procedure
      CoSimTrans(ManagerRec, Done, Error, Int, Node) ;

      -- Alter if an error
      AlertIf(Error /= 0, "CoSimTrans flagged an error") ;
      
      if (ManagerRec.Operation = WRITE_OP) and (ManagerRec.Address = x"AFFFFFFC") then
        -- IntReq <= ManagerRec.DataToModel(0) ;
        Send(InterruptRecArray(0), "" & ManagerRec.DataToModel(0)) ; 
      end if;

      -- Finish flagged by software
      exit when Done /= 0;

    end loop OperationLoop ;
 
    -- Wait for outputs to propagate and signal TestDone
    WaitForClock(ManagerRec, 2) ;
    WaitForBarrier(TestDone) ;
    wait ;
  end process ManagerProc ;

  ------------------------------------------------------------
  -- InterruptProc
  --   Generate interupts in lieu of a DUT
  ------------------------------------------------------------
  InterruptProc : process
  begin
  
    wait ;
  
  end process InterruptProc ;

  ------------------------------------------------------------
  -- SubordinateProc
  --   Generate transactions for AxiSubordinate
  ------------------------------------------------------------
  SubordinateProc : process
    variable Addr : std_logic_vector(AXI_ADDR_WIDTH-1 downto 0) ;
    variable Data : std_logic_vector(AXI_DATA_WIDTH-1 downto 0) ;
  begin

    -- Wait for outputs to propagate and signal TestDone
    WaitForClock(SubordinateRec, 2) ;
    WaitForBarrier(TestDone) ;
    wait ;
  end process SubordinateProc ;


end InterruptCoSim3 ;

Configuration TbAb_InterruptCoSim3 of TbAddressBusMemory is
  for TestHarness
    for TestCtrl_1 : TestCtrl
      use entity work.TestCtrl(InterruptCoSim3) ;
    end for ;
--!!    for Subordinate_1 : Axi4Subordinate
--!!      use entity OSVVM_AXI4.Axi4Memory ;
--!!    end for ;
  end for ;
end TbAb_InterruptCoSim3 ;