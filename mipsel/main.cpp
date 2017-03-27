#include <qpe/qpeapplication.h>
#include "mainw.h"

int main(int argc, char *argv[])
{
	QPEApplication a(argc, argv);
	MainW w;
	a.setMainWidget(&w);
	w.showMaximized();
	return a.exec();
}
