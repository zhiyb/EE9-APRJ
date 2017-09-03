#!/usr/bin/env gnuplot
avg = 50

set terminal eps size 4, 2
set output 'diff.eps'
set datafile separator ","

set key off
set grid ytics xtics
set autoscale xfix
set xlabel 'Timestamp (s)'
set ylabel 'Difference %'
set yrange [0:100]

plot 'diff.csv' using ($1/50):($2/(17268*avg)*100) with lines
