// SPDX-License-Identifier: GPL-2.0
/*
 * Real Time Clock Driver range test
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
#include <time.h>
#include <unistd.h>

static char *rtc_file = "/dev/rtc0";

#define IOCTL(f, r, d, rc) rc = ioctl(f, r, d); \
if (rc) { \
	fprintf(stderr, "KO %s returned %d (line %d)\n", #r, errno, __LINE__); \
	continue; \
}

#define ISODATEFMT "%04d-%02d-%02d %02d:%02d:%02d"
#define ISODATE(tm)  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
		       tm.tm_hour, tm.tm_min, tm.tm_sec

#ifndef ARRAY_SIZE
# define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

static struct {
	struct rtc_time tm;
	struct rtc_time expected;
} dates [] = {
	{ /* UNIX epoch */
		.tm = { .tm_year = 70, .tm_mon = 0, .tm_mday = 1,
			.tm_hour = 0, .tm_min = 0, .tm_sec = 0 },
		.expected = { .tm_year = 70, .tm_mon = 0, .tm_mday = 1,
			      .tm_hour = 0, .tm_min = 0, .tm_sec = 1 }
	},
	{ /* 2000 is a leap year */
		.tm = { .tm_year = 100, .tm_mon = 1, .tm_mday = 28,
			.tm_hour = 23, .tm_min = 59, .tm_sec = 59 },
		.expected = { .tm_year = 100, .tm_mon = 1, .tm_mday = 29,
			      .tm_hour = 0, .tm_min = 0, .tm_sec = 0 }
	},
	{ /* 2020 is a leap year */
		.tm = { .tm_year = 120, .tm_mon = 1, .tm_mday = 28,
			.tm_hour = 23, .tm_min = 59, .tm_sec = 59 },
		.expected = { .tm_year = 120, .tm_mon = 1, .tm_mday = 29,
			      .tm_hour = 0, .tm_min = 0, .tm_sec = 0 }
	},
	{ /* signed 32bit time_t overflow */
		.tm = { .tm_year = 138, .tm_mday = 19,
			.tm_hour = 3, .tm_min = 14, .tm_sec = 7 },
		.expected = { .tm_year = 138, .tm_mday = 19,
			      .tm_hour = 3, .tm_min = 14, .tm_sec = 8 }
	},
	{ /* 2069 to 2070 */
		.tm = { .tm_year = 169, .tm_mon = 11, .tm_mday = 31,
			.tm_hour = 23, .tm_min = 59, .tm_sec = 59 },
		.expected = { .tm_year = 170, .tm_mday = 1,
			      .tm_hour = 0, .tm_min = 0, .tm_sec = 0 }
	},
	{ /* 2079 to 2080 */
		.tm = { .tm_year = 179, .tm_mon = 11, .tm_mday = 31,
			.tm_hour = 23, .tm_min = 59, .tm_sec = 59 },
		.expected = { .tm_year = 180, .tm_mday = 1,
			      .tm_hour = 0, .tm_min = 0, .tm_sec = 0 }
	},
	{ /* 2099 to 2100 */
		.tm = { .tm_year = 199, .tm_mon = 11, .tm_mday = 31,
			.tm_hour = 23, .tm_min = 59, .tm_sec = 59 },
		.expected = { .tm_year = 200, .tm_mday = 1,
			      .tm_hour = 0, .tm_min = 0, .tm_sec = 0 }
	},
	{ /* 2100 is not a leap year */
		.tm = { .tm_year = 200, .tm_mon = 1, .tm_mday = 28,
			.tm_hour = 23, .tm_min = 59, .tm_sec = 59 },
		.expected = { .tm_year = 200, .tm_mon = 2, .tm_mday = 1,
			      .tm_hour = 0, .tm_min = 0, .tm_sec = 0 }
	},
	{ /* unsigned 32bit time_t overflow */
		.tm = { .tm_year = 206, .tm_mon = 1, .tm_mday = 7,
			.tm_hour = 6, .tm_min = 28, .tm_sec = 15 },
		.expected = { .tm_year = 206, .tm_mon = 1, .tm_mday = 7,
			      .tm_hour = 6, .tm_min = 28, .tm_sec = 16 }
	},
	{ /* ktime_t overflow */
		.tm = { .tm_year = 362, .tm_mon = 3, .tm_mday = 11,
			.tm_hour = 23, .tm_min = 47, .tm_sec = 16 },
		.expected = { .tm_year = 362, .tm_mon = 3, .tm_mday = 11,
			      .tm_hour = 23, .tm_min = 47, .tm_sec = 17 }
	},
};

static int compare_dates(struct rtc_time *a, struct rtc_time *b)
{
	if (a->tm_year != b->tm_year ||
	    a->tm_mon != b->tm_mon ||
	    a->tm_mday != b->tm_mday ||
	    a->tm_hour != b->tm_hour ||
	    a->tm_min != b->tm_min ||
	    a->tm_sec != b->tm_sec)
		return 1;

	return 0;
}

int main(int argc, char **argv)
{
	int fd, i, rc;

	switch (argc) {
	case 2:
		rtc_file = argv[1];
		/* FALLTHROUGH */
	case 1:
		break;
	default:
		fprintf(stderr, "usage: %s [rtcdev]\n", argv[0]);
		return 1;
	}

	fd = open(rtc_file, O_RDONLY);
	if (fd ==  -1) {
		perror(rtc_file);
		exit(errno);
	}

	for (i = 0; i < ARRAY_SIZE(dates); i++) {
		struct rtc_time tm;

		printf("\nTesting " ISODATEFMT ".\n", ISODATE(dates[i].tm));

		IOCTL(fd, RTC_SET_TIME, &dates[i].tm, rc);

		IOCTL(fd, RTC_RD_TIME, &tm, rc);

		rc = compare_dates(&dates[i].tm, &tm);
		if (rc) {
			printf("KO  Read back " ISODATEFMT ".\n", ISODATE(tm));
			continue;
		}

		/*
		 * We can't rely on alarms to work and because update interrupts
		 * are implemented using alarms, they are not usable either
		 */
		sleep(1);

		IOCTL(fd, RTC_RD_TIME, &tm, rc);

		rc = compare_dates(&dates[i].expected, &tm);
		if (rc) {
			printf("KO  Expected " ISODATEFMT ".\n",
			       ISODATE(dates[i].expected));
			printf("    Got      " ISODATEFMT ".\n", ISODATE(tm));
			continue;
		}

		printf("OK\n");
		continue;

		/*
		 * Test alarms note: this will always fail the ktime_t overflow
		 * because it is stored internally in a ktime_t
		 */
		IOCTL(fd, RTC_SET_TIME, &dates[i].tm, rc);

		IOCTL(fd, RTC_WKALM_SET, &dates[i].tm, rc);

		IOCTL(fd, RTC_WKALM_RD, &dates[i].tm, rc);

		rc = compare_dates(&dates[i].tm, &tm);
		if (rc) {
			printf("KO ALM Read back " ISODATEFMT ".\n",
			       ISODATE(tm));
			continue;
		}
	}

	close(fd);
}
