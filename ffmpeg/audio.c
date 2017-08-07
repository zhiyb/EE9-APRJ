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
	codec_init();

	void *dp = codec_alloc();
	if (!dp)
		return 1;

	// Open input file for decoding
	int gota, gotv;
	if (!decode_open_input(dp, input, NULL, &gota, &gotv))
		return 1;
	if (!gota) {
		av_log(NULL, AV_LOG_ERROR, "Audio stream not found\n");
		return 1;
	}
	// Get audio codec context
	void *ac = decode_context(dp, 0);

	void *ep = codec_alloc();
	if (!ep) {
		decode_close(dp);
		return 1;
	}

	// Open output file for encoding
	if (!encode_open_output(ep, output, NULL)) {
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
		//log_packet_data(dp, pkt);
		if (video) {
			decode_free_packet(pkt);
			vframe++;
		} else {
			encode_write_packet_or_frame(ep, pkt, NULL);
			//encode_write_audio_packet(ep, pkt);
			aframe++;
		}
	}
	fprintf(stderr, "%ld audio frames, %ld video frames\n", aframe, vframe);

	// Clean up
	encode_close(ep);
	codec_free(ep);
	decode_close(dp);
	codec_free(dp);

	return 0;
}
