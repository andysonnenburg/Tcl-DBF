package require dbf

set f "../maps/Common Interest/BASINS0811.shp"
puts [dbf length $f]
puts [dbf size $f]
puts [dbf keys $f]
puts 0
set f "../maps/Common Interest/BASINS0811.shp"
puts 1
puts [dbf index $f 0]
puts 2
puts [dbf index $f 1]
puts 3
dbf foreach fields $f {
  puts $fields
}
set g $f
dbf keys $g
puts $fields
puts $f
puts [dbf keys $f]
unset g
unset f
proc exit {args} {}
