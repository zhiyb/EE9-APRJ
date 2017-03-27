#ifndef MAINW_H
#define MAINW_H

#include <stdint.h>
#include <qt.h>

class MainW : public QMainWindow
{
	Q_OBJECT
public:
	MainW();

protected:
	void timerEvent(QTimerEvent *e);

private slots:
	void open();
	void connectTo(bool e);
	void connected();
	void connectionClosed();
	void updateChannels(int v);
	void start();
	void stop();
	void next();

private:
	void cpuPerf();
	void sendFrame();
	void readFrame();
	void closeFile();

	QLineEdit *lePath;
	QSpinBox *sbResolution, *sbStart, *sbChannels;
	QLineEdit *leHost, *lePort;
	QPushButton *pbBrowse, *pbConnect;
	QPushButton *pbStart, *pbStop;
	QProgressBar *pbStatus;
	QLabel *lSkipped;

	QSocket *socket;

	QFile f;
	QDataStream s;

	bool _opened, _started;
	unsigned long _resolution, _frames, _skipped;
	uint16_t _chCount, _sendStart, _sendChannels;
	QByteArray _data;

	struct timespec _start;

	struct {
		int progress, perf;
	} timer;

	struct cpu_t {
		QProgressBar *pb[8];
		union stat_t {
			unsigned long val[7];
			struct {
				unsigned long user, nice, system, idle;
				unsigned long iowait, irq, softirq;

			};

			unsigned long total()
			{
				unsigned long sum = 0;
				for (int i = 0; i != 7; i++)
					sum += val[i];
				return sum;
			}
		} stat[2], *prev, *now;
	} cpu;
};

#endif
