#  File Name:         TestCases_Axi4Stream.pro
#  Revision:          STANDARD VERSION
#
#  Maintainer:        Simon Southwell  email:  simon.southwell@gmail.com
#  Contributor(s):
#     Simon Southwell   simon.southwell@gmail.com
#
#
#  Description:
#        Script to run one Axi Stream test
#
#  Developed for:
#        SynthWorks Design Inc.
#        VHDL Training Classes
#        11898 SW 128th Ave.  Tigard, Or  97223
#        http://www.SynthWorks.com
#
#  Revision History:
#    Date      Version    Description
#     3/2023   2023.04    Compile Script for OSVVM
#
#
#  This file is part of OSVVM.
#
#  Copyright (c) 2023 by [OSVVM Authors](AUTHORS.md)
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

analyze Tb_Axi4Stream.vhd

ChangeWorkingDirectory ../../tests
MkVproc  stream_axi4

TestName   CoSim_axi4_streams
simulate Tb_Axi4Stream [CoSim]
