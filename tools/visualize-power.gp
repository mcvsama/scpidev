system('cp log > visualize-power.dat')

set key autotitle columnhead
set xlabel "time (UTC)"
set ylabel "power [W]"
set xdata time
set timefmt "%s"
set format x "%H:%M:%S"
set grid x y mx my lt 1 lc rgb 'gray90'
set datafile separator ","
plot 'log' using 1:6 with lines lw 1.5 title 'power'

pause -1

