#ifndef PROF_WINDOW_H
#define PROF_WINDOW_H

#include <QtGui>
#include <QtWidgets>

extern "C" {
#include "libfastk.h"
#include "piler.h"
#include "DB.h"
}

class MainWindow;

#define SIZE_TICKS 100

typedef struct
{ int     alen;
  char   *aseq;
  int     kmer;
  int     plen;
  uint16 *cnts;
} Profile;

typedef struct
{ QRect          wGeom;
} ProfState;

class ProfCanvas : public QWidget
{
  Q_OBJECT

public:
  ProfCanvas(Profile *pro, QWidget *parent = 0);

protected:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);

private slots:

private:
  int  pick(int x, int y);

  Profile    *prof;

  bool        buttonDown;
  int         mouseX;
  int         mouseY;

  double      hscale;
  double      vscale;
  QScrollBar *vbar;
  QScrollBar *hbar;
};

class ProfScroll : public QWidget
{
  Q_OBJECT

  friend class ProfCanvas;

public:
  ProfScroll(Profile *pro, QWidget *parent = 0);

  void setRange(int h, int v);

  void hsToRange(int beg, int end);

private slots:
  void vsValue(int);
  void vsSize(int);
  void hsValue(int);
  void hsSize(int);

private:
  QScrollBar *vertScroll;
  QScrollBar *horzScroll;
  QSlider    *vertZoom;
  QSlider    *horzZoom;
  ProfCanvas *profcanvas;

  int vsize;
  int vmax, vstep[SIZE_TICKS+1];
  int hmax, hstep[SIZE_TICKS+1];
};

class ProfWindow : public QMainWindow
{
  Q_OBJECT

public:
  ProfWindow(QString title, int alen, char *aseq, int plen, uint16 *cnts);
  ~ProfWindow();

  static QRect *screenGeometry;
  static int    frameWidth;
  static int    frameHeight;

protected:
  void closeEvent(QCloseEvent *event);

private slots:
  void fullScreen();
  void pushState();

private:
  static ProfState  lastState;
  static bool       firstTime;

  ProfState state;
  Profile   prof;

  void createActions();
  void createMenus();

  QAction *fullScreenAct;
  QAction *minimizeAct;

  ProfScroll *profscroll;

  QVBoxLayout *drawPanel;
};

#endif
