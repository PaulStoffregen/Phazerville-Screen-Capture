#include "gui.h"
#include "usb.h"

#ifdef LINUX
#include <libudev.h>            // sudo apt-get install libudev-dev
#include <linux/hidraw.h>
#include <poll.h>
#endif
#ifdef WINDOWS
#include <windows.h>
#include <winuser.h>
extern "C" {
#include <setupapi.h>
#include <dbt.h>
#include <sys/time.h>
#include <hidsdi.h>
#include <usbiodef.h>
#include <cfgmgr32.h>
#include <wctype.h>
#include <devguid.h>
#include <hidpi.h>
}
#endif
#ifdef MACOS
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#endif


// input str expects 2048 uppercase hex characters
void USB_Communication_Thread::receive_data(const char *str)
{
	//static unsigned int counter=0;
	//printf("receive %u\n%s\n", ++counter, str);
	RawScreenData bin;

	for (int i=0; i < 1024; i++) {
		char c = *str++;
		if (c >= '0' && c <= '9') {
			bin.buf[i] = (c - '0') * 16;	
		} else if (c >= 'A' && c <= 'F') {
			bin.buf[i] = (c - 'A' + 10) * 16;	
		} else {
			return;
		}
		c = *str++;
		if (c >= '0' && c <= '9') {
			bin.buf[i] += (c - '0');	
		} else if (c >= 'A' && c <= 'F') {
			bin.buf[i] += (c - 'A' + 10);	
		} else {
			return;
		}
	}
	// transmit event to GUI with 1024 bytes raw data
	wxThreadEvent *event = new wxThreadEvent(wxEVT_THREAD, ID_RAW_DATA);
	event->SetPayload<RawScreenData>(bin);
	wxQueueEvent(main_window, event);
}




#ifdef LINUX


wxThread::ExitCode USB_Communication_Thread::Entry()
{
	int fd = open(devpath, O_RDWR);
	free(devpath);
	if (fd < 0) return NULL;
	printf("begin listening\n");
	while (1) {
		uint8_t wbuf[32];
		memset(wbuf, 0, sizeof(wbuf));
		wbuf[0] = 'S';
		int w = write(fd, wbuf, 32);
		//printf(" w = %d\n", w);
		if (w != 32) {
			printf(" write error\n");
			break;
		}
		uint8_t rbuf[2560];
		memset(rbuf, 0, sizeof(rbuf));
		int count = 0;
		while (count < 2049) {
			struct pollfd p;
			p.fd = fd;
			p.events = POLLIN;
			p.revents = 0;

			int r = poll(&p, 1, 100);
			if (r < 0) {
				printf("poll error\n");
				break;
			} else if (r == 0) {
				printf("poll timeout\n");
				break;
			} else {
				if (p.revents == POLLIN) {
					//printf("poll success\n");
					r = read(fd, rbuf + count, sizeof(rbuf) - count);
					if (r < 0) break;
					int n=0;
					while (n < r && isxdigit(rbuf[count + n])) n++;
					//printf(" r = %d, n = %d\n", r, n);
					//int i;
					//for (i=0; i < n; i++) printf("%c", rbuf[count + i]);
					//printf("\n");
					count += n;
					if (n < 64) break;
				} else {
					printf("poll error status\n");
					break;
				}
			}
		}
		rbuf[count] = 0;
		//printf("count = %d: %s\n", count, rbuf);
		if (count == 2048) {
			receive_data((char *)rbuf);
		}
	}
	close(fd);
	return NULL;
}	



void udev_add(struct udev_device *dev, MyFrame *window)
{
	struct udev_device *pdev;
	const char *vidname, *pidname, *path;
	int vid, pid;

	if (!dev) return;
	pdev = udev_device_get_parent_with_subsystem_devtype(dev, "usb", "usb_device");
	vidname = udev_device_get_sysattr_value(pdev, "idVendor");
	pidname = udev_device_get_sysattr_value(pdev, "idProduct");
	if (!vidname || !pidname) return;
	if (sscanf(vidname, "%x", &vid) != 1) return;
	if (sscanf(pidname, "%x", &pid) != 1) return;
	if (vid != 0x16C0 || pid < 0x0476 || pid > 0x04D8) return;
	path = udev_device_get_devnode(dev);
	if (!path) return;
	printf("found %04x:%04x -> %s\n", vid, pid, path);
	USB_Communication_Thread *t = new USB_Communication_Thread(window, strdup(path));
	t->Run();
}

wxThread::ExitCode USB_Device_Detect_Thread::Entry()
{
	struct udev *udev;
	struct udev_monitor *mon;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *entry;
	struct udev_device *dev;
	const char *path;

	udev = udev_new();
	if (!udev) return NULL;
	mon = udev_monitor_new_from_netlink(udev, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, "hidraw", NULL);
	udev_monitor_enable_receiving(mon);
	enumerate = udev_enumerate_new(udev);
	udev_enumerate_add_match_subsystem(enumerate, "hidraw");
	udev_enumerate_scan_devices(enumerate);
	devices = udev_enumerate_get_list_entry(enumerate);
	udev_list_entry_foreach(entry, devices) {
		path = udev_list_entry_get_name(entry);
		dev = udev_device_new_from_syspath(udev, path);
		udev_add(dev, main_window);
		udev_device_unref(dev);
	}
	udev_enumerate_unref(enumerate);

        int mon_fd = udev_monitor_get_fd(mon);
        int errcount = 0;
        while (1) {
                fd_set rd;
                FD_ZERO(&rd);
                FD_SET(mon_fd, &rd);
                int r = select(mon_fd + 1, &rd, NULL, NULL, NULL);
                if (r < 0) {
                        if (++errcount > 10) break;
                } else if (r == 0) {
                        continue; // timeout
                } else {
                        errcount = 0;
                        if (FD_ISSET(mon_fd, &rd)) {
                                //usb_device_scan(1);
				dev = udev_monitor_receive_device(mon);
				const char *action = udev_device_get_action(dev);
				if (action && strcmp(action, "add") == 0) {
					udev_add(dev, main_window);
				}
				udev_device_unref(dev);
                        }
                }
        }
	return NULL;
}

#endif // LINUX






#ifdef WINDOWS

wxThread::ExitCode USB_Communication_Thread::Entry()
{
	HANDLE h = CreateFile((wchar_t *)devpath, GENERIC_READ | GENERIC_WRITE, 0, NULL,
		OPEN_EXISTING, /*FILE_FLAG_OVERLAPPED |*/ FILE_FLAG_NO_BUFFERING, NULL);
	free(devpath);
	if (h == INVALID_HANDLE_VALUE) return NULL;
	printf("begin listening\n");
	bool ok = true;
	while (ok) {
		uint8_t txbuf[33];
		memset(txbuf, 0, sizeof(txbuf));
		txbuf[1] = 'S';
		DWORD nbyte = 33;
		if (!WriteFile(h, txbuf, 33, &nbyte, NULL)) {
			printf(" write error\n");
			break;
		}
		uint8_t rbuf[2560];
		memset(rbuf, 0, sizeof(rbuf));
		int count = 0;
		while (count < 2049) {
			char buf[65];
			if (ReadFile(h, buf, 65, &nbyte, NULL) || nbyte != 65) {
				int n=0;
				while (n < 64 && isxdigit(buf[1 + n])) n++;
				//printf(" read ok, nbyte = %lu, n = %d\n", nbyte, n);
				if (n) memcpy(rbuf + count, buf + 1, n);
				count += n;
				if (n < 64) break;
			} else {
				printf(" read error\n");
				ok = false;
				break;
			}
		}
		rbuf[count] = 0;
		//printf("count = %d: %s\n", count, rbuf);
		if (count == 2048) {
			receive_data((char *)rbuf);
		}
	}
	CloseHandle(h);
	return NULL;
}


static MyFrame *main_window_pointer;

// Windows doesn't tell us add or remove events, only that something changes,
// so we must maintain this list of previously seen paths to detect add & remove.
typedef struct known_device_struct {
	bool seen;
	wchar_t *path;
	struct known_device_struct *prev;
	struct known_device_struct *next;
} known_device_t;

static known_device_t *known_devices;

static void mark_all_devices_unseen()
{
	for (known_device_t *dev=known_devices; dev; dev = dev->next) {
		dev->seen = false;
	}
}

static bool is_previously_unseen_device(const wchar_t *path)
{
	for (known_device_t *dev=known_devices; dev; dev = dev->next) {
		if (wcscmp(path, dev->path) == 0) {
			dev->seen = true;
			return false;
		}
	}
	known_device_t *newdev = (known_device_t *)malloc(sizeof(known_device_t));
	wchar_t *pathcopy = wcsdup(path);
	if (!newdev || !pathcopy) return false;
	newdev->seen = true;
	newdev->path = pathcopy;
	newdev->prev = NULL;
	newdev->next = known_devices;
	if (known_devices) known_devices->prev = newdev;
	known_devices = newdev;
	return true;
}

static void unseen_devices_are_assumed_removed()
{
	known_device_t *dev=known_devices;
	while (dev) {
		if (dev->seen == false) {
			printf(" remove : %ls\n", dev->path);
			if (dev->prev) {
				dev->prev->next = dev->next;
			} else {
				known_devices = dev->next;
			}
			if (dev->next) {
				dev->next->prev = dev->prev;
			}
			known_device_t *next = dev->next;
			free(dev->path);
			free(dev);
			dev = next;
		} else {
			dev = dev->next;
		}
	}
}

static bool check_hid_interface(const wchar_t *devpath)
{
	HIDD_ATTRIBUTES attrib;
	PHIDP_PREPARSED_DATA hid_data;
	HIDP_CAPS capabilities;

	HANDLE h = CreateFile(devpath, GENERIC_READ|GENERIC_WRITE,
		FILE_SHARE_READ|FILE_SHARE_WRITE, NULL,
		OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
	if (h == INVALID_HANDLE_VALUE) return false;
	do {
		attrib.Size = sizeof(HIDD_ATTRIBUTES);
		if (!HidD_GetAttributes(h, &attrib)) break;
		const int vid = attrib.VendorID;
		const int pid = attrib.ProductID;
		if (vid != 0x16C0 || pid < 0x0476 || pid > 0x04D8) break;
		if (!HidD_GetPreparsedData(h, &hid_data)) break;
		bool capsok = HidP_GetCaps(hid_data, &capabilities);
		const int usepage = capabilities.UsagePage;
		const int use = capabilities.Usage;
		HidD_FreePreparsedData(hid_data);
		if (!capsok || usepage != 0xFFC9 || use != 4) break;
		CloseHandle(h);
		printf(" found Teensy seremu interface\n");
		return true;
	} while (0);
	CloseHandle(h);
	return false;
}


static void usb_scan()
{
	GUID guid;
	HDEVINFO info;
	DWORD index=0;
	SP_DEVICE_INTERFACE_DATA iface;
	static SP_DEVICE_INTERFACE_DETAIL_DATA *details=NULL;
	SP_DEVINFO_DATA devinfo;
	DWORD size;
	BOOL ret;

	HidD_GetHidGuid(&guid);
	if (!details) {
		details = (SP_DEVICE_INTERFACE_DETAIL_DATA *)malloc(65536);
		if (!details) return;
	}
	mark_all_devices_unseen();
	info = SetupDiGetClassDevs(&guid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
	for (index=0; info != INVALID_HANDLE_VALUE; index++) {
		//printf("hid, index=%ld\n", index);
		iface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
		ret = SetupDiEnumDeviceInterfaces(info, NULL, &guid, index, &iface);
		if (!ret) break;
		SetupDiGetInterfaceDeviceDetail(info, &iface, NULL, 0, &size, NULL);
		//printf("  detail size = %lu (hopefully less than 65536)\n", size);
		if (size > 65536) continue;
		memset(details, 0, size);
		details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		memset(&devinfo, 0, sizeof(devinfo));
		devinfo.cbSize = sizeof(devinfo);
		ret = SetupDiGetDeviceInterfaceDetail(info, &iface, details, size, NULL, &devinfo);
		if (!ret) continue;
		if (is_previously_unseen_device(details->DevicePath)) {
			printf(" add: %ls\n", details->DevicePath);
			if (check_hid_interface(details->DevicePath)) {
				wchar_t *path = wcsdup(details->DevicePath);
				USB_Communication_Thread *t =
					new USB_Communication_Thread(main_window_pointer,
					(char *)path);
				t->Run();
			}
		}
	}
	if (info != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(info);
	unseen_devices_are_assumed_removed();
}


extern "C" LRESULT CALLBACK window_callback(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_DEVICECHANGE) {
		printf("window_callback WM_DEVICECHANGE\n");
		usb_scan();
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

#include "wx/msw/private.h" // https://forums.wxwidgets.org/viewtopic.php?t=2999

wxThread::ExitCode USB_Device_Detect_Thread::Entry()
{
	printf("USB_Device_Detect_Thread begin\n");
	main_window_pointer = main_window;
	usb_scan();
	WNDCLASS window;
	memset(&window, 0, sizeof(window));
	window.style = CS_NOCLOSE;
	window.lpfnWndProc = window_callback;
	window.hInstance = GetModuleHandle(NULL);
	window.hCursor = LoadCursor(0, IDC_ARROW);
	window.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
	window.lpszClassName = TEXT("Teensy Port Discovery");
	if (!RegisterClass(&window)) return NULL;
	HWND hWnd = CreateWindow(window.lpszClassName, L"dummy window", 0,
		50, 50, 100, 100, NULL, NULL, wxGetInstance(), NULL);
	if (!hWnd) return NULL;
	SetTimer(hWnd, 0, 1171, NULL);
	while (1) {
		WaitMessage();
		MSG msg;
		while (GetMessage(&msg, hWnd, 0, 0)) {
			if (msg.message == WM_DEVICECHANGE) {
				printf("USB_Device_Detect_Thread WM_DEVICECHANGE\n");
				usb_scan();
			}
			if (msg.message == WM_TIMER) {
				// wastes CPU time, but Windows messages sometimes
				// get lost, so sadly this is needed in some cases
				//printf("USB_Device_Detect_Thread WM_TIMER\n");
				usb_scan();
			}
		}
	}
	return NULL;
}

int printf_logfile(const char *format, ...)
{
	va_list args;
	static FILE *fp=NULL;
	if (!fp) {
		fp = fopen("/tmp/logfile.txt", "a");
		if (!fp) return 0;
	}
	va_start(args, format);
	int r = vfprintf(fp, format, args);
	va_end(args);
	fflush(fp);
	return r;
}

#endif // WINDOWS










#ifdef MACOS

extern "C" void attach_callback(void *context, IOReturn r, void *hid_mgr, IOHIDDeviceRef dev);
extern "C" void receive_callback(void *context, IOReturn r, void *dev, IOHIDReportType type,
	uint32_t id, uint8_t *report, CFIndex len);

typedef struct {
	uint8_t rbuf[2560];
	uint8_t hidbuf[64];
	int count;
	bool end;
} rxstate_t;

wxThread::ExitCode USB_Communication_Thread::Entry()
{
	IOHIDDeviceRef dev = (IOHIDDeviceRef)devpath;
	if (IOHIDDeviceOpen(dev, kIOHIDOptionsTypeNone) != kIOReturnSuccess) return NULL;
	CFRelease(dev);
	rxstate_t state;
	IOHIDDeviceScheduleWithRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDDeviceRegisterInputReportCallback(dev, state.hidbuf, 64, receive_callback, &state);
	printf("begin listening\n");

	while (1) {
		state.count = 0;
		state.end = false;
		uint8_t wbuf[32];
		memset(wbuf, 0, sizeof(wbuf));
		wbuf[0] = 'S';
		int r = IOHIDDeviceSetReport(dev, kIOHIDReportTypeOutput, 0, wbuf, 32);
		if (r != kIOReturnSuccess) {
			printf(" write error\n");
			break;
		}
		//printf("write ok?\n");
		while (state.end == false) {
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, INFINITY, true);
		}
		state.rbuf[state.count] = 0;
		//printf(" rx count=%u, data = %s\n", state.count, state.rbuf);
		if (state.count == 2048) {
			receive_data((char *)state.rbuf);
		}
	}
	return NULL;
}

void receive_callback(void *context, IOReturn r, void *dev, IOHIDReportType type, uint32_t id,
        uint8_t *report, CFIndex len)
{
	rxstate_t &state = *(rxstate_t *)context;
	unsigned int n = 0;
	while (n < len && isxdigit(state.hidbuf[n])) n++;
	if (n != 64) state.end = true;
	//printf("receive callback, len = %ld, n=%u\n", len, n);
	if (n > 0 && state.count + n < sizeof(state.rbuf)) {
		memcpy(state.rbuf + state.count, state.hidbuf, n);
		state.count += n;
	}
}

wxThread::ExitCode USB_Device_Detect_Thread::Entry()
{
	IOHIDManagerRef hid_manager;
	CFMutableDictionaryRef dict;

	printf("USB_Device_Detect_Thread begin\n");
	hid_manager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
	if (!hid_manager) return NULL;
	dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	int vid = 0x16C0;
	CFDictionarySetValue(dict, CFSTR(kIOHIDVendorIDKey),
		CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &vid));
	int usepage = 0xFFC9;
	CFDictionarySetValue(dict, CFSTR(kIOHIDPrimaryUsagePageKey),
		CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usepage));
	int usage = 4;
	CFDictionarySetValue(dict, CFSTR(kIOHIDPrimaryUsageKey),
		CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage));
	if (!dict) return NULL;
	IOHIDManagerSetDeviceMatching(hid_manager, dict);
	CFRelease(dict);
	IOHIDManagerScheduleWithRunLoop(hid_manager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	IOHIDManagerRegisterDeviceMatchingCallback(hid_manager, attach_callback, main_window);
	if (IOHIDManagerOpen(hid_manager, kIOHIDOptionsTypeNone) != kIOReturnSuccess) return NULL;
	printf("HID Manager started\n");
	unsigned int loopcount=0;
	while (1) {
		CFRunLoopRun();
		printf("restart runloop %u\n", ++loopcount);
		usleep(10000);
	}
	return NULL;
}

void attach_callback(void *context, IOReturn r, void *hid_mgr, IOHIDDeviceRef dev)
{
	printf("attach_callback\n");
	CFTypeRef type = IOHIDDeviceGetProperty(dev, CFSTR(kIOHIDProductIDKey));
	if (!type) return;
	if (CFGetTypeID(type) != CFNumberGetTypeID()) return;
	int pid=0;
	if (!CFNumberGetValue((CFNumberRef)type, kCFNumberSInt32Type, &pid)) return;
	printf(" pid=%04X\n", pid);
	if (pid < 0x0476 || pid > 0x04D8) return;
	CFRetain(dev);
	IOHIDDeviceUnscheduleFromRunLoop(dev, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
	MyFrame *window = (MyFrame *)context;
	USB_Communication_Thread *t = new USB_Communication_Thread(window, (char *)dev);
	t->Run();
}



#endif // MACOS

