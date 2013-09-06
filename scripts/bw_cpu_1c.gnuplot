set terminal pdf size 3.125,2.5 font "Times-Roman, 8"
set key left top
set ylabel "CPU Utilization"
set xlabel "Throughput in GBit/s"

do for [tso in "tso notso"] {
   do for [loadgen in "VM external"] {

      set output sprintf('bw_cpu_1c_%s_%s.pdf',tso,loadgen)

      datafile_vhost = sprintf('< grep ^1 bw_cpu_%s_vhost_%s.csv | sort -k 2 -n',tso,loadgen)
      datafile_sv3 = sprintf('< grep ^1 bw_cpu_%s_sv3_%s.csv | sort -k 2 -n',tso,loadgen)
      if (tso eq "tso") {
            trail = ", tso"
      } else {
            trail = ""
      }

      if (loadgen eq "VM") {
            # Subtract load of load generating VM
            loadsub = 1
      } else {
            loadsub = 0
      }

      plot datafile_vhost using ($2/1000):($2!=0?$3-loadsub:1/0) t sprintf('vhost%s',trail) w linesp, \
           datafile_sv3   using ($2/1000):($2!=0?$3-loadsub:1/0) t sprintf('sv3%s',trail)   w linesp, \
           datafile_sv3   using ($2/1000):($2!=0?$5:1/0) t sprintf('sv3, switch only%s',trail) w linesp
   }
}

# plot '< sort -k 2 -n bw_cpu_vhost_external.csv' using ($2/1000):($1==1&&$2!=0?$3:1/0) t 'ext-to-VM (vhost)' w linesp, \
#      '< sort -k 2 -n bw_cpu_sv3_external.csv'   using ($2/1000):($1==1&&$2!=0?$3:1/0) t 'ext-to-VM (sv3)'   w linesp, \
#      '' using ($2/1000):($1==1&&$2!=0?$5:1/0) t 'ext-to-VM (sv3, switch only)' w linesp

# set output 'bw_cpu_1c_tso.pdf'
# plot '< sort -k 2 -n bw_cpu_vhost_VM.csv ' using ($2/1000):($1==1&&$2!=0?$3-1:1/0) t 'VM-to-VM (vhost)' w linesp, \
#      '< sort -k 2 -n bw_cpu_sv3_VM.csv' using ($2/1000):($1==1&&$2!=0?$3-1:1/0) t 'VM-to-VM (sv3)' w linesp, \
#      '' using ($2/1000):($1==1&&$2!=0?$5:1/0) t 'VM-to-VM (sv3, switch only)' w linesp
