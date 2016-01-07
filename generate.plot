set term png
set output "plot.png"
plot [1:59][3.5:5] 'evo.dat' using 1:2 title 'mejor' with linespoints,\
                     'evo.dat' using 1:3 title 'peor' with points
