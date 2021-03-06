system('tail -n 1000 log > visualize-power-last.dat')
system('tail -n 1000 log | ./make_stats 8 20 > visualize-power-last-candlesticks.dat')

set key autotitle columnhead
set xlabel "time (UTC)"
set ylabel "power [W]"
set xdata time
set timefmt "%s"
set format x "%H:%M:%S"
set grid x y mx my lt 1 lc rgb 'gray90'
set datafile separator ","
set xrange [*:*]
#set yrange [*:*]
set yrange [-30:30.0]

# voltage: columns 8,11
# power: columns 9,13
plot \
	'visualize-power-last.dat'              using 1:9       with lines        lw 8.0 lc rgb '#ffffff00' notitle, \
	'visualize-power-last-candlesticks.dat' using 1:3:2:6:5 with candlesticks lw 2.0 lc rgb '#00ffaa00' notitle whiskerbars, \
	'visualize-power-last.dat'              using ($1 - 0.3):13 with lines    lw 3.0 lc rgb '#00f70000' title 'Hann moving avg.', \
	'visualize-power-last-candlesticks.dat' using 1:4       with dots         lw 8.0 lc rgb '#00000000' notitle

pause 1
replot
reread

