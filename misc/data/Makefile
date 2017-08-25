# Makefile for collecting and summarising performance test data
HOSTS	= NP1380 zhiyb-RPi zhiyb-RPi0 zhiyb-RPi3 zhiyb-TX2 NAS
LOGS	= $(shell find -name instrumentation.log)
CSVS	= $(LOGS:%.log=%.csv)
SUMS	= $(CSVS:%.csv=%_summary.txt)
LINES	= $(CSVS:%.csv=%.line)

.PHONY: all
all: raw-seq-p-c-eps.pdf raw-controller.csv

.SECONDARY:
.DELETE_ON_ERROR:

%instrumentation.csv: %instrumentation.log instocsv.sh
	./instocsv.sh < $< > $@

%_summary.txt: %.csv
	R -q -e "options(digits = 16)" -e "x <- read.csv('$<', header = TRUE)" -e "s <- summary(cbind(x[2] + x[3], x[4], x[5]))" -e "sink('$@'); s"

%.line: %_summary.txt line.sh
	./line.sh $< > $@
	
summary.csv: $(LINES)
	cat header.csv $^ > $@

summary_sorted.csv: summary.csv
	cat header.csv > $@
	for i in $(HOSTS); do grep -E "\<$$i\>" $<; done | sed 's/zhiyb-//' >> $@

%.csv: summary_sorted.csv
	cat header.csv > $@
	grep -F $(shell echo "$*" | tr - ,) $< >> $@

raw-seq-p-c.pdf: %.pdf: %.plot \
	raw-playback.csv raw-controller.csv seq-playback.csv seq-controller.csv
	gnuplot -e 'out="$@"' -c "$<"

%-crop.pdf: %.pdf
	pdfcrop $< $@

%-rotated270.pdf: %.pdf
	pdf270 $<

%.eps: %-crop-rotated270.pdf
	pdftops -eps $< $@

%-eps.pdf: %.eps
	epstopdf -o="$@" "$<"
