// SPDX-License-Identifier: GPL-2.0
/*
 * Real Time Clock tool
 *
 * Copyright (c) 2018 Alexandre Belloni <alexandre.belloni@bootlin.com>
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char *rtc_file = "/dev/rtc0";

#define IOCTL(f, r, d, rc) rc = ioctl(f, r, d); \
if (rc) { \
	fprintf(stderr, "%s returned %s (%d) at line %d\n", #r, \
		strerror(errno), errno, __LINE__); \
	exit(errno); \
}

#define ISODATEFMT "%04d-%02d-%02dT%02d:%02d:%02d"
#define ISODATE(tm)  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
		       tm.tm_hour, tm.tm_min, tm.tm_sec

static void usage(char *name)
{
	fprintf(stderr, "Usage: %s <command>\n", name);
	fprintf(stderr, "       %s rd [rtc]\n", name);
	fprintf(stderr, "       %s set YYYY-MM-DDThh:mm:ss [rtc]\n", name);
	fprintf(stderr, "       %s wkalmrd [rtc]\n", name);
	fprintf(stderr, "       %s wkalmset YYYY-MM-DDThh:mm:ss [rtc]\n", name);
	fprintf(stderr, "       %s almread [rtc]\n", name);
	fprintf(stderr, "       %s almset YYYY-MM-DDThh:mm:ss [rtc]\n", name);
	fprintf(stderr, "       %s aieon [rtc]\n", name);
	fprintf(stderr, "       %s aieoff [rtc]\n", name);
	fprintf(stderr, "       %s vlrd [rtc]\n", name);
	fprintf(stderr, "       %s vlclr [rtc]\n", name);

	exit(EINVAL);
}

int main(int argc, char **argv)
{
	struct rtc_time tm;
	struct rtc_wkalrm alm;
	int fd, rc, cmd = 0;
	unsigned int flags;

	if (argc < 2)
		usage(argv[0]);

	if (!strcmp(argv[1], "rd")) {
		if (argc > 2)
			rtc_file = argv[2];
		cmd = RTC_RD_TIME;
	} else if (!strcmp(argv[1], "set")) {
		if (argc < 3)
			usage(argv[0]);
		if (argc > 3)
			rtc_file = argv[3];
		cmd = RTC_SET_TIME;
		rc = sscanf(argv[2], "%d-%d-%dT%d:%d:%d", &tm.tm_year,
			    &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min,
			    &tm.tm_sec);
		tm.tm_year -= 1900;
		tm.tm_mon -= 1;
	} else if (!strcmp(argv[1], "wkalmrd")) {
		if (argc > 2)
			rtc_file = argv[2];
		cmd = RTC_WKALM_RD;
	} else if (!strcmp(argv[1], "wkalmset")) {
		if (argc < 3)
			usage(argv[0]);
		if (argc > 3)
			rtc_file = argv[3];
		cmd = RTC_WKALM_SET;
		rc = sscanf(argv[2], "%d-%d-%dT%d:%d:%d", &alm.time.tm_year,
			    &alm.time.tm_mon, &alm.time.tm_mday,
			    &alm.time.tm_hour, &alm.time.tm_min,
			    &alm.time.tm_sec);
		alm.time.tm_year -= 1900;
		alm.time.tm_mon -= 1;
		alm.enabled = 1;
	} else if (!strcmp(argv[1], "aieon")) {
		if (argc > 2)
			rtc_file = argv[2];
		cmd = RTC_AIE_ON;
	} else if (!strcmp(argv[1], "aieoff")) {
		if (argc > 2)
			rtc_file = argv[2];
		cmd = RTC_AIE_OFF;
	} else if (!strcmp(argv[1], "almread")) {
		if (argc > 2)
			rtc_file = argv[2];
		cmd = RTC_ALM_READ;
	} else if (!strcmp(argv[1], "almset")) {
		if (argc < 3)
			usage(argv[0]);
		if (argc > 3)
			rtc_file = argv[3];
		cmd = RTC_ALM_SET;
		rc = sscanf(argv[2], "%d-%d-%dT%d:%d:%d", &tm.tm_year,
			    &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min,
			    &tm.tm_sec);
		alm.time.tm_year -= 1900;
		alm.time.tm_mon -= 1;
		alm.enabled = 1;
	} else if (!strcmp(argv[1], "vlrd")) {
		if (argc > 2)
			rtc_file = argv[2];
		cmd = RTC_VL_READ;
	} else if (!strcmp(argv[1], "vlclr")) {
		if (argc > 2)
			rtc_file = argv[2];
		cmd = RTC_VL_CLR;
	}

	if (!cmd)
		usage(argv[0]);

	fd = open(rtc_file, O_RDONLY);
	if (fd ==  -1) {
		perror(rtc_file);
		exit(errno);
	}

	switch (cmd) {
	case RTC_RD_TIME:
		IOCTL(fd, RTC_RD_TIME, &tm, rc);
		printf("%s: " ISODATEFMT "\n", rtc_file, ISODATE(tm));
		break;
	case RTC_SET_TIME:
		IOCTL(fd, RTC_SET_TIME, &tm, rc);
		break;
	case RTC_WKALM_RD:
		IOCTL(fd, RTC_WKALM_RD, &alm, rc);
		printf("%s: " ISODATEFMT "\n", rtc_file, ISODATE(alm.time));
		break;
	case RTC_WKALM_SET:
		IOCTL(fd, RTC_WKALM_SET, &alm, rc);
		break;
	case RTC_ALM_READ:
		IOCTL(fd, RTC_ALM_READ, &tm, rc);
		printf("%s: " ISODATEFMT "\n", rtc_file, ISODATE(tm));
		break;
	case RTC_ALM_SET:
		IOCTL(fd, RTC_ALM_SET, &tm, rc);
		break;
	case RTC_AIE_ON:
		IOCTL(fd, RTC_AIE_ON, 0, rc);
		break;
	case RTC_AIE_OFF:
		IOCTL(fd, RTC_AIE_OFF, 0, rc);
		break;
	case RTC_VL_READ:
		IOCTL(fd, RTC_VL_READ, &flags, rc);
		printf("%s: voltage low flags: %x\n", rtc_file, flags);
		break;
	case RTC_VL_CLR:
		IOCTL(fd, RTC_VL_CLR, 0, rc);
		break;
	}

	close(fd);

	return 0;
}
