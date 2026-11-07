set prj_dir [pwd]

set OPENCV_INCLUDE "$prj_dir/External/opencv/include"
set OPENCV_LIB "$prj_dir/External/opencv/x64/mingw/staticlib"

set CLOCK_PERIOD_NS 10

set all_cpp [glob -nocomplain "$prj_dir/HW_Vitis/src/*.cpp"]
set design_files {}

foreach f $all_cpp {
    if {![string match -nocase "*_tb.cpp" $f] && ![string match -nocase "*TB.cpp" $f]} {
        lappend design_files $f
    }
}

if {[llength $design_files] == 0} {
    puts "Error: No design files found."
    exit
}

puts "\n--- Select Design File (Top Function) ---"
set idx 1
foreach f $design_files {
    puts "$idx. [file tail $f]"
    incr idx
}
puts -nonewline "Enter the number of the file to compile (serves as Top Function): "
flush stdout
set file_choice [gets stdin]
set selected_file [lindex $design_files [expr {$file_choice - 1}]]
set top_func [file rootname [file tail $selected_file]]
set tb_file_1 "$prj_dir/HW_Vitis/src/${top_func}_tb.cpp"
set tb_file_2 "$prj_dir/HW_Vitis/src/${top_func}TB.cpp"

puts "\n--- Select Action ---"
puts "1. Synthesis IP"
puts "2. Run C-Simulation"
puts "3. Synthesis IP and Run Co-Simulation"
puts "4. Synthesis IP and Export IP"
puts "5. View Synthesis History (Skip Compilation)"
puts "6. Run Integration Testbench (other_tests - Compiles all design files)"
puts -nonewline "Enter your choice (1-6): "
flush stdout
set action_choice [gets stdin]

proc format_time {cycles clock_period} {
    if {$cycles eq "N/A" || $cycles eq "" || ![string is wideinteger $cycles]} { return $cycles }
    set time_ns [expr {wide($cycles) * $clock_period}]
    
    if {$time_ns >= 1000000000} {
        return [format "%.3f s" [expr {$time_ns / 1000000000.0}]]
    } elseif {$time_ns >= 1000000} {
        return [format "%.3f ms" [expr {$time_ns / 1000000.0}]]
    } elseif {$time_ns >= 1000} {
        return [format "%.3f us" [expr {$time_ns / 1000.0}]]
    } else {
        return [format "%d ns" $time_ns]
    }
}

proc print_synthesis_history {log_file top_func clock_period} {
    if {![file exists $log_file]} {
        puts "\nNo synthesis history found for $top_func (File not found: $log_file)\n"
        return
    }
    
    set fp [open $log_file r]
    set lines [split [read -nonewline $fp] "\n"]
    close $fp
    
    puts "\n=========================================================================================================================="
    puts " SYNTHESIS HISTORY FOR: $top_func (Clock: ${clock_period}ns)"
    puts "=========================================================================================================================="
    puts [format "%-20s | %-8s | %-8s | %-10s | %-10s | %-15s | %-15s" "Run Date/Time" "BRAM" "DSP" "FF" "LUT" "Min Latency" "Max Latency"]
    puts "--------------------------------------------------------------------------------------------------------------------------"
    
    set is_first 1
    foreach line $lines {
        if {$is_first == 1} {
            set is_first 0
            continue
        }
        if {[string trim $line] != ""} {
            set parts [split $line ","]
            set l_date [lindex $parts 0]
            set l_bram [lindex $parts 1]
            set l_dsp  [lindex $parts 2]
            set l_ff   [lindex $parts 3]
            set l_lut  [lindex $parts 4]
            
            if {[llength $parts] > 6} {
                set l_lat_min [lindex $parts 5]
                set l_lat_max [lindex $parts 6]
            } else {
                set l_lat_min "N/A"
                set l_lat_max [lindex $parts 5]
            }
            
            set str_lat_min [format_time $l_lat_min $clock_period]
            set str_lat_max [format_time $l_lat_max $clock_period]
            
            puts [format "%-20s | %-8s | %-8s | %-10s | %-10s | %-15s | %-15s" $l_date $l_bram $l_dsp $l_ff $l_lut $str_lat_min $str_lat_max]
        }
    }
    puts "==========================================================================================================================\n"
}

set log_file "$prj_dir/HW_Vitis/logs/${top_func}_summary.csv"

if {$action_choice == 5} {
    print_synthesis_history $log_file $top_func $CLOCK_PERIOD_NS
} else {
    open_project -reset "HW_Vitis"
    set_top $top_func
    
    set selected_tb ""

    if {$action_choice == 6} {
        puts "\n--- Select Integration Testbench ---"
        set tb_files [lsort -unique [glob -nocomplain "$prj_dir/HW_Vitis/other_tests/*.cpp" "$prj_dir/HW_Vitis/src/*_tb.cpp" "$prj_dir/HW_Vitis/src/*TB.cpp"]]
        if {[llength $tb_files] == 0} {
            puts "Error: No testbench files found in src/ or other_tests/."
            exit
        }
        set tb_idx 1
        foreach f $tb_files {
            puts "$tb_idx. [file tail [file dirname $f]]/[file tail $f]"
            incr tb_idx
        }
        puts -nonewline "Enter the number of the testbench to run: "
        flush stdout
        set tb_choice [gets stdin]
        set selected_tb [lindex $tb_files [expr {$tb_choice - 1}]]

        foreach f $design_files {
            add_files $f -cflags "-I$prj_dir/External/Vitis_Libraries/vision/L1/include -std=c++14"
        }
        add_files -tb $selected_tb -cflags "-I$prj_dir/External/Vitis_Libraries/vision/L1/include -I$OPENCV_INCLUDE -std=c++14"

    } else {
        add_files $selected_file -cflags "-I$prj_dir/External/Vitis_Libraries/vision/L1/include -std=c++14"

        if {[file exists $tb_file_1]} {
            add_files -tb $tb_file_1 -cflags "-I$prj_dir/External/Vitis_Libraries/vision/L1/include -I$OPENCV_INCLUDE -std=c++14"
        } elseif {[file exists $tb_file_2]} {
            add_files -tb $tb_file_2 -cflags "-I$prj_dir/External/Vitis_Libraries/vision/L1/include -I$OPENCV_INCLUDE -std=c++14"
        }
    }

    open_solution -reset "solution1" -flow_target vivado
    set_part {xc7z020clg400-1}
    create_clock -period $CLOCK_PERIOD_NS -name default

    if {$action_choice == 6} {
        set tb_name_only [file rootname [file tail $selected_tb]]
        regsub -nocase {_?tb(_2|_3|_4|_multi)?$} $tb_name_only "" img_prefix
        
        set test_images [glob -nocomplain "$prj_dir/HW_Vitis/assets/${img_prefix}*.png" "$prj_dir/HW_Vitis/assets/${img_prefix}*.jpg"]
        
        if {[llength $test_images] == 0} {
            puts "Warning: No images found matching '${img_prefix}*.jpg/png' in assets/."
        } else {
            puts "Found [llength $test_images] images for testbench: $test_images"
        }
    } else {
        set img_base [glob -nocomplain "$prj_dir/HW_Vitis/assets/${top_func}*.png" "$prj_dir/HW_Vitis/assets/${top_func}*.jpg"]
        set test_images $img_base
    }
    set argv_string [join $test_images " "]

    set opencv_ldflags "-L$OPENCV_LIB -Wl,--start-group -lopencv_world440 -llibjpeg-turbo -llibpng -llibwebp -llibtiff -llibjasper -lIlmImf -lzlib -lade -llibprotobuf -lquirc -lgdi32 -lole32 -loleaut32 -luuid -lcomdlg32 -Wl,--end-group"

    if {$action_choice == 1 || $action_choice == 3 || $action_choice == 4} {
        csynth_design

        set report_xml "$prj_dir/HW_Vitis/solution1/syn/report/${top_func}_csynth.xml"
        if {[file exists $report_xml]} {
            set fp [open $report_xml r]
            set xml_data [read $fp]
            close $fp
            
            set cur_bram 0
            set cur_dsp 0
            set cur_ff 0
            set cur_lut 0
            set cur_lat_min 0
            set cur_lat_max 0
            
            regexp {<BRAM_18K>(\d+)</BRAM_18K>} $xml_data -> cur_bram
            regexp {<DSP>(\d+)</DSP>} $xml_data -> cur_dsp
            if {$cur_dsp == 0} { regexp {<DSP48E>(\d+)</DSP48E>} $xml_data -> cur_dsp }
            regexp {<FF>(\d+)</FF>} $xml_data -> cur_ff
            regexp {<LUT>(\d+)</LUT>} $xml_data -> cur_lut
            
            if {[regexp {<Best-caseLatency>([^<]+)</Best-caseLatency>} $xml_data -> match_lat_min]} {
                if {[string is integer -strict $match_lat_min]} { set cur_lat_min $match_lat_min } else { set cur_lat_min "N/A" }
            }
            if {[regexp {<Worst-caseLatency>([^<]+)</Worst-caseLatency>} $xml_data -> match_lat_max]} {
                if {[string is integer -strict $match_lat_max]} { set cur_lat_max $match_lat_max } else { set cur_lat_max "N/A" }
            }
            
            set log_dir "$prj_dir/HW_Vitis/logs"
            file mkdir $log_dir
            
            if {![file exists $log_file]} {
                set fp [open $log_file w]
                puts $fp "Date,BRAM,DSP,FF,LUT,MinLatency,MaxLatency"
                close $fp
            }
            
            set timestamp [clock format [clock seconds] -format "%Y-%m-%d %H:%M:%S"]
            set fp [open $log_file a]
            puts $fp "$timestamp,$cur_bram,$cur_dsp,$cur_ff,$cur_lut,$cur_lat_min,$cur_lat_max"
            close $fp
            
            print_synthesis_history $log_file $top_func $CLOCK_PERIOD_NS
        }
    }

    if {$action_choice == 2 || $action_choice == 6} {
        csim_design -ldflags "$opencv_ldflags" -argv "$argv_string"
    }

    if {$action_choice == 3} {
        cosim_design -ldflags "$opencv_ldflags" -argv "$argv_string"
    }

    if {$action_choice == 4} {
        export_design -format ip_catalog

        set target_ip_dir "$prj_dir/ip_repo/$top_func"
        file delete -force $target_ip_dir
        file mkdir $target_ip_dir

        foreach item [glob -nocomplain "HW_Vitis/solution1/impl/ip/*"] {
            file copy -force $item $target_ip_dir/
        }
        puts "SUCCESS: IP exported to: $target_ip_dir\n"
    }

    puts "\nProject setup and actions complete. Opening Vitis HLS GUI..."
    open_project "HW_Vitis"
}