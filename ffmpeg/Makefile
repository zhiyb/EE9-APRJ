# Author: Yubo Zhi (normanzyb@gmail.com)

PRG	= dec_enc avio_reading metadata encoding decoding

#CROSS	= mipsel-linux-
OPTIMIZE	= -g -O3
CFLAGS		= -Wall -std=c99 -U__STRICT_ANSI__ -I/usr/include/libxml2 \
		  -Wno-deprecated-declarations
CXXFLAGS	= -Wall -std=c++11 -I/usr/include/libxml2 \
		  -Wno-deprecated-declarations
LIBS	= -lavformat -lavfilter -lavcodec -lavutil -lswscale -lxml2 -lm

CC	= $(CROSS)gcc
CXX	= $(CROSS)g++
LD	= $(CROSS)g++

.PHONY: all
all: $(PRG)

$(PRG): %: %.o
	$(LD) $(OPTIMIZE) -o $@ $^ $(LIBS)

%.o: %.cpp
	$(CXX) -c $(OPTIMIZE) $(CXXFLAGS) -o $@ $<

%.o: %.c
	$(CC) -c $(OPTIMIZE) $(CFLAGS) -o $@ $<

# Dependency:
-include $(OBJ:.o=.d)
%.d: %.cpp
	@set -e; rm -f $@; \
	$(CXX) -MM $(CXXFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.SECONDARY:
.DELETE_ON_ERROR:

.PHONY: test
test: out.enc.enc.mp4

SEQ	= /mnt/nas/Vixen/luf2016_the-ark_20ms

.PHONY: diff
diff: out.mp4 out.enc.enc.mp4
	ffprobe out.mp4 2>&1 | tail
	ffprobe out.enc.enc.mp4 2>&1 | tail
	-diff $^
	-diff out.raw out.enc.raw
	-diff out.raw $(SEQ).raw

.PHONY: vimdiff
vimdiff: out.hd out.enc.hd
	vimdiff $^

out.mp4: encoding $(SEQ)_Network.xml
	./$^ $@ libx264

%.raw: decoding %.mp4
	./$^ $@

%.hd: %.raw
	hd $< > $@

%.enc.mp4: encoding %.raw
	./$< $*.xml $@ libx264

# Clean up
.PHONY: clean
clean:
	rm -f $(PRG) $(PRG:%=%.o) $(PRG:%=%.d) $(PRG:%=%.d.*)
