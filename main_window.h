#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QtGui>
#include <QtWidgets>

extern "C" {
#include "piler.h"
#include "DB.h"
}

#define SIZE_TICKS 100
#define MAX_TRACKS 100

class MyLineEdit : public QLineEdit
{
  Q_OBJECT

public:
  MyLineEdit(QWidget *parent = 0);

  void processed(bool on);

signals:
  void touched();
  void focusOut();

protected:
  void keyPressEvent(QKeyEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void focusOutEvent(QFocusEvent *event);

private:
  bool process;
};

class MyMenu : public QMenu
{
  Q_OBJECT

public:
  MyMenu(QWidget *parent = 0);

protected:
  void mouseReleaseEvent(QMouseEvent *ev);
};

class MyCanvas : public QWidget
{
  Q_OBJECT

public:
  MyCanvas(QWidget *parent = 0);

  static int rulerHeight;
  static int rulerWidth;
  static int labelWidth;
  static int rulerT1, rulerT2;

  static int digits(int a, int b);
  static int mapToA(int mode, LA *align, uint8 *trace, int tspace, int bp, int lim); 
  static int find_pile(int beg, Pile *a, int l, int r);
  static int find_mask(int beg, int *a, int l, int r);

  void haloUpdate(bool on);
  void setColor(QColor &color, int read);

protected:
  void paintEvent(QPaintEvent *event);
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);
  void mouseReleaseEvent(QMouseEvent *event);

private slots:
  void assignColor();
  // void showAlignment();

private:
  int  pick(int x, int y, int &aread, int &bread);

  MyMenu  *popup;
  MyMenu  *annup;
  QAction *aline;
  QAction *bline;
  // QAction *alignAct;
  QAction *colorAct;
  QAction *mline;

  bool   doHalo;
  int    haloed;
  bool   avail[DB_QV+1];
  QColor colors[DB_QV+1];

  bool        buttonDown;
  int         mouseX;
  int         mouseY;
  double      hscale;
  double      vscale;
  QScrollBar *vbar;
  QScrollBar *hbar;
};


typedef struct
  { int hzPos;
    int hsPos;
    int hsPage;
    int hsMax;
    int hmax;
    int vzPos;
    int vsPos;
    int vsPage;
    int vsMax;
    int vmax;
  } Scroll_State;

class MyScroll : public QWidget
{
  Q_OBJECT

  friend class MyCanvas;

public:
  MyScroll(QWidget *parent = 0);

  void setRange(int h, int v);

  void hsToRange(int beg, int end);

  void haloUpdate(bool on);

  void getState(Scroll_State &state);
  void putState(Scroll_State &state);

  void setColor(QColor &color, int read);

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
  MyCanvas   *mycanvas;

  int vmax, vstep[SIZE_TICKS+1];
  int hmax, hstep[SIZE_TICKS+1];
};

typedef struct
  { QColor backColor;
    QColor readColor;
    QColor alignColor;
    QColor branchColor;

    bool   showGrid;
    bool   showHalo;
    bool   showElim;
    int    drawElim;
    QColor gridColor;
    QColor haloColor;
    QColor elimColor;

    bool   bridges;
    bool   overlaps;
    int    stretchMax;
    int    compressMax;
    QColor stretchColor;
    QColor neutralColor;
    QColor compressColor;

    bool   matchVis;
    bool   matchqv;
    int    matchMode;
    QColor matchColor[10];
    QColor matchHue[3];
    int    matchGood;
    int    matchBad;

    bool   qualVis;
    bool   qualqv;
    int    qualMode;
    QColor qualColor[10];
    QColor qualHue[3];
    int    qualGood;
    int    qualBad;
    bool   qualonB;

    bool   profVis;
    bool   profqv;
    int    profMode;
    QColor profColor[5];
    QColor profHue[3];
    int    profLow;
    int    profHgh;

    int       nmasks;
    bool        showTrack[MAX_TRACKS];
    QColor      trackColor[MAX_TRACKS];
    DAZZ_TRACK *track[MAX_TRACKS];
    int         Arail[MAX_TRACKS];
    bool        showonB[MAX_TRACKS];
  } Palette_State;

class TrackWidget : public QWidget
{
  Q_OBJECT

public:
  TrackWidget(QWidget *parent = 0);

protected:
  void mousePressEvent(QMouseEvent *ev);
  void dragEnterEvent(QDragEnterEvent *ev);
  void dragMoveEvent(QDragMoveEvent *ev);
  void dropEvent(QDropEvent *ev);

private:
  int dropY;
};

class PaletteDialog : public QDialog
{
  Q_OBJECT

public:
  PaletteDialog(QWidget *parent = 0);

  void getState(Palette_State &state);
  void putState(Palette_State &state);

  void setView();
  int  getView();

  void readAndApplySettings(QSettings &);
  void writeSettings(QSettings &);

  void writeView(Palette_State &, QString &);
  bool readView(Palette_State &, QString &);

  void restoreLayout();
  int  loadTracks(Palette_State &, DataModel *);
  int  liveCount();

  void symmetricDB(bool);

private slots:
  void backChange();
  void readChange();
  void alignChange();
  void branchChange();
  void gridChange();
  void haloChange();
  void elimChange();
  void stretchChange();
  void neutralChange();
  void compressChange();
  void matchRampChange();
  void matchTriChange();
  void qualRampChange();
  void qualTriChange();
  void profRampChange();
  void profTriChange();
  void trackChange();

  void stretchCheck();
  void compressCheck();
  void matchGoodCheck();
  void matchBadCheck();
  void qualGoodCheck();
  void qualBadCheck();
  void profLowCheck();
  void profHghCheck();

  void activateGrid(int);
  void activateHalo(int);
  void activateElim(int);

  void activateElastic(int);
  void drawLeftDash(const QColor &);
  void drawRightDash(const QColor &);

  void activateMatchQV(int);
  void activateQualQV(int);
  void activateRepProfile(int);
  void enforceMatchOff(int);
  void activateTracks(int);

  void addView();
  void updateView();
  void deleteView();
  void changeView(int);

private:
  QToolButton *backBox;
    QColor backColor;
  QToolButton *readBox;
    QColor readColor;
  QToolButton *alignBox;
    QColor alignColor;
  QToolButton *branchBox;
    QColor branchColor;

  QCheckBox *gridCheck;
    QLabel      *gridLabel;
    QToolButton *gridBox;
    QColor       gridColor;

  QCheckBox *haloCheck;
    QLabel      *haloLabel;
    QToolButton *haloBox;
    QColor       haloColor;

  QCheckBox *elimCheck;
    QLabel      *elimLabel;
    QToolButton *elimBox;
    QColor       elimColor;

  QCheckBox *bridgeCheck;
  QCheckBox *overlapCheck;
    QLabel      *bridgeLabel;
    QLabel      *overLabel;
    QLabel      *leftLabel;
    QLabel      *leftDash;
    QLabel      *neutralLabel;
    QLabel      *rightLabel;
    QLabel      *rightDash;
    QToolButton *stretchBox;
    QToolButton *neutralBox;
    QToolButton *compressBox;
    QColor       stretchColor;
    QColor       neutralColor;
    QColor       compressColor;
    QLineEdit   *maxStretch;
    QLineEdit   *maxCompress;
    int          stretchMax;
    int          compressMax;

  QCheckBox *matchCheck;
    QLabel         *matchLabel;
    QLabel         *matchLabelScale;
    QRadioButton   *matchRadioScale;
    QLabel         *matchLabelTri;
    QRadioButton   *matchRadioTri;
    QStackedLayout *matchStack;
    QToolButton    *matchBox[10];
    QColor          matchColor[10];
    QToolButton    *matchLev[3];
    QColor          matchHue[3];
    QLabel         *matchLevLabel[3];
    QLineEdit      *matchBot;
    QLineEdit      *matchTop;
    int             matchGood;
    int             matchBad;

  QWidget *qualPanel;
  bool     qualVis;
  QCheckBox      *qualCheck;
    QLabel         *qualLabel;
    QLabel         *qualLabelScale;
    QRadioButton   *qualRadioScale;
    QLabel         *qualLabelTri;
    QRadioButton   *qualRadioTri;
    QStackedLayout *qualStack;
    QToolButton    *qualBox[10];
    QColor          qualColor[10];
    QToolButton    *qualLev[3];
    QColor          qualHue[3];
    QLabel         *qualLevLabel[3];
    QLineEdit      *qualBot;
    QLineEdit      *qualTop;
    int             qualGood;
    int             qualBad;
    QCheckBox      *qualonB;

  QWidget *profPanel;
  bool     profVis;
  QCheckBox      *profCheck;
    QLabel         *profLabel;
    QLabel         *profLabelScale;
    QRadioButton   *profRadioScale;
    QLabel         *profLabelTri;
    QRadioButton   *profRadioTri;
    QStackedLayout *profStack;
    QToolButton    *profBox[5];
    QColor          profColor[5];
    QToolButton    *profLev[3];
    QColor          profHue[3];
    QLabel         *profLevLabel[3];
    QLineEdit      *profBot;
    QLineEdit      *profTop;
    int             profLow;
    int             profHgh;

  TrackWidget *maskPanel;
  int          nmasks;
  QWidget   *trackPanel[MAX_TRACKS];
    bool         trackVis[MAX_TRACKS];
    QCheckBox   *trackCheck[MAX_TRACKS];
    QLabel      *trackLabel[MAX_TRACKS];
    QToolButton *trackBox[MAX_TRACKS];
    QColor       trackColor[MAX_TRACKS];
    DAZZ_TRACK  *track[MAX_TRACKS];
    QSpinBox    *trackApos[MAX_TRACKS];
    QCheckBox   *trackonB[MAX_TRACKS];

  QComboBox *viewList;
};

typedef struct
  { QFileInfo *lasInfo;
    QFileInfo *AInfo;
    QFileInfo *BInfo;
    QString    lasText;
    QString    AText;
    bool       asym;
    QString    BText;
    bool       subrange;
    int        first;
    int        last;
  } Open_State;

class OpenDialog : public QDialog
{
  Q_OBJECT

public:
  OpenDialog(QWidget *parent = 0);

  bool openDataSet(int link, int laps, int elim, int comp, int expn);

  void getState(Open_State &state);
  void putState(Open_State &state);

  void setView();
  int  getView();

  void readAndApplySettings(QSettings &);
  void writeSettings(QSettings &);

private slots:
  void activateB(int);
  void activateSubset(int);

  void openLAS();
  void openADB();
  void openBDB();

  void lasCheck();
  void ACheck();
  void BCheck();

  void firstCheck();
  void lastCheck();

  void aboutTo();

private:
  QCheckBox   *BBox;
    QLabel      *BLabel;
    QPushButton *BSelect;

  QCheckBox   *rBox;
    QLabel      *rLabel;
    QLabel      *from;
    QLineEdit   *beg;
    QLabel      *to;
    QLineEdit   *end;

  QPushButton *open;
  QPushButton *cancel;

  QLineEdit   *lasFile;
  QLineEdit   *AFile;
  QLineEdit   *BFile;

  QFileInfo *lasInfo;
  QFileInfo *AInfo;
  QFileInfo *BInfo;

  QString    lasText;
  QString    AText;
  QString    BText;

  int       first, last;

  QComboBox *viewList;
};

class MainWindow : public QMainWindow
{
  Q_OBJECT

public:
  MainWindow(MainWindow *origin);

  typedef enum { INFORM, WARNING, ERROR } MessageKind;

  static int warning(const QString& message, QWidget *parent, MessageKind,
                     const QString& label1 = QString(),
                     const QString& label2 = QString(),
                     const QString& label3 = QString() );

  static QRect *screenGeometry;
  static int    frameWidth;
  static int    frameHeight;

  static QList<MainWindow *> frames;
  static Palette_State       palette;
  static Open_State          dataset;
  static int                 numLive;

  static QStringList views;
  static int         cview;

protected:
  void closeEvent(QCloseEvent *event);

private slots:
  void openFiles();
  void openPalette();
  void openCopy();
  void closeAll();

  void toggleToolBar();
  void toggleQuery();
  void tileImages();
  void cascadeImages();

  void fullScreen();
  void unminimizeAll();
  void raiseAll();

  void query();
  void clearMesg();

private:
  OpenDialog    *openDialog;
  PaletteDialog *paletteDialog;

  void createMenus();
  void createActions();
  void createToolBars();

  void readAndApplySettings();
  void writeSettings();

  QAction *exitAct;

  QAction *openAct;
  QAction *paletteAct;
  QAction *copyAct;
  QAction *toolAct;
  QAction *queryAct;

  QAction *tileAct;
  QAction *cascadeAct;

  QAction *fullScreenAct;
  QAction *minimizeAct;
  QAction *unminimizeAllAct;
  QAction *raiseAllAct;

  QToolBar        *fileToolBar;
  Qt::ToolBarArea  toolArea;

  MyScroll *myscroll;

  MyLineEdit  *queryEntry;
  QLineEdit   *queryMesg;

  QFrame      *hbar;
  QWidget     *queryPanel;
  QVBoxLayout *drawPanel;
};

#endif
