set xlabel 'Y'
set ylabel 'N'
plot 'outputNoPhaseY.txt' using 1:2 with lines,\
	'outputPhaseY.txt' using 1:2 with lines,\
	'outputSpecialCase.txt' using 1:2 with lines