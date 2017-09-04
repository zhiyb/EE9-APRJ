#!/usr/bin/env gnuplot
# Box plot of performance comparison between
# VixenLinky and VixenConsole on different platforms

set terminal pdf size 3.5, 5
set output out
set tmargin at screen 0.9

raw_p = "types-raw-playback.csv"
raw_c = "types-raw-controller.csv"
seq_p = "types-seq-playback.csv"
seq_c = "types-seq-controller.csv"
set datafile separator ","
set key autotitle columnhead

set grid y2tics my2tics
set boxwidth 0.25
boxoff = 0.15

set offsets 0.4, 0.4, 0, 0
set xtics rotate scale 0
unset ytics
set y2tics rotate
set my2tics 10
set logscale y
set logscale y2
set y2label rotate offset -2 "Refresh rate (fps)"
set key horizontal samplen 0.1 lm b reverse width -1.8

xspc = 2.0
y = 5.5
x = 2.4
set label "VixenLinky playback unlimited" at character x, y left rotate
x = x + xspc
set label "VixenLinky controller unlimited" at character x, y left rotate
x = x + xspc
set label "VixenConsole playback unlimited" at character x, y left rotate
x = x + xspc
set label "VixenConsole controller unlimited" at character x, y left rotate

plot \
	raw_p using ($0 - boxoff):11:10:15:14 lc 1 title " " with candlesticks whiskerbars, \
	'' using ($0 - boxoff):12:12:12:12 lc 1 lt -1 notitle with candlesticks, \
	'' using ($0 - boxoff):13 lc 1 pt 2 notitle with points, \
	raw_c using ($0 + boxoff):17:16:21:20 lc 2 title " " with candlesticks whiskerbars, \
	'' using ($0 + boxoff):18:18:18:18 lc 2 lt -1 notitle with candlesticks, \
	'' using ($0 + boxoff):19 lc 2 pt 2 notitle with points, \
	seq_p using ($0 - boxoff):11:10:15:14 lc 3 title " " with candlesticks whiskerbars, \
	'' using ($0 - boxoff):12:12:12:12 lc 3 lt -1 notitle with candlesticks, \
	'' using ($0 - boxoff):13 lc 3 pt 2 notitle with points, \
	seq_c using ($0 + boxoff):17:16:21:20 lc 4 title " " with candlesticks whiskerbars, \
	'' using ($0 + boxoff):18:18:18:18 lc 4 lt -1 notitle with candlesticks, \
	'' using ($0 + boxoff):19 lc 4 pt 2 notitle with points, \
	'' using 0:18:xticlabels(1) lc -1 pt -1 title " " with points,
