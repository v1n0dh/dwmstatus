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
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/wireless.h>

#include <X11/Xlib.h>

char *tzone = "America/Los_Angeles";
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
	char *mem_icon = "💾";

	snprintf(cmd, 64, "free -m | grep ^M | awk {'print ($3/$2)*100'}");

	pd = popen(cmd, "r");
	if (pd == NULL) {
		return smprintf("%s ", mem_icon);
	}

	fgets(buf, 16, pd);

	pclose(pd);

	return smprintf("%s %d%%", mem_icon, atoi(buf));
}

char *
getwifistatus(char *iface)
{
	int sockfd;
	struct iwreq wreq;
	char *id;
	char *status_icon;
	char buf[24];

	memset(buf, '\0', sizeof(buf));
	memset(&wreq, '\0', sizeof(struct iwreq));

	snprintf(wreq.ifr_name, IFNAMSIZ, iface);

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		fprintf(stderr, "Failed to create socket\n");
		status_icon = "🌐";
		snprintf(buf, sizeof(buf), "---");
		goto returnwifistatus;
	}

	id = malloc(IW_ESSID_MAX_SIZE+1);
	memset(id, '\0', IW_ESSID_MAX_SIZE+1);

	wreq.u.essid.pointer = id;
	wreq.u.essid.length = IW_ESSID_MAX_SIZE;

	if (ioctl(sockfd, SIOCGIWESSID, &wreq) < 0) {
		fprintf(stderr, "Ioctl failed to get essid\n");
		status_icon = "🌐";
		snprintf(buf, sizeof(buf), "---");
		goto returnwifistatus;
	}

	snprintf(buf, sizeof(buf), "%s", (char *)(wreq.u.essid.pointer));
	if (strncmp(buf, "", sizeof(buf)) == 0) {
		status_icon = "🌐";
		snprintf(buf, sizeof(buf), "---");
		goto returnwifistatus;
	}

	status_icon = "🛜";

	free(id);
	close(sockfd);

returnwifistatus:
	return smprintf("%s %s [%s]", status_icon, buf, iface);
}

int
getrxbytes(char *iface)
{
	char *base, *rbytes;

	base = smprintf("/sys/class/net/%s/statistics", iface);

	rbytes = readfile(base, "rx_bytes");
	if (rbytes == NULL)
		return 0;

	free(base);
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

	free(base);
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
check__for_iface(char *iface)
{
	struct ifaddrs *ifaddr, *ifa;
	if (getifaddrs(&ifaddr) == -1) {
		fprintf(stderr, "Error at getifaddrs");
		return 0;
	}

	// loop through the linked list of ifaddrs to find iface
	for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_PACKET ||
				ifa->ifa_name == NULL)
			continue;

		if (strncmp(ifa->ifa_name, iface, IFNAMSIZ) == 0)
			return 1;
	}

	freeifaddrs(ifaddr);
	return 0;
}

int
check_iface_up(char *iface)
{
	char *base, *status;

	base = smprintf("/sys/class/net/%s", iface);
	status = readfile(base, "operstate");

	return (strncmp(status, "up", 2) == 0) ? 1 : 0;
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
	char *iface_name;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	for (;;sleep(1)) {
		bat = getbattery("/sys/class/power_supply/BAT0");
		tmin = mktimes("%a %b %d %I:%M%p", tzone);
		temp = gettemperature("/sys/class/thermal/thermal_zone0/hwmon0/", "temp1_input");
		mem = getmemstatus();

		iface_name = check__for_iface("alpha0") && check_iface_up("alpha0") ? "alpha0" : "wlp1s0";
		wifi = getwifistatus(iface_name);
		netspeed = getnetworkspeed(iface_name);

		status = smprintf(" %s | %s | %s | %s | %s | %s|",
				netspeed, wifi, temp, mem, bat, tmin);
		setstatus(status);

		iface_name = NULL;

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
