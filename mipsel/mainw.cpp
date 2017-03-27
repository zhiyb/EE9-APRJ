#include <algorithm>
#include <stdint.h>
#include <time.h>
#include "mainw.h"

#define ASIZE(a)	(sizeof(a) / sizeof((a)[0]))
#define SPACING	6

MainW::MainW(): QMainWindow()
{
#if 0
	struct timespec t;
	clock_getres(CLOCK_MONOTONIC, &t);
	qDebug("CLOCK_MONOTONIC res: %lu, %lu", t.tv_sec, t.tv_nsec);
#endif
	worker.init(this);
	setCaption(tr("Vixen Qt2 demo"));

	QWidget *w = new QWidget(this);
	setCentralWidget(w);

	QVBoxLayout *vLay = new QVBoxLayout(w);
	vLay->setMargin(SPACING);
	vLay->setSpacing(SPACING);

	// File & browse
	QHBoxLayout *hLay = new QHBoxLayout;
	hLay->setSpacing(SPACING);
	vLay->addLayout(hLay);

	hLay->addWidget(new QLabel(tr("File:"), w));
	hLay->addWidget(lePath = new QLineEdit("/mnt/UsrDisk/UsrDisk/Vixen", w));
	hLay->addWidget(pbBrowse = new QPushButton(tr("&Browse"), w));

	// File information
	hLay = new QHBoxLayout;
	hLay->setSpacing(SPACING);
	vLay->addLayout(hLay);

	hLay->addWidget(new QLabel(tr("Resolution:"), w));
	hLay->addWidget(sbResolution = new QSpinBox(10, 1000, 5, w));
	sbResolution->setValue(20);
	sbResolution->setMinimumWidth(32);
	hLay->addWidget(new QLabel(tr("Start:"), w));
	hLay->addWidget(sbStart = new QSpinBox(0, 0, 1, w));
	sbStart->setMinimumWidth(48);
	hLay->addWidget(new QLabel(tr("CHs:"), w));
	hLay->addWidget(sbChannels = new QSpinBox(0, 0, 1, w));
	sbChannels->setMinimumWidth(48);

	// Host connection
	hLay = new QHBoxLayout;
	hLay->setSpacing(SPACING);
	vLay->addLayout(hLay);

	hLay->addWidget(new QLabel(tr("Host:"), w));
	hLay->addWidget(leHost = new QLineEdit("192.168.2.1", w));
	hLay->addWidget(new QLabel(tr("Port:"), w));
	hLay->addWidget(lePort = new QLineEdit("12345", w));
	lePort->setMaxLength(5);
	lePort->setMaximumWidth(48);
	lePort->setValidator(new QIntValidator(0, 65535, lePort));
	hLay->addWidget(pbConnect = new QPushButton(tr("&Connect"), w));
	pbConnect->setToggleButton(true);

	// Start & stop & progress
	hLay = new QHBoxLayout;
	hLay->setSpacing(SPACING);
	vLay->addLayout(hLay);

	hLay->addWidget(pbStart = new QPushButton(tr("&Start"), w));
	pbStart->setEnabled(false);
	hLay->addWidget(pbStop = new QPushButton(tr("S&top"), w));
	pbStop->setEnabled(false);
	hLay->addWidget(pbStatus = new QProgressBar(w));
	pbStatus->setCenterIndicator(true);
	hLay->addWidget(lTime = new QLabel("00:00:00", w));

	// Instrumentation displays
	QGridLayout *gLay = new QGridLayout;
	vLay->addLayout(gLay);
	static const char *perfNames[] = {
		"total", "user", "nice", "system",
		"idle", "iowait", "irq", "softirq",
	};
	int idx = 0;
	for (int i = 0; i != ASIZE(perfNames); i++) {
#if 0
		switch (i) {
		case 1:	// user
		case 2:	// nice
		case 3:	// system
		case 6:	// irq
		case 7:	// softirq
			cpu.pb[i] = 0;
			continue;
		}
#endif
		gLay->addWidget(new QLabel(perfNames[i], w), idx / 3, 2 * (idx % 3));
		gLay->addWidget(cpu.pb[i] = new QProgressBar(100, w), idx / 3, 2 * (idx % 3) + 1);
		cpu.pb[i]->setCenterIndicator(true);
		idx++;
	}

	vLay->addWidget(lSkipped = new QLabel(tr("Statistics"), w));

	// Done
	vLay->addStretch(1);

	connect(pbBrowse, SIGNAL(clicked()), this, SLOT(open()));
	connect(pbConnect, SIGNAL(toggled(bool)), this, SLOT(connectTo(bool)));
	connect(pbStart, SIGNAL(clicked()), this, SLOT(start()));
	connect(pbStop, SIGNAL(clicked()), this, SLOT(stop()));

	connect(sbStart, SIGNAL(valueChanged(int)), this, SLOT(updateChannels(int)));

	connect(&worker.socket, SIGNAL(connected()), this, SLOT(connected()));
	connect(&worker.socket, SIGNAL(error(int)), this, SLOT(connectionClosed()));
	connect(&worker.socket, SIGNAL(connectionClosed()), this, SLOT(connectionClosed()));

	// Instrumentation
	cpu.now = &cpu.stat[0];
	cpu.prev = &cpu.stat[1];
	cpuPerf();
	*cpu.prev = *cpu.now;

	timer.progress = startTimer(500);
	timer.perf = startTimer(2000);
}

void MainW::timerEvent(QTimerEvent *e)
{
	if (e->timerId() == timer.progress) {
		// Playback progress
		if (worker.isOpen())
			pbStatus->setProgress(worker.at());
		if (worker._frames == 0l || worker._resolution == 0u)
			return;
		unsigned long sec = (worker._frames + worker._skipped) * worker._resolution / 1000ul;
		lTime->setText(QTime().addSecs(sec).toString());
		lSkipped->setText(QString("diff ms: %1").arg(worker.diffms()));
	} else if (e->timerId() == timer.perf) {
		// CPU usages
		cpuPerf();
		unsigned long diffTotal = cpu.now->total() - cpu.prev->total();
		if (diffTotal == 0)
			return;
		unsigned long diffIdle = 0;
		for (int i = 0; i != 7; i++) {
			unsigned long diff = cpu.now->val[i] - cpu.prev->val[i];
			if (cpu.pb[i + 1])
				cpu.pb[i + 1]->setProgress((diff * 100 + 50) / diffTotal);
			if (i == 3)
				diffIdle = diff;
		}
		cpu.pb[0]->setProgress(((diffTotal - diffIdle) * 100 + 50) / diffTotal);
	}
}

void MainW::customEvent(QCustomEvent *e)
{
	if (e->data() == 0)
		stop();
}

void MainW::cpuPerf()
{
	QFile f("/proc/stat");
	if (!f.open(IO_ReadOnly))
		return;
	QTextStream s(&f);
	QString str;
	s >> str;
	if (str != "cpu")
		return;

	std::swap(cpu.prev, cpu.now);
	MainW::cpu_t::stat_t *stat = cpu.now;
	for (int i = 0; i != 7; i++)
		s >> stat->val[i];
	f.close();
}

void MainW::open()
{
	QString path = QFileDialog::getOpenFileName(lePath->text());
	lePath->setText(path);

	worker.close();
	stop();
	pbStop->setEnabled(false);
	pbStatus->setProgress(0);
	pbStatus->setTotalSteps(0);
	if (worker.open(path)) {
		sbStart->setMinValue(1u);
		sbStart->setMaxValue(worker._chCount);
		sbStart->setValue(8669u);
		sbChannels->setValue(sbChannels->maxValue());
		pbStatus->setTotalSteps(worker.size());
		pbStatus->setProgress(worker.at());
	}
	pbStart->setEnabled(worker.isOpen());
}

void MainW::connectTo(bool e)
{
	if (e)
		worker.socket.connectToHost(leHost->text(), lePort->text().toUShort());
	else {
		worker.socket.close();
		stop();
	}
}

void MainW::connected()
{
	pbConnect->setOn(true);
	pbStart->setEnabled(worker.isOpen());
}

void MainW::connectionClosed()
{
	pbConnect->setOn(false);
	stop();
}

void MainW::updateChannels(int v)
{
	sbChannels->setMaxValue(worker._chCount - v + 1u);
}

void MainW::start()
{
	if (!worker.isOpen())
		return;

	pbStatus->setProgress(worker.at());
	pbStart->setEnabled(false);
	pbStop->setEnabled(worker.isOpen());

	worker._resolution = sbResolution->value();
	worker._sendStart = sbStart->value() - 1u;
	worker._sendChannels = sbChannels->value();
	worker.start();
}

void MainW::stop()
{
	worker.stop();
	pbStart->setEnabled(worker.isOpen());
	pbStop->setEnabled(false);
}
