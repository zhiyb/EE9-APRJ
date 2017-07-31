#ifndef CODEC_H
#define CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <libavcodec/avcodec.h>

typedef struct data_t data_t;

void init();
int version();
void log_packet_data(const data_t *data, const AVPacket *pkt);
AVCodec *find_codec(const char *codec_name);

// Video encoder
data_t *encode_open_output(const char *file, const char *comment);
int encode_add_audio_stream_copy(data_t *data, AVCodecContext *dec_ac);
int encode_add_video_stream(data_t *data, AVCodec *vcodec,
		const char *pix_fmt_name, int resolution, int channels);
data_t *encode_write_header(data_t *data, const char *file);
int encode_write_audio_packet(data_t *data, AVPacket *pkt);
int encode_write_audio_frame(data_t *data, AVFrame *frame);
int encode_write_video_frame(data_t *data, AVFrame *frame);
// Audio packet (0), video frame (1) or none (-1)
int encode_write_packet_or_frame(data_t *data, AVPacket *pkt, AVFrame *frame);
void encode_close(data_t *data);

// Video decoder
data_t *decode_open_input(const char *file, char **comment,
		int *gota, int *gotv);
AVCodecContext *decode_context(data_t *data, const int video);
AVPacket *decode_read_packet(data_t *data, int *got, int *video);
AVFrame *decode_video_frame(data_t *data, AVPacket *pkt);
void decode_free_packet(AVPacket *pkt);
void decode_close(data_t *data);

// Channel data
AVFrame *encode_channels(data_t *data, void *fp, int channels);
void *decode_channels(data_t *data, AVFrame *frame, int channels);

#ifdef __cplusplus
}
#endif

#endif
