vlib modelsim_lib/work
vlib modelsim_lib/msim

vlib modelsim_lib/msim/xilinx_vip
vlib modelsim_lib/msim/xpm
vlib modelsim_lib/msim/axi_infrastructure_v1_1_0
vlib modelsim_lib/msim/axi_vip_v1_1_12
vlib modelsim_lib/msim/processing_system7_vip_v1_0_14
vlib modelsim_lib/msim/xil_defaultlib
vlib modelsim_lib/msim/xlconstant_v1_1_7
vlib modelsim_lib/msim/v_vid_in_axi4s_v5_0_1
vlib modelsim_lib/msim/lib_cdc_v1_0_2
vlib modelsim_lib/msim/lib_pkg_v1_0_2
vlib modelsim_lib/msim/fifo_generator_v13_2_7
vlib modelsim_lib/msim/lib_fifo_v1_0_16
vlib modelsim_lib/msim/blk_mem_gen_v8_4_5
vlib modelsim_lib/msim/lib_bmg_v1_0_14
vlib modelsim_lib/msim/lib_srl_fifo_v1_0_2
vlib modelsim_lib/msim/axi_datamover_v5_1_28
vlib modelsim_lib/msim/axi_vdma_v6_3_14
vlib modelsim_lib/msim/proc_sys_reset_v5_0_13
vlib modelsim_lib/msim/generic_baseblocks_v2_1_0
vlib modelsim_lib/msim/axi_register_slice_v2_1_26
vlib modelsim_lib/msim/axi_data_fifo_v2_1_25
vlib modelsim_lib/msim/axi_crossbar_v2_1_27
vlib modelsim_lib/msim/v_tpg_v8_2_1
vlib modelsim_lib/msim/axis_infrastructure_v1_1_0
vlib modelsim_lib/msim/axis_register_slice_v1_1_26
vlib modelsim_lib/msim/axis_switch_v1_1_26
vlib modelsim_lib/msim/axi_protocol_converter_v2_1_26

vmap xilinx_vip modelsim_lib/msim/xilinx_vip
vmap xpm modelsim_lib/msim/xpm
vmap axi_infrastructure_v1_1_0 modelsim_lib/msim/axi_infrastructure_v1_1_0
vmap axi_vip_v1_1_12 modelsim_lib/msim/axi_vip_v1_1_12
vmap processing_system7_vip_v1_0_14 modelsim_lib/msim/processing_system7_vip_v1_0_14
vmap xil_defaultlib modelsim_lib/msim/xil_defaultlib
vmap xlconstant_v1_1_7 modelsim_lib/msim/xlconstant_v1_1_7
vmap v_vid_in_axi4s_v5_0_1 modelsim_lib/msim/v_vid_in_axi4s_v5_0_1
vmap lib_cdc_v1_0_2 modelsim_lib/msim/lib_cdc_v1_0_2
vmap lib_pkg_v1_0_2 modelsim_lib/msim/lib_pkg_v1_0_2
vmap fifo_generator_v13_2_7 modelsim_lib/msim/fifo_generator_v13_2_7
vmap lib_fifo_v1_0_16 modelsim_lib/msim/lib_fifo_v1_0_16
vmap blk_mem_gen_v8_4_5 modelsim_lib/msim/blk_mem_gen_v8_4_5
vmap lib_bmg_v1_0_14 modelsim_lib/msim/lib_bmg_v1_0_14
vmap lib_srl_fifo_v1_0_2 modelsim_lib/msim/lib_srl_fifo_v1_0_2
vmap axi_datamover_v5_1_28 modelsim_lib/msim/axi_datamover_v5_1_28
vmap axi_vdma_v6_3_14 modelsim_lib/msim/axi_vdma_v6_3_14
vmap proc_sys_reset_v5_0_13 modelsim_lib/msim/proc_sys_reset_v5_0_13
vmap generic_baseblocks_v2_1_0 modelsim_lib/msim/generic_baseblocks_v2_1_0
vmap axi_register_slice_v2_1_26 modelsim_lib/msim/axi_register_slice_v2_1_26
vmap axi_data_fifo_v2_1_25 modelsim_lib/msim/axi_data_fifo_v2_1_25
vmap axi_crossbar_v2_1_27 modelsim_lib/msim/axi_crossbar_v2_1_27
vmap v_tpg_v8_2_1 modelsim_lib/msim/v_tpg_v8_2_1
vmap axis_infrastructure_v1_1_0 modelsim_lib/msim/axis_infrastructure_v1_1_0
vmap axis_register_slice_v1_1_26 modelsim_lib/msim/axis_register_slice_v1_1_26
vmap axis_switch_v1_1_26 modelsim_lib/msim/axis_switch_v1_1_26
vmap axi_protocol_converter_v2_1_26 modelsim_lib/msim/axi_protocol_converter_v2_1_26

vlog -work xilinx_vip  -incr -mfcu -sv -L axi_vip_v1_1_12 -L processing_system7_vip_v1_0_14 -L xilinx_vip "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi4stream_vip_axi4streampc.sv" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi_vip_axi4pc.sv" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/xil_common_vip_pkg.sv" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi4stream_vip_pkg.sv" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi_vip_pkg.sv" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi4stream_vip_if.sv" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/axi_vip_if.sv" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/clk_vip_if.sv" \
"C:/Xilinx/Vivado/2022.1/data/xilinx_vip/hdl/rst_vip_if.sv" \

vlog -work xpm  -incr -mfcu -sv -L axi_vip_v1_1_12 -L processing_system7_vip_v1_0_14 -L xilinx_vip "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"C:/Xilinx/Vivado/2022.1/data/ip/xpm/xpm_cdc/hdl/xpm_cdc.sv" \
"C:/Xilinx/Vivado/2022.1/data/ip/xpm/xpm_fifo/hdl/xpm_fifo.sv" \
"C:/Xilinx/Vivado/2022.1/data/ip/xpm/xpm_memory/hdl/xpm_memory.sv" \

vcom -work xpm  -93 \
"C:/Xilinx/Vivado/2022.1/data/ip/xpm/xpm_VCOMP.vhd" \

vlog -work axi_infrastructure_v1_1_0  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl/axi_infrastructure_v1_1_vl_rfs.v" \

vlog -work axi_vip_v1_1_12  -incr -mfcu -sv -L axi_vip_v1_1_12 -L processing_system7_vip_v1_0_14 -L xilinx_vip "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/1033/hdl/axi_vip_v1_1_vl_rfs.sv" \

vlog -work processing_system7_vip_v1_0_14  -incr -mfcu -sv -L axi_vip_v1_1_12 -L processing_system7_vip_v1_0_14 -L xilinx_vip "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl/processing_system7_vip_v1_0_vl_rfs.sv" \

vlog -work xil_defaultlib  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../bd/main_flow/ip/main_flow_processing_system7_0_0/sim/main_flow_processing_system7_0_0.v" \
"../../../bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/sim/ila_pixclk.v" \
"../../../bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/sim/ila_refclk.v" \

vcom -work xil_defaultlib  -93 \
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

vlog -work xlconstant_v1_1_7  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fcfc/hdl/xlconstant_v1_1_vl_rfs.v" \

vlog -work xil_defaultlib  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../bd/main_flow/ip/main_flow_xlconstant_0_0/sim/main_flow_xlconstant_0_0.v" \

vlog -work v_vid_in_axi4s_v5_0_1  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/b872/hdl/v_vid_in_axi4s_v5_0_vl_rfs.v" \

vlog -work xil_defaultlib  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../bd/main_flow/ip/main_flow_v_vid_in_axi4s_0_0/sim/main_flow_v_vid_in_axi4s_0_0.v" \

vcom -work lib_cdc_v1_0_2  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ef1e/hdl/lib_cdc_v1_0_rfs.vhd" \

vcom -work lib_pkg_v1_0_2  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/0513/hdl/lib_pkg_v1_0_rfs.vhd" \

vlog -work fifo_generator_v13_2_7  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/83df/simulation/fifo_generator_vlog_beh.v" \

vcom -work fifo_generator_v13_2_7  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/83df/hdl/fifo_generator_v13_2_rfs.vhd" \

vlog -work fifo_generator_v13_2_7  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/83df/hdl/fifo_generator_v13_2_rfs.v" \

vcom -work lib_fifo_v1_0_16  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/6c82/hdl/lib_fifo_v1_0_rfs.vhd" \

vlog -work blk_mem_gen_v8_4_5  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/25a8/simulation/blk_mem_gen_v8_4.v" \

vcom -work lib_bmg_v1_0_14  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/3a3e/hdl/lib_bmg_v1_0_rfs.vhd" \

vcom -work lib_srl_fifo_v1_0_2  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/51ce/hdl/lib_srl_fifo_v1_0_rfs.vhd" \

vcom -work axi_datamover_v5_1_28  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/1bb8/hdl/axi_datamover_v5_1_vh_rfs.vhd" \

vlog -work axi_vdma_v6_3_14  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl/axi_vdma_v6_3_rfs.v" \

vcom -work axi_vdma_v6_3_14  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl/axi_vdma_v6_3_rfs.vhd" \

vcom -work xil_defaultlib  -93 \
"../../../bd/main_flow/ip/main_flow_axi_vdma_0_0/sim/main_flow_axi_vdma_0_0.vhd" \

vcom -work proc_sys_reset_v5_0_13  -93 \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8842/hdl/proc_sys_reset_v5_0_vh_rfs.vhd" \

vcom -work xil_defaultlib  -93 \
"../../../bd/main_flow/ip/main_flow_rst_ps7_0_100M_0/sim/main_flow_rst_ps7_0_100M_0.vhd" \

vlog -work xil_defaultlib  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../bd/main_flow/sim/main_flow.v" \

vlog -work generic_baseblocks_v2_1_0  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/b752/hdl/generic_baseblocks_v2_1_vl_rfs.v" \

vlog -work axi_register_slice_v2_1_26  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/0a3f/hdl/axi_register_slice_v2_1_vl_rfs.v" \

vlog -work axi_data_fifo_v2_1_25  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5390/hdl/axi_data_fifo_v2_1_vl_rfs.v" \

vlog -work axi_crossbar_v2_1_27  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/3fa0/hdl/axi_crossbar_v2_1_vl_rfs.v" \

vlog -work xil_defaultlib  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../bd/main_flow/ip/main_flow_xbar_0/sim/main_flow_xbar_0.v" \

vlog -work v_tpg_v8_2_1  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_v_tpg_0_2/hdl/v_tpg_v8_2_rfs.v" \

vlog -work xil_defaultlib  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../bd/main_flow/ip/main_flow_v_tpg_0_2/sim/main_flow_v_tpg_0_2.v" \

vlog -work axis_infrastructure_v1_1_0  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl/axis_infrastructure_v1_1_vl_rfs.v" \

vlog -work axis_register_slice_v1_1_26  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/aca3/hdl/axis_register_slice_v1_1_vl_rfs.v" \

vlog -work axis_switch_v1_1_26  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/6c33/hdl/axis_switch_v1_1_vl_rfs.v" \

vlog -work xil_defaultlib  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../bd/main_flow/ip/main_flow_axis_switch_0_1/sim/main_flow_axis_switch_0_1.v" \

vlog -work axi_protocol_converter_v2_1_26  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/90c8/hdl/axi_protocol_converter_v2_1_vl_rfs.v" \

vlog -work xil_defaultlib  -incr -mfcu "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/ec67/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/5765/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/fc4b/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ipshared/8713/hdl" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_refclk/hdl/verilog" "+incdir+../../../../HW_Vivado.gen/sources_1/bd/main_flow/ip/main_flow_dvi2rgb_0_0/src/ila_pixclk/hdl/verilog" "+incdir+C:/Xilinx/Vivado/2022.1/data/xilinx_vip/include" \
"../../../bd/main_flow/ip/main_flow_auto_pc_0/sim/main_flow_auto_pc_0.v" \
"../../../bd/main_flow/ip/main_flow_auto_pc_1/sim/main_flow_auto_pc_1.v" \

vlog -work xil_defaultlib \
"glbl.v"

