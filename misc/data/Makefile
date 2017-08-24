# Makefile for collecting and summarising performance test data

LOGS	= $(shell find -name instrumentation.log)
CSVS	= $(LOGS:%.log=%.csv)
SUMS	= $(CSVS:%.csv=%_summary.txt)
LINES	= $(CSVS:%.csv=%.line)

.PHONY: all
all: summary.csv

.SECONDARY:
.DELETE_ON_ERROR:

%.csv: %.log instocsv.sh
	./instocsv.sh < $< > $@

%_summary.txt: %.csv
	R -q -e "options(digits = 16)" -e "x <- read.csv('$<', header = TRUE)" -e "s <- summary(cbind(x[2] + x[3], x[4], x[5]))" -e "sink('$@'); s"

%.line: %_summary.txt line.sh
	./line.sh $< > $@
	
summary.csv: $(LINES)
	cat header.csv $^ > $@
