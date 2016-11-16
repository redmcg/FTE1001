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
#define MAX_CONTACT_EVENTS 7
#define EVERY_TIME_EVENTS 4
#define ONE_OFF_EVENTS 3
#define MAX_EVENTS (EVERY_TIME_EVENTS + MAX_CONTACT_EVENTS*MAX_CONTACTS + ONE_OFF_EVENTS)

#define CONTACT_SIZE 5	

#define MAX_X 0x0aea
#define MAX_Y 0x06de
#define MAX_TOUCH_MAJOR 8
#define MAX_PRESSURE 0x80

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
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_TOOL_WIDTH);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_POSITION_Y);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_POSITION_X);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_TRACKING_ID);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_SLOT);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_TOOL_TYPE);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_TOUCH_MAJOR);
	res = ioctl(ifd, UI_SET_ABSBIT, ABS_MT_PRESSURE);
	res = ioctl(ifd, UI_SET_PROPBIT, INPUT_PROP_POINTER);
	res = ioctl(ifd, UI_SET_PROPBIT, INPUT_PROP_BUTTONPAD);

	struct uinput_user_dev uidev;
	memset(&uidev, 0, sizeof(uidev));
	snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "FTE1001:00 0B05:0101 Userspace Driver");
	uidev.id.bustype = BUS_I2C;
	uidev.id.vendor  = 0x0b05;
	uidev.id.product = 0x0101;
	uidev.id.version = 1;
	uidev.absmax[ABS_X] = uidev.absmax[ABS_MT_POSITION_X] = MAX_X;
	uidev.absmax[ABS_Y] = uidev.absmax[ABS_MT_POSITION_Y] = MAX_Y;
	uidev.absmax[ABS_MT_SLOT] = MAX_CONTACTS - 1;
	uidev.absmax[ABS_MT_TOOL_TYPE] = MT_TOOL_MAX;
	uidev.absmax[ABS_MT_TOUCH_MAJOR] = uidev.absmax[ABS_TOOL_WIDTH] = MAX_TOUCH_MAJOR;
	uidev.absmax[ABS_MT_PRESSURE] = MAX_PRESSURE;

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
	struct contacts_t {
		int trackingId;
		int toolType;
	} contacts[MAX_CONTACTS];
	for (i = 0; i < MAX_CONTACTS; i++) contacts[i].trackingId = -1;

        while(1) {
		res = read(fd, buf, sizeof(buf));
		if (res <= 0) {
			perror("read");
		} else if (buf[0] == 0x5d) {
			struct input_event ev[MAX_EVENTS];
                        int contact = 0;
			int eventNum = 0, contactNum = 0;
			int toolType;

			memset(&ev, 0, sizeof(ev));

			for (i = 0; i < MAX_CONTACTS; i++) {
				int report = 0;
				if (buf[1] & (0x08 << i)) {
					report = contact = 1;
					toolType = buf[5 + contactNum*CONTACT_SIZE] & 0x80 ? MT_TOOL_PALM : MT_TOOL_FINGER;
					if (contacts[i].trackingId == -1 || contacts[i].toolType != toolType) {
						contacts[i].trackingId = trackingid++;
						contacts[i].toolType = toolType;
						if (trackingid < 0) trackingid = 0;
					}

				} else if (contacts[i].trackingId != -1) {
					report = 1;
					contacts[i].trackingId = -1;
				}

				if (report) {
					/* The next seven events could occur for every event - hence MAX_CONTACT_EVENTS is 7 */
					ev[eventNum].type = EV_ABS;
					ev[eventNum].code = ABS_MT_SLOT;
					ev[eventNum++].value = i;
					ev[eventNum].type = EV_ABS;
					ev[eventNum].code = ABS_MT_TRACKING_ID;
					ev[eventNum++].value = contacts[i].trackingId;
					ev[eventNum].type = EV_ABS;
					ev[eventNum].code = ABS_MT_TOOL_TYPE;
					ev[eventNum++].value = contacts[i].toolType;

					if (contacts[i].trackingId >= 0) {
	                        		int x = ((buf[2 + contactNum*CONTACT_SIZE] >> 4) << 8) | buf[3 + contactNum*CONTACT_SIZE];
       		                		int y = MAX_Y - (((buf[2 + contactNum*CONTACT_SIZE] & 0x0f) << 8) | buf[4 + contactNum*CONTACT_SIZE]);
						int touchMajor = toolType == MT_TOOL_FINGER ? (buf[5 + contactNum*CONTACT_SIZE] >> 4) & 0x07 : MAX_TOUCH_MAJOR;
						int pressure = toolType == MT_TOOL_FINGER ? buf[6 + contactNum*CONTACT_SIZE] & 0x7f : MAX_PRESSURE;

						// if (touchMajor != 1 || pressure != 0x7f || toolType != MT_TOOL_FINGER) printf("slot %d: tool = %s, touchMajor = %d, pressure = %02x, raw = %02x%02x\n", i, toolType == MT_TOOL_FINGER ? "Finger" : "Palm", touchMajor, pressure, buf[5 + contactNum*CONTACT_SIZE], buf[6 + contactNum*CONTACT_SIZE]);

						ev[eventNum].type = EV_ABS;
						ev[eventNum].code = ABS_MT_POSITION_X;
						ev[eventNum++].value = x;
						ev[eventNum].type = EV_ABS;
						ev[eventNum].code = ABS_MT_POSITION_Y;
						ev[eventNum++].value = y;
						ev[eventNum].type = EV_ABS;
						ev[eventNum].code = ABS_MT_TOUCH_MAJOR;
						ev[eventNum++].value = touchMajor;
						ev[eventNum].type = EV_ABS;
						ev[eventNum].code = ABS_MT_PRESSURE;
						ev[eventNum++].value = pressure;

                        			if (contactNum == 0) {
							/* These three events are one offs (they're not MT) - hence ONE_OFF_EVENTS is 3 */
							ev[eventNum].type = EV_ABS;
							ev[eventNum].code = ABS_X;
							ev[eventNum++].value = x;
							ev[eventNum].type = EV_ABS;
							ev[eventNum].code = ABS_Y;
							ev[eventNum++].value = y;
							ev[eventNum].type = EV_ABS;
							ev[eventNum].code = ABS_TOOL_WIDTH;
							ev[eventNum++].value = touchMajor;
						}
					}
				}

				if (buf[1] & (0x08 << i)) contactNum++; 
			}

			/* These four events will always occur - thus EVERY_TIME_EVENTS is 4 */
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
