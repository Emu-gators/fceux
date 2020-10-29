// TraceLogger.cpp
//
#include <stdio.h>

#include <QDir>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QPainter>

#include "../../types.h"
#include "../../fceu.h"
#include "../../cart.h"
#include "../../x6502.h"
#include "../../debug.h"
#include "../../asm.h"
#include "../../ppu.h"
#include "../../ines.h"
#include "../../nsf.h"
#include "../../movie.h"

#include "Qt/ConsoleUtilities.h"
#include "Qt/TraceLogger.h"
#include "Qt/main.h"
#include "Qt/dface.h"
#include "Qt/input.h"
#include "Qt/config.h"
#include "Qt/SymbolicDebug.h"
#include "Qt/fceuWrapper.h"

#define LOG_REGISTERS           0x00000001
#define LOG_PROCESSOR_STATUS    0x00000002
#define LOG_NEW_INSTRUCTIONS    0x00000004
#define LOG_NEW_DATA            0x00000008
#define LOG_TO_THE_LEFT         0x00000010
#define LOG_FRAMES_COUNT        0x00000020
#define LOG_MESSAGES            0x00000040
#define LOG_BREAKPOINTS         0x00000080
#define LOG_SYMBOLIC            0x00000100
#define LOG_CODE_TABBING        0x00000200
#define LOG_CYCLES_COUNT        0x00000400
#define LOG_INSTRUCTIONS_COUNT  0x00000800
#define LOG_BANK_NUMBER         0x00001000

#define LOG_LINE_MAX_LEN 160
// Frames count - 1+6+1 symbols
// Cycles count - 1+11+1 symbols
// Instructions count - 1+11+1 symbols
// AXYS state - 20
// Processor status - 11
// Tabs - 31
// Address - 6
// Data - 10
// Disassembly - 45
// EOL (/0) - 1
// ------------------------
// 148 symbols total
#define LOG_AXYSTATE_MAX_LEN 21
#define LOG_PROCSTATUS_MAX_LEN 12
#define LOG_TABS_MASK 31
#define LOG_ADDRESS_MAX_LEN 13
#define LOG_DATA_MAX_LEN 11
#define LOG_DISASSEMBLY_MAX_LEN 46
#define NL_MAX_MULTILINE_COMMENT_LEN 1000

static int logging = 0;
static int logging_options = LOG_REGISTERS | LOG_PROCESSOR_STATUS | LOG_TO_THE_LEFT | LOG_MESSAGES | LOG_BREAKPOINTS | LOG_CODE_TABBING;
static int oldcodecount = 0, olddatacount = 0;

static traceRecord_t  *recBuf = NULL;
static int  recBufMax  = 0;
static int  recBufHead = 0;
static int  recBufTail = 0;
static FILE *logFile   = NULL;
static TraceLoggerDialog_t *traceLogWindow =  NULL;
static void pushMsgToLogBuffer( const char *msg );
//----------------------------------------------------
TraceLoggerDialog_t::TraceLoggerDialog_t(QWidget *parent)
	: QDialog( parent, Qt::Window )
{
	QVBoxLayout *mainLayout;
	QHBoxLayout *hbox;
	QGridLayout *grid;
	QGroupBox *frame;
	QLabel    *lbl;

   if ( recBufMax == 0 )
   {
      initTraceLogBuffer( 1000000 );
   }

   setWindowTitle( tr("Trace Logger") );

	mainLayout   = new QVBoxLayout();
	grid         = new QGridLayout();
	mainLayout->addLayout( grid, 100 );

	traceView = new QTraceLogView(this);
	vbar      = new QScrollBar( Qt::Vertical, this );
	hbar      = new QScrollBar( Qt::Horizontal, this );

   connect( hbar, SIGNAL(valueChanged(int)), this, SLOT(hbarChanged(int)) );
   connect( vbar, SIGNAL(valueChanged(int)), this, SLOT(vbarChanged(int)) );

	traceView->setScrollBars( hbar, vbar );
	hbar->setMinimum(0);
	hbar->setMaximum(100);
	vbar->setMinimum(0);
	vbar->setMaximum(recBufMax);
   vbar->setValue(recBufMax);

	grid->addWidget( traceView, 0, 0);
	grid->addWidget( vbar     , 0, 1 );
	grid->addWidget( hbar     , 1, 0 );

	grid         = new QGridLayout();
	mainLayout->addLayout( grid, 1 );

	lbl = new QLabel( tr("Lines") );
	logLastCbox  = new QCheckBox( tr("Log Last") );
	logMaxLinesComboBox = new QComboBox();

	logLastCbox->setChecked(true);
	logMaxLinesComboBox->addItem( tr("3,000,000"), 3000000 );
	logMaxLinesComboBox->addItem( tr("1,000,000"), 1000000 );
	logMaxLinesComboBox->addItem( tr("300,000")  , 300000 );
	logMaxLinesComboBox->addItem( tr("100,000")  , 100000 );
	logMaxLinesComboBox->addItem( tr("30,000")   , 30000 );
	logMaxLinesComboBox->addItem( tr("10,000")   , 10000 );
	logMaxLinesComboBox->addItem( tr("3,000")    , 3000 );
	logMaxLinesComboBox->addItem( tr("1,000")    , 1000 );

	for (int i=0; i<logMaxLinesComboBox->count(); i++)
	{
		if ( logMaxLinesComboBox->itemData(i).toInt() == recBufMax )
		{
			logMaxLinesComboBox->setCurrentIndex( i );
		}
	}
	connect( logMaxLinesComboBox, SIGNAL(activated(int)), this, SLOT(logMaxLinesChanged(int)) );

	logFileCbox  = new QCheckBox( tr("Log to File") );
	selLogFileButton = new QPushButton( tr("Browse...") );
	startStopButton = new QPushButton( tr("Start Logging") );
	autoUpdateCbox = new QCheckBox( tr("Automatically update this window while logging") );

	autoUpdateCbox->setChecked(true);

	if ( logging )
	{
		startStopButton->setText( tr("Stop Logging") );
	}
	connect( startStopButton, SIGNAL(clicked(void)), this, SLOT(toggleLoggingOnOff(void)) );
   connect( selLogFileButton, SIGNAL(clicked(void)), this, SLOT(openLogFile(void)) );

	hbox = new QHBoxLayout();
	hbox->addWidget( logLastCbox );
	hbox->addWidget( logMaxLinesComboBox );
	hbox->addWidget( lbl );

	grid->addLayout( hbox, 0, 0, Qt::AlignLeft );
	grid->addWidget( startStopButton, 0, 1, Qt::AlignCenter );

	hbox = new QHBoxLayout();
	hbox->addWidget( logFileCbox );
	hbox->addWidget( selLogFileButton );

	grid->addLayout( hbox, 1, 0, Qt::AlignLeft );
	grid->addWidget( autoUpdateCbox, 1, 1, Qt::AlignCenter );

	grid  = new QGridLayout();
	frame = new QGroupBox(tr("Log Options"));
	frame->setLayout( grid );

	logRegCbox            = new QCheckBox( tr("Log State of Registers") );
	logFrameCbox          = new QCheckBox( tr("Log Frames Count") );
	logEmuMsgCbox         = new QCheckBox( tr("Log Emulator Messages") );
	symTraceEnaCbox       = new QCheckBox( tr("Symbolic Trace") );
	logProcStatFlagCbox   = new QCheckBox( tr("Log Processor Status Flags") );
	logCyclesCountCbox    = new QCheckBox( tr("Log Cycles Count") );
	logBreakpointCbox     = new QCheckBox( tr("Log Breakpoint Hits") );
	useStackPointerCbox   = new QCheckBox( tr("Use Stack Pointer for Code Tabbing (Nesting Visualization)") );
	toLeftDisassemblyCbox = new QCheckBox( tr("To the Left from Disassembly") );
	logInstrCountCbox     = new QCheckBox( tr("Log Instructions Count") );
	logBankNumCbox        = new QCheckBox( tr("Log Bank Number") );

   logRegCbox->setChecked( (logging_options & LOG_REGISTERS) ? true : false );
   logFrameCbox->setChecked( (logging_options & LOG_FRAMES_COUNT) ? true : false );
   logEmuMsgCbox->setChecked( (logging_options & LOG_MESSAGES) ? true : false );
   symTraceEnaCbox->setChecked( (logging_options & LOG_SYMBOLIC) ? true : false );
   logProcStatFlagCbox->setChecked( (logging_options & LOG_PROCESSOR_STATUS) ? true : false );
   logCyclesCountCbox->setChecked( (logging_options & LOG_CYCLES_COUNT) ? true : false );
   logBreakpointCbox->setChecked( (logging_options & LOG_BREAKPOINTS) ? true : false );
   useStackPointerCbox->setChecked( (logging_options & LOG_CODE_TABBING) ? true : false );
   toLeftDisassemblyCbox->setChecked( (logging_options & LOG_TO_THE_LEFT) ? true : false );
   logInstrCountCbox->setChecked( (logging_options & LOG_INSTRUCTIONS_COUNT) ? true : false );
   logBankNumCbox->setChecked( (logging_options & LOG_BANK_NUMBER) ? true : false );

   connect( logRegCbox, SIGNAL(stateChanged(int)), this, SLOT(logRegStateChanged(int)) );
   connect( logFrameCbox, SIGNAL(stateChanged(int)), this, SLOT(logFrameStateChanged(int)) );
   connect( logEmuMsgCbox, SIGNAL(stateChanged(int)), this, SLOT(logEmuMsgStateChanged(int)) );
   connect( symTraceEnaCbox, SIGNAL(stateChanged(int)), this, SLOT(symTraceEnaStateChanged(int)) );
   connect( logProcStatFlagCbox, SIGNAL(stateChanged(int)), this, SLOT(logProcStatFlagStateChanged(int)) );
   connect( logCyclesCountCbox, SIGNAL(stateChanged(int)), this, SLOT(logCyclesCountStateChanged(int)) );
   connect( logBreakpointCbox, SIGNAL(stateChanged(int)), this, SLOT(logBreakpointStateChanged(int)) );
   connect( useStackPointerCbox, SIGNAL(stateChanged(int)), this, SLOT(useStackPointerStateChanged(int)) );
   connect( toLeftDisassemblyCbox, SIGNAL(stateChanged(int)), this, SLOT(toLeftDisassemblyStateChanged(int)) );
   connect( logInstrCountCbox, SIGNAL(stateChanged(int)), this, SLOT(logInstrCountStateChanged(int)) );
   connect( logBankNumCbox, SIGNAL(stateChanged(int)), this, SLOT(logBankNumStateChanged(int)) );

	grid->addWidget( logRegCbox     , 0, 0, Qt::AlignLeft );
	grid->addWidget( logFrameCbox   , 1, 0, Qt::AlignLeft );
	grid->addWidget( logEmuMsgCbox  , 2, 0, Qt::AlignLeft );
	grid->addWidget( symTraceEnaCbox, 3, 0, Qt::AlignLeft );
	grid->addWidget( logProcStatFlagCbox, 0, 1, Qt::AlignLeft );
	grid->addWidget( logCyclesCountCbox , 1, 1, Qt::AlignLeft );
	grid->addWidget( logBreakpointCbox  , 2, 1, Qt::AlignLeft );
	grid->addWidget( useStackPointerCbox, 3, 1, 1, 2, Qt::AlignLeft );
	grid->addWidget( toLeftDisassemblyCbox, 0, 2, Qt::AlignLeft );
	grid->addWidget( logInstrCountCbox    , 1, 2, Qt::AlignLeft );
	grid->addWidget( logBankNumCbox       , 2, 2, Qt::AlignLeft );

	mainLayout->addWidget( frame, 1 );

	grid  = new QGridLayout();
	frame = new QGroupBox(tr("Extra Log Options that work with the Code/Data Logger"));
	frame->setLayout( grid );

	logNewMapCodeCbox = new QCheckBox( tr("Only Log Newly Mapped Code") );
	logNewMapDataCbox = new QCheckBox( tr("Only Log that Accesses Newly Mapped Data") );

   logNewMapCodeCbox->setChecked( (logging_options & LOG_NEW_INSTRUCTIONS) ? true : false );
   logNewMapDataCbox->setChecked( (logging_options & LOG_NEW_DATA) ? true : false );

   connect( logNewMapCodeCbox, SIGNAL(stateChanged(int)), this, SLOT(logNewMapCodeChanged(int)) );
   connect( logNewMapDataCbox, SIGNAL(stateChanged(int)), this, SLOT(logNewMapDataChanged(int)) );

	grid->addWidget( logNewMapCodeCbox, 0, 0, Qt::AlignLeft );
	grid->addWidget( logNewMapDataCbox, 0, 1, Qt::AlignLeft );

	mainLayout->addWidget( frame, 1 );

	setLayout( mainLayout );

	traceViewCounter = 0;

   updateTimer  = new QTimer( this );

   connect( updateTimer, &QTimer::timeout, this, &TraceLoggerDialog_t::updatePeriodic );

   updateTimer->start( 10 ); // 100hz
}
//----------------------------------------------------
TraceLoggerDialog_t::~TraceLoggerDialog_t(void)
{
   updateTimer->stop();

	traceLogWindow = NULL;
	logging = 0;

	if ( logFile )
	{
		fclose( logFile ); logFile = NULL;
	}
	printf("Trace Logger Window Deleted\n");
}
//----------------------------------------------------
void TraceLoggerDialog_t::closeEvent(QCloseEvent *event)
{
   printf("Trace Logger Close Window Event\n");
   done(0);
	deleteLater();
   event->accept();
}
//----------------------------------------------------
void TraceLoggerDialog_t::closeWindow(void)
{
   printf("Trace Logger Close Window\n");
   done(0);
	deleteLater();
}
//----------------------------------------------------
void TraceLoggerDialog_t::updatePeriodic(void)
{
	char traceViewDrawEnable;

	if ( logLastCbox->isChecked() )
	{
		if ( FCEUI_EmulationPaused() )
		{
			traceViewDrawEnable = 1;
		}
		else
		{
			traceViewDrawEnable = autoUpdateCbox->isChecked();
		}
	}
	else
	{
		traceViewDrawEnable = 0;
	}

	if ( logFile && logFileCbox->isChecked() )
	{
		char line[256];

		while ( recBufHead != recBufTail )
		{
			recBuf[ recBufTail ].convToText( line );

			fprintf( logFile, "%s\n", line );

			recBufTail = (recBufTail + 1) % recBufMax;
		}
	}
	else
	{
		recBufTail = recBufHead;
	}

	if ( traceViewCounter > 20 )
	{
		if ( traceViewDrawEnable )
		{
   		traceView->update();
		}
		traceViewCounter = 0;
	}
	traceViewCounter++;
}
//----------------------------------------------------
void TraceLoggerDialog_t::logMaxLinesChanged(int index)
{
	int logPrev;
	int maxLines = logMaxLinesComboBox->itemData(index).toInt();

	logPrev = logging;
	logging = 0;

	usleep(1000);

	initTraceLogBuffer( maxLines );

	vbar->setMaximum(recBufMax);
	vbar->setValue(recBufMax);

	logging = logPrev;
}
//----------------------------------------------------
void TraceLoggerDialog_t::toggleLoggingOnOff(void)
{
	if ( logging )
	{
		logging = 0;
		usleep( 1000 );
		pushMsgToLogBuffer("Logging Finished");
		startStopButton->setText( tr("Start Logging") );

		if ( logFile )
		{
			fclose( logFile ); logFile = NULL;
		}
	}
	else
	{
		if ( logFileCbox->isChecked() )
		{
			openLogFile();
		}
		pushMsgToLogBuffer("Log Start");
		startStopButton->setText( tr("Stop Logging") );
		logging = 1;
	}
}
//----------------------------------------------------
void TraceLoggerDialog_t::openLogFile(void)
{
	const char *romFile;
	int ret, useNativeFileDialogVal;
	QString filename;
	QFileDialog  dialog(this, tr("Select Log File") );

	printf("Log File Select\n");

	dialog.setFileMode(QFileDialog::AnyFile);

	dialog.setNameFilter(tr("LOG files (*.log *.LOG) ;; All files (*)"));

	dialog.setViewMode(QFileDialog::List);
	dialog.setFilter( QDir::AllEntries | QDir::AllDirs | QDir::Hidden );
	dialog.setLabelText( QFileDialog::Accept, tr("Open") );
	dialog.setDefaultSuffix( tr(".log") );

	romFile = getRomFile();

	if ( romFile != NULL )
	{
		char dir[512];
		getDirFromFile( romFile, dir );
		dialog.setDirectory( tr(dir) );
	}

	// Check config option to use native file dialog or not
	g_config->getOption ("SDL.UseNativeFileDialog", &useNativeFileDialogVal);

	dialog.setOption(QFileDialog::DontUseNativeDialog, !useNativeFileDialogVal);

	dialog.show();
	ret = dialog.exec();

	if ( ret )
	{
		QStringList fileList;
		fileList = dialog.selectedFiles();

		if ( fileList.size() > 0 )
		{
			filename = fileList[0];
		}
	}

   if ( filename.isNull() )
   {
      return;
   }
	//qDebug() << "selected file path : " << filename.toUtf8();

	if ( logFile )
	{
		fclose( logFile ); logFile = NULL;
	}
	logFile = fopen( filename.toStdString().c_str(), "w");

   return;
}
//----------------------------------------------------
void TraceLoggerDialog_t::hbarChanged(int val)
{
   traceView->update();
}
//----------------------------------------------------
void TraceLoggerDialog_t::vbarChanged(int val)
{
   traceView->update();
}
//----------------------------------------------------
void TraceLoggerDialog_t::logRegStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_REGISTERS;
   }
   else
   {
      logging_options |=  LOG_REGISTERS;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logFrameStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_FRAMES_COUNT;
   }
   else
   {
      logging_options |=  LOG_FRAMES_COUNT;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logEmuMsgStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_MESSAGES;
   }
   else
   {
      logging_options |=  LOG_MESSAGES;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::symTraceEnaStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_SYMBOLIC;
   }
   else
   {
      logging_options |=  LOG_SYMBOLIC;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logProcStatFlagStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_PROCESSOR_STATUS;
   }
   else
   {
      logging_options |=  LOG_PROCESSOR_STATUS;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logCyclesCountStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_CYCLES_COUNT;
   }
   else
   {
      logging_options |=  LOG_CYCLES_COUNT;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logBreakpointStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_BREAKPOINTS;
   }
   else
   {
      logging_options |=  LOG_BREAKPOINTS;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::useStackPointerStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_CODE_TABBING;
   }
   else
   {
      logging_options |=  LOG_CODE_TABBING;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::toLeftDisassemblyStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_TO_THE_LEFT;
   }
   else
   {
      logging_options |=  LOG_TO_THE_LEFT;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logInstrCountStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_INSTRUCTIONS_COUNT;
   }
   else
   {
      logging_options |=  LOG_INSTRUCTIONS_COUNT;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logBankNumStateChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_BANK_NUMBER;
   }
   else
   {
      logging_options |=  LOG_BANK_NUMBER;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logNewMapCodeChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_NEW_INSTRUCTIONS;
   }
   else
   {
      logging_options |=  LOG_NEW_INSTRUCTIONS;
   }
}
//----------------------------------------------------
void TraceLoggerDialog_t::logNewMapDataChanged(int state)
{
   if ( state == Qt::Unchecked )
   {
      logging_options &= ~LOG_NEW_DATA;
   }
   else
   {
      logging_options |=  LOG_NEW_DATA;
   }
}
//----------------------------------------------------
traceRecord_t::traceRecord_t(void)
{
	cpu.PC = 0;
	cpu.A = 0;
	cpu.X = 0;
	cpu.Y = 0;
	cpu.S = 0;
	cpu.P = 0;

	opCode[0] = 0;
	opCode[1] = 0;
	opCode[2] = 0;
	opSize = 0;
	asmTxtSize = 0;
	asmTxt[0] = 0;

	cycleCount = 0;
	instrCount = 0;
	flags = 0;

	callAddr = -1;
	romAddr = -1;
	bank = -1;
	skippedLines = 0;
}
//----------------------------------------------------
int traceRecord_t::appendAsmText( const char *txt )
{
	int i=0;

	while ( txt[i] != 0 )
	{
		asmTxt[ asmTxtSize ] = txt[i]; i++; asmTxtSize++;
	}
	asmTxt[ asmTxtSize ] = 0;

	return 0;
}
//----------------------------------------------------
static int convToXchar( int i )
{
   int c = 0;

	if ( (i >= 0) && (i < 10) )
	{
      c = i + '0';
	}
	else if ( i < 16 )
	{
		c = (i - 10) + 'A';
	}
	return c;
}
//----------------------------------------------------
int traceRecord_t::convToText( char *txt )
{
   int i=0, j=0;
   char stmp[128];
   char str_axystate[32], str_procstatus[32];

	str_axystate[0] = 0;
	str_procstatus[0] = 0;

   txt[0] = 0;
   if ( opSize == 0 )
	{
		j=0;
   	while ( asmTxt[j] != 0 )
   	{
   	   txt[i] = asmTxt[j]; i++; j++;
   	}
		txt[i] = 0;

		return -1;
	}

   if ( skippedLines > 0 )
   {
		sprintf(stmp, "(%d lines skipped) ", skippedLines);

      j=0;
      while ( stmp[j] != 0 )
      {
         txt[i] = stmp[j]; i++; j++;
      }
   }

   // Start filling the str_temp line: Frame count, Cycles count, Instructions count, AXYS state, Processor status, Tabs, Address, Data, Disassembly
	if (logging_options & LOG_FRAMES_COUNT)
	{
		sprintf(stmp, "f%-6llu ", (long long unsigned int)frameCount);

      j=0;
      while ( stmp[j] != 0 )
      {
         txt[i] = stmp[j]; i++; j++;
      }
	}

   if (logging_options & LOG_CYCLES_COUNT)
	{
		sprintf(stmp, "c%-11llu ", (long long unsigned int)cycleCount);

      j=0;
      while ( stmp[j] != 0 )
      {
         txt[i] = stmp[j]; i++; j++;
      }
	}

	if (logging_options & LOG_INSTRUCTIONS_COUNT)
	{
		sprintf(stmp, "i%-11llu ", (long long unsigned int)instrCount);

      j=0;
      while ( stmp[j] != 0 )
      {
         txt[i] = stmp[j]; i++; j++;
      }
	}
	
	if (logging_options & LOG_REGISTERS)
	{
		sprintf(str_axystate,"A:%02X X:%02X Y:%02X S:%02X ",(cpu.A),(cpu.X),(cpu.Y),(cpu.S));
	}

   if (logging_options & LOG_PROCESSOR_STATUS)
	{
		int tmp = cpu.P^0xFF;
		sprintf(str_procstatus,"P:%c%c%c%c%c%c%c%c ",
			'N'|(tmp&0x80)>>2,
			'V'|(tmp&0x40)>>1,
			'U'|(tmp&0x20),
			'B'|(tmp&0x10)<<1,
			'D'|(tmp&0x08)<<2,
			'I'|(tmp&0x04)<<3,
			'Z'|(tmp&0x02)<<4,
			'C'|(tmp&0x01)<<5
			);
	}

   if (logging_options & LOG_TO_THE_LEFT)
	{
		if (logging_options & LOG_REGISTERS)
		{
         j=0;
         while ( str_axystate[j] != 0 )
         {
            txt[i] = str_axystate[j]; i++; j++;
         }
		}
		if (logging_options & LOG_PROCESSOR_STATUS)
		{
         j=0;
         while ( str_procstatus[j] != 0 )
         {
            txt[i] = str_procstatus[j]; i++; j++;
         }
		}
	}

   if (logging_options & LOG_CODE_TABBING)
	{
		// add spaces at the beginning of the line according to stack pointer
		int spaces = (0xFF - cpu.S) & LOG_TABS_MASK;

		while ( spaces > 0 )
		{
			txt[i] = ' '; i++; spaces--;
		}
	} 
	else if (logging_options & LOG_TO_THE_LEFT)
	{
		txt[i] = ' '; i++; 
	}

   if (logging_options & LOG_BANK_NUMBER)
	{
		if (cpu.PC >= 0x8000)
		{
			sprintf(stmp, "$%02X:%04X: ", bank, cpu.PC);
		}
		else
		{
			sprintf(stmp, "  $%04X: ", cpu.PC);
		}
	} 
	else
	{
		sprintf(stmp, "$%04X: ", cpu.PC);
	}
   j=0;
   while ( stmp[j] != 0 )
   {
      txt[i] = stmp[j]; i++; j++;
   }

   for (j=0; j<opSize; j++)
   {
      txt[i] = convToXchar( (opCode[j] >> 4) & 0x0F ); i++;
      txt[i] = convToXchar(  opCode[j] & 0x0F ); i++;
      txt[i] = ' '; i++;
   }
   while ( j < 3 )
   {
      txt[i] = ' '; i++;
      txt[i] = ' '; i++;
      txt[i] = ' '; i++;
      j++;
   }
   j=0;
   while ( asmTxt[j] != 0 )
   {
      txt[i] = asmTxt[j]; i++; j++;
   }
   if ( callAddr >= 0 )
   {
		sprintf(stmp, " (from $%04X)", callAddr);

      j=0;
      while ( stmp[j] != 0 )
      {
         txt[i] = stmp[j]; i++; j++;
      }
   }

   if (!(logging_options & LOG_TO_THE_LEFT))
	{
		if (logging_options & LOG_REGISTERS)
		{
         j=0;
         while ( str_axystate[j] != 0 )
         {
            txt[i] = str_axystate[j]; i++; j++;
         }
		}
		if (logging_options & LOG_PROCESSOR_STATUS)
		{
         j=0;
         while ( str_procstatus[j] != 0 )
         {
            txt[i] = str_procstatus[j]; i++; j++;
         }
		}
	}

   txt[i] = 0;

	return 0;
}
//----------------------------------------------------
int initTraceLogBuffer( int maxRecs )
{
   if ( maxRecs != recBufMax )
   {
      size_t size;

      size = maxRecs * sizeof(traceRecord_t);

      recBuf = (traceRecord_t*)malloc( size );

      if ( recBuf )
      {
         memset( (void*)recBuf, 0, size );
         recBufMax = maxRecs;
      }
      else
      {
         recBufMax = 0;
      }
   }
   return recBuf == NULL;
}
//----------------------------------------------------
void openTraceLoggerWindow( QWidget *parent )
{
	// Only allow one trace logger window to be open
	if ( traceLogWindow != NULL )
	{
		return;
	}
	//printf("Open Trace Logger Window\n");
	
   traceLogWindow = new TraceLoggerDialog_t(parent);
	
   traceLogWindow->show();
}
//----------------------------------------------------
static void pushToLogBuffer( traceRecord_t &rec )
{
   recBuf[ recBufHead ] = rec;
   recBufHead = (recBufHead + 1) % recBufMax;

	if ( recBufHead == recBufTail )
	{
		printf("Trace Log Overrun!!!\n");
	}
}
//----------------------------------------------------
static void pushMsgToLogBuffer( const char *msg )
{
	traceRecord_t rec;

	strncpy( rec.asmTxt, msg, sizeof(rec.asmTxt) );
	
	rec.asmTxt[ sizeof(rec.asmTxt)-1 ] = 0;

	pushToLogBuffer( rec );
}
//----------------------------------------------------
//todo: really speed this up
void FCEUD_TraceInstruction(uint8 *opcode, int size)
{
	if (!logging)
		return;

	traceRecord_t  rec;

	char asmTxt[256];
	unsigned int addr = X.PC;
	static int unloggedlines = 0;
	int asmFlags = 0;

	rec.cpu.PC = X.PC;
	rec.cpu.A  = X.A;
	rec.cpu.X  = X.X;
	rec.cpu.Y  = X.Y;
	rec.cpu.S  = X.S;
	rec.cpu.P  = X.P;

	for (int i=0; i<size; i++)
	{
		rec.opCode[i] = opcode[i];
	}
	rec.opSize  = size;
	rec.romAddr = GetPRGAddress(addr);
	rec.bank    = getBank(addr);

	rec.frameCount = currFrameCounter;
	rec.instrCount = total_instructions;

	int64 counter_value = timestampbase + (uint64)timestamp - total_cycles_base;
	if (counter_value < 0)	// sanity check
	{
		ResetDebugStatisticsCounters();
		counter_value = 0;
	}
	rec.cycleCount = counter_value;

	if ( logging_options & LOG_SYMBOLIC )
	{
		asmFlags = ASM_DEBUG_SYMS | ASM_DEBUG_REGS;
	}

	// if instruction executed from the RAM, skip this, log all instead
	// TODO: loops folding mame-lyke style
	if (rec.romAddr != -1)
	{
		if (((logging_options & LOG_NEW_INSTRUCTIONS) && (oldcodecount != codecount)) ||
		    ((logging_options & LOG_NEW_DATA) && (olddatacount != datacount)))
		{
			//something new was logged
			oldcodecount = codecount;
			olddatacount = datacount;
			if (unloggedlines > 0)
			{
				//sprintf(str_result, "(%d lines skipped)", unloggedlines);
				rec.skippedLines = unloggedlines;
				unloggedlines = 0;
			}
		} 
		else
		{
			if ((logging_options & LOG_NEW_INSTRUCTIONS) ||
				 (logging_options & LOG_NEW_DATA))
			{
				if (FCEUI_GetLoggingCD())
				{
					unloggedlines++;
				}
				return;
			}
		}
	}

	if ((addr + size) > 0xFFFF)
	{
		//sprintf(str_data, "%02X        ", opcode[0]);
		//sprintf(str_disassembly, "OVERFLOW");
		rec.flags |= 0x01;
	} 
	else
	{
		char* a = 0;
		switch (size)
		{
			case 0:
				//sprintf(str_disassembly,"UNDEFINED");
				rec.flags |= 0x02;
				break;
			case 1:
			{
				DisassembleWithDebug(addr + 1, opcode, asmFlags, asmTxt);
				// special case: an RTS opcode
				if (opcode[0] == 0x60)
				{
					// add the beginning address of the subroutine that we exit from
					unsigned int caller_addr = GetMem(((X.S) + 1)|0x0100) + (GetMem(((X.S) + 2)|0x0100) << 8) - 0x2;
					if (GetMem(caller_addr) == 0x20)
					{
						// this was a JSR instruction - take the subroutine address from it
						unsigned int call_addr = GetMem(caller_addr + 1) + (GetMem(caller_addr + 2) << 8);
						rec.callAddr = call_addr;
					}
				}
				a = asmTxt;
				break;
			}
			case 2:
				DisassembleWithDebug(addr + 2, opcode, asmFlags, asmTxt);
				a = asmTxt;
				break;
			case 3:
				DisassembleWithDebug(addr + 3, opcode, asmFlags, asmTxt);
				a = asmTxt;
				break;
		}

		if (a)
		{
			rec.appendAsmText(a);
		}
	}

   pushToLogBuffer( rec );

	return; // TEST
	// All of the following log text creation is very cpu intensive, to keep emulation 
	// running realtime save data and have a separate thread do this translation.

	//if (size == 1 && GetMem(addr) == 0x60)
	//{
	//	// special case: an RTS opcode
	//	// add "----------" to emphasize the end of subroutine
	//	static const char* emphasize = " -------------------------------------------------------------------------------------------------------------------------";
	//	strncat(str_disassembly, emphasize, LOG_DISASSEMBLY_MAX_LEN - strlen(str_disassembly) - 1);
	//}
	
	return;
}
//----------------------------------------------------
QTraceLogView::QTraceLogView(QWidget *parent)
	: QWidget(parent)
{
   QPalette pal;
	QColor fg("black"), bg("white");

	font.setFamily("Courier New");
	font.setStyle( QFont::StyleNormal );
	font.setStyleHint( QFont::Monospace );

   pal = this->palette();
	pal.setColor(QPalette::Base      , bg );
	pal.setColor(QPalette::Background, bg );
	pal.setColor(QPalette::WindowText, fg );

	this->setPalette(pal);

	calcFontData();

	vbar = NULL;
	hbar = NULL;
}
//----------------------------------------------------
QTraceLogView::~QTraceLogView(void)
{

}
//----------------------------------------------------
void QTraceLogView::calcFontData(void)
{
	this->setFont(font);
    QFontMetrics metrics(font);
#if QT_VERSION > QT_VERSION_CHECK(5, 11, 0)
    pxCharWidth = metrics.horizontalAdvance(QLatin1Char('2'));
#else
    pxCharWidth = metrics.width(QLatin1Char('2'));
#endif
    pxCharHeight   = metrics.height();
	 pxLineSpacing  = metrics.lineSpacing() * 1.25;
    pxLineLead     = pxLineSpacing - pxCharHeight;
    pxCursorHeight = pxCharHeight;
	 pxLineWidth    = pxCharWidth * LOG_LINE_MAX_LEN;

	 viewLines   = (viewHeight / pxLineSpacing) + 1;
}
//----------------------------------------------------------------------------
void QTraceLogView::setScrollBars( QScrollBar *h, QScrollBar *v )
{
	hbar = h; vbar = v;
}
//----------------------------------------------------
void QTraceLogView::resizeEvent(QResizeEvent *event)
{
	viewWidth  = event->size().width();
	viewHeight = event->size().height();

	//printf("QAsmView Resize: %ix%i\n", viewWidth, viewHeight );

	viewLines = (viewHeight / pxLineSpacing) + 1;

	//maxLineOffset = 0; // mb.numLines() - viewLines + 1;

	if ( viewWidth >= pxLineWidth )
	{
		pxLineXScroll = 0;
	}
	else
	{
		pxLineXScroll = (int)(0.010f * (float)hbar->value() * (float)(pxLineWidth - viewWidth) );
	}

}
//----------------------------------------------------
void QTraceLogView::paintEvent(QPaintEvent *event)
{
   int x,y, v, ofs, row, start, end, nrow;
	QPainter painter(this);
   char line[256];
   traceRecord_t rec[64];

	painter.setFont(font);
	viewWidth  = event->rect().width();
	viewHeight = event->rect().height();

	nrow = (viewHeight / pxLineSpacing);

	if (nrow < 1 ) nrow = 1;

   viewLines = nrow;

   painter.fillRect( 0, 0, viewWidth, viewHeight, this->palette().color(QPalette::Background) );

   painter.setPen( this->palette().color(QPalette::WindowText));

	v = vbar->value();

	if ( v < viewLines ) v = viewLines;

   ofs = recBufMax - v;

   end = recBufHead - ofs;

   if ( end < 0 ) end += recBufMax;
  
   start = (end - nrow);

   if ( start < 0 ) start += recBufMax;

   row = 0;
   while ( start != end )
   {
      rec[row] = recBuf[ start ]; row++;
      start = (start + 1) % recBufMax;
   }

	pxLineXScroll = (int)(0.010f * (float)hbar->value() * (float)(pxLineWidth - viewWidth) );

   x = -pxLineXScroll;
   y =  pxLineSpacing;

   for (row=0; row<nrow; row++)
   {

      rec[row].convToText( line );

      //printf("Line %i: '%s'\n", row, line );

      painter.drawText( x, y, tr(line) );

      y += pxLineSpacing;
   }

}
//----------------------------------------------------
