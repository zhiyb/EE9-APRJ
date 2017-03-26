#ifndef GLWIDGET_H
#define GLWIDGET_H

#include <QtWidgets>
#include <QOpenGLFunctions_3_3_Core>
#include <QTcpSocket>

class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core
{
	Q_OBJECT
public:
	explicit GLWidget(QTcpSocket *socket, QWidget *parent = 0);

protected:
	void initializeGL();
	void resizeGL(int w, int h);
	void paintGL();
	void closeEvent(QCloseEvent *event);
	void wheelEvent(QWheelEvent *e);
	void mousePressEvent(QMouseEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void keyPressEvent(QKeyEvent *e);

private slots:
	void disconnected();
	void readData();

private:
	void render();
	void loadShaders(QString file);
	GLuint loadShader(GLenum type, const QByteArray& context);
	GLuint loadShaderFile(GLenum type, QString path);

	QTcpSocket *_socket;
	uint32_t _bytes, _payloadSize;
	uint16_t _chCount;
	uint8_t _command, _stream;

	QVector<GLuint> _channels;
	QAtomicInt _update;

	struct data_t {
		struct loc_t {
			GLint projection, dimension;
			GLint vertex, intensity;
			GLint zoom, move;
		} loc;
		GLuint program, vsh, fsh;
		GLuint vao, bVertex, bIntensity;
		GLfloat zoom;
		GLfloat moveX, moveY;
		QPoint prevPos;
		QMatrix4x4 projection;
		bool pause;
	} data;

	struct stat_t {
		uint32_t pTotal;
		uint64_t cTotal;
		uint32_t uTotal;
		uint32_t maxSize, maxUpdates;
	} stat;
};

#endif // GLWIDGET_H
