#include "network.h"
#include <QtDebug>
#include <QHostAddress>
#include <QThread>
#include "glwidget.h"

Network::Network(QObject *parent) :
	QObject(parent)
{
	qApp->setQuitOnLastWindowClosed(false);
	server = new QTcpServer(this);
	if (!server->listen(QHostAddress::Any, 12345))
		qFatal("Cannot listen on port 12345!");
	connect(server, SIGNAL(newConnection()), this, SLOT(newConnection()));
	qDebug() << "[INFO] Server started, port 12345";
}

Network::~Network(void)
{
	qDebug() << "[INFO] Stopping the server...";
	foreach (QObject *handler, handlers) {
		qDebug() << "[HANDLE]" << handler << "deleted";
		delete handler;
	}
}

void Network::newConnection(void)
{
	QTcpSocket *socket = server->nextPendingConnection();
	qDebug() << "[SOCKET]" << socket << "connected";
	GLWidget *glw = new GLWidget(socket);
	glw->show();
}
