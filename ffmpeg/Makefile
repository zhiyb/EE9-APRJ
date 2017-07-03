# Author: Yubo Zhi (normanzyb@gmail.com)

PRG	= dec_enc avio_reading metadata encoding

#CROSS	= mipsel-linux-
OPTIMIZE	= -g -O3
CFLAGS		= -Wall -std=c99 -U__STRICT_ANSI__ -I/usr/include/libxml2 \
		  -Wno-deprecated-declarations
CXXFLAGS	= -Wall -std=c++11 -I/usr/include/libxml2 \
		  -Wno-deprecated-declarations
LIBS	= -lavformat -lavcodec -lavutil -lswscale -lxml2 -lm

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

# Clean up
.PHONY: clean
clean:
	rm -f $(PRG) $(PRG:%=%.o) $(PRG:%=%.d) $(PRG:%=%.d.*)
