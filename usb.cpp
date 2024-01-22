#include "gui.h"
#include "usb.h"
#include <thread>

#ifdef LINUX
#include <libudev.h>            // sudo apt-get install libudev-dev
#include <linux/hidraw.h>
#include <poll.h>

#endif


// input str expects 2048 uppercase hex characters
void receive_data(const char *str)
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


void usb_comm_thread(char *devpath)
{
	int fd = open(devpath, O_RDWR);
	free(devpath);
	if (fd < 0) return;
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
}	



void udev_add(struct udev_device *dev)
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
	
	std::thread *t = new std::thread(usb_comm_thread, strdup(path));
	t->detach();
}

void usb_scan_thread()
{
	struct udev *udev;
	struct udev_monitor *mon;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *entry;
	struct udev_device *dev;
	const char *path;

	udev = udev_new();
	if (!udev) return;
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
		udev_add(dev);
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
					udev_add(dev);
				}
				udev_device_unref(dev);
                        }
                }
        }
}

#endif // LINUX

