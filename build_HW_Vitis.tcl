set prj_dir [pwd]

set OPENCV_INCLUDE "$prj_dir/External/opencv/include"
set OPENCV_LIB "$prj_dir/External/opencv/x64/mingw/staticlib"

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

puts "\n--- Select Design File ---"
set idx 1
foreach f $design_files {
    puts "$idx. [file tail $f]"
    incr idx
}
puts -nonewline "Enter the number of the file to compile: "
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
puts -nonewline "Enter your choice (1-4): "
flush stdout
set action_choice [gets stdin]


open_project -reset "HW_Vitis"
set_top $top_func

add_files $selected_file -cflags "-I$prj_dir/External/Vitis_Libraries/vision/L1/include -std=c++14"

if {[file exists $tb_file_1]} {
    add_files -tb $tb_file_1 -cflags "-I$prj_dir/External/Vitis_Libraries/vision/L1/include -I$OPENCV_INCLUDE -std=c++14"
} elseif {[file exists $tb_file_2]} {
    add_files -tb $tb_file_2 -cflags "-I$prj_dir/External/Vitis_Libraries/vision/L1/include -I$OPENCV_INCLUDE -std=c++14"
}


open_solution -reset "solution1" -flow_target vivado
set_part {xc7z020clg400-1}
create_clock -period 10 -name default

set TEST_IMAGE "$prj_dir/HW_Vitis/assets/${top_func}.jpg"
if {![file exists $TEST_IMAGE]} {
    set TEST_IMAGE "$prj_dir/HW_Vitis/assets/${top_func}.png"
}

set opencv_ldflags "-L$OPENCV_LIB -Wl,--start-group -lopencv_world440 -llibjpeg-turbo -llibpng -llibwebp -llibtiff -llibjasper -lIlmImf -lzlib -lade -llibprotobuf -lquirc -lgdi32 -lole32 -loleaut32 -luuid -lcomdlg32 -Wl,--end-group"

if {$action_choice == 1 || $action_choice == 3 || $action_choice == 4} {
    csynth_design
}

if {$action_choice == 2} {
    csim_design -ldflags "$opencv_ldflags" -argv "$TEST_IMAGE"
}

if {$action_choice == 3} {
    cosim_design -ldflags "$opencv_ldflags" -argv "$TEST_IMAGE"
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