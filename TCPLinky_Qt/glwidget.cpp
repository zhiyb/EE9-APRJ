#include "glwidget.h"
#include <QDebug>
#include <QDialog>
#include <QVBoxLayout>

// 4 bytes header, 1 byte command, 1 byte stream
static const uint8_t header[] = {0xde, 0xad, 0xbe, 0xef, 0x01, 0x00};

#define ZOOMSTEP	0.25

GLWidget::GLWidget(QTcpSocket *socket, QWidget *parent) : QOpenGLWidget(parent)
{
	_socket = socket;
	_bytes = 0;
	_packet = 0;
	stat.cTotal = 0;
	stat.uTotal = 0;
	stat.pTotal = 0;
	stat.maxSize = 0;
	stat.maxUpdates = 0;
	connect(socket, SIGNAL(disconnected()), this, SLOT(disconnected()));
	connect(socket, SIGNAL(readyRead()), this, SLOT(readData()));

	QSurfaceFormat fmt = format();
	fmt.setSamples(4);
	fmt.setSwapInterval(0);
	fmt.setVersion(3, 3);
	fmt.setOption(0);
	fmt.setProfile(QSurfaceFormat::CoreProfile);
	fmt.setDepthBufferSize(0);
	fmt.setStencilBufferSize(0);
	fmt.setAlphaBufferSize(0);
	QSurfaceFormat::setDefaultFormat(fmt);
	setFormat(fmt);

	setFocusPolicy(Qt::StrongFocus);
	setAutoFillBackground(false);
	resize(640, 640);
}

void GLWidget::initializeGL()
{
	initializeOpenGLFunctions();
	qDebug() << format();
	data.pause = false;
	data.zoom = 0;
	data.moveX = 0;
	data.moveY = 0;

	glEnable(GL_MULTISAMPLE);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_CULL_FACE);
	glClearColor(0.0, 0.0, 0.0, 1.0);

	qDebug("Loading vertex shader...");
	data.vsh = loadShaderFile(GL_VERTEX_SHADER, ":/vertex.vsh");
	qDebug("Loading fragment shader...");
	data.fsh = loadShaderFile(GL_FRAGMENT_SHADER, ":/fragment.fsh");
	if (data.vsh == 0 || data.fsh == 0) {
		close();
		return;
	}

	data.program = glCreateProgram();
	glAttachShader(data.program, data.vsh);
	glAttachShader(data.program, data.fsh);

	qDebug() << "Linking program...";
	glLinkProgram(data.program);

	int logLength;
	glGetProgramiv(data.program, GL_INFO_LOG_LENGTH, &logLength);
	char log[logLength];
	glGetProgramInfoLog(data.program, logLength, &logLength, log);
	qWarning(log);

	glUseProgram(data.program);

	glGenVertexArrays(1, &data.vao);
	glBindVertexArray(data.vao);

	const static QVector3D vertices[4] = {
		QVector3D(1.f, 1.f, 0.f),
		QVector3D(1.f, 0.f, 0.f),
		QVector3D(0.f, 0.f, 0.f),
		QVector3D(0.f, 1.f, 0.f),
	};
	data.loc.vertex = glGetAttribLocation(data.program, "vertex");
	glGenBuffers(1, &data.bVertex);
	glBindBuffer(GL_ARRAY_BUFFER, data.bVertex);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glEnableVertexAttribArray(data.loc.vertex);
	glVertexAttribPointer(data.loc.vertex, 3, GL_FLOAT, GL_FALSE, 0, 0);

	data.loc.intensity = glGetAttribLocation(data.program, "intensity");
	glGenBuffers(1, &data.bIntensity);
	glBindBuffer(GL_ARRAY_BUFFER, data.bIntensity);
	glBufferData(GL_ARRAY_BUFFER, 65536 * sizeof(GLfloat), 0, GL_DYNAMIC_DRAW);
	glEnableVertexAttribArray(data.loc.intensity);
	glVertexAttribPointer(data.loc.intensity, 1, GL_FLOAT, GL_FALSE, 0, 0);
	glVertexAttribDivisor(data.loc.intensity, 1);

	data.loc.projection = glGetUniformLocation(data.program, "projection");
	data.loc.zoom = glGetUniformLocation(data.program, "zoom");
	data.loc.move = glGetUniformLocation(data.program, "move");
	data.loc.dimension = glGetUniformLocation(data.program, "dimension");
}

void GLWidget::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
	float asp = (float)h / (float)w;
	data.projection.setToIdentity();
	data.projection.ortho(-1, 1, -asp, asp, -1, 1);
}

void GLWidget::paintGL()
{
	glUseProgram(data.program);
	glBindVertexArray(data.vao);
	if (_update) {
		glBindBuffer(GL_ARRAY_BUFFER, data.bIntensity);
		_mutex.lock();
		glBufferSubData(GL_ARRAY_BUFFER, 0, _channels.size() * sizeof(GLfloat), \
			     _channels.constData());
		_update = false;
		_mutex.unlock();
		//qDebug() << _channels.size() << ceil(sqrt(_channels.size()));
	}

	glUniformMatrix4fv(data.loc.projection, 1, GL_FALSE, data.projection.constData());
	glUniform1i(data.loc.dimension, ceil(sqrt(_channels.size())));
	//glUniform1i(data.loc.dimension, 93);
	glUniform1f(data.loc.zoom, pow(2, -data.zoom));
	glUniform2f(data.loc.move, data.moveX, data.moveY);

	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArraysInstanced(GL_TRIANGLE_FAN, 0, 4, _channels.size());
}

void GLWidget::closeEvent(QCloseEvent *event)
{
	Q_UNUSED(event);
	_socket->disconnectFromHost();
}

void GLWidget::wheelEvent(QWheelEvent *e)
{
	float zoom = -(float)e->angleDelta().y() / 120. * ZOOMSTEP;
	data.zoom += zoom;
	update();
}

void GLWidget::mousePressEvent(QMouseEvent *e)
{
	data.prevPos = e->pos();
}

void GLWidget::mouseMoveEvent(QMouseEvent *e)
{
	QPointF p = e->pos() - data.prevPos;
	data.moveX += -p.x() * 2.f * pow(2, data.zoom) / (double)width();
	data.moveY += p.y() * 2.f * pow(2, data.zoom) / (double)width();
	data.prevPos = e->pos();
	update();
}

void GLWidget::keyPressEvent(QKeyEvent *e)
{
	const float moveTh = 10;
	switch (e->key()) {
	case 'r':	// Refresh
	case 'R':
		break;
	case 'q':	// Quit
	case 'Q':
	case Qt::Key_Escape:
		if (e->modifiers() & Qt::ShiftModifier)
			qApp->quit();
		else
			close();
		return;
	case 'p':	// Pause
	case 'P':
		data.pause = !data.pause;
		break;
	case ' ':	// Reset view
		data.zoom = 0;
		data.moveX = 0;
		data.moveY = 0;
		break;
	case Qt::Key_Up:
		data.moveY += -moveTh * 2.f * pow(2, data.zoom) / (double)width();
		break;
	case Qt::Key_Down:
		data.moveY += moveTh * 2.f * pow(2, data.zoom) / (double)width();
		break;
	case Qt::Key_Left:
		data.moveX += moveTh * 2.f * pow(2, data.zoom) / (double)width();
		break;
	case Qt::Key_Right:
		data.moveX += -moveTh * 2.f * pow(2, data.zoom) / (double)width();
		break;
	case '+':	// Zoom in
	case '=':
		data.zoom -= ZOOMSTEP;
		break;
	case '-':	// Zoom out
	case '_':
		data.zoom += ZOOMSTEP;
		break;
	};
	update();
}

void GLWidget::disconnected()
{
	qDebug() << "Total number of packets:\t" << stat.pTotal;
	qDebug() << "Total number of bytes:\t" << stat.cTotal;
	qDebug() << "Total number of updates:\t" << stat.uTotal;
	qDebug() << "Maximum packet size:\t" << stat.maxSize;
	qDebug() << "Maximum update count:\t" << stat.maxUpdates;
	qDebug() << "Average data per packet:\t" << (float)stat.cTotal / (float)stat.pTotal;
	qDebug() << "Number of channels:\t" << _channels.size();
	qDebug() << "[SOCKET]" << _socket << "disconnected";
	_socket->deleteLater();
	deleteLater();
}

void GLWidget::readData()
{
	while (_socket->bytesAvailable()) {
		auto data = _socket->readAll();
		uint32_t len = data.length();
		const uint8_t *c = (uint8_t *)data.constData();
		//qDebug() << __func__ << data << _bytes << header[_bytes] << (uint8_t)*c;
		while (len) {
			//qDebug() << len << _bytes << header[_bytes] << (uint8_t)*c;
			if (_bytes < sizeof(header)) {
				// Header
				if (*c != header[_bytes]) {
					_bytes = *c++ == header[0];
					len--;
					qDebug() << "[DATA] Packet header error";
					continue;
				}
				c++;
				len--;
				_bytes++;
			} else if (_bytes == sizeof(header)) {
				// Channel count low byte
				_totalCount = *c++;
				len--;
				_bytes++;
			} else if (_bytes == sizeof(header) + 1) {
				// Channel count high byte
				_totalCount |= *c++ << 8;
				len--;
				_bytes++;
				// Data
				uint32_t size = std::min(_totalCount * 3, len);
				_packet = QByteArray((char *)c, size);
				_packet.resize(_totalCount * 3);
				c += size;
				len -= size;
				_bytes += size;
			} else {
				// Continuous data
				uint32_t bytes = _bytes - sizeof(header) - 2;
				uint32_t size = std::min(_totalCount * 3 - bytes, len);
				_packet.replace(bytes, size, (char *)c, size);
				c += size;
				len -= size;
				_bytes += size;
			}
			if (_bytes == sizeof(header) + 2 + _totalCount * 3) {
				// Packet complete
				// Statistics
				if (_bytes > stat.maxSize) {
					stat.maxSize = _bytes;
					stat.maxUpdates = _totalCount;
				}
				stat.cTotal += _bytes;
				stat.uTotal += _totalCount;
				stat.pTotal++;
				// Update channels
				while (_totalCount--) {
					const uint8_t *p = (uint8_t *)_packet.constData() + _totalCount * 3;
					uint32_t channel = (uint32_t)*p++;
					channel |= (uint32_t)(*p++) << 8;
					if ((uint32_t)_channels.size() <= channel)
						_channels.resize(channel + 1);
					_channels[channel] = (float)*p / 255.f;
					//qDebug() << __func__ << _totalCount << channel << _channels[channel];
				}
				_mutex.lock();
				_update = true;
				_mutex.unlock();
				//qDebug() << __func__ << _channels.count();
				_totalCount = 0;
				_bytes = 0;
				//qDebug() << "[DATA] Packet of" << _packet->size() << "bytes";
				update();
			}
		}
	}
}

GLuint GLWidget::loadShader(GLenum type, const QByteArray& context)
{
	GLuint shader = glCreateShader(type);
	const char *p = context.constData();
	int length = context.length();
	glShaderSource(shader, 1, &p, &length);
	glCompileShader(shader);

	int status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

	int logLength;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
	char log[logLength];
	glGetShaderInfoLog(shader, logLength, &logLength, log);
	qWarning(log);
	if (status == GL_TRUE)
		return shader;
	glDeleteShader(shader);
	return 0;
}

GLuint GLWidget::loadShaderFile(GLenum type, QString path)
{
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly)) {
		qWarning(QString("Cannot open file %1").arg(path).toLocal8Bit());
		return 0;
	}
	QByteArray context = f.readAll();
	f.close();
	return loadShader(type, context);
}
