// HotKeyConf.cpp
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>

#include <SDL.h>
#include <QHeaderView>
#include <QScrollBar>
#include <QPainter>
#include <QMenuBar>

#include "../../types.h"
#include "../../fceu.h"
#include "../../cheat.h"
#include "../../debug.h"
#include "../../driver.h"
#include "../../version.h"
#include "../../movie.h"
#include "../../palette.h"
#include "../../fds.h"
#include "../../cart.h"
#include "../../ines.h"
#include "../common/configSys.h"

#include "Qt/main.h"
#include "Qt/dface.h"
#include "Qt/input.h"
#include "Qt/config.h"
#include "Qt/keyscan.h"
#include "Qt/fceuWrapper.h"
#include "Qt/HexEditor.h"

//enum {
//	MODE_NES_RAM = 0,
//	MODE_NES_PPU,
//	MODE_NES_OAM,
//	MODE_NES_ROM
//};
//----------------------------------------------------------------------------
static int getRAM( unsigned int i )
{
	return GetMem(i);
}
static int getPPU( unsigned int i )
{
	i &= 0x3FFF;
	if (i < 0x2000)return VPage[(i) >> 10][(i)];
	//NSF PPU Viewer crash here (UGETAB) (Also disabled by 'MaxSize = 0x2000')
	if (GameInfo->type == GIT_NSF)
		return 0;
	else
	{
		if (i < 0x3F00)
			return vnapage[(i >> 10) & 0x3][i & 0x3FF];
		return READPAL_MOTHEROFALL(i & 0x1F);
	}
	return 0;
}
static int getOAM( unsigned int i )
{
	return SPRAM[i & 0xFF];
}
static int getROM( unsigned int offset)
{
	if (offset < 16)
	{
		return *((unsigned char *)&head+offset);
	}
	else if (offset < (16+PRGsize[0]) )
	{
		return PRGptr[0][offset-16];
	}
	else if (offset < (16+PRGsize[0]+CHRsize[0]) )
	{
		return CHRptr[0][offset-16-PRGsize[0]];
	}
	return -1;
}
static void PalettePoke(uint32 addr, uint8 data)
{
	data = data & 0x3F;
	addr = addr & 0x1F;
	if ((addr & 3) == 0)
	{
		addr = (addr & 0xC) >> 2;
		if (addr == 0)
		{
			PALRAM[0x00] = PALRAM[0x04] = PALRAM[0x08] = PALRAM[0x0C] = data;
		}
		else
		{
			UPALRAM[addr-1] = UPALRAM[0x10|(addr-1)] = data;
		}
	}
	else
	{
		PALRAM[addr] = data;
	}
}
static int writeMem( int mode, unsigned int addr, int value )
{
	value = value & 0x000000ff;

	switch ( mode )
	{
		default:
      case HexEditorDialog_t::MODE_NES_RAM:
		{
			if ( addr < 0x8000 )
			{
				writefunc wfunc;
            
				wfunc = GetWriteHandler (addr);
            
				if (wfunc)
				{
					wfunc ((uint32) addr,
					       (uint8) (value & 0x000000ff));
				}
			}
			else
			{
				fprintf( stdout, "Error: Writing into RAM addresses >= 0x8000 is unsafe. Operation Denied.\n");
			}
		}
		break;
      case HexEditorDialog_t::MODE_NES_PPU:
		{
			addr &= 0x3FFF;
			if (addr < 0x2000)
			{
				VPage[addr >> 10][addr] = value; //todo: detect if this is vrom and turn it red if so
			}
			if ((addr >= 0x2000) && (addr < 0x3F00))
			{
				vnapage[(addr >> 10) & 0x3][addr & 0x3FF] = value; //todo: this causes 0x3000-0x3f00 to mirror 0x2000-0x2f00, is this correct?
			}
			if ((addr >= 0x3F00) && (addr < 0x3FFF))
			{
				PalettePoke(addr, value);
			}
		}
		break;
      case HexEditorDialog_t::MODE_NES_OAM:
		{
			addr &= 0xFF;
			SPRAM[addr] = value;
		}
		break;
      case HexEditorDialog_t::MODE_NES_ROM:
		{
			if (addr < 16)
			{
				fprintf( stdout, "You can't edit ROM header here, however you can use iNES Header Editor to edit the header if it's an iNES format file.");
			}
			else if ( (addr >= 16) && (addr < PRGsize[0]+16) )
			{
			  	*(uint8 *)(GetNesPRGPointer(addr-16)) = value;
			}
			else if ( (addr >= PRGsize[0]+16) && (addr < CHRsize[0]+PRGsize[0]+16) )
			{
				*(uint8 *)(GetNesCHRPointer(addr-16-PRGsize[0])) = value;
			}
		}
		break;
	}
   return 0;
}

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

static int convFromXchar( int i )
{
   int c = 0;

   i = ::toupper(i);

	if ( (i >= '0') && (i <= '9') )
	{
      c = i - '0';
	}
	else if ( (i >= 'A') && (i <= 'F') )
	{
		c = (i - 'A') + 10;
	}
	return c;
}

//----------------------------------------------------------------------------
memBlock_t::memBlock_t( void )
{
	buf = NULL;
	_size = 0;
   _maxLines = 0;
	memAccessFunc = NULL;
}
//----------------------------------------------------------------------------

memBlock_t::~memBlock_t(void)
{
	if ( buf != NULL )
	{
		::free( buf ); buf = NULL;
	}
	_size = 0;
   _maxLines = 0;
}

//----------------------------------------------------------------------------
int memBlock_t::reAlloc( int newSize )
{
	if ( newSize == 0 )
	{
		return 0;
	}
	if ( _size == newSize )
	{
		return 0;
	}

	if ( buf != NULL )
	{
		::free( buf ); buf = NULL;
	}
	_size = 0;
   _maxLines = 0;

	buf = (struct memByte_t *)malloc( newSize * sizeof(struct memByte_t) );

	if ( buf != NULL )
	{
		_size = newSize;
		init();

      if ( (_size % 16) )
	   {
	   	_maxLines = (_size / 16) + 1;
	   }
	   else
	   {
	   	_maxLines = (_size / 16);
	   }
	}
	return (buf == NULL);
}
//----------------------------------------------------------------------------
void memBlock_t::setAccessFunc( int (*newMemAccessFunc)( unsigned int offset) )
{
	memAccessFunc = newMemAccessFunc;
}
//----------------------------------------------------------------------------
void memBlock_t::init(void)
{
	for (int i=0; i<_size; i++)
	{
		buf[i].data  = memAccessFunc(i);
		buf[i].color = 0;
		buf[i].actv  = 0;
		//buf[i].draw  = 1;
	}
}
//----------------------------------------------------------------------------
HexEditorDialog_t::HexEditorDialog_t(QWidget *parent)
	: QDialog( parent )
{
	//QVBoxLayout *mainLayout;
	QGridLayout *grid;
	QMenuBar *menuBar;
	QMenu *fileMenu;
	QAction *saveROM;

	setWindowTitle("Hex Editor");

	resize( 512, 512 );

	menuBar = new QMenuBar(this);

	//-----------------------------------------------------------------------
	// Menu 
	//-----------------------------------------------------------------------
	// File
   fileMenu = menuBar->addMenu(tr("File"));

	// File -> Open ROM
	saveROM = new QAction(tr("Save ROM"), this);
   saveROM->setShortcuts(QKeySequence::Open);
   saveROM->setStatusTip(tr("Save ROM File"));
   //connect(saveROM, SIGNAL(triggered()), this, SLOT(saveROMFile(void)) );

   fileMenu->addAction(saveROM);

	//-----------------------------------------------------------------------
	// Menu End 
	//-----------------------------------------------------------------------
	//mainLayout = new QVBoxLayout();

	grid   = new QGridLayout(this);
	editor = new QHexEdit( &mb, this);
	vbar   = new QScrollBar( Qt::Vertical, this );
	hbar   = new QScrollBar( Qt::Horizontal, this );

	grid->setMenuBar( menuBar );

	grid->addWidget( editor, 0, 0 );
	grid->addWidget( vbar  , 0, 1 );
	grid->addWidget( hbar  , 1, 0 );
	//mainLayout->addLayout( grid );

	setLayout( grid );

	hbar->setMinimum(0);
	hbar->setMaximum(100);
	vbar->setMinimum(0);
	vbar->setMaximum( 0x10000 / 16 );

   editor->setScrollBars( hbar, vbar );

   //connect( vbar, SIGNAL(sliderMoved(int)), this, SLOT(vbarMoved(int)) );
   connect( vbar, SIGNAL(valueChanged(int)), this, SLOT(vbarChanged(int)) );

	mode = MODE_NES_RAM;
	memAccessFunc = getRAM;
	redraw = false;
	total_instructions_lp = 0;

	showMemViewResults(true);

	periodicTimer  = new QTimer( this );

   connect( periodicTimer, &QTimer::timeout, this, &HexEditorDialog_t::updatePeriodic );

	periodicTimer->start( 100 ); // 10hz
}
//----------------------------------------------------------------------------
HexEditorDialog_t::~HexEditorDialog_t(void)
{
	periodicTimer->stop();
}
//----------------------------------------------------------------------------
void HexEditorDialog_t::closeWindow(void)
{
   //printf("Close Window\n");
   done(0);
}
//----------------------------------------------------------------------------
void HexEditorDialog_t::vbarMoved(int value)
{
	//printf("VBar Moved: %i\n", value);
	editor->setLine( value );
}
//----------------------------------------------------------------------------
void HexEditorDialog_t::vbarChanged(int value)
{
	//printf("VBar Changed: %i\n", value);
	editor->setLine( value );
}
//----------------------------------------------------------------------------
void HexEditorDialog_t::setMode(int new_mode)
{
	if ( mode != new_mode )
	{
		mode = new_mode;
		showMemViewResults(true);
      editor->setMode( mode );
	}
}
//----------------------------------------------------------------------------
void HexEditorDialog_t::showMemViewResults (bool reset)
{
	int memSize;

	switch ( mode )
	{
		default:
		case MODE_NES_RAM:
			memAccessFunc = getRAM;
			memSize       = 0x10000;
		break;
		case MODE_NES_PPU:
			memAccessFunc = getPPU;
			memSize       = (GameInfo->type == GIT_NSF ? 0x2000 : 0x4000);
		break;
		case MODE_NES_OAM:
			memAccessFunc = getOAM;
			memSize       = 0x100;
		break;
		case MODE_NES_ROM:

			if ( GameInfo != NULL )
			{
				memAccessFunc = getROM;
				memSize       = 16 + CHRsize[0] + PRGsize[0];
			}
			else
			{  // No Game Loaded!!! Get out of Function
				memAccessFunc = NULL;
				memSize = 0;
				return;
			}
		break;
	}

	if ( memSize != mb.size() )
	{
		mb.setAccessFunc( memAccessFunc );

		if ( mb.reAlloc( memSize ) )
		{
			printf("Error: Failed to allocate memview buffer size\n");
			return;
		}
	}
}
//----------------------------------------------------------------------------
void HexEditorDialog_t::updatePeriodic(void)
{
	//printf("Update Periodic\n");

	checkMemActivity();

	showMemViewResults(false);

	editor->update();
}
//----------------------------------------------------------------------------
int HexEditorDialog_t::checkMemActivity(void)
{
	int c;

	// Don't perform memory activity checks when:
	// 1. In ROM View Mode
	// 2. The simulation is not cycling (paused)

	if ( ( mode == MODE_NES_ROM ) ||
	      ( total_instructions_lp == total_instructions ) )
	{
		return -1;
	}

	for (int i=0; i<mb.size(); i++)
	{
		c = memAccessFunc(i);

		if ( c != mb.buf[i].data )
		{
			mb.buf[i].actv  = 15;
			mb.buf[i].data  = c;
			//mb.buf[i].draw  = 1;
		}
		else
		{
			if ( mb.buf[i].actv > 0 )
			{
				//mb.buf[i].draw = 1;
				mb.buf[i].actv--;
			}
		}
	}
	total_instructions_lp = total_instructions;

   return 0;
}
//----------------------------------------------------------------------------
QHexEdit::QHexEdit(memBlock_t *blkPtr, QWidget *parent)
	: QWidget( parent )
{
	QPalette pal;

	mb = blkPtr;

	this->setFocusPolicy(Qt::StrongFocus);

	font.setFamily("Courier New");
	font.setStyle( QFont::StyleNormal );
	font.setStyleHint( QFont::Monospace );

	pal = this->palette();
	pal.setColor(QPalette::Base      , Qt::black);
	pal.setColor(QPalette::Background, Qt::black);
	pal.setColor(QPalette::WindowText, Qt::white);

	//editor->setAutoFillBackground(true);
	this->setPalette(pal);

	calcFontData();

   viewMode    = HexEditorDialog_t::MODE_NES_RAM;
	lineOffset  = 0;
	cursorPosX  = 0;
	cursorPosY  = 0;
	cursorBlink = true;
	cursorBlinkCount = 0;
   maxLineOffset = 0;
   editAddr  = -1;
   editValue =  0;
   editMask  =  0;

}
//----------------------------------------------------------------------------
QHexEdit::~QHexEdit(void)
{

}
//----------------------------------------------------------------------------
void QHexEdit::calcFontData(void)
{
	 this->setFont(font);
    QFontMetrics metrics(font);
#if QT_VERSION > QT_VERSION_CHECK(5, 11, 0)
    pxCharWidth = metrics.horizontalAdvance(QLatin1Char('2'));
#else
    pxCharWidth = metrics.width(QLatin1Char('2'));
#endif
    pxCharHeight  = metrics.height();
	 pxLineSpacing = metrics.lineSpacing() * 1.25;
    pxLineLead    = pxLineSpacing - pxCharHeight;
	 pxXoffset     = pxCharWidth;
	 pxYoffset     = pxLineSpacing * 2.0;
	 pxHexOffset   = pxXoffset + (7*pxCharWidth);
    pxHexAscii    = pxHexOffset + (16*3*pxCharWidth) + (pxCharWidth);
    //_pxGapAdr = _pxCharWidth / 2;
    //_pxGapAdrHex = _pxCharWidth;
    //_pxGapHexAscii = 2 * _pxCharWidth;
    pxCursorHeight = pxCharHeight;
    //_pxSelectionSub = _pxCharHeight / 5;
	 viewLines   = (viewHeight - pxLineSpacing) / pxLineSpacing;
}
//----------------------------------------------------------------------------
void QHexEdit::setMode( int mode )
{
	viewMode = mode;
}
//----------------------------------------------------------------------------
void QHexEdit::setLine( int newLineOffset )
{
	lineOffset = newLineOffset;
}
//----------------------------------------------------------------------------
void QHexEdit::setScrollBars( QScrollBar *h, QScrollBar *v )
{
	hbar = h; vbar = v;
}
//----------------------------------------------------------------------------
void QHexEdit::resizeEvent(QResizeEvent *event)
{
	viewWidth  = event->size().width();
	viewHeight = event->size().height();

	//printf("QHexEdit Resize: %ix%i\n", viewWidth, viewHeight );

	viewLines = (viewHeight - pxLineSpacing) / pxLineSpacing;

	maxLineOffset = mb->numLines() - viewLines + 1;
}
//----------------------------------------------------------------------------
void QHexEdit::resetCursor(void)
{
	cursorBlink = true;
	cursorBlinkCount = 0;
   editAddr = -1;
   editValue = 0;
   editMask  = 0;
}
//----------------------------------------------------------------------------
void QHexEdit::keyPressEvent(QKeyEvent *event)
{
   printf("Hex Window Key Press: 0x%x \n", event->key() );
	
	if (event->matches(QKeySequence::MoveToNextChar))
   {
		cursorPosX++;

		if ( cursorPosX >= 48  )
		{
			cursorPosX = 47;
		}
		resetCursor();
   }
	else if (event->matches(QKeySequence::MoveToPreviousChar))
   {
		cursorPosX--;

		if ( cursorPosX < 0 )
		{
			cursorPosX = 0;
		}
		resetCursor();
   }
	else if (event->matches(QKeySequence::MoveToEndOfLine))
   {
		cursorPosX = 47;
		resetCursor();
   }
	else if (event->matches(QKeySequence::MoveToStartOfLine))
   {
		cursorPosX = 0;
		resetCursor();
   }
	else if (event->matches(QKeySequence::MoveToPreviousLine))
   {
		cursorPosY--;

		if ( cursorPosY < 0 )
		{
			lineOffset--;

			if ( lineOffset < 0 )
			{
				lineOffset = 0;
			}
			cursorPosY = 0;

         vbar->setValue( lineOffset );
		}
		resetCursor();
   }
	else if (event->matches(QKeySequence::MoveToNextLine))
   {
		cursorPosY++;

		if ( cursorPosY >= viewLines )
		{
			lineOffset++;

         if ( lineOffset >= maxLineOffset )
         {
            lineOffset = maxLineOffset;
         }
			cursorPosY = viewLines-1;

         vbar->setValue( lineOffset );
		}
		resetCursor();

   }
   else if (event->matches(QKeySequence::MoveToNextPage))
   {
      lineOffset += ( (3 * viewLines) / 4);

      if ( lineOffset >= maxLineOffset )
      {
         lineOffset = maxLineOffset;
      }
		resetCursor();
   }
   else if (event->matches(QKeySequence::MoveToPreviousPage))
   {
      lineOffset -= ( (3 * viewLines) / 4);

      if ( lineOffset < 0 )
      {
         lineOffset = 0;
      }
		resetCursor();
   }
   else if (event->matches(QKeySequence::MoveToEndOfDocument))
   {
      lineOffset = maxLineOffset;
		resetCursor();
   }
   else if (event->matches(QKeySequence::MoveToStartOfDocument))
   {
      lineOffset = 0;
		resetCursor();
   }
   else if (event->key() == Qt::Key_Tab && (cursorPosX < 32) )
   {  // switch from hex to ascii edit
       cursorPosX = 32 + (cursorPosX / 2);
   }
   else if (event->key() == Qt::Key_Backtab  && (cursorPosX >= 32) )
   {  // switch from ascii to hex edit
      cursorPosX = 2 * (cursorPosX - 32);
   }
   else
   {
      int key;
      if ( cursorPosX >= 32 )
      {  // Edit Area is ASCII
         key = (uchar)event->text()[0].toLatin1();

         if ( ::isascii( key ) )
         {
            int offs = (cursorPosX-32);
            int addr = 16*(lineOffset+cursorPosY) + offs;
            writeMem( viewMode, addr, key );

            editAddr  = -1;
            editValue =  0;
            editMask  =  0;
         }
      }
      else
      {  // Edit Area is Hex
         key = int(event->text()[0].toUpper().toLatin1());

         if ( ::isxdigit( key ) )
         {
            int offs, nibbleValue, nibbleIndex;

            offs = (cursorPosX / 2);
            nibbleIndex = (cursorPosX % 2);

            editAddr = 16*(lineOffset+cursorPosY) + offs;

            nibbleValue = convFromXchar( key );

            if ( nibbleIndex )
            {
               nibbleValue = editValue | nibbleValue;

               writeMem( viewMode, editAddr, nibbleValue );

               editAddr  = -1;
               editValue =  0;
               editMask  =  0;
            }
            else
            {
               editValue = (nibbleValue << 4);
               editMask  = 0x00f0;
            }
            cursorPosX++;

            if ( cursorPosX >= 32 )
            {
               cursorPosX = 0;
            }
         }
      }
      //printf("Key: %c  %i \n", key, key);
   }
}
//----------------------------------------------------------------------------
void QHexEdit::keyReleaseEvent(QKeyEvent *event)
{
   printf("Hex Window Key Release: 0x%x \n", event->key() );
	//assignHotkey( event );
}
//----------------------------------------------------------------------------
void QHexEdit::mousePressEvent(QMouseEvent * event)
{
	int cx = 0, cy = 0;
	QPoint p = event->pos();

	//printf("Pos: %ix%i \n", p.x(), p.y() );

	if ( p.x() < pxHexOffset )
	{
		cx = 0;
	}
	else if ( (p.x() >= pxHexOffset) && (p.x() < pxHexAscii) )
	{
		float px = ( (float)p.x() - (float)pxHexOffset) / (float)(pxCharWidth);
		float ox = (px/3.0);
		float rx = fmodf(px,3.0);

		if ( rx >= 2.50 )
		{
			cx = 2*( (int)ox + 1 );
		}
		else
		{
			if ( rx >= 1.0 )
			{
				cx = 2*( (int)ox ) + 1;
			}
			else
			{
				cx = 2*( (int)ox );
			}
		}
	}
	else
	{
		cx = 32 + (p.x() - pxHexAscii) / pxCharWidth;
	}
	if ( cx >= 48 )
	{
		cx = 47;
	}

	if ( p.y() < pxYoffset )
	{
		cy = 0;
	}
	else
	{
		float ly = ( (float)pxLineLead / (float)pxLineSpacing );
		float py = ( (float)p.y() -  (float)pxLineSpacing) /  (float)pxLineSpacing;
		float ry = fmod( py, 1.0 );

		if ( ry < ly )
		{
			cy = ((int)py) - 1;
		}
		else
		{
			cy = (int)py;
		}
	}
	if ( cy < 0 )
	{
		cy = 0;
	}
	else if ( cy >= viewLines )
	{
		cy = viewLines - 1;
	}
	//printf("c: %ix%i \n", cx, cy );

	if ( event->button() == Qt::LeftButton )
	{
		cursorPosX = cx;
		cursorPosY = cy;
		resetCursor();
	}

}
//----------------------------------------------------------------------------
void QHexEdit::paintEvent(QPaintEvent *event)
{
	int x, y, w, h, row, col, nrow, addr;
	int c;
	char txt[32], asciiTxt[32];
	QPainter painter(this);

	painter.setFont(font);
	w = event->rect().width();
	h = event->rect().height();

	viewWidth  = w;
	viewHeight = h;
	//painter.fillRect( 0, 0, w, h, QColor("white") );

	nrow = (h - pxLineSpacing) / pxLineSpacing;

	if ( nrow < 1 ) nrow = 1;

	viewLines = nrow;

	if ( cursorPosY >= viewLines )
	{
		cursorPosY = viewLines-1;
	}
	//printf("Draw Area: %ix%i \n", event->rect().width(), event->rect().height() );
	//
	maxLineOffset = mb->numLines() - nrow + 1;

	if ( lineOffset > maxLineOffset )
	{
		lineOffset = maxLineOffset;
	}
	
	painter.fillRect( 0, 0, w, h, this->palette().color(QPalette::Background) );

	if ( cursorBlinkCount >= 5 )
	{
		cursorBlink = !cursorBlink;
		cursorBlinkCount = 0;
	} 
	else
  	{
		cursorBlinkCount++;
	}

	if ( cursorBlink )
	{
		y = pxYoffset + (pxLineSpacing*cursorPosY) - pxCursorHeight + pxLineLead;

		if ( cursorPosX < 32 )
		{
			int a = (cursorPosX / 2);
			int r = (cursorPosX % 2);
			x = pxHexOffset + (a*3*pxCharWidth) + (r*pxCharWidth);
		}
		else
		{
			x = pxHexAscii + (cursorPosX - 32)*pxCharWidth;
		}

		//painter.setPen( this->palette().color(QPalette::WindowText));
		painter.fillRect( x , y, pxCharWidth, pxCursorHeight, QColor("gray") );
	}

	painter.setPen( this->palette().color(QPalette::WindowText));

	//painter.setPen( QColor("white") );
	addr = lineOffset * 16;
	y = pxYoffset;

	for ( row=0; row < nrow; row++)
	{
		x = pxXoffset;

		sprintf( txt, "%06X", addr );
		painter.drawText( x, y, tr(txt) );

		x = pxHexOffset;

		for (col=0; col<16; col++)
		{
			if ( addr < mb->size() )
			{
				c =  mb->buf[addr].data;

				if ( ::isprint(c) )
				{
					asciiTxt[col] = c;
				}
				else
				{
					asciiTxt[col] = '.';
				}
            if ( addr == editAddr )
            {  // Set a cell currently being editting to red text
	            painter.setPen( QColor("red") );
               txt[0] = convToXchar( (editValue >> 4) & 0x0F );
               txt[1] = convToXchar( c & 0x0F );
               txt[2] = 0;
				   painter.drawText( x, y, tr(txt) );
	            painter.setPen( this->palette().color(QPalette::WindowText));
            } 
            else
            {
               txt[0] = convToXchar( (c >> 4) & 0x0F );
               txt[1] = convToXchar( c & 0x0F );
               txt[2] = 0;
				   painter.drawText( x, y, tr(txt) );
            }
			}
			else
			{
				asciiTxt[col] = 0;
			}
			x += (3*pxCharWidth);
			addr++;
		}
		asciiTxt[16] = 0;

		painter.drawText( pxHexAscii, y, tr(asciiTxt) );

		//addr += 16;
		y += pxLineSpacing;
	}

	painter.drawText( pxHexOffset, pxLineSpacing, "00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F" );
	painter.drawLine( pxHexOffset - (pxCharWidth/2), 0, pxHexOffset - (pxCharWidth/2), h );
	painter.drawLine( pxHexAscii  - (pxCharWidth/2), 0, pxHexAscii  - (pxCharWidth/2), h );
	painter.drawLine( 0, pxLineSpacing + (pxLineLead), w, pxLineSpacing + (pxLineLead) );

}
//----------------------------------------------------------------------------
