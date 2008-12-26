
#include "App.h"

#include <errno.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/dir.h>
#include <sys/stat.h>

#include <fat.h>
#include <dswifi9.h>
#include <DSGUI/BGUI.h>

#include <nds/registers_alt.h>
#include <nds/reload.h>

#include "ndsx_brightness.h"
#include "types.h"
#include "main.h"
#include "parse.h"
#include "Book.h"
#include "Button.h"
#include "Text.h"

#define MIN(x,y) (x < y ? x : y)
#define MAX(x,y) (x > y ? x : y)
#define BACKGROUND           (*((bg_attribute *)0x04000008))
#define BACKGROUND_SUB       (*((bg_attribute *)0x04001008))

App::App()
{	
	ts = NULL;
	browserstart = 0;
	pages = new page_t[MAXPAGES];
	pagebuf = new u8[PAGEBUFSIZE];
	pagecount = 0;
	pagecurrent = 0;
	pagewidth = PAGE_WIDTH;
	pageheight = PAGE_HEIGHT;

	fontdir = string(FONTDIR);
	bookdir = string(BOOKDIR);
	bookcount = 0;
	bookselected = 0;
	bookcurrent = -1;
	option.reopen = false;
	mode = APP_MODE_BROWSER;
	filebuf = (char*)malloc(sizeof(char) * BUFSIZE);

	screenwidth = SCREEN_WIDTH;
	screenheight = SCREEN_HEIGHT;
	marginleft = MARGINLEFT;
	margintop = MARGINTOP;
	marginright = MARGINRIGHT;
	marginbottom = MARGINBOTTOM;
	linespacing = LINESPACING;
	orientation = 0;
	paraspacing = 1;
	paraindent = 0;
	brightness = 1;

	enableftp = FALSE;
	enablelogging = TRUE;

	prefs = new Prefs(this);
}

App::~App()
{
	free(filebuf);
	delete pages;
	delete prefs;
}           

int App::Run(void)
{
	char filebuf[BUFSIZE];
	char msg[128];

	powerSET(POWER_LCD|POWER_2D_A|POWER_2D_B);
	irqInit();
	irqEnable(IRQ_VBLANK);
	irqEnable(IRQ_VCOUNT);
	REG_IPC_FIFO_CR = IPC_FIFO_ENABLE | IPC_FIFO_SEND_CLEAR;

	InitScreens();

	// Get the filesystem going first so we can write a log.

	if (!fatInitDefault()) {
		Fatal("[filesystem mount failed]");
		while(1) swiWaitForVBlank();
	}

	Log("\n");
	Log("info : dslibris starting up.\n");

	ts = new Text();
	ts->app = this;
	ts->SetFontFile(FONTFILEPATH, TEXT_STYLE_NORMAL);
	ts->SetFontFile(FONTBOLDFILEPATH, TEXT_STYLE_BOLD);
	ts->SetFontFile(FONTITALICFILEPATH, TEXT_STYLE_ITALIC);
	ts->SetFontFile(FONTBROWSERFILEPATH, TEXT_STYLE_BROWSER);
	ts->SetFontFile(FONTSPLASHFILEPATH, TEXT_STYLE_SPLASH);

	XML_Parser p = XML_ParserCreate(NULL);
	if (!p)
	{
		Log("fatal: parser creation failed.\n");
		Fatal("fatal: parser creation failed.\n");
	}
	XML_SetUnknownEncodingHandler(p,unknown_hndl,NULL);
	parse_init(&parsedata);

	// read preferences (to load bookdir)

   	if(!prefs->Read(p))
	{
		Log("warn : could not open preferences.\n");
	} else 
		Log("info : read preferences.\n");
	
	// construct library.

	sprintf(msg,"info : scanning '%s' for books.\n",bookdir.c_str());
	Log(msg);
	
	DIR_ITER *dp = diropen(bookdir.c_str());
	if (!dp)
	{
		Log("fatal: no book directory.\n");
		Fatal("fatal: no book directory.\n");
	}

	char filename[MAXPATHLEN];
	while(!dirnext(dp, filename, NULL))
	{
		char *c;
		for (c=filename;c!=filename+strlen(filename) && *c!='.';c++);
		if (!stricmp(".xht",c) || !stricmp(".xhtml",c))
		{
			Book *book = new Book();
			books.push_back(book);
			book->SetFolderName(bookdir.c_str());
			
			book->SetFileName(filename);
			
			sprintf(msg,"info : indexing book '%s'.\n", book->GetFileName());
			Log(msg);

			u8 rc = book->Index(filebuf);
			if(rc == 255) {
				sprintf(msg, "fatal: cannot index book '%s'.\n",
					book->GetFileName());
				Log(msg);
				Fatal(msg);
			}
			else if(rc == 254) {
				sprintf(msg, "fatal: cannot make book parser.\n");
				Log(msg);
				Fatal(msg);
			}
			sprintf(msg, "info : book title '%s'.\n",book->GetTitle());
			Log(msg);
			bookcount++;
		}
	}
	dirclose(dp);
	swiWaitForVBlank();

	Log("progr: books indexed.\n");

	// read preferences.
	
   	if(!prefs->Read(p))
	{
		Log("warn : could not open preferences.\n");
	}
	else Log("info : read preferences.\n");

	// Sort bookmarks for each book.
	for(u8 i = 0; i < bookcount; i++)
	{
		books[i]->GetBookmarks()->sort();
	}

	// init typesetter.

	int err = ts->Init();
   	if (err) {
		sprintf(msg, "fatal: starting typesetter failed (%d).\n", err);
		Log(msg);
		Fatal(msg);
	} else Log("info : typesetter started.\n");

	// initialize screens.

	//InitScreens();

	Log("progr: display oriented.\n");

	if(orientation) ts->PrintSplash(screen1);
	else ts->PrintSplash(screen0);

	Log("progr: splash presented.\n");

	PrefsInit();
	browser_init();

	Log("progr: browsers populated.\n");

	mode = APP_MODE_BROWSER;
	browser_draw();

	Log("progr: browser displayed.\n");

	if(option.reopen && !OpenBook())
	{
		Log("info : reopened current book.\n");
		mode = APP_MODE_BOOK;
	}
	else Log("warn : could not reopen current book.\n");

	swiWaitForVBlank();

	// start event loop.
	
	keysSetRepeat(60,2);
	bool poll = true;
	while (poll)
	{
		scanKeys();

		if(mode == APP_MODE_BROWSER)
			HandleEventInBrowser();
		else if (mode == APP_MODE_BOOK)
			HandleEventInBook();
		else if (mode == APP_MODE_PREFS)
			HandleEventInPrefs();
		else if (mode == APP_MODE_PREFS_FONT 
			|| mode == APP_MODE_PREFS_FONT_BOLD
			|| mode == APP_MODE_PREFS_FONT_ITALIC)
			HandleEventInFont();

		swiWaitForVBlank();
	}

	if(p)
		XML_ParserFree(p);
	exit(0);
}

void App::CycleBrightness()
{
	brightness++;
	brightness = brightness % 4;
	BGUI::get()->setBacklightBrightness(brightness);
	prefs->Write();
}

void App::UpdateClock()
{
	char tmsg[8];
	sprintf(tmsg, "%02d:%02d", IPC->time.rtc.hours, IPC->time.rtc.minutes);
	u8 offset = marginleft;
	u16 *screen = ts->GetScreen();
	ts->SetScreen(screen0);
	ts->ClearRect(offset, 240, offset+30, 255);
	ts->SetPen(offset,250);
	ts->PrintString(tmsg);
	ts->SetScreen(screen);
}

void App::Log(const char *msg)
{
	if(enablelogging)
	{
		FILE *logfile = fopen(LOGFILEPATH,"a");
		fprintf(logfile,msg);
		fclose(logfile);
	}
}

void App::Log(std::string msg)
{
	if(enablelogging)
	{
		FILE *logfile = fopen(LOGFILEPATH,"a");
		fprintf(logfile,msg.c_str());
		fclose(logfile);
	}
}

void App::Log(int i)
{
	if(enablelogging)
	{
		FILE *logfile = fopen(LOGFILEPATH,"a");
		fprintf(logfile,"%d",i);		
		fclose(logfile);
	}
}

void App::Log(const char *format, const char *msg)
{
	if(enablelogging)
	{
		FILE *logfile = fopen(LOGFILEPATH,"a");
		fprintf(logfile,format,msg);
		fclose(logfile);
	}
}

void App::InitScreens() {
//	NDSX_SetBrightness_0();
//	for(int b=0; b<brightness; b++)
//		NDSX_SetBrightness_Next();

	BACKGROUND.control[3] = BG_BMP16_256x256 | BG_BMP_BASE(0);
	videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	BACKGROUND_SUB.control[3] = BG_BMP16_256x256 | BG_BMP_BASE(0);
	videoSetModeSub(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);

	u16 angle;
	if(orientation) {
		angle = 128;
		BG3_CX = 192 << 8;
		BG3_CY = 0 << 8;
		SUB_BG3_CX = 192 << 8;
		SUB_BG3_CY = 0 << 8;
		screen1 = (u16*)BG_BMP_RAM_SUB(0);
		screen0 = (u16*)BG_BMP_RAM(0);
	}
	else
	{
		angle = 384;
		BG3_CX = 0 << 8;
		BG3_CY = 256 << 8;
		SUB_BG3_CX = 0 << 8;
		SUB_BG3_CY = 256 << 8;
		screen0 = (u16*)BG_BMP_RAM_SUB(0);
		screen1 = (u16*)BG_BMP_RAM(0);
	}

	s16 s = SIN[angle & 0x1FF] >> 4;
	s16 c = COS[angle & 0x1FF] >> 4;
	BG3_XDX = c;
	BG3_XDY = -s;
	BG3_YDX = s;
	BG3_YDY = c;
	SUB_BG3_XDX = c;
	SUB_BG3_XDY = -s;
	SUB_BG3_YDX = s;
	SUB_BG3_YDY = c;

	screen1 = (u16*)BG_BMP_RAM(0);
	screen0 = (u16*)BG_BMP_RAM_SUB(0);
}

void App::Fatal(const char *msg)
{
	//! We never return from this!
	videoSetMode(0);
	videoSetModeSub(MODE_0_2D | DISPLAY_BG0_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG);
	SUB_BG0_CR = BG_MAP_BASE(31);
	BG_PALETTE_SUB[255] = RGB15(31,31,31);
	consoleInitDefault(
		(u16*)SCREEN_BASE_BLOCK_SUB(31),
		(u16*)CHAR_BASE_BLOCK_SUB(0),16);
	iprintf(msg);
	while(1) swiWaitForVBlank();
}

