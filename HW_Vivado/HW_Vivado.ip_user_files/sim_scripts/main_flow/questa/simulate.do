onbreak {quit -f}
onerror {quit -f}

vsim -lib xil_defaultlib main_flow_opt

set NumericStdNoWarnings 1
set StdArithNoWarnings 1

do {wave.do}

view wave
view structure
view signals

do {main_flow.udo}

run -all

quit -force
