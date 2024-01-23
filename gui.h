#pragma once
#include "wx_all.h"

#define MARGIN 3
#define COLOR_WHITE 0xFFFFFF
#define COLOR_PALE  0xB8F0FF
#define MAX_SCALE 10

class MyApp : public wxApp
{
public:
	virtual bool OnInit();
	virtual int OnExit();
};

class MyFrame : public wxFrame
{
public:
	MyFrame(long x, long y, long w, long h, wxConfig *c);
	~MyFrame();
private:
	wxStaticBitmap *bitmap;
	size_t scale;
	uint32_t color;
	unsigned char framebuffer[((128+MARGIN*2)*MAX_SCALE)*((64+MARGIN*2)*MAX_SCALE)*3];
	wxConfig *config;
	void OnRawData(wxThreadEvent& event);
	void OnCopy(wxCommandEvent& event);
	void OnScale(wxCommandEvent& event);
	void OnColor(wxCommandEvent& event);
	void OnExit(wxCommandEvent& event);
};

class RawScreenData
{
public:
	uint8_t buf[1024];
};

#define ID_RAW_DATA	1
#define ID_SCALE1	2 // +MAX_SCALE more
#define ID_COLOR1	100
#define ID_COLOR2	101
#define ID_COLOR3	102
