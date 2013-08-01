#!/usr/bin/env gnuplot
set term wxt
splot 't.dat' using 1:2:3 w points pointtype 7, "" u 1:2:4:(0):(0):($5-$4) w vectors nohead lt -1
pause -1
