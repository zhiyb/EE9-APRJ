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

typedef struct data_t {
	AVFormatContext *fmt_ctx;
	AVCodecContext *c;
	AVFrame *iframe, *oframe;
	struct SwsContext *sc;
	AVPacket pkt;
	int resolution;
	int pts;
	void *data;
} data_t;

void init()
{
	av_register_all();
	avfilter_register_all();
}

int version()
{
	return 0x1234;
}

static void free_data(data_t *data)
{
	if (!data)
		return;
	if (data->data)
		free(data->data);
	if (data->sc)
		sws_freeContext(data->sc);
	if (data->oframe)
		av_frame_free(&data->oframe);
	if (data->iframe)
		av_frame_free(&data->iframe);
	if (data->fmt_ctx)
		avformat_free_context(data->fmt_ctx);
	free(data);
}

/* {{{ Encode */
static data_t *encode_allocate(AVCodecContext *c)
{
	AVFrame *iframe = 0, *oframe = 0;
	struct SwsContext *sc = 0;
	data_t *data = 0;

	// Allocate input frame
	oframe = av_frame_alloc();
	if (!oframe) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate video frame\n");
		goto error;
	}
	oframe->format = c->pix_fmt;
	oframe->width = c->width;
	oframe->height = c->height;
	oframe->channel_layout = c->channel_layout;

	if (av_image_alloc(oframe->data, oframe->linesize, c->width, c->height,
			oframe->format, 32) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate video frame buffer\n");
		goto error;
	}

	// Allocate input frame buffer (packed RGB)
	iframe = av_frame_alloc();
	if (!iframe) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate input frame\n");
		goto error;
	}

	iframe->format = AV_PIX_FMT_RGB24;
	iframe->width = c->width;
	iframe->height = c->height;

	if (av_image_alloc(iframe->data, iframe->linesize, c->width, c->height,
			iframe->format, 32) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate input frame buffer\n");
		goto error;
	}
	memset(iframe->data[0], 0, c->height * iframe->linesize[0]);

	// Create format converter
	sc = sws_getContext(c->width, c->height, iframe->format,
			c->width, c->height, c->pix_fmt,
			0, NULL, NULL, NULL);
	if (!sc) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create format converter\n");
		goto error;
	}

	data = (data_t *)calloc(1, sizeof(data_t));
	if (!data)
		goto error;
	av_init_packet(&data->pkt);
	data->iframe = iframe;
	data->oframe = oframe;
	data->sc = sc;
	data->pts = 0;
	return data;

error:
	if (sc)
		sws_freeContext(sc);
	if (iframe) {
		if (iframe->data)
			av_freep(iframe->data);
		av_frame_free(&iframe);
	}
	if (oframe)
		av_frame_free(&oframe);
	return 0;
}

void *encode_open_output(const char *file, const char *codec_name,
		const char *pix_fmt_name, const int resolution, const int channels,
		const char *comment)
{
	AVFormatContext *fmt_ctx;
	if (avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, file) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Could not allocate format context for %s\n", file);
		return 0;
	}

	AVCodec *codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		av_log(NULL, AV_LOG_ERROR, "Encoder %s not recognised\n", codec_name);
		return 0;
	}

	AVStream *out_stream = avformat_new_stream(fmt_ctx, codec);
	if (!out_stream) {
		av_log(NULL, AV_LOG_ERROR, "Failed allocating output stream\n");
		return 0;
	}

	AVCodecContext *c = out_stream->codec;
	/* resolution must be a multiple of two */
	int ch = (channels + 2) / 3;		// Rounding
	c->height = (unsigned int)round(sqrt(ch)) & ~1u;
	c->width = (ch + c->height - 1u) / c->height;
	c->width = (c->width + 1u) & ~1u;
	out_stream->time_base = c->time_base = (AVRational){resolution, 1000};
	if (pix_fmt_name != 0)
		c->pix_fmt = av_get_pix_fmt(pix_fmt_name);
	if (codec->id == AV_CODEC_ID_H264)
		if (av_opt_set_double(c->priv_data, "crf", 0, 0) != 0)
			av_log(NULL, AV_LOG_WARNING, "Cannot set crf option\n");

	/* open it */
	if (avcodec_open2(c, codec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Could not open codec\n");
		return 0;
	}

	if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE)) {
		if (avio_open(&fmt_ctx->pb, file, AVIO_FLAG_WRITE) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Could not open output file '%s'", file);
			return 0;
		}
	}

	// Add comment to metadata
	if (comment)
		av_dict_set(&fmt_ctx->metadata, "comment", comment, 0);

	/* init muxer, write output file header */
	if (avformat_write_header(fmt_ctx, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error occurred when opening output file");
		return 0;
	}

	av_dump_format(fmt_ctx, out_stream->id, file, 1);

	data_t *data = encode_allocate(c);
	if (!data)
		return 0;
	data->fmt_ctx = fmt_ctx;
	data->c = c;
	data->resolution = resolution;
	return data;
}

int encode_write_frame(void *dp, void *fp, int channels)
{
	data_t *data = dp;
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVCodecContext *c = data->c;
	AVFrame *iframe = data->iframe, *oframe = data->oframe;
	struct SwsContext *sc = data->sc;
	AVRational time_base = {data->resolution, 1000};

	// Generate input frame
	uint8_t *ptr = iframe->data[0];
	unsigned int w = iframe->width * 3u;
	while (channels) {
		unsigned int s = channels > w ? w : channels;
		memcpy(ptr, fp, s);
		channels -= s;
		fp += s;
		ptr += iframe->linesize[0];
	}
	sws_scale(sc, (void *)iframe->data, iframe->linesize, 0, c->height,
			(void *)oframe->data, oframe->linesize);
	oframe->pts = av_rescale_q_rnd(data->pts++,
			time_base,
			fmt_ctx->streams[0]->time_base,
			(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	// Encode the image
	int got_output = 0;
	if (avcodec_encode_video2(c, &data->pkt, oframe, &got_output) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error encoding frame");
		return 0;
	}
	if (!got_output)
		return 1;

	// Prepare packet for muxing
	// TODO: stream 0 ?
	data->pkt.stream_index = 0;
	if (av_interleaved_write_frame(fmt_ctx, &data->pkt) < 0)
		return 0;
	return 1;
}

void encode_close(void *dp)
{
	data_t *data = dp;
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVCodecContext *c = data->c;
	AVFrame *iframe = data->iframe, *oframe = data->oframe;
	struct SwsContext *sc = data->sc;

	// Flush encoder
	for (;;) {
		/* encode the image */
		int got_output = 0;
		if (avcodec_encode_video2(c, &data->pkt, NULL, &got_output) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error encoding frame");
			break;
		}
		if (!got_output)
			break;

		// Prepare packet for muxing
		// TODO: stream 0 ?
		data->pkt.stream_index = 0;
		if (av_interleaved_write_frame(fmt_ctx, &data->pkt) < 0)
			break;
	}
	av_write_trailer(fmt_ctx);

	if (data->oframe)
		av_freep(data->oframe->data);
	if (data->iframe)
		av_freep(data->iframe->data);
	free_data((data_t *)dp);
}
/* }}} */

/* {{{ Decode */
static data_t *decode_allocate(AVCodecContext *c)
{
	AVFrame *iframe = 0, *oframe = 0;
	struct SwsContext *sc = 0;
	data_t *data = 0;

	// Allocate input frame
	iframe = av_frame_alloc();
	if (!iframe) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate video frame buffer\n");
		goto error;
	}

	// Allocate output frame buffer (packed RGB)
	oframe = av_frame_alloc();
	if (!oframe) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate output frame\n");
		goto error;
	}

	oframe->format = AV_PIX_FMT_RGB24;
	oframe->width = c->width;
	oframe->height = c->height;

	if (av_image_alloc(oframe->data, oframe->linesize, c->width, c->height,
			oframe->format, 32) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate output frame buffer\n");
		goto error;
	}

	// Create format converter
	// TODO: Test different algorithms
	sc = sws_getContext(c->width, c->height, c->pix_fmt,
			c->width, c->height, oframe->format,
			0, NULL, NULL, NULL);
	if (!sc) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create format converter\n");
		goto error;
	}

	data = (data_t *)calloc(1, sizeof(data_t));
	if (!data)
		goto error;
	av_init_packet(&data->pkt);
	data->iframe = iframe;
	data->oframe = oframe;
	data->sc = sc;
	return data;

error:
	if (data) {
		if (data->data)
			free(data->data);
		free(data);
	}
	if (sc)
		sws_freeContext(sc);
	if (oframe) {
		if (oframe->data)
			av_freep(oframe->data);
		av_frame_free(&oframe);
	}
	if (iframe)
		av_frame_free(&iframe);
	return 0;
}

void *decode_open_input(const char *file, char **comment)
{
	int stream;
	AVFormatContext *fmt_ctx = NULL;
	if (avformat_open_input(&fmt_ctx, file, NULL, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file %s\n", file);
		return 0;
	}
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return 0;
	}
	AVCodec *codec = NULL;
	if ((stream = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO,
			-1, -1, &codec, 0)) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find video stream\n");
		return 0;
	}
	if (!codec) {
		av_log(NULL, AV_LOG_ERROR, "Failed to find video codec\n");
		return 0;
	}
	AVCodecContext *c = fmt_ctx->streams[stream]->codec;
	if (avcodec_open2(c, codec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Failed to open video codec\n");
		return 0;
	}

	if (comment) {
		AVDictionaryEntry *tag = av_dict_get(fmt_ctx->metadata, "comment", NULL, 0);
		*comment = tag ? tag->value : NULL;
	}

	av_dump_format(fmt_ctx, stream, file, 0);

	// TODO: Read channel number from metadata
	data_t *data = decode_allocate(c);
	if (!data)
		return 0;
	data->fmt_ctx = fmt_ctx;
	data->c = c;
	return data;
}

static void *decode_write_output(data_t *data, int channels)
{
	data->data = realloc(data->data, channels);
	AVFrame *iframe = data->iframe, *oframe = data->oframe;
	unsigned int w = oframe->width * 3u;
	uint8_t *ptr = oframe->data[0], *dptr = data->data;
	while (channels) {
		unsigned int s = channels < w ? channels : w;
		memcpy(dptr, ptr, s);
		channels -= s;
		dptr += s;
		ptr += oframe->linesize[0];
	}
	return data->data;
}

void *decode_read_frame(void *dp, int channels, int *got)
{
	data_t *data = dp;
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVCodecContext *c = data->c;
	AVFrame *iframe = data->iframe, *oframe = data->oframe;
	struct SwsContext *sc = data->sc;

	// Frame processing
	*got = 0;
	if (av_read_frame(fmt_ctx, &data->pkt) < 0)
		return 0;
	int got_frame;
	if (avcodec_decode_video2(c, iframe, &got_frame, &data->pkt) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error decoding frame\n");
		return 0;
	}
	*got = 1;
	if (got_frame)
		sws_scale(sc, (void *)iframe->data, iframe->linesize, 0, c->height,
				oframe->data, oframe->linesize);
	return decode_write_output(data, channels);
}

void decode_close(void *dp)
{
	data_t *data = (data_t *)dp;
	if (data->oframe)
		av_freep(data->oframe->data);
	free_data(data);
}
/* }}} */
