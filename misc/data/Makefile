# Makefile for collecting and summarising performance test data
HOSTS	= NP1380 zhiyb-RPi zhiyb-RPi0 zhiyb-RPi3 zhiyb-TX2 NAS
TYPES	= raw seq va v vyuv v420
LOGS	= $(shell find -name instrumentation.log)
CSVS	= $(LOGS:%.log=%.csv)
SUMS	= $(CSVS:%.csv=%_summary.txt)
LINES	= $(CSVS:%.csv=%.line)

.PHONY: all
all: raw-seq-p-c-eps.pdf $(subst zhiyb-,,$(HOSTS:%=%-perf-eps.pdf))

.SECONDARY:
.DELETE_ON_ERROR:

%instrumentation.csv: %instrumentation.log instocsv.sh
	./instocsv.sh < $< > $@

%_summary.txt: %.csv $(MAKEFILE_LIST)
	R -q -e "options(digits = 16)" -e "x <- read.csv('$<', header = TRUE)" -e "s <- summary(cbind(100.0 * (x[2] + x[3]), x[4], x[5]))" -e "sink('$@'); s"

%.line: %_summary.txt line.sh
	./line.sh $< > $@
	
summary.csv: $(LINES)
	cat header.csv $^ > $@

summary_vsorted.csv: summary.csv $(MAKEFILE_LIST)
	cat header.csv > $@
	for i in $(TYPES); do grep -E "\<$$i\>" $<; done >> $@

summary_sorted.csv: summary_vsorted.csv $(MAKEFILE_LIST)
	cat header.csv > $@
	for i in $(HOSTS); do grep -E "\<$$i\>" $<; done | sed 's/zhiyb-//' >> $@

%.csv: summary_sorted.csv
	cat header.csv > $@
	grep -E "$(shell echo "$*" | sed 's/-/,.*/g')" $< >> $@

raw-seq-p-c.pdf: %.pdf: %.plot \
	raw-playback.csv raw-controller.csv seq-playback.csv seq-controller.csv
	gnuplot -e 'out="$@"' -c "$<"

%-perf.pdf: perf.plot %-normal.csv %-playback.csv %-controller.csv
	gnuplot -e 'out="$@"' -e 'host="$*"' -c "$<"

%-crop.pdf: %.pdf
	pdfcrop $< $@

%-rotated270.pdf: %.pdf
	pdf270 $<

%.eps: %-crop-rotated270.pdf
	pdftops -eps $< $@

%-eps.pdf: %.eps
	epstopdf -o="$@" "$<"
