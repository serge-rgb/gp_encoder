set term png
set output "plot.png"
plot [1:70][3.5:5] 'evo.dat' using 1:2 title 'best' with linespoints,\
                     'evo.dat' using 1:3 title 'worst' with points
