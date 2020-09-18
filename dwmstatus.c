/*
 * Copy me if you can.
 * by 20h
 */

#define _BSD_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <X11/Xlib.h>

char *tzindia = "Asia/Kolkata";

static Display *dpy;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("");
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0)
		return smprintf("");

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

char *
readfile(char *base, char *file)
{
	char *path, line[513];
	FILE *fd;

	memset(line, 0, sizeof(line));

	path = smprintf("%s/%s", base, file);
	fd = fopen(path, "r");
	free(path);
	if (fd == NULL)
		return NULL;

	if (fgets(line, sizeof(line)-1, fd) == NULL)
		return NULL;
	fclose(fd);

	return smprintf("%s", line);
}

char *
getbattery(char *base)
{
	char *co, status;
	int cap;

	co = readfile(base, "present");
	if (co == NULL)
		return smprintf("");
	if (co[0] != '1') {
		free(co);
		return smprintf("not present");
	}
	free(co);

	co = readfile(base, "capacity");
	if (co == NULL) {
		return smprintf("");
	}
	sscanf(co, "%d", &cap);
	free(co);

	co = readfile(base, "status");
	if (!strncmp(co, "Discharging", 11) && cap <= 15) {
		status = '!';
	} else if(!strncmp(co, "Charging", 8)) {
		status = '+';
	}
	free(co);

	if (cap < 0)
		return smprintf("invalid");

	return smprintf("%d%%%c", cap, status);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co;

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("");

	return smprintf("%02.0f°C", atof(co) / 1000);
}

char *
getmemstatus()
{
	FILE *pd;
	char buf[16];
	char cmd[64];

	snprintf(cmd, 64, "free -m | grep ^M | awk {'print ($3/$2)*100'}");

	pd = popen(cmd, "r");
	if (pd == NULL)
		return smprintf("");

	fgets(buf, 16, pd);

	pclose(pd);

	return smprintf("%d%%", atoi(buf));
}

char *
getwifistatus()
{
	FILE *pd;
	char buf[24];
	char cmd[64];

	snprintf(cmd, 64, "iwgetid | sed 's/.*:\"//; s/\"$//; /^$/d' ");

	pd = popen(cmd, "r");
	if (pd == NULL)
		return smprintf("");

	memset(buf, '\0', sizeof(buf));
	fgets(buf, 24, pd);
	int len = strlen(buf);
	if (len == 0)
		return smprintf("--");
	buf[len-1] = '\0';

	pclose(pd);

	return smprintf("%s", buf);
}

int
getrxbytes(char *iface)
{
	char *base, *rbytes;

	base = smprintf("/sys/class/net/%s/statistics", iface);

	rbytes = readfile(base, "rx_bytes");
	if (rbytes == NULL)
		return 0;

	return atoi(rbytes);
}

int
gettxbytes(char *iface)
{
	char *base, *tbytes;

	base = smprintf("/sys/class/net/%s/statistics", iface);

	tbytes = readfile(base, "tx_bytes");
	if (tbytes == NULL)
		return 0;

	return atoi(tbytes);
}

int
main(void)
{
	char *status;
	char *bat;
	char *tmin;
	char *temp;
	char *mem;
	char *wifi;
	int rx, tx;
	int old_rx = 0, old_tx = 0;
	int rx_bytes, tx_bytes;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		bat = getbattery("/sys/class/power_supply/BAT0");
		tmin = mktimes("%a %b %d %I:%M%p", tzindia);
		temp = gettemperature("/sys/class/thermal/thermal_zone0/hwmon0/", "temp1_input");
		mem = getmemstatus();
		wifi = getwifistatus();
		rx = getrxbytes("wlp1s0");
		tx = gettxbytes("wlp1s0");

		rx_bytes = (rx - old_rx) / 1024;
		tx_bytes = (tx - old_tx) / 1024;

		status = smprintf("Net: %dKB/%dKB | Wifi: %s | Temp: %s | Mem: %s | Bat: %s | %s ",
				 tx_bytes, rx_bytes, wifi, temp, mem, bat, tmin);
		setstatus(status);

		old_rx = rx;
		old_tx = tx;

		free(temp);
		free(bat);
		free(tmin);
		free(mem);
		free(wifi);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}
