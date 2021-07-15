// ConsoleUtilities.h

#pragma once

#include <QColor>
#include <QTimer>
#include <QValidator>
#include <QDialog>
#include <QHelpEvent>

int  getDirFromFile( const char *path, char *dir );

const char *getRomFile( void );

int getFileBaseName( const char *filepath, char *base, char *suffix = NULL );

int parseFilepath( const char *filepath, char *dir, char *base, char *suffix = NULL );

int fceuExecutablePath( char *outputPath, int outputSize );

int fceuLoadConfigColor( const char *confName, QColor *color );

class fceuDecIntValidtor : public QValidator
{ 
   public:
   	fceuDecIntValidtor( int min, int max, QObject *parent);

		QValidator::State validate(QString &input, int &pos) const;

		void  setMinMax( int min, int max );
	private:
		int  min;
		int  max;
};

class fceuHexIntValidtor : public QValidator
{ 
	public:
		fceuHexIntValidtor( int min, int max, QObject *parent);

		QValidator::State validate(QString &input, int &pos) const;

		void  setMinMax( int min, int max );
	private:
		int  min;
		int  max;
};

class fceuCustomToolTip : public QDialog
{
	public:
		fceuCustomToolTip( QWidget *parent = nullptr );
		~fceuCustomToolTip( void );

	protected:
		bool eventFilter(QObject *obj, QEvent *event) override;
		void mouseMoveEvent( QMouseEvent *event ) override;

		void hideTip(void);
		void hideTipImmediately(void);
	private:
		QWidget *w;
		QTimer  *hideTimer;

		static fceuCustomToolTip *instance;

	private slots:
		void  hideTimerExpired(void);
};

QString fceuGetOpcodeToolTip( uint8_t *opcode, int size );

QDialog *fceuCustomToolTipShow( QHelpEvent *helpEvent, QDialog *popup );
