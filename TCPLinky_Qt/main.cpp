#include <QApplication>
#include "network.h"

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	Network n;

	return a.exec();
}
