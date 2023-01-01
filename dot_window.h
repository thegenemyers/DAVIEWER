#ifndef IMAGE_WINDOW_H
#define IMAGE_WINDOW_H

#include <QtGui>
#include <QtWidgets>

extern "C" {
#include "DB.h"
#include "doter.h"
}

class DotCanvas;
class DotWindow;
class MainWindow;


/***************************************************************************************/
/*                                                                                     */
/*   VIEW CONTROL STATE & DOT PLOT MODEL                                               */
/*                                                                                     */
/***************************************************************************************/

typedef enum { TOP_LEFT = 0, TOP_RIGHT = 1, BOTTOM_LEFT = 2, BOTTOM_RIGHT = 3 } LocatorQuad;

typedef struct
{ QRect          wGeom;

  int             kmer;

  QRect           view;
  double          zoom;

  Qt::CheckState  fOn;
  QPoint          focus;
  QColor          fColor;
  Qt::CheckState  fViz;
  Qt::CheckState  sViz;
  
  QColor          lColor;
  Qt::CheckState  lViz;
  LocatorQuad     lQuad;

} DotState;

typedef struct
{ int          Alen;
  int          Blen;
  char        *Aseq;
  char        *Bseq;
  DotList     *dots;
} DotPlot;


class DotLineEdit : public QLineEdit
{
  Q_OBJECT

public:
  DotLineEdit(QWidget *parent = 0);
  void setChain(QWidget *p, QWidget *s);

signals:
  void focusOut();

protected:
  void focusOutEvent(QFocusEvent *event);
  void keyPressEvent(QKeyEvent *event);
  bool focusNextPrevChild(bool next);

private:
  QWidget *mypred;
  QWidget *mysucc;
};

class SeqLineEdit : public QLineEdit
{
  Q_OBJECT

public:
  SeqLineEdit(QWidget *parent = 0, int id = 0);

signals:
  void focusShift(int, int);

protected:
  void keyPressEvent(QKeyEvent *);
  void mousePressEvent(QMouseEvent *);
  void mouseMoveEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);
  void enterEvent(QEvent *);
  void leaveEvent(QEvent *);

private:
  int  myid;
};


/***************************************************************************************/
/*                                                                                     */
/*   DOT CANVAS                                                                        */
/*                                                                                     */
/***************************************************************************************/

class DotCanvas : public QWidget
{
  Q_OBJECT

public:
  DotCanvas(QWidget *parent = 0);
  ~DotCanvas();

  Frame *shareData(DotState *state, DotPlot *plot);
  bool   zoomView(double zoomDel);
  void   resetView();
  bool   viewToFrame();

  static QVector<QRgb> ctable;

signals:
  void NewFrame(double newZ);
  void NewFocus();
  void Moved();

protected:
  void resizeEvent(QResizeEvent *event);

  void mousePressEvent(QMouseEvent *);
  void mouseMoveEvent(QMouseEvent *);
  void mouseReleaseEvent(QMouseEvent *);

  void keyPressEvent(QKeyEvent *);
  void keyReleaseEvent(QKeyEvent *);

  void paintEvent(QPaintEvent *event);

  void enterEvent(QEvent *);
  void leaveEvent(QEvent *);

private:
  bool         noFrame;
  Frame        frame;

  int          rectW;
  int          rectH;

  QImage      *image;
  uchar      **raster;

  DotPlot     *plot;
  DotState    *state;

  int          mouseX;
  int          mouseY;
  double       scaleX;
  double       scaleY;

  bool         select;
  bool         nograb;

  QBasicTimer *timer;
  int          xpos;
  int          ypos;
  int          vwid;
  int          vhgt;

  QRubberBand *rubber;
};


/***************************************************************************************/
/*                                                                                     */
/*   DOT WINDOW DATA TYPE                                                              */
/*                                                                                     */
/***************************************************************************************/

class DotWindow : public QMainWindow
{
  Q_OBJECT

public:
  DotWindow(QString title, int alen, char *aseq, int blen, char *bseq);
  ~DotWindow();

  typedef enum { INFORM, WARNING, ERROR } MessageKind;

  static int warning(const QString& message, QWidget *parent, MessageKind,
                     const QString& label1 = QString(),
                     const QString& label2 = QString(),
                     const QString& label3 = QString() );

public slots:
  void kmerChange();

  void zoomUp();
  void zoomDown();
  void zoomOut();
  void zoomTo();

  void viewChange();
 
  void focusOnChange();
  void focusChange();
  void focusColorChange();
  void hairsChange();
  void seqChange();
  void seqRealign();

  void locatorChange();
  void locatorColorChange();

protected:
  void closeEvent(QCloseEvent *event);

public slots:
  void   frameToView(double newZ);
  void   clickToFocus();
  void   seqMove(int,int);

private slots:
  void fullScreen();
  void pushState();
  bool checkRangeA();
  bool checkRangeB();
  bool checkFocus();
  bool checkPair(QLineEdit *w, int &a, int &b, char del);

private:
  static DotState     lastState;
  static bool         firstTime;

  DotState            state;
  DotPlot             plot;
  Frame              *frame;
  QImage             *pixels;

  void                createMenus();
  void                createActions();

  DotCanvas          *canvas;

  SeqLineEdit        *seqUL;
  SeqLineEdit        *seqUR;
  SeqLineEdit        *seqLL;
  SeqLineEdit        *seqLR;

  QAction            *fullScreenAct;
  QAction            *minimizeAct;
  QAction            *zoomInAct;
  QAction            *zoomOutAct;

  DotLineEdit        *zoomEdit;

  QSpinBox           *kmerSpin;

  DotLineEdit        *Arng;
  DotLineEdit        *Brng;
  QPoint              Aval;
  QPoint              Bval;

  QCheckBox          *focusOn;
  DotLineEdit        *Fpnt;
  QPoint              Fval;
  QColor              focusColor;
  QToolButton        *focusBox;
  QCheckBox          *focusCheck;
  QCheckBox          *seqCheck;

  QColor              locatorColor;
  QToolButton        *locatorBox;
  QCheckBox          *locatorCheck;
  QButtonGroup       *locatorQuad;

  QVBoxLayout        *controlLayout;
  QWidget            *controlPart;
};

#endif
