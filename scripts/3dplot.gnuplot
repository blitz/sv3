#!/usr/bin/env gnuplot
set term wxt
set xlabel "Poll (us)"
set ylabel "Batch size"
splot 't.dat' using 1:2:3 w points pointtype 7 t "", "" u 1:2:4:(0):(0):($5-$4) w vectors nohead lt -1 t ""
pause -1
