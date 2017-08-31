#include <iostream>
#include <stdexcept>
#include <mutex>
#include <thread>
#include <string.h>
#include <unistd.h>
#include <codec.h>

using namespace std;

int main(int argc, char *argv[])
{
	unsigned int result = 1;
	codec_init();
	data_t *data = codec_alloc();
	if (!data) {
		cerr << "Error initialise libcodec" << endl;
		return 1;
	}
	clog << "Using libcodec version: " << std::hex << codec_version() << endl;

	int gota, gotv;
	if (!decode_open_input(data, "audio.mp3", 0, &gota, &gotv))
		goto err_codec;
	if (!gota) {
		cerr << "No audio stream found" << endl;
		goto err_dec;
	}

	if (fmod_init(data))
		goto err_dec;
	if ((result = fmod_version(data)) != 0)
		clog << "Using FMOD version " << std::hex << result << std::dec << endl;
	if (fmod_create_stream(data, data))
		goto err_fmod;
	if (fmod_play(data))
		goto err_fmod;

	for (;;) {
		if (!fmod_is_playing(data))
			break;
		int got, video;
		AVPacket *pkt = decode_read_packet(data, &got, &video);
		if (!got)
			break;
		if (video) {
			decode_free_packet(pkt);
			continue;
		}
		AVFrame *frame = decode_audio_frame(data, pkt);
		fmod_queue_frame(data, frame);
		fmod_update(data);

		unsigned int ms = 1000u * frame->nb_samples / frame->sample_rate;
		timespec sleepTime = {0, ms * 1000 * 1000};
		nanosleep(&sleepTime, NULL);
	}

	clog << "Waiting for playback..." << endl;
	for (;;) {
		if (!fmod_is_playing(data))
			break;
		if (fmod_update(data))
			break;
		timespec sleepTime = {0, 20 * 1000 * 1000};
		nanosleep(&sleepTime, NULL);
	}

err_fmod:
	fmod_close(data);
err_dec:
	decode_close(data);
err_codec:
	codec_free(data);
	return result;
}
