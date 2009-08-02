#include <nds.h>
#include <stdio.h>
#include "Button.h"

Button::Button() {
}

void Button::Init(Text *typesetter) {
	ts = typesetter;
	origin.x = 0;
	origin.y = 0;
	extent.x = 192;
	extent.y = 32;
	text = "";
}

void Button::Label(const char *s) {
	std::string str = s;	
	SetLabel(str);
}

void Button::SetLabel(std::string &s) {
	text = s.substr(0,26);
	if(s.length() > 27) text.append("...");
}

void Button::Move(u16 x, u16 y) {
	origin.x = x;
	origin.y = y;
}

void Button::Resize(u16 x, u16 y) {
	extent.x = x;
	extent.y = y;
}

void Button::Draw(u16 *fb, bool highlight) {
	coord_t ul, lr;
	ul.x = origin.x;
	ul.y = origin.y;
	lr.x = origin.x + extent.x;
	lr.y = origin.y + extent.y;

	u16 x;
	u16 y;

	u16 bgcolor;
	if(highlight) bgcolor = RGB15(31,31,15) | BIT(15);
	else bgcolor = RGB15(31,31,31) | BIT(15);
	for (y=ul.y;y<lr.y;y++) {
		for (x=ul.x;x<lr.x;x++) {
			fb[y*SCREEN_WIDTH + x] = bgcolor;
		}
	}

	u16 bordercolor = RGB15(22,22,22) | BIT(15);
	for (x=ul.x;x<lr.x;x++) {
		fb[ul.y*SCREEN_WIDTH + x] = bordercolor;
		fb[lr.y*SCREEN_WIDTH + x] = bordercolor;
	}
	for (y=ul.y;y<lr.y;y++) {
		fb[y*SCREEN_WIDTH + ul.x] = bordercolor;
		fb[y*SCREEN_WIDTH + lr.x-1] = bordercolor;
	}

	bool invert = ts->GetInvert();
	ts->SetScreen(fb);
	ts->SetInvert(false);
	ts->GetPen(&x,&y);
	ts->SetPen(ul.x+6, ul.y + ts->GetHeight());
	if(highlight) ts->usebgcolor = true;
	ts->PrintString((const char*)text.c_str(), TEXT_STYLE_BROWSER);
	ts->usebgcolor = false;
	ts->SetPen(x,y);
	ts->SetInvert(invert);
}

bool Button::EnclosesPoint(u16 x, u16 y)
{
	if (x > origin.x && 
		y > origin.y && 
		x < origin.x + extent.x && 
		y < origin.y + extent.y) return true;
	return false;
}
