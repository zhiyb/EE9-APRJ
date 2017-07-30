#include <libgen.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <libavformat/avformat.h>
#include <libavfilter/avfiltergraph.h>
#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/common.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>

#include "codec.h"

int main(int argc, char *argv[])
{
	if (argc != 2)
		return 1;

	init();
	int gota, gotv;
	void *dp = decode_open_input(argv[1], NULL, &gota, &gotv);
	if (!dp)
		return 1;
	long aframe = 0, vframe = 0;
	for (;;) {
		int got = 0, video = 0;
		void *frame = decode_read_frame(dp, &got, &video);
		if (!got)
			break;
		if (video)
			vframe++;
		else
			aframe++;
	}
	decode_close(dp);
	fprintf(stderr, "%ld audio frames, %ld video frames\n", aframe, vframe);

	return 0;
}
