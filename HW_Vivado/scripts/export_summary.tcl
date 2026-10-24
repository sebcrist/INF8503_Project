# Locate the permanent HW_Vivado/scripts/logs directory so logs are NEVER
# wiped when Vivado cleans HW_Vivado.runs/impl_1 on re-runs
if {[info script] ne ""} {
    set script_dir [file normalize [file dirname [info script]]]
} else {
    set script_dir [file normalize "[get_property DIRECTORY [current_project]]/scripts"]
}
regsub {/HW_Vivado\.runs/.*} $script_dir "" root_hw
if {$root_hw ne $script_dir} {
    set script_dir "$root_hw/scripts"
}
set logs_dir "$script_dir/logs"
file mkdir $logs_dir

set timestamp [clock format [clock seconds] -format {%Y%m%d_%H%M%S}] 
set current_report "$logs_dir/perf_report_$timestamp.txt" 
set comparison_log "$logs_dir/performance_verdict.txt" 

set max_history_runs 100

report_utilization -file $current_report 
report_timing_summary -file $current_report -append 
report_power -file $current_report -append

proc get_metric {filename pattern} { 
    if {![file exists $filename] || $filename == ""} { return "N/A" } 
    set fp [open $filename r] 
    set value "N/A" 
    while {[gets $fp line] >= 0} { 
        if {[regexp $pattern $line match val]} { 
            set value [string trim $val] 
            break 
        } 
    } 
    close $fp 
    return $value 
} 

# NEW: Procedure to parse the Vivado Design Timing Summary table
proc get_timing_metrics {filename} {
    set wns "N/A"
    set whs "N/A"
    set wpws "N/A"
    if {![file exists $filename] || $filename == ""} { return [list $wns $whs $wpws] } 
    set fp [open $filename r] 
    set state 0
    while {[gets $fp line] >= 0} { 
        if {$state == 0 && [string match "*WNS(ns)*TNS(ns)*WHS(ns)*" $line]} {
            set state 1
        } elseif {$state == 1 && [string match "*-------*" $line]} {
            set state 2
        } elseif {$state == 2} {
            set tokens [regexp -inline -all -- {\S+} $line]
            if {[llength $tokens] >= 9} {
                set wns [lindex $tokens 0]
                set whs [lindex $tokens 4]
                set wpws [lindex $tokens 8]
            }
            break
        }
    } 
    close $fp 
    return [list $wns $whs $wpws]
}

set pat_lut {\|\s*Slice LUTs\s*\|\s*(\d+)} 
set pat_ff  {\|\s*Register as Flip Flop\s*\|\s*(\d+)} 
set pat_bram {\|\s*Block RAM Tile\s*\|\s*([\d\.]+)} 
set pat_dsp {\|\s*DSPs\s*\|\s*(\d+)} 
set pat_iob {\|\s*Bonded IOB\s*\|\s*(\d+)}
set pat_pwr {\|\s*Total On-Chip Power \(W\)\s*\|\s*([\d\.]+)}

set all_reports [lsort -decreasing [glob -nocomplain "$logs_dir/perf_report_*.txt"]] 

proc format_val {curr prev is_hw} {
    if {$curr == "N/A"} { return "N/A" }
    if {$prev == "N/A" || $curr == $prev} { return [format "%s " $curr] }
    if {$is_hw} {
        if {$curr < $prev} { return [format "%s+" $curr] } else { return [format "%s-" $curr] }
    } else {
        if {$curr > $prev} { return [format "%s+" $curr] } else { return [format "%s-" $curr] }
    }
}

set out [open $comparison_log w] 
puts $out "================================================================================================================================" 
puts $out " \[2\] SOMMAIRE DES $max_history_runs DERNIERS RUNS" 
puts $out "================================================================================================================================"
puts $out "  Legende: (+) Amelioration, (-) Deterioration, ( ) Inchange"
puts $out "--------------------------------------------------------------------------------------------------------------------------------" 
puts $out "  Date & Heure        | WNS(ns) | WHS(ns) | WPWS(ns)| LUTs    | FFs     | BRAM    | DSP   | IOB   | Pwr(W)" 
puts $out "  -------------------|---------|---------|---------|---------|---------|---------|-------|-------|-------" 

set count 0 
set total_reps [llength $all_reports]

for {set i 0} {$i < $total_reps} {incr i} {
    if {$count >= $max_history_runs} { break } 
    
    set rep [lindex $all_reports $i]
    set prev_rep ""
    if {$i + 1 < $total_reps} {
        set prev_rep [lindex $all_reports [expr {$i + 1}]]
    }

    regexp {perf_report_(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})} $rep m ans mois jour h min 
    set date_str "$jour/$mois/$ans $h:$min" 
    set tag [expr {$rep == $current_report ? "* " : "  "}] 
    
    # CHANGED: Retrieve timing values using the new table parser
    set curr_timing [get_timing_metrics $rep]
    set curr_wns [lindex $curr_timing 0]
    set curr_whs [lindex $curr_timing 1]
    set curr_wpws [lindex $curr_timing 2]

    if {$prev_rep ne ""} {
        set prev_timing [get_timing_metrics $prev_rep]
        set prev_wns [lindex $prev_timing 0]
        set prev_whs [lindex $prev_timing 1]
        set prev_wpws [lindex $prev_timing 2]
    } else {
        set prev_wns "N/A"
        set prev_whs "N/A"
        set prev_wpws "N/A"
    }

    set str_wns [format_val $curr_wns $prev_wns 0]
    set str_whs [format_val $curr_whs $prev_whs 0]
    set str_wpws [format_val $curr_wpws $prev_wpws 0]

    set curr_lut [get_metric $rep $pat_lut]
    set prev_lut [expr {$prev_rep ne "" ? [get_metric $prev_rep $pat_lut] : "N/A"}]
    set str_lut [format_val $curr_lut $prev_lut 1]

    set curr_ff [get_metric $rep $pat_ff]
    set prev_ff [expr {$prev_rep ne "" ? [get_metric $prev_rep $pat_ff] : "N/A"}]
    set str_ff [format_val $curr_ff $prev_ff 1]

    set curr_bram [get_metric $rep $pat_bram]
    set prev_bram [expr {$prev_rep ne "" ? [get_metric $prev_rep $pat_bram] : "N/A"}]
    set str_bram [format_val $curr_bram $prev_bram 1]

    set curr_dsp [get_metric $rep $pat_dsp]
    set prev_dsp [expr {$prev_rep ne "" ? [get_metric $prev_rep $pat_dsp] : "N/A"}]
    set str_dsp [format_val $curr_dsp $prev_dsp 1]

    set curr_iob [get_metric $rep $pat_iob]
    set prev_iob [expr {$prev_rep ne "" ? [get_metric $prev_rep $pat_iob] : "N/A"}]
    set str_iob [format_val $curr_iob $prev_iob 1]

    set curr_pwr [get_metric $rep $pat_pwr]
    set prev_pwr [expr {$prev_rep ne "" ? [get_metric $prev_rep $pat_pwr] : "N/A"}]
    set str_pwr [format_val $curr_pwr $prev_pwr 1]

    puts $out [format "%s%-17s | %-7s | %-7s | %-7s | %-7s | %-7s | %-7s | %-5s | %-5s | %-5s" $tag $date_str $str_wns $str_whs $str_wpws $str_lut $str_ff $str_bram $str_dsp $str_iob $str_pwr] 
    incr count 
} 

puts $out "" 
puts $out "================================================================================================================================" 
close $out 

exec notepad.exe $comparison_log &