set term png
set output "plot.png"
plot [1:500][4:11] 'evo.dat' using 1:2 title 'mejor' with lines,\
                     'evo.dat' using 1:3 title 'peor' with points
