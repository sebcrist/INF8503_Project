-makelib xcelium_lib/xilinx_vip -sv \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi4stream_vip_axi4streampc.sv" \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi_vip_axi4pc.sv" \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/xil_common_vip_pkg.sv" \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi4stream_vip_pkg.sv" \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi_vip_pkg.sv" \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi4stream_vip_if.sv" \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi_vip_if.sv" \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/clk_vip_if.sv" \
  "C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/rst_vip_if.sv" \
-endlib
-makelib xcelium_lib/xpm -sv \
  "C:/Xilinx/Vivado/2022.1/data/ip/xpm/xpm_cdc/hdl/xpm_cdc.sv" \
  "C:/Xilinx/Vivado/2022.1/data/ip/xpm/xpm_fifo/hdl/xpm_fifo.sv" \
  "C:/Xilinx/Vivado/2022.1/data/ip/xpm/xpm_memory/hdl/xpm_memory.sv" \
-endlib
-makelib xcelium_lib/xpm \
  "C:/Xilinx/Vivado/2022.1/data/ip/xpm/xpm_VCOMP.vhd" \
-endlib
-makelib xcelium_lib/axi_infrastructure_v1_1_0 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl/axi_infrastructure_v1_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/axi_vip_v1_1_12 -sv \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/1033/hdl/axi_vip_v1_1_vl_rfs.sv" \
-endlib
-makelib xcelium_lib/processing_system7_vip_v1_0_14 -sv \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl/processing_system7_vip_v1_0_vl_rfs.sv" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_processing_system7_0_0/sim/main_flow_processing_system7_0_0.v" \
  "../../../bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/sim/ila_pixclk.v" \
  "../../../bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/sim/ila_refclk.v" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/DVI_Constants.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/ChannelBond.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/SyncAsync.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/GlitchFilter.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/TWI_SlaveCtl.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/EEPROM_8b.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/InputSERDES.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/PhaseAlign.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/ResyncToBUFG.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/SyncAsyncReset.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/SyncBase.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/TMDS_Clocking.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/TMDS_Decoder.vhd" \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/f99d/src/dvi2rgb.vhd" \
  "../../../bd/main_flow/ip/main_flow_dvi2rgb_0_0/sim/main_flow_dvi2rgb_0_0.vhd" \
-endlib
-makelib xcelium_lib/xlconstant_v1_1_7 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fcfc/hdl/xlconstant_v1_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_xlconstant_0_0/sim/main_flow_xlconstant_0_0.v" \
-endlib
-makelib xcelium_lib/v_vid_in_axi4s_v5_0_1 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/b872/hdl/v_vid_in_axi4s_v5_0_vl_rfs.v" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_v_vid_in_axi4s_0_0/sim/main_flow_v_vid_in_axi4s_0_0.v" \
-endlib
-makelib xcelium_lib/lib_cdc_v1_0_2 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ef1e/hdl/lib_cdc_v1_0_rfs.vhd" \
-endlib
-makelib xcelium_lib/lib_pkg_v1_0_2 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/0513/hdl/lib_pkg_v1_0_rfs.vhd" \
-endlib
-makelib xcelium_lib/fifo_generator_v13_2_7 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/83df/simulation/fifo_generator_vlog_beh.v" \
-endlib
-makelib xcelium_lib/fifo_generator_v13_2_7 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/83df/hdl/fifo_generator_v13_2_rfs.vhd" \
-endlib
-makelib xcelium_lib/fifo_generator_v13_2_7 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/83df/hdl/fifo_generator_v13_2_rfs.v" \
-endlib
-makelib xcelium_lib/lib_fifo_v1_0_16 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/6c82/hdl/lib_fifo_v1_0_rfs.vhd" \
-endlib
-makelib xcelium_lib/blk_mem_gen_v8_4_5 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/25a8/simulation/blk_mem_gen_v8_4.v" \
-endlib
-makelib xcelium_lib/lib_bmg_v1_0_14 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/3a3e/hdl/lib_bmg_v1_0_rfs.vhd" \
-endlib
-makelib xcelium_lib/lib_srl_fifo_v1_0_2 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/51ce/hdl/lib_srl_fifo_v1_0_rfs.vhd" \
-endlib
-makelib xcelium_lib/axi_datamover_v5_1_28 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/1bb8/hdl/axi_datamover_v5_1_vh_rfs.vhd" \
-endlib
-makelib xcelium_lib/axi_vdma_v6_3_14 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl/axi_vdma_v6_3_rfs.v" \
-endlib
-makelib xcelium_lib/axi_vdma_v6_3_14 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl/axi_vdma_v6_3_rfs.vhd" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_axi_vdma_0_0/sim/main_flow_axi_vdma_0_0.vhd" \
-endlib
-makelib xcelium_lib/proc_sys_reset_v5_0_13 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8842/hdl/proc_sys_reset_v5_0_vh_rfs.vhd" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_rst_ps7_0_100M_0/sim/main_flow_rst_ps7_0_100M_0.vhd" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/sim/main_flow.v" \
-endlib
-makelib xcelium_lib/generic_baseblocks_v2_1_0 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/b752/hdl/generic_baseblocks_v2_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/axi_register_slice_v2_1_26 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/0a3f/hdl/axi_register_slice_v2_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/axi_data_fifo_v2_1_25 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5390/hdl/axi_data_fifo_v2_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/axi_crossbar_v2_1_27 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/3fa0/hdl/axi_crossbar_v2_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_xbar_0/sim/main_flow_xbar_0.v" \
-endlib
-makelib xcelium_lib/v_tpg_v8_2_1 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_v_tpg_0_2/hdl/v_tpg_v8_2_rfs.v" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_v_tpg_0_2/sim/main_flow_v_tpg_0_2.v" \
-endlib
-makelib xcelium_lib/axis_infrastructure_v1_1_0 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl/axis_infrastructure_v1_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/axis_register_slice_v1_1_26 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/aca3/hdl/axis_register_slice_v1_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/axis_switch_v1_1_26 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/6c33/hdl/axis_switch_v1_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_axis_switch_0_1/sim/main_flow_axis_switch_0_1.v" \
-endlib
-makelib xcelium_lib/axi_protocol_converter_v2_1_26 \
  "../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/90c8/hdl/axi_protocol_converter_v2_1_vl_rfs.v" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  "../../../bd/main_flow/ip/main_flow_auto_pc_0/sim/main_flow_auto_pc_0.v" \
  "../../../bd/main_flow/ip/main_flow_auto_pc_1/sim/main_flow_auto_pc_1.v" \
-endlib
-makelib xcelium_lib/xil_defaultlib \
  glbl.v
-endlib

