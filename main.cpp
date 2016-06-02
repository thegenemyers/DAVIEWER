#include <unistd.h>
#include <sys/types.h>

#include <QtGui>

#ifdef Q_WS_MAC
#include <QMacStyle>
#endif

#include "main_window.h"

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

  MainWindow::frames.clear();

  MainWindow *main = new MainWindow(NULL);
  main->raise();
  main->show();

  return app.exec();
}
