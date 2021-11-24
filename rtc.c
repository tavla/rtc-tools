// SPDX-License-Identifier: GPL-2.0
/*
 * Real Time Clock tool
 *
 * Copyright (c) 2018 Alexandre Belloni <alexandre.belloni@bootlin.com>
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/const.h>
#include <linux/rtc.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char *rtc_file = "/dev/rtc0";

#ifndef RTC_VL_DATA_INVALID
#define RTC_VL_DATA_INVALID	_BITUL(0) /* Voltage too low, RTC data is invalid */
#define RTC_VL_BACKUP_LOW	_BITUL(1) /* Backup voltage is low */
#define RTC_VL_BACKUP_EMPTY	_BITUL(2) /* Backup empty or not present */
#define RTC_VL_ACCURACY_LOW	_BITUL(3) /* Voltage is low, RTC accuracy is reduced */
#define RTC_VL_BACKUP_SWITCH	_BITUL(4) /* Backup switchover happened */
#endif

#ifndef RTC_PARAM_GET
struct rtc_param {
	__u64 param;
	union {
		__u64 uvalue;
		__s64 svalue;
		__u64 ptr;
	};
	__u32 index;
	__u32 __pad;
};

#define RTC_PARAM_GET	_IOW('p', 0x13, struct rtc_param)  /* Get parameter */
#define RTC_PARAM_SET	_IOW('p', 0x14, struct rtc_param)  /* Set parameter */

#define RTC_FEATURE_ALARM		0
#define RTC_FEATURE_ALARM_RES_MINUTE	1
#define RTC_FEATURE_NEED_WEEK_DAY	2
#define RTC_FEATURE_ALARM_RES_2S	3
#define RTC_FEATURE_UPDATE_INTERRUPT	4
#define RTC_FEATURE_CORRECTION		5
#define RTC_FEATURE_BACKUP_SWITCH_MODE	6

#define RTC_PARAM_FEATURES		0
#define RTC_PARAM_CORRECTION		1
#define RTC_PARAM_BACKUP_SWITCH_MODE	2

#define RTC_BSM_DISABLED	0
#define RTC_BSM_DIRECT		1
#define RTC_BSM_LEVEL		2
#define RTC_BSM_STANDBY		3

#endif

#define IOCTL(f, r, d, rc) rc = ioctl(f, r, d); \
if (rc) { \
	fprintf(stderr, "%s returned %s (%d) at line %d\n", #r, \
		strerror(errno), errno, __LINE__); \
	exit(errno); \
}

#define ISODATEFMT "%04d-%02d-%02dT%02d:%02d:%02d"
#define ISODATE(tm)  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, \
		       tm.tm_hour, tm.tm_min, tm.tm_sec

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

static const char *param_names[] = {
	"RTC_PARAM_FEATURES",
	"RTC_PARAM_CORRECTION",
	"RTC_PARAM_BACKUP_SWITCH_MODE",
};

static const char *bsm_names[] = {
	"RTC_BSM_DISABLED",
	"RTC_BSM_DIRECT",
	"RTC_BSM_LEVEL",
	"RTC_BSM_STANDBY",
};

static const char *feature_names[] = {
	"RTC_FEATURE_ALARM",
	"RTC_FEATURE_ALARM_RES_MINUTE",
	"RTC_FEATURE_NEED_WEEK_DAY",
	"RTC_FEATURE_ALARM_RES_2S",
	"RTC_FEATURE_UPDATE_INTERRUPT",
	"RTC_FEATURE_CORRECTION",
	"RTC_FEATURE_BACKUP_SWITCH_MODE",
};

static __attribute__ ((noreturn)) void usage(char *name)
{
	unsigned int i;

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
	fprintf(stderr, "       %s paramget param index [rtc]\n", name);
	fprintf(stderr, "       %s paramset param index value [rtc]\n", name);
	fprintf(stderr, "         Valid parameters:\n");
	for (i = 0; i < ARRAY_SIZE(param_names); i++)
		fprintf(stderr, "         - %s\n", param_names[i]);

	exit(EINVAL);
}

static int parse_rtc_param(struct rtc_param *param, char *param_name, char *index, char *value)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(param_names); i++)
		if (!strcmp(param_name, param_names[i]))
			break;

	if (i == ARRAY_SIZE(param_names))
		return -EINVAL;

	param->param = i;

	param->index = strtoul(index, NULL, 10);

	if (value) {
		switch(param->param) {
		case RTC_PARAM_BACKUP_SWITCH_MODE:
			for (i = 0; i < ARRAY_SIZE(bsm_names); i++)
				if (!strcmp(value, bsm_names[i]))
					break;

			if (i == ARRAY_SIZE(bsm_names))
				return -EINVAL;

			param->uvalue = i;
			break;

		case RTC_PARAM_CORRECTION:
			param->svalue = strtol(value, NULL, 10);
			break;

		default:
			return -EINVAL;
		}
	}

	return 0;
}

int main(int argc, char **argv)
{
	struct rtc_time tm;
	struct rtc_wkalrm alm;
	struct rtc_param param;
	int fd, rc;
	unsigned int i, flags;
	unsigned long cmd = 0;

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
	} else if (!strcmp(argv[1], "paramget")) {

		if (argc < 4)
			usage(argv[0]);
		if (argc > 4)
			rtc_file = argv[4];
		cmd = RTC_PARAM_GET;

		if (parse_rtc_param(&param, argv[2], argv[3], NULL) < 0)
			usage(argv[0]);

	} else if (!strcmp(argv[1], "paramset")) {
		if (argc < 5)
			usage(argv[0]);
		if (argc > 5)
			rtc_file = argv[5];
		cmd = RTC_PARAM_SET;

		if (parse_rtc_param(&param, argv[2], argv[3], argv[4]) < 0)
			usage(argv[0]);
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
		if (flags & RTC_VL_DATA_INVALID)
			printf("Voltage too low, RTC data is invalid\n");
		if (flags & RTC_VL_BACKUP_LOW)
			printf("Backup voltage is low\n");
		if (flags & RTC_VL_BACKUP_EMPTY)
			printf("Backup empty or not present\n");
		if (flags & RTC_VL_ACCURACY_LOW)
			printf("Voltage is low, RTC accuracy is reduced\n");
		if (flags & RTC_VL_BACKUP_SWITCH)
			printf("Backup switchover happened\n");
		break;
	case RTC_VL_CLR:
		IOCTL(fd, RTC_VL_CLR, 0, rc);
		break;
	case RTC_PARAM_SET:
		IOCTL(fd, RTC_PARAM_SET, &param, rc);
		break;
	case RTC_PARAM_GET:
		IOCTL(fd, RTC_PARAM_GET, &param, rc);
		switch(param.param) {
		case RTC_PARAM_FEATURES:
			printf("%s[%u]:\n", param_names[param.param], param.index);
			for (i = 0; i < ARRAY_SIZE(feature_names); i++)
				if (param.uvalue & _BITUL(i))
					printf("	%s\n", feature_names[i]);
			break;
		case RTC_PARAM_CORRECTION:
			printf("%s[%u] = %lld\n", param_names[param.param], param.index, param.svalue);
			break;
		case RTC_PARAM_BACKUP_SWITCH_MODE:
			printf("%s[%u] = %s\n", param_names[param.param], param.index, bsm_names[param.uvalue]);
			break;
		default:
			printf("%s[%u] = %llx\n", param_names[param.param], param.index, param.uvalue);
		}
	}

	close(fd);

	return 0;
}
