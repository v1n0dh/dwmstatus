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

char *tzindia = "UTC";
int rx_old = 0, tx_old = 0;

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
	char *clock_icon = "🕕";

	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL)
		return smprintf("");

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		return smprintf("%s ", clock_icon);
	}

	return smprintf("%s %s", clock_icon, buf);
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
	char *co;
	int cap;
	char *status_icon;

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
		status_icon = "🪫";
	} else if(!strncmp(co, "Charging", 8)) {
		status_icon = "⚡️";
	} else { status_icon = "🔋"; }
	free(co);

	if (cap < 0)
		return smprintf("invalid");

	return smprintf("%s %d%%", status_icon, cap);
}

char *
gettemperature(char *base, char *sensor)
{
	char *co, *temp_icon = "🌡️";

	co = readfile(base, sensor);
	if (co == NULL)
		return smprintf("%s ", temp_icon);

	return smprintf("%s %02.0f°C", temp_icon, atof(co) / 1000);
}

char *
getmemstatus()
{
	FILE *pd;
	char buf[16];
	char cmd[64];
	char *mem_icon = "⛓️";

	snprintf(cmd, 64, "free -m | grep ^M | awk {'print ($3/$2)*100'}");

	pd = popen(cmd, "r");
	if (pd == NULL) {
		pclose(pd);
		return smprintf("%s ", mem_icon);
	}

	fgets(buf, 16, pd);

	pclose(pd);

	return smprintf("%s %d%%", mem_icon, atoi(buf));
}

char *
getwifistatus(char *iface)
{
	FILE *pd;
	char buf[24];
	char cmd[64];
	char *status_icon;

	memset(cmd, '\0', sizeof(cmd));
	snprintf(cmd, 64, "iwgetid | sed 's/.*:\"//; s/\"$//; /^$/d' ");

	pd = popen(cmd, "r");
	if (pd == NULL) {
		pclose(pd);
		return smprintf("");
	}

	memset(buf, '\0', sizeof(buf));
	fgets(buf, 24, pd);
	int len = strlen(buf);
	if (len == 0) {
		status_icon = "🌐";
		pclose(pd);
		return smprintf("%s --", status_icon);
	}
	buf[len-1] = '\0';

	pclose(pd);

	status_icon = "🛜";
	return smprintf("%s %s", status_icon, buf);
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

char*
getnetworkspeed(char *interface)
{
	int rx, tx;
	int rx_bytes, tx_bytes;
	char *rx_units, *tx_units;
	char *tx_icon = "⬆️", *rx_icon = "⬇️";
	char *netspeed;

	rx = getrxbytes(interface);
	tx = gettxbytes(interface);

	rx_bytes = (rx - rx_old) / 1024;
	tx_bytes = (tx - tx_old) / 1024;

	if (rx_bytes >= 1024) {
		rx_units = smprintf("%s", "MB");
		rx_bytes /= 1024;
	} else { rx_units = smprintf("%s", "KB"); }

	if (tx_bytes >= 1024) {
		tx_units = smprintf("%s", "MB");
		tx_bytes /= 1024;
	} else { tx_units = smprintf("%s", "KB"); }

	netspeed = smprintf("%s %d%s %s %d%s", tx_icon, tx_bytes, tx_units, rx_icon, rx_bytes, rx_units);

	rx_old = rx;
	tx_old = tx;

	free(rx_units);
	free(tx_units);

	return netspeed;
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
	char *netspeed;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		bat = getbattery("/sys/class/power_supply/BAT0");
		tmin = mktimes("%a %b %d %I:%M%p", tzindia);
		temp = gettemperature("/sys/class/thermal/thermal_zone0/hwmon1/", "temp1_input");
		mem = getmemstatus();
		wifi = getwifistatus();
		netspeed = getnetworkspeed("wlp1s0");

		status = smprintf(" %s | %s | %s | %s | %s | %s |",
				netspeed, wifi, temp, mem, bat, tmin);
		setstatus(status);

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
