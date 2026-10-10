############################################################
## This file is generated automatically by Vitis HLS.
## Please DO NOT edit it.
## Copyright 1986-2022 Xilinx, Inc. All Rights Reserved.
############################################################
open_project HW_Vitis
set_top gaussian
add_files HW_Vitis/src/gaussian.cpp -cflags "-I../../External/Vitis_Libraries/vision/L1/include -std=c++14"
open_solution "solution1" -flow_target vivado
set_part {xc7z020clg400-1}
create_clock -period 10 -name default
#source "./HW_Vitis/solution1/directives.tcl"
#csim_design
csynth_design
#cosim_design
export_design -format ip_catalog
