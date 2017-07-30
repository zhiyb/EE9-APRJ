#ifndef CODEC_H
#define CODEC_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct data_t data_t;

void init();
int version();
AVCodec *find_codec(const char *codec_name);

// Video encoder
data_t *encode_open_output(const char *file,
		AVCodec *acodec, AVCodec *vcodec, const char *pix_fmt_name,
		const int resolution, const int channels, const char *comment);
int encode_write_video_frame(data_t *data, void *fp, int channels);
void encode_close(data_t *data);

// Video decoder
data_t *decode_open_input(const char *file, char **comment, int *gota, int *gotv);
AVFrame *decode_read_frame(data_t *data, int *got, int *video);
void *decode_channels(data_t *data, AVFrame *frame, int channels);
void decode_close(data_t *data);

#ifdef __cplusplus
}
#endif

#endif
