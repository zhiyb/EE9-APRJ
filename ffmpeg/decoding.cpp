#include <iostream>
#include <fstream>
#include <string>
#include <libgen.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

extern "C" {
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
}

using namespace std;

static const uint8_t header[6] = {0xde, 0xad, 0xbe, 0xef, 0x02, 0x00};

AVFormatContext *open_input(const char *file, int *stream)
{
	AVFormatContext *fmt_ctx = NULL;
	if (avformat_open_input(&fmt_ctx, file, NULL, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
		return 0;
	}
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return 0;
	}
	AVCodec *codec = NULL;
	if ((*stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO,
			-1, -1, &codec, 0)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find video stream\n");
		return 0;
	}
	if (!codec) {
		av_log(NULL, AV_LOG_ERROR, "Failed to find video codec\n");
		return 0;
	}
	AVCodecContext *c = fmt_ctx->streams[*stream]->codec;
	if (avcodec_open2(c, codec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Failed to open video codec\n");
		return 0;
	}
	av_dump_format(fmt_ctx, *stream, file, 0);
	return fmt_ctx;
}

void write_output(ofstream &fs, AVFrame *frame, uint16_t channels)
{
	fs.write((char *)header, sizeof(header));
	fs.write((char *)&channels, 2u);
	uint8_t *ptr = frame->data[0];
	while (channels) {
		unsigned int s = min((unsigned int)channels, frame->width * 3u);
		fs.write((char *)ptr, s);
		channels -= s;
		ptr += frame->linesize[0];
	}
}

int main(int argc, char *argv[])
{
	if (argc < 3) {
		printf("usage: %s input_file output_raw\n", argv[0]);
		return 1;
	}
	char *input = argv[1], *output = argv[2];

	ofstream fs(output, ofstream::binary);
	if (!fs.good()) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open output file\n");
		return 1;
	}

	av_register_all();
	avfilter_register_all();

	// TODO: Read timebase from metadata
	AVRational time_base = {20, 1000};

	int stream;
	AVFormatContext *fmt_ctx = open_input(input, &stream);
	if (!fmt_ctx)
		return 1;
	AVCodecContext *c = fmt_ctx->streams[stream]->codec;

	// Allocate input frame
	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate frame buffer\n");
		return 1;
	}

	// Allocate output frame buffer (packed RGB)
	AVFrame *oframe = av_frame_alloc();
	if (!oframe) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate output frame\n");
		return 1;
	}

	oframe->format = AV_PIX_FMT_RGB24;
	oframe->width = c->width;
	oframe->height = c->height;

	if (av_image_alloc(oframe->data, oframe->linesize, c->width, c->height,
			(AVPixelFormat)oframe->format, 32) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate output frame buffer\n");
		return 1;
	}

	// Create format converter
	// TODO: Test different algorithms
	SwsContext *sc = sws_getContext(c->width, c->height, c->pix_fmt,
			c->width, c->height, (AVPixelFormat)oframe->format,
			0, NULL, NULL, NULL);
	if (!sc) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create format converter\n");
		return 1;
	}

	// Frame processing
	AVPacket pkt;
	while (av_read_frame(fmt_ctx, &pkt) >= 0) {
		int got_frame;
		if (avcodec_decode_video2(c, frame, &got_frame, &pkt) < 0) {
			sws_freeContext(sc);
			av_frame_free(&frame);
			av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
		}
		if (got_frame)
			sws_scale(sc, frame->data, frame->linesize, 0, c->height,
					oframe->data, oframe->linesize);
#if 0
		int64_t pts = av_rescale_q_rnd(frame->pts,
				fmt_ctx->streams[stream]->time_base,
				time_base,
				(AVRounding)(AV_ROUND_NEAR_INF |
					AV_ROUND_PASS_MINMAX));
		printf("%ld, ", pts);
#endif
		// TODO: Read channel number from metadata
		write_output(fs, oframe, 8669 + 8600 - 1);
	}

	sws_freeContext(sc);
	av_freep(oframe->data);
	av_frame_free(&oframe);
	av_frame_free(&frame);
	avformat_free_context(fmt_ctx);
	return 0;
}
