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
	n->channels = start + channels - 1u;
	xmlFreeTextReader(reader);
	if (ret != 0) {
		fprintf(stderr, "Failed to parse %s\n", file);
		return false;
	}
	return true;
}

AVFormatContext *open_output(const char *file, const char *codec_name,
		const char *pix_fmt_name, network_t *n)
{
	AVFormatContext *fmt_ctx;
	if (avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, file) < 0) {
		cerr << "Could not allocate format context for "
			<< file << endl;
		return 0;
	}

	AVCodec *codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		fprintf(stderr, "Encoder %s not recognised\n", codec_name);
		return 0;
	}

	AVStream *out_stream = avformat_new_stream(fmt_ctx, codec);
	if (!out_stream) {
		cerr << "Failed allocating output stream" << endl;
		return 0;
	}

	AVCodecContext *c = out_stream->codec;
	/* resolution must be a multiple of two */
	int ch = (n->channels + 2) / 3;		// Rounding
	c->height = (unsigned int)round(sqrt(ch)) & ~1u;
	c->width = (ch + c->height - 1u) / c->height;
	c->width = (c->width + 1u) & ~1u;
	out_stream->time_base = c->time_base = (AVRational){n->resolution, 1000};
	if (pix_fmt_name != 0)
		c->pix_fmt = av_get_pix_fmt(pix_fmt_name);
	if (codec->id == AV_CODEC_ID_H264)
		if (av_opt_set_double(c->priv_data, "crf", 0, 0) != 0)
			av_log(NULL, AV_LOG_WARNING, "Cannot set crf option\n");

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

	printf("Encoding %s using %s, format %s\n", file, codec->long_name,
			av_get_pix_fmt_name(c->pix_fmt));
	return fmt_ctx;
}

int main(int argc, char *argv[])
{
	if (argc < 5) {
		printf("usage: %s input_xml output_file codec pix_fmt\n",
				argv[0]);
		return 1;
	}
	char *input, *output, *codec_name, *pix_fmt_name = 0;
	input = argv[1];
	output = argv[2];
	codec_name = argv[3];
	pix_fmt_name = argv[4];

	network_t n;
	if (!network_parse(&n, input))
		return 1;
	clog << "Resolution: " << n.resolution;
	clog << ", file: " << n.file << ", channels: " << n.channels << endl;
	if (n.channels == 0) {
		cerr << "No channels defined" << endl;
		return 1;
	}
	AVRational time_base = {n.resolution, 1000};

	string ifile(string(dirname(input)) + "/" + n.file);
	ifstream ifs(ifile, ifstream::binary);
	if (!ifs.good()) {
		cerr << "Unable to open " << ifile << " for read";
		return 1;
	}

	av_register_all();

	AVFormatContext *fmt_ctx = open_output(output, codec_name, pix_fmt_name, &n);
	if (!fmt_ctx)
		return 1;

	AVFrame *frame = av_frame_alloc();
	if (!frame) {
		fprintf(stderr, "Cannot allocate video frame\n");
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
		fprintf(stderr, "Cannot allocate frame buffer\n");
		return 1;
	}

	// Allocate input frame buffer (packed RGB)
	AVFrame *iframe = av_frame_alloc();
	if (!iframe) {
		fprintf(stderr, "Cannot allocate input frame\n");
		return 1;
	}

	iframe->format = AV_PIX_FMT_RGB24;
	iframe->width = c->width;
	iframe->height = c->height;

	if (av_image_alloc(iframe->data, iframe->linesize, c->width, c->height,
			(AVPixelFormat)iframe->format, 32) < 0) {
		fprintf(stderr, "Cannot allocate input frame buffer\n");
		return 1;
	}

	// Create format converter
	SwsContext *sc = sws_getContext(c->width, c->height, (AVPixelFormat)iframe->format,
			c->width, c->height, c->pix_fmt,
			0, NULL, NULL, NULL);
	if (!sc) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create format converter\n");
		return 1;
	}

	// Frame processing
	uint8_t buf[8];
	int64_t pts = 0;
	while (ifs.read((char *)buf, sizeof(buf)).good()) {
		for (int i = 0; i != sizeof(header); i++)
			if (buf[i] != header[i]) {
				av_log(NULL, AV_LOG_ERROR, "Header mismatch");
				break;
			}
		uint16_t chs;
		memcpy(&chs, &buf[sizeof(buf) - 2], 2);

		uint8_t *ptr = iframe->data[0];
		while (chs) {
			unsigned int s = min((unsigned int)chs, iframe->width * 3u);
			ifs.read((char *)ptr, s);
			chs -= s;
			ptr += iframe->linesize[0];
		}
		if (!ifs.good())
			break;

		sws_scale(sc, iframe->data, iframe->linesize, 0, c->height,
				frame->data, frame->linesize);

		frame->pts = av_rescale_q_rnd(pts++,
				time_base,
				fmt_ctx->streams[0]->time_base,
				(AVRounding)(AV_ROUND_NEAR_INF |
				AV_ROUND_PASS_MINMAX));

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
		if (av_interleaved_write_frame(fmt_ctx, &pkt) < 0)
			break;
	}

	for (;;) {
		AVPacket pkt;
		av_init_packet(&pkt);
		// packet data will be allocated by the encoder
		pkt.data = NULL;
		pkt.size = 0;

		/* encode the image */
		int got_output = 0;
		if (avcodec_encode_video2(c, &pkt, NULL, &got_output) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error encoding frame");
			break;
		}
		if (!got_output)
			break;

		// Prepare packet for muxing
		pkt.stream_index = 0;
		if (av_interleaved_write_frame(fmt_ctx, &pkt) < 0)
			break;
	}

	av_write_trailer(fmt_ctx);

	sws_freeContext(sc);
	av_freep(iframe->data);
	av_frame_free(&iframe);
	av_freep(frame->data);
	av_frame_free(&frame);
	if (fmt_ctx && !(fmt_ctx->oformat->flags & AVFMT_NOFILE))
		avio_close(fmt_ctx->pb);
	avformat_free_context(fmt_ctx);

	return 0;
}

