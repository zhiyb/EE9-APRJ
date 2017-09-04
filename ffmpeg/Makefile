# Author: Yubo Zhi (normanzyb@gmail.com)

PRG	= dec_enc avio_reading metadata encoding decoding audio
LIB	= libcodec.so
EXE	= codec.exe
MONO	= MONO_IOMAP=all mono

FMOD_FLAGS	= -DENABLE_FMOD
FMOD_LIBS	= -lfmod

#CROSS	= mipsel-linux-
OPTIMIZE	:= -g -O3
CFLAGS		:= -Wall -std=c99 -U__STRICT_ANSI__ -I/usr/include/libxml2 \
		  -Wno-deprecated-declarations -Wno-unused-variable -Wno-unused-function \
		  $(LIBCFLAGS) $(FMOD_FLAGS)
CXXFLAGS	:= -Wall -std=c++11 -I/usr/include/libxml2 \
		  -Wno-deprecated-declarations $(FMOD_FLAGS)
LIBS	+= -L/usr/local/lib -lavformat -lavfilter -lavcodec -lavutil -lswscale $(FMOD_LIBS) -lxml2 -lm
LIBS	+= -Wl,--rpath=/usr/local/lib

CC	= $(CROSS)gcc
CXX	= $(CROSS)g++
LD	= $(CROSS)g++

.PHONY: all
all: $(PRG) $(EXE) libs

.PHONY: libs
libs:
	$(MAKE) $(LIB) LIBCFLAGS=-fPIC

$(PRG): %: %.o
	$(LD) $(OPTIMIZE) -o $@ $< $(LIBS)

%.exe: %.cs
	mcs /out:$@ $< /r:System.dll

#libcodec.so: mux.o

audio: LIBS	+= -L. -lcodec -Wl,--rpath=$(PWD)
audio: libs

codec.exe: libs

lib%.so: %.o
	$(CXX) -shared $(OPTIMIZE) -o $@ $^ $(LIBS)

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
#CODEC	= libx264 yuv444p
CODEC	= libx264rgb rgb24

.PHONY: diff
diff: out.test_enc.test_dec.enc.mp4 out.enc.enc.mp4
	ffprobe out.enc.enc.mp4 2>&1 | tail
	-diff out.mp4 out.enc.enc.mp4
	ffprobe out.mp4 2>&1 | tail
	-diff out.mp4 $<
	-diff $(SEQ).raw $(<:%.enc.mp4=%.raw)

.PHONY: vimdiff
vimdiff: out.hd out.test.hd
	vimdiff $^

.PHONY: psnr
psnr: psnr.sh out.mp4 out.enc.enc.mp4
	./$^

out.mp4: encoding $(SEQ)_Network.xml
	./$^ $@ $(CODEC)

%.test_dec.raw: codec.exe %.mp4
	$(MONO) $^ $@

%.raw: decoding %.mp4
	./$^ $@

%.hd: %.raw
	hd $< > $@

%.test_enc.mp4: codec.exe %.raw
	$(MONO) $< $*_Network.xml $@

%.enc.mp4: encoding %.raw
	./$< $*_Network.xml $@ $(CODEC)

%.xml: %.raw
	touch $@

# Clean up
.PHONY: clean
clean:
	rm -f $(PRG) $(PRG:%=%.o) $(PRG:%=%.d) $(PRG:%=%.d.*) \
		$(LIB) $(LIB:lib%.so=%.o) $(EXE)
