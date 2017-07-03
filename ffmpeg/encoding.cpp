#include <iostream>
#include <fstream>
#include <string>
#include <libgen.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <libxml2/libxml/xmlreader.h>

extern "C" {
#include <libavformat/avformat.h>
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

#if 0
/*
 * Video encoding example
 */
static void video_encode_example(const char *filename, int codec_id)
{
	int i, ret, x, y, got_output;

	/* encode 1 second of video */
	for (i = 0; i < 25; i++) {
		av_init_packet(&pkt);
		pkt.data = NULL;    // packet data will be allocated by the encoder
		pkt.size = 0;

		/* encode the image */
		ret = avcodec_encode_video2(c, &pkt, frame, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_free_packet(&pkt);
		}
	}

	/* get the delayed frames */
	for (got_output = 1; got_output; i++) {
		fflush(stdout);

		ret = avcodec_encode_video2(c, &pkt, NULL, &got_output);
		if (ret < 0) {
			fprintf(stderr, "Error encoding frame\n");
			exit(1);
		}

		if (got_output) {
			printf("Write frame %3d (size=%5d)\n", i, pkt.size);
			fwrite(pkt.data, 1, pkt.size, f);
			av_free_packet(&pkt);
		}
	}
}
#endif

typedef struct network_t {
	int resolution;
	string file;
	int channels;
} network_t;

// Parse XML Network metadata
bool network_parse(network_t *n, const char *file)
{
	xmlTextReaderPtr reader;
	reader = xmlReaderForFile(file, NULL, 0);
	if (reader == NULL) {
		fprintf(stderr, "Unable to open %s\n", file);
		return false;
	}
	int ret, start = 0, channels = 0;
	while ((ret = xmlTextReaderRead(reader)) == 1) {
		if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_ELEMENT)
			continue;
		const xmlChar *name = xmlTextReaderConstName(reader);
		if (name == NULL)
			continue;
		if ((ret = xmlTextReaderRead(reader)) != 1)
			break;
		if (xmlTextReaderNodeType(reader) != XML_READER_TYPE_TEXT)
			continue;
		const xmlChar *value = xmlTextReaderConstValue(reader);
		if (value == NULL)
			continue;
		string strname((const char *)name);
		if (strname == "Resolution") {
			n->resolution = atoi((const char *)value);
		} else if (strname == "OutFile") {
			n->file = (const char *)value;
		} else if (strname == "StartChan") {
			int c = atoi((const char *)value);
			start = c > start ? c : start;
		} else if (strname == "Channels") {
			int c = atoi((const char *)value);
			channels = c > channels ? c : channels;
		}
	}
	n->channels = start + channels;
	xmlFreeTextReader(reader);
	if (ret != 0) {
		fprintf(stderr, "Failed to parse %s\n", file);
		return false;
	}
	return true;
}

AVFormatContext *open_output(const char *file, const char *codec_name,
		network_t *n)
{
	AVFormatContext *fmt_ctx;
	if (avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, file) < 0) {
		cerr << "Could not allocate format context for "
			<< file << endl;
		return 0;
	}

	AVStream *out_stream = avformat_new_stream(fmt_ctx, NULL);
	if (!out_stream) {
		cerr << "Failed allocating output stream" << endl;
		return 0;
	}

	AVCodec *codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		fprintf(stderr, "Encoder %s not recognised\n", codec_name);
		return 0;
	}
	printf("Encode %s using %s\n", file, codec->long_name);

	AVCodecContext *c = out_stream->codec = avcodec_alloc_context3(codec);
	/* put sample parameters */
	c->bit_rate = 8 * n->channels * (1000 / n->resolution);
	/* resolution must be a multiple of two */
	int ch = (n->channels + 2) / 3;		// Rounding
	c->height = (unsigned int)round(sqrt(ch)) & ~1u;
	c->width = (ch + c->height - 1u) / c->height;
	c->width = (c->width + 1u) & ~1u;
	/* frames per second */
	c->time_base = (AVRational){n->resolution, 1000};
	clog << "S: " << c->width << "x" << c->height
		<< ", B: " << c->bit_rate << endl;
	/* emit one intra frame every ten frames
	 * check frame pict_type before passing frame
	 * to encoder, if frame->pict_type is AV_PICTURE_TYPE_I
	 * then gop_size is ignored and the output of encoder
	 * will always be I frame irrespective to gop_size
	 */
	c->gop_size = 10;
	c->max_b_frames = 1;
	c->pix_fmt = AV_PIX_FMT_YUV444P;
	if (codec->id == AV_CODEC_ID_H264)
		av_opt_set(c->priv_data, "preset", "slow", 0);

	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		fprintf(stderr, "Could not open codec\n");
		return 0;
	}

	if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		if (avio_open(&fmt_ctx->pb, file, AVIO_FLAG_WRITE) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", file);
			return 0;
		}
	}

	/* init muxer, write output file header */
	if (avformat_write_header(fmt_ctx, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file");
		return 0;
	}
	return fmt_ctx;
}

int main(int argc, char **argv)
{
	char *input, *output, *codec_name;

	if (argc < 4) {
		printf("usage: %s input_xml output_file codec\n",
				argv[0]);
		return 1;
	}
	input = argv[1];
	output = argv[2];
	codec_name = argv[3];

	network_t n;
	if (!network_parse(&n, input))
		return 1;
	clog << "Resolution: " << n.resolution;
	clog << ", file: " << n.file << ", channels: " << n.channels << endl;
	if (n.channels == 0) {
		cerr << "No channels defined" << endl;
		return 1;
	}

	string ifile(string(dirname(input)) + "/" + n.file);
	ifstream ifs(ifile);
	if (!ifs.good()) {
		cerr << "Unable to open " << ifile << " for read";
		return 1;
	}

	av_register_all();

	AVFormatContext *fmt_ctx = open_output(output, codec_name, &n);
	if (!fmt_ctx)
		return 1;

	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Could not allocate video frame\n");
		return 1;
	}

	AVCodecContext *c = fmt_ctx->streams[0]->codec;
	frame->format = c->pix_fmt;
	frame->width = c->width;
	frame->height = c->height;
	frame->channel_layout = c->channel_layout;

	/* the image can be allocated by any means and av_image_alloc() is
	 * just the most convenient way if av_malloc() is to be used */
	if (av_image_alloc(frame->data, frame->linesize, c->width, c->height,
			c->pix_fmt, 32) < 0) {
		fprintf(stderr, "Could not allocate raw picture buffer\n");
		return 1;
	}

#if 0	// Use channel data as YUV directly
	SwsContext *sws_ctx = sws_getContext(c->width, c->height,
			AV_PIX_FMT_RGB24, c->width, c->height,
			AV_PIX_FMT_YUV420P, 0, 0, 0, 0);
	if (!sws_ctx) {
		av_log(NULL, AV_LOG_ERROR, "Failed allocating swscale context");
		return 0;
	}
#endif

	// Frame processing
	uint8_t buf[8];
	uint8_t *chdata = (uint8_t *)malloc(n.channels + 3);
	if (!chdata) {
		av_log(NULL, AV_LOG_ERROR, "Failed allocating channel buffer");
		return 0;
	}
	int64_t pts = -1;
	while (ifs.read((char *)buf, sizeof(buf)).good()) {
		for (int i = 0; i != sizeof(header); i++)
			if (buf[i] != header[i]) {
				av_log(NULL, AV_LOG_ERROR, "Header mismatch");
				break;
			}
		uint16_t chs;
		memcpy(&chs, &buf[sizeof(buf) - 2], 2);
		if (!ifs.read((char *)chdata, chs).good())
			break;

		for (int i = 0; i != (chs + 2) / 3; i++) {
			int x = i % frame->width;
			int y = i / frame->height;
			// Y, Cb and Cr
			frame->data[0][y * frame->linesize[0] + x] = chdata[i * 3];
			frame->data[1][y * frame->linesize[1] + x] = chdata[i * 3 + 1];
			frame->data[2][y * frame->linesize[2] + x] = chdata[i * 3 + 2];
		}
		frame->pts = ++pts;

		AVPacket pkt;
		av_init_packet(&pkt);
		// packet data will be allocated by the encoder
		pkt.data = NULL;
		pkt.size = 0;

		/* encode the image */
		int got_output;
		if (avcodec_encode_video2(c, &pkt, frame, &got_output) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error encoding frame");
			break;
		}
		if (!got_output)
			continue;

		// Prepare packet for muxing
		pkt.stream_index = 0;
		pkt.dts = av_rescale_q_rnd(pkt.dts,
				fmt_ctx->streams[0]->codec->time_base,
				fmt_ctx->streams[0]->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF |
				AV_ROUND_PASS_MINMAX));
		pkt.pts = av_rescale_q_rnd(pkt.pts,
				fmt_ctx->streams[0]->codec->time_base,
				fmt_ctx->streams[0]->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF |
					AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration,
				fmt_ctx->streams[0]->codec->time_base,
				fmt_ctx->streams[0]->time_base);
		if (av_interleaved_write_frame(fmt_ctx, &pkt) < 0)
			break;
	}

	av_write_trailer(fmt_ctx);
	free(chdata);

#if 0
	sws_freeContext(sws_ctx);
#endif
	av_freep(frame->data);
	av_frame_free(&frame);
	if (fmt_ctx && !(fmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_close(fmt_ctx->pb);
	avformat_free_context(fmt_ctx);

	return 0;
}

