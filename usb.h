#pragma once
#include "gui.h"


#ifdef WINDOWS
#if 1
#define printf(...) printf_logfile(__VA_ARGS__) // send all printf to a log file
#else
#define printf(...) // just discard all printf
#endif
int printf_logfile(const char *format, ...) __attribute__((format (printf, 1, 2)));
#endif // WINDOWS


// wxThread instances must be created on the heap with C++ new.
// If wxTHREAD_DETACHED is used, no other functions can be
// called after Run() starts the thread running.

class USB_Device_Detect_Thread : public wxThread
{
public:
	USB_Device_Detect_Thread(MyFrame *window) :
		wxThread(wxTHREAD_DETACHED), main_window(window) {}
	virtual ExitCode Entry();
private:
	MyFrame *main_window;
};


class USB_Communication_Thread : public wxThread
{
public:
	USB_Communication_Thread(MyFrame *window, char *path) :
		 wxThread(wxTHREAD_DETACHED), main_window(window), devpath(path) {}
	virtual ExitCode Entry();
	void receive_data(const char *str);
private:
	MyFrame *main_window;
	char *devpath;
};


