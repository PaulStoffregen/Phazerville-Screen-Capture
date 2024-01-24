#include "gui.h"
#include "usb.h"
#include "icon/icon.xpm"

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
	// open config settings, get previous window position and size
	wxConfig *config = new wxConfig("phazerville_screencapture", "Paul");
	const long screen_x = wxSystemSettings::GetMetric(wxSYS_SCREEN_X);
	const long screen_y = wxSystemSettings::GetMetric(wxSYS_SCREEN_Y);
	long w = config->Read("w", 360);
	if (w < 150) w = 150;
	if (w > screen_x / 2) w = screen_x / 2;
	long h = config->Read("h", 240);
	if (h < 120) h = 120;
	if (h > screen_y / 2) h = screen_y / 2;
	long x = config->Read("x", screen_x / 2 - w / 2);
	long y = config->Read("y", screen_y / 2 - h / 2);
	srand(getpid() + time(NULL));
	// create main window
	MyFrame *main_window = new MyFrame(x, y, w, h, config);
	main_window->Show(true);
	return true;
}

int MyApp::OnExit()
{
	return 0;
}

MyFrame::MyFrame(long x, long y, long w, long h, wxConfig *c) :
	wxFrame(NULL, wxID_ANY, "Phazerville Screen Capture", wxPoint(x, y), wxSize(w, h))
{
	// get the scale setting
	config = c;
	scale = config->Read("s", 3);
	if (scale < 1) scale = 1;
	else if (scale > MAX_SCALE) scale = MAX_SCALE;
	color = COLOR_WHITE;
	// create the GUI
	wxMenu *menuFile = new wxMenu;
	menuFile->Append(wxID_EXIT);
	wxMenu *menuEdit = new wxMenu;
	menuEdit->Append(wxID_COPY);
	wxMenu *menuScale = new wxMenu;
	for (unsigned int i=1; i <= MAX_SCALE; i++) {
		char buf[4];
		snprintf(buf, sizeof(buf), "%uX", i);
		menuScale->AppendRadioItem(ID_SCALE1 + i - 1, buf);
	}
	menuScale->Check(ID_SCALE1 + scale - 1, true);
	wxMenu *menuColor = new wxMenu;
	menuColor->AppendRadioItem(ID_COLOR1, "White");
	menuColor->AppendRadioItem(ID_COLOR2, "Pale");
	menuColor->AppendRadioItem(ID_COLOR3, "Random");
	wxMenuBar *menuBar = new wxMenuBar;
	menuBar->Append(menuFile, "&File");
	menuBar->Append(menuEdit, "&Edit");
	menuBar->Append(menuScale, "&Scale");
	menuBar->Append(menuColor, "&Color");
	SetMenuBar(menuBar);
	CreateStatusBar();
	SetStatusText("For Science");
	SetIcon(wxIcon(icon_xpm));
	wxPanel *panel = new wxPanel(this, wxID_ANY);
	panel->SetDoubleBuffered(true);
	memset(framebuffer, 0, sizeof(framebuffer));
	bitmap = new wxStaticBitmap(panel, wxID_ANY,
		wxImage((128+MARGIN*2)*scale, (64+MARGIN*2)*scale, framebuffer, true));
	// set up GUI events to run functions
	Bind(wxEVT_THREAD, &MyFrame::OnRawData, this, ID_RAW_DATA);
	Bind(wxEVT_MENU, &MyFrame::OnCopy, this, wxID_COPY);
	Bind(wxEVT_COMMAND_MENU_SELECTED, &MyFrame::OnScale, this, ID_SCALE1,
		ID_SCALE1 + MAX_SCALE - 1);
	Bind(wxEVT_COMMAND_MENU_SELECTED, &MyFrame::OnColor, this, ID_COLOR1, ID_COLOR3);
	Bind(wxEVT_MENU, &MyFrame::OnExit, this, wxID_EXIT);
	// start the USB device detection thread
	USB_Device_Detect_Thread *t = new USB_Device_Detect_Thread(this);
	t->Run();
}

// from javascript at https://eh2k.github.io/%E2%96%A1%E2%97%8F/display.html
static bool getPixel(const uint8_t *data, size_t x, size_t y)
{
	return ((data[x + (y / 8) * 128] & (1 << (y & 7))) == 0) ? true : false;
}

void MyFrame::OnRawData(wxThreadEvent& event)
{
	RawScreenData data = event.GetPayload<RawScreenData>();
	char linebuffer[128*MAX_SCALE*3];
	const size_t fb_linelen = (128 + MARGIN * 2) * scale * 3;
	const size_t top_margin = fb_linelen * MARGIN * scale;
	const size_t left_margin = MARGIN * scale * 3;
	for (size_t y=0; y < 64; y++) {
		char *p = linebuffer;
		for (size_t x=0; x < 128; x++) {
			uint32_t c = getPixel(data.buf, x, y) ? 0 : color;
			for (size_t i=0; i < scale; i++) {
				*p++ = c >> 16;
				*p++ = c >> 8;
				*p++ = c;
			}
		}
		size_t offset = y * fb_linelen * scale + top_margin;
		for (size_t i=0; i < scale; i++) {
			memcpy(framebuffer + offset + i * fb_linelen + left_margin,
				linebuffer, 128 * scale * 3);
		}
	}
	bitmap->SetBitmap(wxImage((128+MARGIN*2)*scale, (64+MARGIN*2)*scale, framebuffer, true));
}

void MyFrame::OnCopy(wxCommandEvent& event)
{
	wxInitAllImageHandlers();
	wxClipboard *clipboard = wxClipboard::Get();
	if (clipboard && clipboard->Open()) {
		printf("Copy to clipboard\n");
		wxBitmapDataObject *data = new wxBitmapDataObject(wxBitmap(
			wxImage((128+MARGIN*2)*scale, (64+MARGIN*2)*scale, framebuffer, true)));
		clipboard->SetData(data);
		clipboard->Close();
	}
}

void MyFrame::OnScale(wxCommandEvent& event)
{
	unsigned int newscale = event.GetId() - ID_SCALE1 + 1;
	printf("scale event, newscale = %u\n", newscale);
	if (newscale < 1 || newscale > MAX_SCALE) return;
	scale = newscale;
	memset(framebuffer, 0, sizeof(framebuffer));
	bitmap->SetBitmap(wxImage((128+MARGIN*2)*scale, (64+MARGIN*2)*scale, framebuffer, true));
}

void MyFrame::OnColor(wxCommandEvent& event)
{
	switch (event.GetId()) {
	  case ID_COLOR1:
		color = COLOR_WHITE;
		break;
	  case ID_COLOR2:
		color = COLOR_PALE;
		break;
	  case ID_COLOR3:
		uint8_t r = rand() % 156 + 100;
		uint8_t g = rand() % 156 + 100;
		uint8_t b = rand() % 156 + 100;
		color = (r << 16) | (g << 8) | b;
		break;
	}
}

MyFrame::~MyFrame()
{
	wxSize size = GetSize();
	config->Write("w", size.GetWidth());
	config->Write("h", size.GetHeight());
	wxPoint pos = GetScreenPosition();
	config->Write("x", pos.x);
	config->Write("y", pos.y);
	config->Write("s", scale);
	config->Flush();
}
 
void MyFrame::OnExit(wxCommandEvent& event)
{
	Unlink();  // Unbind(wxEVT_THREAD, &MyFrame::OnRawData);
	Close(true);
}
 
