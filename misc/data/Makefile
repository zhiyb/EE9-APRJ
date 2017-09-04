# Makefile for collecting and summarising performance test data

HOSTS	= NP1380 zhiyb-RPi zhiyb-RPi0 zhiyb-RPi3 zhiyb-TX2 zhiyb-Laptop NAS
TYPES	= raw seq seqa va v vyuv v420
FMTS	= seq vavi vmkv v vmov vqt v265rgb v265yuv
LOGS	= $(shell find -name instrumentation.log)
CSVS	= $(LOGS:%.log=%.csv)
SUMS	= $(CSVS:%.csv=%_summary.txt)
LINES	= $(CSVS:%.csv=%.line)

.PHONY: all
all: raw-seq-p-c-eps.pdf
all: $(subst zhiyb-,,$(HOSTS:%=types-%-perf-eps.pdf))
all: $(subst zhiyb-,,$(HOSTS:%=fmts-%-perf-eps.pdf)) fmts-more-TX2-perf-eps.pdf fmts-more-Laptop-perf-eps.pdf

.SECONDARY:
.DELETE_ON_ERROR:

%instrumentation.csv: %instrumentation.log instocsv.sh
	./instocsv.sh < $< > $@

%_summary.txt: %.csv
	R -q -e "options(digits = 16)" -e "x <- read.csv('$<', header = TRUE)" -e "s <- summary(cbind(100.0 * (x[2] + x[3]), x[4], x[5]))" -e "sink('$@'); s"

%.line: %_summary.txt line.sh
	./line.sh $< > $@
	
summary.csv: $(LINES)
	cat header.csv $^ > $@

summary_types.csv: summary.csv
	cat header.csv > $@
	for i in $(TYPES); do grep -E "\<$$i\>" $<; done >> $@

summary_fmts.csv: summary.csv
	cat header.csv > $@
	for i in $(FMTS); do grep -E "\<$$i\>" $<; done | sed 's/\<v\>/mp4/;s/\<vavi\>/avi/;s/\<vmkv\>/mkv/;s/\<v265rgb\>/h265rgb/;s/\<v265yuv\>/h265yuv/;s/\<vqt\>/qtrle/;s/\<vmov\>/mov/' >> $@

%_sorted.csv: %.csv
	cat header.csv > $@
	for i in $(HOSTS); do grep -E "\<$$i\>" $<; done | sed 's/zhiyb-//' >> $@

fmts-%.csv: summary_fmts_sorted.csv
	cat header.csv > $@
	grep -E "$(shell echo "$*" | sed 's/-/,.*/g')" $< >> $@

types-%.csv: summary_types_sorted.csv
	cat header.csv > $@
	grep -E "$(shell echo "$*" | sed 's/-/,.*/g')" $< >> $@

raw-seq-p-c.pdf: %.pdf: %.plot \
	types-raw-playback.csv types-raw-controller.csv types-seq-playback.csv types-seq-controller.csv
	gnuplot -e 'out="$@"' -c "$<"

fmts-more-%-perf.pdf: perf-fmts-more.plot fmts-%-normal.csv fmts-%-playback.csv fmts-%-controller.csv
	gnuplot -e 'out="$@"' -e 'host="fmts-$*"' -c "$<"

fmts-%-perf.pdf: perf-fmts.plot fmts-%-normal.csv fmts-%-playback.csv fmts-%-controller.csv
	gnuplot -e 'out="$@"' -e 'host="fmts-$*"' -c "$<"

types-%-perf.pdf: perf.plot types-%-normal.csv types-%-playback.csv types-%-controller.csv
	gnuplot -e 'out="$@"' -e 'host="types-$*"' -c "$<"

%-crop.pdf: %.pdf
	pdfcrop $< $@

%-rotated270.pdf: %.pdf
	pdf270 $<

%.eps: %-crop-rotated270.pdf
	pdftops -eps $< $@

%-eps.pdf: %.eps
	epstopdf -o="$@" "$<"
