// SPDX-License-Identifier: GPL-2.0
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include <linux/rtc.h>

#define NSEC_PER_SEC	1000000000L

int set_realtime_priority(void)
{
	int ret;
	pthread_t this_thread = pthread_self();
	struct sched_param params;

	params.sched_priority = sched_get_priority_max(SCHED_FIFO);
	ret = pthread_setschedparam(this_thread, SCHED_FIFO, &params);
	if (ret != 0)
		fprintf(stderr, "Unable to set realtime priority\n");

	return ret;
}

void timespec_diff(struct timespec *start, struct timespec *stop,
                   struct timespec *result)
{
    if ((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + NSEC_PER_SEC;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }

    return;
}

int get_offset_uie(struct timespec *diff, int rtc)
{
	unsigned long data;
	struct timespec now;
	struct tm stm;
	int rc;
	int i;

	rc = ioctl(rtc, RTC_UIE_ON, 0);
	if (rc < 0) {
		perror("RTC_UIE_ON");
		return rc;
	}

	for (i = 0; i < 5; i++) {
		rc = read(rtc, &data, sizeof(data));
		if (rc < 0) {
			perror("read");
			return rc;
		}
		clock_gettime(CLOCK_REALTIME, &now);
		ioctl(rtc, RTC_RD_TIME, &stm);
		if (rc < 0) {
			perror("RTC_RD_TIME");
			break;
		}
		printf("%d %d.%09d\n", timegm(&stm), now.tv_sec, now.tv_nsec);
	}

	rc = ioctl(rtc, RTC_UIE_OFF, 0);
	if (rc < 0) {
		perror("RTC_UIE_OFF");
		return rc;
	}

	diff->tv_sec = now.tv_sec - timegm(&stm);
	diff->tv_nsec = now.tv_nsec;

	return 0;
}

int get_offset_alarm(struct timespec *diff, int rtc)
{
	struct rtc_wkalrm alarm = { 0 };
	struct timespec now;
	struct tm stm;
	time_t secs;
	unsigned long data;
	int rc;

	rc = ioctl(rtc, RTC_RD_TIME, &stm);
	if (rc < 0) {
		perror("RTC_RD_TIME");
		return rc;
	}

	secs = timegm(&stm);

	secs++;

	gmtime_r(&secs, &stm);

	alarm.time.tm_sec = stm.tm_sec;
	alarm.time.tm_min = stm.tm_min;
	alarm.time.tm_hour = stm.tm_hour;
	alarm.time.tm_mday = stm.tm_mday;
	alarm.time.tm_mon = stm.tm_mon;
	alarm.time.tm_year = stm.tm_year;
	alarm.time.tm_wday = -1;
	alarm.time.tm_yday = -1;
	alarm.time.tm_isdst = -1;
	alarm.enabled = 1;
	rc = ioctl(rtc, RTC_WKALM_SET, &alarm);
	if (rc < 0) {
		perror("RTC_WKALM_SET");
		return rc;
	}

	rc = read(rtc, &data, sizeof(data));
	if (rc < 0) {
		perror("read");
		return rc;
	}

	clock_gettime(CLOCK_REALTIME, &now);

	rc = ioctl(rtc, RTC_RD_TIME, &stm);
	if (rc < 0) {
		perror("RTC_RD_TIME");
		return rc;
	}

	secs = timegm(&stm);

	printf("%d %d.%09d\n", secs, now.tv_sec, now.tv_nsec);
	diff->tv_sec = now.tv_sec - secs;
	diff->tv_nsec = now.tv_nsec;

	return 0;
}

int get_offset_poll(struct timespec *diff, int rtc)
{
	struct tm stm;
	struct timespec now;
	int rc;
	int i, secs;
	unsigned long m = 0;
	struct timespec b, a, d;

	for (i = 0; i < 100; i++) {
		clock_gettime(CLOCK_MONOTONIC, &b);
		rc = ioctl(rtc, RTC_RD_TIME, &stm);
		if (rc < 0) {
			perror("RTC_RD_TIME");
			return rc;
		}
		clock_gettime(CLOCK_MONOTONIC, &a);

		timespec_diff(&b, &a, &d);

		m += d.tv_nsec/100 + d.tv_sec * 10000000;
	}

	printf("POLL: Mean time to read: %lu\n", m);

	rc = ioctl(rtc, RTC_RD_TIME, &stm);
	if (rc < 0) {
		perror("RTC_RD_TIME");
		return rc;
	}

	secs = stm.tm_sec;
	do {
		clock_gettime(CLOCK_MONOTONIC, &b);
		ioctl(rtc, RTC_RD_TIME, &stm);
		if (rc < 0) {
			perror("RTC_RD_TIME");
			return rc;
		}
		clock_gettime(CLOCK_MONOTONIC, &a);
	} while (stm.tm_sec == secs);

	clock_gettime(CLOCK_REALTIME, &now);
	printf("POLL: corrected: %d %d.%09d\n", timegm(&stm), now.tv_sec, now.tv_nsec - m);
	timespec_diff(&b, &a, &d);
	printf("POLL: Last time to read: %lu %d\n", d.tv_nsec, i);

	diff->tv_sec = now.tv_sec - timegm(&stm);
	diff->tv_nsec = now.tv_nsec - m;

	return 0;
}

int main(void)
{
	struct timespec now, ts, diff;
	struct tm stm;
	time_t secs;
	int rtc;
	int rc;

	int (*get_offset)(struct timespec *diff, int rtc);

	get_offset = get_offset_alarm;

	clock_getres(CLOCK_REALTIME, &ts);
	printf("CLOCK_REALTIME %d.%09d\n", ts.tv_sec, ts.tv_nsec);
	clock_getres(CLOCK_MONOTONIC, &ts);
	printf("CLOCK_MONOTONIC %d.%09d\n", ts.tv_sec, ts.tv_nsec);

	rtc = open("/dev/rtc0", O_RDONLY);
	if (rtc < 0) {
		perror("open");
		return rtc;
	}

	set_realtime_priority();

	rc = get_offset(&diff, rtc);
	if (rc)
		return rc;
	printf("Current offset: %ds + %09dns = %dns\n", diff.tv_sec, diff.tv_nsec, diff.tv_sec * NSEC_PER_SEC + diff.tv_nsec);

	clock_gettime(CLOCK_REALTIME, &now);

	/* sleep to the next second unless now is too close */
	ts.tv_nsec = 0;
	ts.tv_sec = now.tv_sec + 1;
	if (ts.tv_nsec > 900000000)
		ts.tv_sec++;

	gmtime_r(&ts.tv_sec, &stm);
	printf("setting %d at %d.%09d\n", ts.tv_sec, ts.tv_sec, ts.tv_nsec);

	rc = clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
	if (rc) {
		perror("clock_nanosleep");
		return rc;
	}

	rc = ioctl(rtc, RTC_SET_TIME, &stm);
	if (rc < 0) {
		perror("RTC_SET_TIME");
		return rc;
	}

	rc = get_offset(&diff, rtc);
	if (rc)
		return rc;
	printf("Set offset: %ds + %09dns = %dns\n", diff.tv_sec, diff.tv_nsec, diff.tv_sec * NSEC_PER_SEC + diff.tv_nsec);

	clock_gettime(CLOCK_REALTIME, &now);

	/* Calculate the next full second */
	if (diff.tv_sec < 0) {
		/*
		 * RTC is set earlier than system time unless system time is going
		 * back, this will never be more than 1s
		 */
		secs = now.tv_sec;
		ts.tv_sec = now.tv_sec;
		ts.tv_nsec =  NSEC_PER_SEC - diff.tv_nsec;
	} else {
		/*
		 * RTC is late so at the next second minus diff.tv_nsec, we need to set
		 * the RTC to next second + diff.tv_sec
		 */
		ts.tv_sec = now.tv_sec;
		ts.tv_nsec = NSEC_PER_SEC - diff.tv_nsec;
		secs = ts.tv_sec + 1 + diff.tv_sec;
	}

	/* too close, use next second */
	if (ts.tv_nsec - now.tv_nsec < 100000000) {
		ts.tv_sec++;
		secs++;
	}

	gmtime_r(&secs, &stm);
	printf("setting %d at %d.%09d\n", secs, ts.tv_sec, ts.tv_nsec);

	clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);

	rc = ioctl(rtc, RTC_SET_TIME, &stm);
	if (rc < 0) {
		perror("RTC_SET_TIME");
		return rc;
	}

	rc = get_offset(&diff, rtc);
	if (rc)
		return rc;
	printf("New offset: %ds + %09dns = %dns\n", diff.tv_sec, diff.tv_nsec, diff.tv_sec * NSEC_PER_SEC + diff.tv_nsec);

	return 0;
}
