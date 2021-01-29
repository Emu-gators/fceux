// NameTableViewer.h

#pragma once

#include <QWidget>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QRadioButton>
#include <QLabel>
#include <QFrame>
#include <QTimer>
#include <QSlider>
#include <QLineEdit>
#include <QGroupBox>
#include <QScrollArea>
#include <QCloseEvent>

struct  ppuNameTablePixel_t
{
	QColor color;
};

struct ppuNameTableTile_t
{
	struct ppuNameTablePixel_t pixel[8][8];

	int  x;
	int  y;
};

struct ppuNameTable_t
{
	struct ppuNameTableTile_t tile[30][32];

	int  x;
	int  y;
	int  w;
	int  h;
};

class ppuNameTableViewerDialog_t;

class ppuNameTableView_t : public QWidget
{
	Q_OBJECT

	public:
		ppuNameTableView_t( QWidget *parent = 0);
		~ppuNameTableView_t(void);

		void setViewScale( int reqScale );
		int  getViewScale( void ){ return viewScale; };

		void setScrollPointer( QScrollArea *sa );

		QColor tileSelColor;
		QColor tileGridColor;
		QColor attrGridColor;
	protected:
		void paintEvent(QPaintEvent *event);
		void resizeEvent(QResizeEvent *event);
		void keyPressEvent(QKeyEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mousePressEvent(QMouseEvent * event);
		void computeNameTableProperties( int NameTable, int TileX, int TileY );
		int  convertXY2TableTile( int x, int y, int *tableIdxOut, int *tileXout, int *tileYout );
		int  calcTableTileAddr( int table, int tileX, int tileY );

		ppuNameTableViewerDialog_t *parent;
		int viewWidth;
		int viewHeight;
		int viewScale;
		int selTable;
		QPoint selTile;
		QPoint selTileLoc;
		QRect  viewRect;
		QScrollArea  *scrollArea;

		bool  ensureVis;
};

class ppuNameTableViewerDialog_t : public QDialog
{
   Q_OBJECT

	public:
		ppuNameTableViewerDialog_t(QWidget *parent = 0);
		~ppuNameTableViewerDialog_t(void);

		void setPropertyLabels( int TileID, int TileX, int TileY, int NameTable, int PPUAddress, int AttAddress, int Attrib, int palAddr );
	protected:
		void closeEvent(QCloseEvent *bar);

		void openColorPicker( QColor *c );
		void changeRate( int divider );

		ppuNameTableView_t *ntView;
		QScrollArea *scrollArea;
		QCheckBox *showScrollLineCbox;
		QCheckBox *showTileGridCbox;
		QCheckBox *showAttrGridCbox;
		QCheckBox *showAttrbCbox;
		QCheckBox *ignorePaletteCbox;
		QLineEdit *scanLineEdit;
		QTimer    *updateTimer;
		QLineEdit *ppuAddrLbl;
		QLineEdit *nameTableLbl;
		QLineEdit *tileLocLbl;
		QLineEdit *tileIdxLbl;
		QLineEdit *tileAddrLbl;
		QLineEdit *attrDataLbl;
		QLineEdit *attrAddrLbl;
		QLineEdit *palAddrLbl;
		QAction *showScrollLineAct;
		QAction *showTileGridAct;
		QAction *showAttributesAct;
		QAction *ignPalAct;
		QLabel  *mirrorLbl;

	public slots:
		void closeWindow(void);
	private slots:
		void periodicUpdate(void);
		void updateMirrorText(void);
		void updateVisibility(void);
		void showAttrbChanged(int state);
		void ignorePaletteChanged(int state);
		void showTileGridChanged(int state);
		void showAttrGridChanged(int state);
		void showScrollLinesChanged(int state);
		void scanLineChanged( const QString &txt );
		void menuScrollLinesChanged( bool checked ); 
		void menuGridLinesChanged( bool checked ); 
		void menuAttributesChanged( bool checked );
		void menuIgnPalChanged( bool checked );
		void setTileSelectorColor(void);
		void setTileGridColor(void);
		void setAttrGridColor(void);
		void changeZoom1x(void);
		void changeZoom2x(void);
		void changeZoom3x(void);
		void changeZoom4x(void);
		void changeRate1(void);
		void changeRate2(void);
		void changeRate4(void);
		void changeRate8(void);
		void changeRate16(void);
		void forceRefresh(void);
};

int openNameTableViewWindow( QWidget *parent );

