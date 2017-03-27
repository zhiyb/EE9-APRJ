#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include "worker.h"

// 4 bytes header, 1 byte command, 1 byte stream
static const uint8_t header[] = {0xde, 0xad, 0xbe, 0xef, 0x02, 0x00};

static Worker *worker;

static void notify(union sigval)
{
	worker->ev.wakeAll();
}

void Worker::init(QObject *recv)
{
	this->recv = recv;
	reset();
}

bool Worker::open(QString path)
{
	if (f.isOpen())
		f.close();
	f.setName(path);
	if (f.open(IO_Raw | IO_ReadOnly)) {
		s.setDevice(&f);
		readFrame();
		reset();
	}
	return f.isOpen();
}

void Worker::readFrame()
{
	// Headers, command and stream
	uint32_t len = 0;
	while (len != sizeof(header)) {
		uint8_t c;
		s.readRawBytes((char *)&c, 1);
		if (s.atEnd())
			return;
		if (c != header[len++]) {
			len = c == header[0] ? 1 : 0;
			qDebug("Data header error");
		}
	}

	// Channel number
	s.readRawBytes((char *)&_chCount, 2);
	if (_data.size() < _chCount)
		_data.resize(_chCount);

	s.readRawBytes(_data.data(), _chCount);
}

void Worker::skipFrame()
{
	f.at(f.at() + sizeof(header) + 2 + _chCount);
	_skipped++;
}

void Worker::sendFrame()
{
	if (!isConnected()) {
		_stop = true;
		return;
	}
	socket.writeBlock((char *)header, sizeof(header));
	socket.writeBlock((char *)&_sendChannels, 2);
	socket.writeBlock(_data.data() + _sendStart, _sendChannels);
	socket.flush();
	_frames++;
}

void Worker::reset()
{
	_frames = 0;
	_skipped = 0;
	_diffms = 0;
	if (isOpen())
		f.at(0);
}

void Worker::run()
{
	_stop = false;
	worker = this;
	reset();

	struct sigevent sigev;
	memset(&sigev, 0, sizeof(sigev));
	sigev.sigev_notify = SIGEV_THREAD;
	sigev.sigev_notify_function = &notify;

	timer_t tid;
	if (timer_create(CLOCK_REALTIME, &sigev, &tid) != 0)
		perror("Error create timer");

	struct timespec itvl;
	itvl.tv_sec = _resolution / 1000u;
	itvl.tv_nsec = _resolution * (1000u * 1000u);
	struct itimerspec spec;
	spec.it_interval = itvl;
	spec.it_value = itvl;
	itvl.tv_sec = 0;
	itvl.tv_nsec = 0;

	if (timer_settime(tid, 0, &spec, 0) != 0)
		perror("Error start timer");
	struct timespec start;
	clock_gettime(CLOCK_MONOTONIC, &start);

	while (!_stop) {
		ev.wait();
		readFrame();
		if (s.atEnd()) {
			_stop = true;
			postEvent(recv, new QCustomEvent(QEvent::User, 0));
		} else
			sendFrame();

		// Instrumentation
#define MS	(1000l)
#define NS	(1000l * 1000l * 1000l)

		struct timespec t;
		clock_gettime(CLOCK_MONOTONIC, &t);
		t.tv_sec -= start.tv_sec;
		if (t.tv_nsec < start.tv_nsec) {
			t.tv_sec -= 1;
			t.tv_nsec = NS + t.tv_nsec - start.tv_nsec;
		} else
			t.tv_nsec -= start.tv_nsec;
		_diffms = _resolution * _frames - t.tv_nsec / 1000000l - t.tv_sec * 1000l;
	}

	memset(&spec, 0, sizeof(spec));
	timer_settime(tid, 0, &spec, 0);
}
