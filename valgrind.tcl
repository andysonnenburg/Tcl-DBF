package require dbf

set f [dbf open "../maps/Common Interest/BASINS0811.shp"]
puts [dbf length $f]
dbf close $f
puts 0
set f [dbf open "../maps/Common Interest/BASINS0811.shp"]
puts 1
puts [dbf index $f 0]
puts 2
puts [dbf index $f 1]
puts 3
dbf close $f
puts $f
