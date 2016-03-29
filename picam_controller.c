#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>           /* Definition of AT_* constants */
#include <syslog.h>
#include <string.h>
#include <errno.h>

#if _DEBUG_
#define LOG_DBG(fmt, ...)	printf("*DBG* %s:"fmt"\n", __FUNCTION__, ## __VA_ARGS__ );
#else
#define LOG_DBG(fmt, ...)
#endif

#if _VERBOSE_
#define LOG_VDBG(fmt, ...)	printf("*DBG* %s:"fmt"\n", __FUNCTION__, ## __VA_ARGS__ );
#else
#define LOG_VDBG(fmt, ...)
#endif

#define LOG_INF(fmt, ...)	{ printf("*INF* %s: "fmt"\n", __FUNCTION__, ## __VA_ARGS__ ); \
				  syslog(LOG_INFO, "*INF* %s: "fmt"\n", __FUNCTION__, ## __VA_ARGS__ ); }
#define LOG_ERROR(fmt, ...)	{ printf("*ERR* %s: "fmt"\n", __FUNCTION__, ## __VA_ARGS__ ); \
				  syslog(LOG_ERR, "*ERR* %s: "fmt"\n", __FUNCTION__, ## __VA_ARGS__ ); }

#define FIFO_NAME "/run/cmd.fifo"
int main()
{
	int fd;
	int rc;
	int run = 1;
	char cmd[1024];

	openlog("pictl", LOG_PID, LOG_DAEMON);
	rc = mkfifo(FIFO_NAME, 0666);
	if (rc && (errno != EEXIST)) {
		LOG_ERROR("failed to create fifo - '%s'", strerror(errno));
		goto out;
	}
	chmod(FIFO_NAME, 0666);

	fd = open(FIFO_NAME, O_RDWR);
	if (0 > fd) {
		LOG_ERROR("failed to open fifo - '%s'", strerror(errno));
		goto out;
	}

	while(run) {
		memset(cmd, 0, sizeof(cmd));
		rc = read(fd, cmd, sizeof(cmd));
		if (0 > rc)
			continue;
		cmd[rc] = '\0';
		LOG_INF("Data: %s.", cmd);

		if (0 == strncmp(cmd, "stop", 4)) {
			system("sudo pkill -TERM picam-run");
			system("sudo pkill -TERM picam");
		}
		else if (0 == strncmp(cmd, "start", 4)) {
			system("sudo /etc/rc.local");
		}
		else if (0 == strncmp(cmd, "poweroff", 8)) {
			system("sudo poweroff");
		}
	}

	close(fd);

out:
	closelog();
	return rc;
}
