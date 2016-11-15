/*
 * Proof of concept for FTE1001 driver - built using the Hidraw Userspace Example
 *
 * Copyright (c) 2010 Alan Ott <alan@signal11.us>
 * Copyright (c) 2010 Signal 11 Software
 *
 * The code may be used by anyone for any purpose,
 * and can serve as a starting point for developing
 * applications using hidraw.
 */

/* Linux */
#include <linux/types.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <linux/hidraw.h>

/*
 * Ugly hack to work around failing compilation on systems that don't
 * yet populate new version of hidraw.h to userspace.
 */
#ifndef HIDIOCSFEATURE
#warning Please have your distro update the userspace kernel headers
#define HIDIOCSFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x06, len)
#define HIDIOCGFEATURE(len)    _IOC(_IOC_WRITE|_IOC_READ, 'H', 0x07, len)
#endif

/* Unix */
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* C */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define MAX_CONTACTS 5
#define MAX_EVENTS (4 + 4*MAX_CONTACTS + 2)

#define CONTACT_SIZE 5	

#define MAX_X 0x0aea
#define MAX_Y 0x06de

int createUIDev(int ifd) {
	int res = ioctl(ifd, UI_SET_EVBIT, EV_SYN);
	res = ioctl(ifd, UI_SET_EVBIT, EV_KEY);
	res = ioctl(ifd, UI_SET_KEYBIT, BTN_LEFT);
	res = ioctl(ifd, UI_SET_KEYBIT, BTN_TOOL_FINGER);
	res = ioctl(ifd, UI_SET_KEYBIT, BTN_TOUCH);
	res = ioctl(ifd, UI_SET_KEYBIT, BTN_TOOL_DOUBLETAP);
	res = ioctl(ifd, UI_SET_KEYBIT, BTN_TOOL_TRIPLETAP);
	res = ioctl(ifd, UI_SET_KEYBIT, BTN_TOOL_QUADTAP);
	res = ioctl(ifd, UI_SET_EVBIT, EV_ABS);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_X);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_Y);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_SLOT);
	res = ioctl(ifd, UI_SET_PROPBIT, INPUT_PROP_POINTER);
	res = ioctl(ifd, UI_SET_PROPBIT, INPUT_PROP_BUTTONPAD);

	struct uinput_user_dev uidev;
	memset(&uidev, 0, sizeof(uidev));
	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "FTE1001:00 0B05:0101 Userspace Driver");
	uidev.id.bustype = BUS_I2C;
	uidev.id.vendor  = 0x0b05;
	uidev.id.product = 0x0101;
	uidev.id.version = 1;
	uidev.absmin[ABS_X] = uidev.absmin[ABS_MT_POSITION_X] = 0;
	uidev.absmin[ABS_Y] = uidev.absmin[ABS_MT_POSITION_Y] = 0;
	uidev.absmax[ABS_X] = uidev.absmax[ABS_MT_POSITION_X] = MAX_X;
	uidev.absmax[ABS_Y] = uidev.absmax[ABS_MT_POSITION_Y] = MAX_Y;
	uidev.absmax[ABS_MT_SLOT] = MAX_CONTACTS - 1;

	res = write(ifd, &uidev, sizeof(uidev));

	res = ioctl(ifd, UI_DEV_CREATE);

	return res;
}

int startMultiTouch(int fd) {
	unsigned char buf[5];
	/* Set Feature */
	buf[0] = 0x0d; /* Report Number */
	buf[1] = 0x00;
	buf[2] = 0x03;
	buf[3] = 0x01;
	buf[4] = 0x00;
	int res = ioctl(fd, HIDIOCSFEATURE(5), buf);
	if (res < 0)
		perror("HIDIOCSFEATURE");

	return res;
}

int mainLoop(int fd, int ifd) {
	unsigned char buf[28];
	int i, res, trackingid = 0;
	int contacts[MAX_CONTACTS];
	for (i = 0; i < MAX_CONTACTS; i++) contacts[i] = -1;

        while(1) {
		res = read(fd, buf, sizeof(buf));
		if (res <= 0) {
			perror("read");
		} else if (buf[0] == 0x5d) {
			struct input_event ev[MAX_EVENTS];
                        int contact = 0;
			int eventNum = 0, contactNum = 0;

			memset(&ev, 0, sizeof(ev));

			for (i = 0; i < MAX_CONTACTS; i++) {
				int report = 0;
				if (buf[1] & (0x08 << i)) {
					report = contact = 1;
					if (contacts[i] == -1) {
						contacts[i] = trackingid++;
						if (trackingid < 0) trackingid = 0;
					}

				} else if (contacts[i] != -1) {
					report = 1;
					contacts[i] = -1;
				}

				if (report) {
					ev[eventNum].type = EV_ABS;
					ev[eventNum].code = ABS_MT_SLOT;
					ev[eventNum++].value = i;
					ev[eventNum].type = EV_ABS;
					ev[eventNum].code = ABS_MT_TRACKING_ID;
					ev[eventNum++].value = contacts[i];

					if (contacts[i] >= 0) {
	                        		int x = ((buf[2 + contactNum*CONTACT_SIZE] >> 4) << 8) | buf[3 + contactNum*CONTACT_SIZE];
       		                		int y = MAX_Y - (((buf[2 + contactNum*CONTACT_SIZE] & 0x0f) << 8) | buf[4 + contactNum*CONTACT_SIZE]);

						ev[eventNum].type = EV_ABS;
						ev[eventNum].code = ABS_MT_POSITION_X;
						ev[eventNum++].value = x;
						ev[eventNum].type = EV_ABS;
						ev[eventNum].code = ABS_MT_POSITION_Y;
						ev[eventNum++].value = y;

                        			if (contactNum == 0) {
							ev[eventNum].type = EV_ABS;
							ev[eventNum].code = ABS_X;
							ev[eventNum++].value = x;
							ev[eventNum].type = EV_ABS;
							ev[eventNum].code = ABS_Y;
							ev[eventNum++].value = y;
						}
					}
				}

				if (buf[1] & (0x08 << i)) contactNum++; 
			}

			ev[eventNum].type = EV_KEY;
			ev[eventNum].code = BTN_TOUCH;
			ev[eventNum++].value = contact;
			ev[eventNum].type = EV_KEY;
			ev[eventNum].code = BTN_TOOL_FINGER;
			ev[eventNum++].value = contact;
			ev[eventNum].type = EV_KEY;
			ev[eventNum].code = BTN_LEFT;
			ev[eventNum++].value = buf[1] & 1;
			ev[eventNum++].type = EV_SYN;

			res = write(ifd, &ev, sizeof(ev[0])*eventNum);
		}
        }

	return res;
}

int main(int argc, char **argv)
{
	int fd, ifd;
	int i, res;
	const char *device = "/dev/hidraw1";
	const char *uinput = "/dev/uinput";

	if (argc > 1)
		device = argv[1];

	if (argc > 2)
		uinput = argv[2];

	fd = open(device, O_RDWR);
	if (fd < 0) {
		perror("Unable to open device");
		return 1;
	}

	ifd = open(uinput, O_WRONLY | O_NONBLOCK);
	if(ifd < 0) {
                perror("Unable to open uinput");
                return 1;
	}

	createUIDev(ifd);
	startMultiTouch(fd);
	mainLoop(fd, ifd);

	close(ifd);
	close(fd);

	return 0;
}
