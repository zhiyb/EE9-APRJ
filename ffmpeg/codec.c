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
#include <libavutil/timestamp.h>
#include <libavutil/imgutils.h>
#include <libavutil/mathematics.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>

/* {{{ Structure and basics */
typedef struct data_t {
	AVFormatContext *fmt_ctx;
	AVFrame *iframe, *oframe;
	AVPacket pkt;
	struct SwsContext *sc;
	int resolution;
	int pts;
	void *data;
	struct {
		int stream;
	} audio, video;
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

static void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
	printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
			av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
			av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
			av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
			pkt->stream_index);
}

void log_packet_data(const data_t *data, const AVPacket *pkt)
{
	log_packet(data->fmt_ctx, pkt);
}

AVCodec *find_codec(const char *codec_name)
{
	AVCodec *codec = avcodec_find_encoder_by_name(codec_name);
	if (!codec) {
		av_log(NULL, AV_LOG_ERROR, "Codec %s not recognised\n", codec_name);
		return 0;
	}
	return codec;
}
/* }}} */

/* {{{ Encode */
static data_t *encode_allocate(data_t *data, AVCodecContext *ac, AVCodecContext *vc)
{
	AVFrame *iframe = 0, *oframe = 0;
	struct SwsContext *sc = 0;

	if (!data)
		return 0;
	if (!vc)
		goto done;

	// Allocate input frame
	oframe = av_frame_alloc();
	if (!oframe) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate video frame\n");
		goto error;
	}
	oframe->format = vc->pix_fmt;
	oframe->width = vc->width;
	oframe->height = vc->height;
	oframe->channel_layout = vc->channel_layout;

	if (av_image_alloc(oframe->data, oframe->linesize, vc->width, vc->height,
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
	iframe->width = vc->width;
	iframe->height = vc->height;

	if (av_image_alloc(iframe->data, iframe->linesize, vc->width, vc->height,
			iframe->format, 32) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate input frame buffer\n");
		goto error;
	}
	memset(iframe->data[0], 0, vc->height * iframe->linesize[0]);

	// Create format converter
	sc = sws_getContext(vc->width, vc->height, iframe->format,
			vc->width, vc->height, vc->pix_fmt,
			0, NULL, NULL, NULL);
	if (!sc) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create format converter\n");
		goto error;
	}

done:
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

data_t *encode_open_output(const char *file, const char *comment)
{
	// Open output media file
	AVFormatContext *fmt_ctx;
	if (avformat_alloc_output_context2(&fmt_ctx, NULL, NULL, file) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Could not allocate format context for %s\n", file);
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

	data_t *data = (data_t *)calloc(1, sizeof(data_t));
	if (!data)
		return 0;
	data->fmt_ctx = fmt_ctx;
	data->audio.stream = -1;
	data->video.stream = -1;
	return data;
}

int encode_add_audio_stream_copy(data_t *data, AVCodecContext *dec_ac)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	if (!dec_ac)
		return -1;

	AVCodec *acodec = avcodec_find_encoder(dec_ac->codec_id);
	if (!acodec) {
		av_log(NULL, AV_LOG_ERROR, "Encoding codec for %s not found\n",
				dec_ac->codec_name);
		return -1;
	}

	AVStream *out_stream = avformat_new_stream(fmt_ctx, acodec);
	if (!out_stream) {
		av_log(NULL, AV_LOG_ERROR,
				"Failed allocating output audio stream\n");
		return -1;
	}
	out_stream->id = fmt_ctx->nb_streams - 1;
	av_log(NULL, AV_LOG_INFO, "New audio stream %d for %p\n",
			out_stream->id, fmt_ctx);

	// Configure codec parameters
	AVCodecContext *ac = out_stream->codec;
	ac->bit_rate = dec_ac->bit_rate;
	ac->sample_rate = dec_ac->sample_rate;
	ac->channel_layout = dec_ac->channel_layout;
	ac->channels = av_get_channel_layout_nb_channels(dec_ac->channel_layout);
	// Take first format from list of supported formats
	ac->sample_fmt = dec_ac->sample_fmt;
	ac->time_base = dec_ac->time_base;
	out_stream->time_base = ac->time_base;

	// Open codec context
	if (avcodec_open2(ac, acodec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open audio codec context\n");
		return -1;
	}
	data->audio.stream = out_stream->id;
	return data->audio.stream;
}

int encode_add_video_stream(data_t *data, AVCodec *vcodec,
		const char *pix_fmt_name, int resolution, int channels)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	if (!vcodec)
		return -1;

	AVStream *out_stream = avformat_new_stream(fmt_ctx, vcodec);
	if (!out_stream) {
		av_log(NULL, AV_LOG_ERROR,
				"Failed allocating output video stream\n");
		return -1;
	}
	out_stream->id = fmt_ctx->nb_streams - 1;
	av_log(NULL, AV_LOG_INFO, "New video stream %d for %p\n",
			out_stream->id, fmt_ctx);

	AVCodecContext *vc = out_stream->codec;
	/* resolution must be a multiple of two */
	int ch = (channels + 2) / 3;		// Rounding
	vc->height = (unsigned int)round(sqrt(ch)) & ~1u;
	vc->width = (ch + vc->height - 1u) / vc->height;
	vc->width = (vc->width + 1u) & ~1u;
	out_stream->time_base = vc->time_base = (AVRational){resolution, 1000};
	if (pix_fmt_name != 0)
		vc->pix_fmt = av_get_pix_fmt(pix_fmt_name);
	if (vcodec->id == AV_CODEC_ID_H264)
		if (av_opt_set_double(vc->priv_data, "crf", 0, 0) != 0)
			av_log(NULL, AV_LOG_WARNING, "Cannot set crf option\n");

	// Open codec context
	if (avcodec_open2(vc, vcodec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open video codec context\n");
		return -1;
	}
	data->video.stream = out_stream->id;
	data->resolution = resolution;
	return data->video.stream;
}

data_t *encode_write_header(data_t *data, const char *file)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	// Audio stream context
	int astream = data->audio.stream;
	AVCodecContext *ac = 0;
	if (astream >= 0)
		ac = fmt_ctx->streams[astream]->codec;
	// Video stream context
	int vstream = data->video.stream;
	AVCodecContext *vc = 0;
	if (vstream >= 0)
		vc = fmt_ctx->streams[vstream]->codec;
	// Init muxer, write file header
	if (avformat_write_header(fmt_ctx, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR,
				"Error occurred when opening output file\n");
		return 0;
	}
	av_dump_format(fmt_ctx, 0, file, 1);
	return encode_allocate(data, ac, vc);
}

int encode_write_audio_packet(data_t *data, AVPacket *pkt)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	int stream = data->audio.stream;
	// Prepare packet for muxing
	pkt->stream_index = data->audio.stream;
	// Assuming same timebase as audio stream
	//log_packet(fmt_ctx, pkt);
	int ret = av_interleaved_write_frame(fmt_ctx, pkt);
	av_free_packet(pkt);
	return ret >= 0;
}

int encode_write_audio_frame(data_t *data, AVFrame *frame)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVCodecContext *c = fmt_ctx->streams[data->audio.stream]->codec;

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	// Encode frame
	int got_output = 0;
	if (avcodec_encode_audio2(c, &pkt, frame, &got_output) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error encoding frame\n");
		av_free_packet(&pkt);
		return 0;
	}
	//av_frame_make_writable(frame);
	if (!got_output) {
		av_free_packet(&pkt);
		return 1;
	}

	// Prepare packet for muxing
	pkt.stream_index = data->audio.stream;
	return av_interleaved_write_frame(fmt_ctx, &pkt) >= 0;
}

AVFrame *encode_channels(data_t *data, void *fp, int channels)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVCodecContext *c = fmt_ctx->streams[data->video.stream]->codec;
	AVFrame *iframe = data->iframe, *oframe = data->oframe;
	struct SwsContext *sc = data->sc;

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
	oframe->pts = av_rescale_q_rnd(data->pts++, c->time_base,
			fmt_ctx->streams[data->video.stream]->time_base,
			(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
	return oframe;
}

int encode_write_video_frame(data_t *data, AVFrame *frame)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVCodecContext *c = fmt_ctx->streams[data->video.stream]->codec;

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	// Encode frame
	int got_output = 0;
	if (avcodec_encode_video2(c, &pkt, frame, &got_output) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error encoding frame\n");
		av_free_packet(&pkt);
		return 0;
	}
	if (!got_output) {
		av_free_packet(&pkt);
		return 1;
	}

	// Prepare packet for muxing
	pkt.stream_index = data->video.stream;
	//log_packet(fmt_ctx, &pkt);
	return av_interleaved_write_frame(fmt_ctx, &pkt) >= 0;
}

static void encode_flush_video(data_t *data)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVCodecContext *c = fmt_ctx->streams[data->video.stream]->codec;

	AVPacket pkt;
	// Flush encoder
	for (;;) {
		// Empty packet
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		int got_output = 0;
		if (avcodec_encode_video2(c, &pkt, NULL, &got_output) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error encoding frame\n");
			break;
		}
		if (!got_output)
			break;

		// Prepare packet for muxing
		pkt.stream_index = data->video.stream;
		if (av_interleaved_write_frame(fmt_ctx, &pkt) < 0)
			break;
	}
	av_write_trailer(fmt_ctx);
}

static void encode_flush_audio(data_t *data)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVCodecContext *c = fmt_ctx->streams[data->audio.stream]->codec;

	AVPacket pkt;
	// Flush encoder
	for (;;) {
		// Empty packet
		av_init_packet(&pkt);
		pkt.data = NULL;
		pkt.size = 0;
		int got_output = 0;
		if (avcodec_encode_audio2(c, &pkt, NULL, &got_output) < 0) {
			av_log(NULL, AV_LOG_ERROR, "Error encoding frame\n");
			break;
		}
		if (!got_output)
			break;

		// Prepare packet for muxing
		pkt.stream_index = data->audio.stream;
		if (av_interleaved_write_frame(fmt_ctx, &pkt) < 0)
			break;
	}
	av_write_trailer(fmt_ctx);
}

int encode_write_packet_or_frame(data_t *data, AVPacket *pkt, AVFrame *frame)
{
	int video;
	if (pkt && frame) {
		int astream = data->audio.stream, vstream = data->video.stream;
		AVRational atb = data->fmt_ctx->streams[astream]->time_base;
		AVRational vtb = data->fmt_ctx->streams[vstream]->time_base;
		// Assuming audio packet has the same timebase as audio stream
		video = av_compare_ts(frame->pts, vtb, pkt->pts, atb) < 0;
	} else
		video = frame ? 1 : pkt ? 0 : -1;
	if (video == 0)
		encode_write_audio_packet(data, pkt);
	else
		encode_write_video_frame(data, frame);
	return video;
}

void encode_close(data_t *data)
{
	if (!data)
		return;
	if (data->video.stream >= 0)
		encode_flush_video(data);
	else if (data->audio.stream >= 0)
		encode_flush_audio(data);

	if (data->oframe)
		av_freep(data->oframe->data);
	if (data->iframe)
		av_freep(data->iframe->data);
	free_data(data);
}
/* }}} */

/* {{{ Decode */
static data_t *decode_allocate(AVCodecContext *ac, AVCodecContext *vc)
{
	AVFrame *iframe = 0, *oframe = 0;
	struct SwsContext *sc = 0;
	data_t *data = 0;

	// Allocate input frame
	iframe = av_frame_alloc();
	if (!iframe) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate frame buffer\n");
		goto error;
	}

	if (!vc)
		goto done;

	// Allocate output frame buffer (packed RGB)
	oframe = av_frame_alloc();
	if (!oframe) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate output frame\n");
		goto error;
	}

	oframe->format = AV_PIX_FMT_RGB24;
	oframe->width = vc->width;
	oframe->height = vc->height;

	if (av_image_alloc(oframe->data, oframe->linesize, vc->width, vc->height,
			oframe->format, 32) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate output frame buffer\n");
		goto error;
	}

	// Create format converter
	// TODO: Test different algorithms
	sc = sws_getContext(vc->width, vc->height, vc->pix_fmt,
			vc->width, vc->height, oframe->format,
			0, NULL, NULL, NULL);
	if (!sc) {
		av_log(NULL, AV_LOG_ERROR, "Cannot create format converter\n");
		goto error;
	}

done:
	data = (data_t *)calloc(1, sizeof(data_t));
	if (!data)
		goto error;
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

static int decode_find_stream(const char *file, AVFormatContext *fmt_ctx,
		enum AVMediaType type)
{
	int stream;
	AVCodec *codec = NULL;
	if ((stream = av_find_best_stream(fmt_ctx, type, -1, -1,
					&codec, 0)) < 0) {
		//av_log(NULL, AV_LOG_ERROR, "Cannot find stream type %d\n", type);
		return -1;
	}
	if (!codec) {
		av_log(NULL, AV_LOG_ERROR, "Failed to find codec\n");
		return -1;
	}
	AVCodecContext *c = fmt_ctx->streams[stream]->codec;
	if (avcodec_open2(c, codec, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Failed to open codec\n");
		return -1;
	}
	if (c->hwaccel)
		av_log(NULL, AV_LOG_INFO, "Using HWAccel %s\n", c->hwaccel->name);
	return stream;
}

data_t *decode_open_input(const char *file, char **comment, int *gota, int *gotv)
{
	AVFormatContext *fmt_ctx = NULL;
	if (avformat_open_input(&fmt_ctx, file, NULL, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot open input file %s\n", file);
		return 0;
	}
	if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return 0;
	}
	av_dump_format(fmt_ctx, 0, file, 0);
	// Open audio stream
	int astream = decode_find_stream(file, fmt_ctx, AVMEDIA_TYPE_AUDIO);
	AVCodecContext *ac = 0;
	if (gota)
		*gota = astream >= 0;
	if (astream >= 0)
		ac = fmt_ctx->streams[astream]->codec;
	// Open video stream
	int vstream = decode_find_stream(file, fmt_ctx, AVMEDIA_TYPE_VIDEO);
	AVCodecContext *vc = 0;
	if (gotv)
		*gotv = vstream >= 0;
	if (vstream >= 0)
		vc = fmt_ctx->streams[vstream]->codec;
	// Find comment metadata
	if (comment) {
		AVDictionaryEntry *tag = av_dict_get(fmt_ctx->metadata,
				"comment", NULL, 0);
		*comment = tag ? tag->value : NULL;
	}
	// Allocate data structure
	data_t *data = decode_allocate(ac, vc);
	if (!data)
		return 0;
	data->fmt_ctx = fmt_ctx;
	data->audio.stream = astream;
	data->video.stream = vstream;
	return data;
}

AVCodecContext *decode_context(data_t *data, const int video)
{
	int stream = video ? data->video.stream : data->audio.stream;
	if (stream < 0)
		return NULL;
	return data->fmt_ctx->streams[stream]->codec;
}

AVPacket *decode_read_packet(data_t *data, int *got, int *video)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVFrame *iframe = data->iframe, *oframe = data->oframe;

	av_init_packet(&data->pkt);
	data->pkt.data = NULL;
	data->pkt.size = 0;

	// Frame processing
	int ret = av_read_frame(fmt_ctx, &data->pkt);
	if (ret < 0 && ret != AVERROR_EOF) {
		*got = 0;
		av_log(NULL, AV_LOG_ERROR, "Could not read frame\n");
		av_free_packet(&data->pkt);
		return NULL;
	}
	*got = ret != AVERROR_EOF;
	int stream = data->pkt.stream_index;
	*video = stream == data->video.stream;
	AVCodecContext *c = fmt_ctx->streams[stream]->codec;
	av_packet_rescale_ts(&data->pkt,
			fmt_ctx->streams[stream]->time_base, c->time_base);
	//log_packet(fmt_ctx, &data->pkt);
	return &data->pkt;
}

AVFrame *decode_video_frame(data_t *data, AVPacket *pkt)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVFrame *iframe = data->iframe, *oframe = data->oframe;
	int stream = pkt->stream_index;
	AVCodecContext *c = fmt_ctx->streams[stream]->codec;

	int got;
	if (avcodec_decode_video2(c, iframe, &got, pkt) < 0) {
		av_log(NULL, AV_LOG_ERROR, "Error decoding video frame\n");
		av_free_packet(pkt);
		return NULL;
	}
	av_free_packet(pkt);
	if (!got)
		return NULL;
	return iframe;
}

void decode_free_packet(AVPacket *pkt)
{
	av_free_packet(pkt);
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

void *decode_channels(data_t *data, AVFrame *frame, int channels)
{
	AVFormatContext *fmt_ctx = data->fmt_ctx;
	AVFrame *iframe = data->iframe, *oframe = data->oframe;
	struct SwsContext *sc = data->sc;
	sws_scale(sc, (void *)iframe->data, iframe->linesize, 0, iframe->height,
			oframe->data, oframe->linesize);
	return decode_write_output(data, channels);
}

void decode_close(data_t *data)
{
	if (!data)
		return;
	if (data->oframe)
		av_freep(data->oframe->data);
	free_data(data);
}
/* }}} */
