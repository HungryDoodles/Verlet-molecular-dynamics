set xlabel 'Y'
set ylabel 'N'
plot 'output.txt' using 1:2 with lines,\
	0.284102 * exp(-3.77827 * x) with lines