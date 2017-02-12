#ifndef NETWORK_H
#define NETWORK_H

#include <QObject>
#include <QTcpServer>
#include <QFile>
#include <QTextStream>

class Network : public QObject
{
	Q_OBJECT
	friend class InputHandle;
public:
	explicit Network(QObject *parent = 0);
	~Network(void);

signals:

public slots:

private slots:
	void newConnection(void);

private:
	QTcpServer *server;
	QList<QObject *> handlers;
};

#endif // NETWORK_H
