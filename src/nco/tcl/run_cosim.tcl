# ============================================================
# Co-Simulation Script for nco
# ============================================================

set IP_NAME    "nco"
set TOP_FUNC   "nco"
set FPGA_PART  "xc7z020clg400-1"
set CLK_PERIOD 10

open_project proj_${IP_NAME}
set_top ${TOP_FUNC}

add_files src/${IP_NAME}.cpp
add_files src/${IP_NAME}.hpp

add_files -tb tb/tb_${IP_NAME}.cpp -cflags "-std=c++14"

open_solution "sol1" -flow_target vivado
set_part ${FPGA_PART}
create_clock -period ${CLK_PERIOD} -name default

cosim_design

exit
