#ifndef MAINW_H
#define MAINW_H

#include <stdint.h>
#include <time.h>
#include <qt.h>
#include "worker.h"

class MainW: public QMainWindow
{
	Q_OBJECT
public:
	MainW();

protected:
	void timerEvent(QTimerEvent *e);
	void customEvent(QCustomEvent *e);

private slots:
	void open();
	void connectTo(bool e);
	void connected();
	void connectionClosed();
	void updateChannels(int v);
	void start();
	void stop();

private:
	void cpuPerf();

	Worker worker;

	QLineEdit *lePath;
	QSpinBox *sbResolution, *sbStart, *sbChannels;
	QLineEdit *leHost, *lePort;
	QPushButton *pbBrowse, *pbConnect;
	QPushButton *pbStart, *pbStop;
	QProgressBar *pbStatus;
	QLabel *lTime, *lSkipped;

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
