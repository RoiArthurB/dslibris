#ifndef _text_h
#define _text_h

#include <map>
#include <ft2build.h>
#include FT_FREETYPE_H

using namespace std;

#define MAXGLYPHS 256
#define FONTFILENAME "dslibris.ttf"

#define PIXELSIZE 10
#define DPI 72	/** probably not true for a DS - measure it **/
#define EMTOPIXEL (float)(POINTSIZE * DPI/72.0)

class Text {
	FT_Library      library;
	FT_Face         face;
	FT_GlyphSlotRec glyphs[MAXGLYPHS];
	FT_Vector       pen;
	FT_Error        error;

	// associates each cached glyph with it's Unicode identifier.
	// give the ucs id, get the element of glyphs[].
	map<int,int> ucsmap;

	u16  *screen, *screenleft, *screenright;
	u8   pixelsize;
	bool usecache;
	bool invert;
	bool justify;
	
public:
	int  InitDefault(void);
	void InitPen(void);
	u8   GetAdvance(u16 code);
	u8   GetHeight(void);
	bool GetInvert();
	void GetPen(u16 *x, u16 *y);
	u8   GetPenX();
	u8   GetPenY();
	u8   GetPixelSize();
	u16* GetScreen();
	u8   GetStringWidth(const char *txt);
	u8   GetUCS(const char *txt, u16 *code);
	void SetInvert(bool);
	void SetPen(u16 x, u16 y);
	void SetPixelSize(u8);
	void SetScreen(u16 *s);

	void Cache();
	void Cache_old();
	void ClearScreen();

	void PrintCache(void);
	void PrintChar(u16 code);
	void PrintString(const char *string);
	bool PrintNewLine(void);
};

#endif

