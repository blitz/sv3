set terminal pdf size 3.125,1.8 font "Times-Roman, 8" monochrome
set output 'latency.pdf'
unset key
set boxwidth 0.2
set style fill solid
set grid ytics

set ylabel "Roundtrip Latency in μs"
#set xlabel "Throughput in GBit/s"

set yrange [0:100]
plot 'latency.csv' using 2:xticlabels(1) w boxes
