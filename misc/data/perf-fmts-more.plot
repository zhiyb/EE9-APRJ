#!/usr/bin/env gnuplot
# Box plot of performance comparison between
# different video formats on the same platform

set terminal pdf size 4, 5
set output out
set tmargin at screen 0.9

u = host."-normal.csv"
p = host."-playback.csv"
c = host."-controller.csv"
set datafile separator ","
set key autotitle columnhead

set offsets 0.4, 0.4, 0, 0
set grid y2tics my2tics
set boxwidth 0.25
boxoff = 0.15

set xtics rotate scale 0

set ylabel rotate offset 1.5 "CPU usage (%)"
set yrange [0<* : *<100]
set ytics rotate nomirror

set y2label rotate offset -2 "Refresh rate (fps)"
set logscale y2
set y2tics rotate
set my2tics 10

set key horizontal samplen 0.1 lm b reverse width -1.8

xspc = 2.0
y = 6
x = 2.4
set label "CPU usage at 50 fps" at character x, y left rotate
x = x + xspc
set label "Playback unlimited" at character x, y left rotate

plot \
	u using ($0 - boxoff):5:4:9:8 lc 3 title " " axes x1y1 with candlesticks whiskerbars, \
	'' using ($0 - boxoff):6:6:6:6 lc 3 lt -1 notitle axes x1y1 with candlesticks, \
	'' using ($0 - boxoff):7 lc 3 pt 2 notitle axes x1y1 with linespoints, \
	p using ($0 + boxoff):11:10:15:14 lc 1 title " " axes x1y2 with candlesticks whiskerbars, \
	'' using ($0 + boxoff):12:12:12:12 lc 1 lt -1 notitle axes x1y2 with candlesticks, \
	'' using ($0 + boxoff):13 lc 1 pt 2 notitle axes x1y2 with linespoints, \
	'' using 0:13:xticlabels(2) lc -1 pt -1 title " " axes x1y2 with points,
