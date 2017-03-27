#ifndef WORKER_H
#define WORKER_H

#include <stdint.h>
#include <time.h>
#include <qthread.h>
#include <qt.h>

class Worker: public QThread
{
public:
	Worker() {}
	~Worker() {stop();}

	void init(QObject *recv);
	void stop() {_stop = true; ev.wakeAll(); wait();}

	QWaitCondition ev;

	bool open(QString path);
	void readFrame();
	void skipFrame();
	void sendFrame();
	void reset();
	void close() {stop(); f.close();}

	bool isOpen() {return f.isOpen() && isConnected();}
	bool isConnected() {return socket.state() == QSocket::Connection;}
	uint size() {return f.size();}
	uint at() {return f.at();}
	long diffms() {return _diffms;}

	QSocket socket;

	unsigned long _resolution, _frames, _skipped;
	uint16_t _chCount, _sendStart, _sendChannels;
	QByteArray _data;

protected:
	void run();

private:
	volatile bool _stop;
	long _diffms;

	QObject *recv;

	QFile f;
	QDataStream s;
};

#endif	// WORKER_H
