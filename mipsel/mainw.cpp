#include <stdint.h>
#include <time.h>
#include "mainw.h"

#define SPACING	6

// 4 bytes header, 1 byte command, 1 byte stream
static const uint8_t header[] = {0xde, 0xad, 0xbe, 0xef, 0x02, 0x00};

MainW::MainW() : QMainWindow()
{
#if 0
	clock_getres(CLOCK_MONOTONIC, &_t);
	qDebug("CLOCK_MONOTONIC res: %lu, %lu", _t.tv_sec, _t.tv_nsec);
	clock_gettime(CLOCK_MONOTONIC, &_t);
	qDebug("CLOCK_MONOTONIC time: %lu, %lu", _t.tv_sec, _t.tv_nsec);
#endif

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
	hLay->addWidget(sbResolution = new QSpinBox(5, 1000, 5, w));
	sbResolution->setValue(20);
	sbResolution->setMinimumWidth(32);
	hLay->addWidget(new QLabel(tr("Start:"), w));
	hLay->addWidget(sbStart = new QSpinBox(0, 0, 0, w));
	sbStart->setMinimumWidth(48);
	sbStart->setLineStep(1);
	hLay->addWidget(new QLabel(tr("CHs:"), w));
	hLay->addWidget(sbChannels = new QSpinBox(0, 0, 0, w));
	sbChannels->setMinimumWidth(48);
	sbChannels->setLineStep(1);

	// Host connection
	hLay = new QHBoxLayout;
	hLay->setSpacing(SPACING);
	vLay->addLayout(hLay);

	hLay->addWidget(new QLabel("Host:", w));
	hLay->addWidget(leHost = new QLineEdit("192.168.2.1", w));
	hLay->addWidget(new QLabel("Port:", w));
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

	// Instrumentation displays
	QGridLayout *gLay = new QGridLayout;
	vLay->addLayout(gLay);

	// Done
	vLay->addStretch(1);

	connect(pbBrowse, SIGNAL(clicked()), this, SLOT(open()));
	connect(pbConnect, SIGNAL(toggled(bool)), this, SLOT(connectTo(bool)));
	connect(pbStart, SIGNAL(clicked()), this, SLOT(start()));
	connect(pbStop, SIGNAL(clicked()), this, SLOT(stop()));

	connect(sbStart, SIGNAL(valueChanged(int)), this, SLOT(updateChannels(int)));

	socket = new QSocket(this);

	connect(socket, SIGNAL(connected()), this, SLOT(connected()));
	connect(socket, SIGNAL(error(int)), this, SLOT(connectionClosed()));
	connect(socket, SIGNAL(connectionClosed()), this, SLOT(connectionClosed()));

	// Instrumentation
	cpu.now = &cpu.stat[0];
	cpu.prev = &cpu.stat[1];
	cpuPerf();
	*cpu.prev = *cpu.now;
	startTimer(500);
}

void MainW::timerEvent(QTimerEvent *)
{
	pbStatus->setProgress(f.at());
	cpuPerf();
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
	MainW::cpu_t::stat_t *stat = cpu.prev;
	for (int i = 0; i != 7; i++)
		s >> stat->val[i];
	f.close();
}

void MainW::open()
{
	QString path = QFileDialog::getOpenFileName(lePath->text());
	lePath->setText(path);

	if (f.isOpen())
		closeFile();
	f.setName(path);
	if (f.open(IO_Raw | IO_ReadOnly)) {
		s.setDevice(&f);
		readFrame();
		stop();
		sbStart->setMaxValue(_chCount - 1u);
		sbStart->setValue(8669);
		sbChannels->setValue(sbChannels->maxValue());

		pbStatus->setTotalSteps(f.size());
		pbStatus->setProgress(f.at());
	}
}

void MainW::connectTo(bool e)
{
	if (e)
		socket->connectToHost(leHost->text(), lePort->text().toUShort());
	else
		socket->close();
}

void MainW::connected()
{
	pbConnect->setOn(true);
}

void MainW::connectionClosed()
{
	pbConnect->setOn(false);
	stop();
}

void MainW::updateChannels(int v)
{
	sbChannels->setMaxValue(_chCount - v + 1);
}

void MainW::start()
{
	if (!f.isOpen() || socket->state() != QSocket::Connection)
		return;

	f.at(0);
	pbStatus->setProgress(f.at());
	pbStart->setEnabled(false);
	pbStop->setEnabled(f.isOpen());

	_started = true;
	_sendStart = sbStart->value();
	_sendChannels = sbChannels->value();

	clock_gettime(CLOCK_MONOTONIC, &_start);
	QTimer::singleShot(sbResolution->value(), this, SLOT(next()));
}

void MainW::stop()
{
	_started = false;
	pbStart->setEnabled(f.isOpen());
	pbStop->setEnabled(false);
}

void MainW::next()
{
	if (socket->state() != QSocket::Connection)
		stop();

	readFrame();
	if (s.atEnd())
		stop();

	sendFrame();

	clock_gettime(CLOCK_MONOTONIC, &_t);
	if (_started)
		QTimer::singleShot(sbResolution->value(), this, SLOT(next()));
}

void MainW::readFrame()
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

void MainW::sendFrame()
{
	if (socket->bytesToWrite())
		return;
	socket->writeBlock((char *)header, sizeof(header));
	socket->writeBlock((char *)&_sendChannels, 2);
	socket->writeBlock(_data.data() + _sendStart, _sendChannels);
	socket->flush();
}

void MainW::closeFile()
{
	f.close();
	pbStart->setEnabled(f.isOpen());
	pbStop->setEnabled(f.isOpen());
	pbStatus->setProgress(0);
	pbStatus->setTotalSteps(0);
}
