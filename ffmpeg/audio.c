#include <libgen.h>
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include "codec.h"

int main(int argc, char *argv[])
{
	if (argc != 3)
		return 1;
	const char *input = argv[1], *output = argv[2];
	init();

	// Open input file for decoding
	int gota, gotv;
	void *dp = decode_open_input(input, NULL, &gota, &gotv);
	if (!dp)
		return 1;
	if (!gota) {
		av_log(NULL, AV_LOG_ERROR, "Audio stream not found\n");
		return 1;
	}
	// Get audio codec context
	void *ac = decode_context(dp, 0);

	// Open output file for encoding
	void *ep = encode_open_output(output, NULL);
	if (!ep) {
		decode_close(dp);
		return 1;
	}
	// Add audio stream
	int astream = encode_add_audio_stream_copy(ep, ac);
	if (astream < 0) {
		encode_close(ep);
		decode_close(dp);
		return 1;
	}
	// Write file header
	if (encode_write_header(ep, output) == 0) {
		encode_close(ep);
		decode_close(dp);
		return 1;
	}

	// Process frames
	long aframe = 0, vframe = 0;
	for (;;) {
		int got = 0, video = 0;
		void *pkt = decode_read_packet(dp, &got, &video);
		if (!got)
			break;
		if (video) {
			decode_free_packet(pkt);
			vframe++;
		} else {
			encode_write_audio_packet(ep, ac, pkt);
			aframe++;
		}
	}

	// Clean up
	encode_close(ep);
	decode_close(dp);
	fprintf(stderr, "%ld audio frames, %ld video frames\n", aframe, vframe);

	return 0;
}
