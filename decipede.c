#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#ifdef __linux__
#include <pty.h>
#else
#include <util.h>
#endif
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#define USE_CHILD_SIGNAL
#define USE_OPENPTY

#define MAX_DATALEN	1024
struct devs {
	char *name;
	int type;
#define DEVTYPE_CON	1
#define DEVTYPE_PTY	2
	int fd;
	int fd2;	/* slave */
	struct devs *dst_head;

	/* XXX should be queue */
	char buf[MAX_DATALEN];	/* read buffer */
	int buflen;	/* buffer length */
	int datalen;	/* data length */

	struct devs *next;
};

struct devs *devs_head = NULL;

char *outfile = "pty_names.txt";
int n_childs = 1;
int n_speed = 115200;
int f_stdout = 0;
int f_hex = 0;
int f_debug = 0;

char *prog_name = NULL;

void
usage()
{
	printf(
"Usage: %s [-dh] [-n num] [-o name] [-C] (dev)\n"
"\n"
"    It reads data from the device specified in the end of parameters.\n"
"    And, it writes the data into some pseudo terminal that it created\n"
"    when it had started.  The baud rate of the pseudo terminal is 115200\n"
"    for that devices.  You can use a special word \"con\" to write\n"
"    the data into the standard output.\n"
"\n"
"    -n: specifies the number of pseudo devices to be created. (default: 1)\n"
"    -b: specifies the baud rate of the read dev. (default is 115200)\n"
"    -o: specifies the file name in which %s will put the device\n"
"        names prepared.\n"
"    -C: writes data into the console as the one of the pseudo devices.\n"
"    -x: writes data in hex string.\n"
"\n"
"TODO:\n"
"    -C should be separated from -n.\n"
	, prog_name, prog_name);

	exit(0);
}

int
check_fd(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		err(1, "ERROR: %s: fcntl(fd=%d, GETFL)", __FUNCTION__, fd);
	if (f_debug)
		printf("DEBUG: flags(fd=%d)=%d\n", fd, flags);

	return 0;
}

int
set_non_block(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		err(1, "ERROR: %s: fcntl(fd=%d, GETFL)", __FUNCTION__, fd);
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		err(1, "ERROR: %s: fcntl(fd=%d, SETFL)", __FUNCTION__, fd);

	return 0;
}

int
set_non_icanon(int fd)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0)
		err(1, "ERROR: %s: tcgetattr(fd=%d)", __FUNCTION__, fd);

	tty.c_lflag &= ~ICANON;
	tty.c_cc[VMIN] = 0;
	tty.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSANOW, &tty) == -1)
		err(1, "ERROR: %s: tcsetattr(fd=%d)", __FUNCTION__, fd);

	return 0;
}

static speed_t
get_brate(int speed)
{
	switch (speed) {
	case 0: return B0;
	case 50: return B50;
	case 75: return B75;
	case 110: return B110;
	case 134: return B134;
	case 150: return B150;
	case 200: return B200;
	case 300: return B300;
	case 600: return B600;
	case 1200: return B1200;
	case 1800: return B1800;
	case 2400: return B2400;
	case 4800: return B4800;
	case 9600: return B9600;
	case 19200: return B19200;
	case 38400: return B38400;
#ifndef _POSIX_SOURCE
	case 7200: return B7200;
	case 14400: return B14400;
	case 28800: return B28800;
	case 57600: return B57600;
	case 76800: return B76800;
	case 115200: return B115200;
	case 230400: return B230400;
#endif
	default: return speed;
	}
}

int
set_speed(int fd, int speed)
{
	struct termios tty;
	speed_t brate;

	if (tcgetattr(fd, &tty) < 0)
		err(1, "ERROR: %s: tcgetattr(fd=%d)", __FUNCTION__, fd);

	brate = get_brate(speed);

	cfsetospeed(&tty, brate);
	cfsetispeed(&tty, brate);

	if (tcsetattr(fd, TCSANOW, &tty) == -1)
		err(1, "ERROR: %s: tcsetattr(fd=%d)", __FUNCTION__, fd);

	return 0;
}

int
set_stdin(int f_revert)
{
	static struct termios saved;
	int fd = STDIN_FILENO;

	/* revert the terminal and just return */
	if (f_revert) {
		if (tcsetattr(fd, TCSANOW, &saved) == -1)
			err(1, "ERROR: %s: tcsetattr(fd=%d)", __FUNCTION__, fd);
		return 0;
	}

#if 0
	/* stdin is not needed to set icanon */
	set_non_icanon(fd);
#endif

	return 0;
}

static void
sigh(int sig)
{
	switch (sig) {
	case SIGHUP:
		set_stdin(1);
		exit(0);
		break;
	case SIGINT:
		set_stdin(1);
		exit(0);
		break;
	default:
		if (f_debug) {
			printf("DEBUG: %s: signal %d was received\n",
			    __FUNCTION__, sig);
		}
	}
	return;
}

static int
dev_open(char *name, int speed)
{
	int fd;
	int mode;

	mode = O_RDWR;
	mode |= O_NOCTTY;
	mode |= O_NONBLOCK;

	if ((fd = open(name, mode)) == -1)
		err(1, "ERROR: %s: open(%s)", __FUNCTION__, name);

	set_non_block(fd);	/* is it verbose ? */
	set_non_icanon(fd);
	set_speed(fd, speed);	/*XXX it should be a parameter */

	/* XXX set termios */

	return fd;
}

static struct devs *
devs_new(char *name, int fd, int fd2)
{
	struct devs *new;

	if ((new = calloc(1, sizeof(struct devs))) == NULL)
		err(1, "ERROR: %s: calloc(ap_socket)", __FUNCTION__);
	new->fd = fd;
	new->fd2 = fd2;
	new->name = strdup(name);
	new->dst_head = NULL;

	new->buflen = sizeof(new->buf);	/* XXX should be queue */

	return new;
}

int
devs_add(struct devs **head, struct devs *new)
{
	struct devs *p;

	if (*head == NULL) {
		*head = new;
		return 0;
	}

	for (p = *head; p->next != NULL; p = p->next)
		;
	p->next = new;

	return 0;
}

int
devfile_init()
{
	FILE *fp;

	if ((fp = fopen(outfile, "w+")) == NULL)
		err(1, "ERROR: %s: fopen(w+)", __FUNCTION__);
	fclose(fp);

	return 0;
}

int
devfile_add(char *name)
{
	FILE *fp;

	if ((fp = fopen(outfile, "a")) == NULL)
		err(1, "ERROR: %s: fopen(a)", __FUNCTION__);
	fprintf(fp, "%s\n", name);
	fclose(fp);

	return 0;
}

int
devs_open_pty(char **name, int *fd, int *fd2)
{
	struct termios pty_term;
	int brate = B115200;

	/* child's termios */
	if (tcgetattr(STDOUT_FILENO, &pty_term) < 0)
		err(1, "ERROR: %s: tcgetattr", __FUNCTION__);

	cfsetospeed(&pty_term, brate);
	cfsetispeed(&pty_term, brate);

	if (*name == NULL) {
		char pty_name[128];
		if (openpty(fd, fd2, pty_name, &pty_term, NULL) == -1)
			err(1, "ERROR: %s: openpty(NULL)", __FUNCTION__);
		if ((*name = strdup(pty_name)) == NULL)
			err(1, "ERROR: %s: strdup(pty_name)", __FUNCTION__);
	} else {
		if (openpty(fd, fd2, *name, &pty_term, NULL) == -1)
			err(1, "ERROR: %s: openpty(%s)", __FUNCTION__, *name);
	}
	if (f_debug)
		printf("DEBUG: pty=%s master=%d slave=%d\n", *name, *fd, *fd2);

	devfile_add(*name);

	set_non_icanon(*fd);
	set_non_block(*fd);

	return *fd;
}

/*
 * name: NULL when pty
 */
struct devs *
devs_prepare(int devs_type, char *name)
{
	int fd = 0, fd2 = 0;

	switch (devs_type) {
	case DEVTYPE_PTY:
		fd = devs_open_pty(&name, &fd, &fd2);
		break;
	case DEVTYPE_CON:
		fd = STDOUT_FILENO;
		name = "stdout";
		set_non_block(fd);
		break;
	default:
		errx(1, "ERROR: %s: invalid device type %d\n",
		    __FUNCTION__, devs_type);
	}

	return devs_new(name, fd, fd2);
}

int
write_hex(int fd, char *data, int datalen)
{
	int i;
	char pb[10];
	int len;

	for (i = 0; i < datalen; i++) {
		if (i != 0) {
			if (i % 16 == 0)
				write(fd, "\n", 1);
			else
			if (i % 4 == 0)
				write(fd, " ", 1);
		}
		len = snprintf(pb, sizeof(pb), "%02x", data[i]&0xff);
		write(fd, pb, len);
	}
	write(fd, "\n", 1);

	return 0;
}

int
devs_write_do(struct devs *d, char *buf, int datalen)
{
	int wlen;

	/* write data to stdout and return */
	if (d->fd == STDOUT_FILENO && f_hex) {
		wlen = write_hex(d->fd, buf, datalen);
		return 0;
	}

	/* write data to the device */
	wlen = write(d->fd, buf, datalen);
	if (wlen < 0) {
		switch (errno) {
		case EAGAIN:
			if (f_debug > 0) {
				printf("DEBUG: %s: write(EAGAIN) on %s\n",
				    __FUNCTION__, d->name);
			}
			return 0;
		case EIO:
		{
			int fd, fd2;
			devs_open_pty(&d->name, &fd, &fd2);
			if (close(d->fd))
				err(1, "ERROR: %s: close(d->fd)", __FUNCTION__);
			if (close(d->fd2))
				err(1, "ERROR: %s: close(d->fd)", __FUNCTION__);
			d->fd = fd;
			d->fd2 = fd2;
			return 0;
		}
		}
		err(1, "ERROR: %s: write()", __FUNCTION__);
	}
	else if (datalen != wlen) {
		warnx("WARN: %s: write(%s) len=%d wlen=%d",
		    __FUNCTION__, d->name, datalen, wlen);
	}

	return 0;
}

int
devs_write(struct devs *head, char *buf, int datalen)
{
	struct devs *d;

	if (f_debug > 2)
		printf("read len=%d\n", datalen);

	for (d = head; d != NULL; d = d->next)
		devs_write_do(d, buf, datalen);

	return 0;
}

int
run(int fd_parent)
{
	struct devs *child_head;
	fd_set rfds, wfds, efds;
	fd_set fdmask;
	struct timeval *timeout;
	int nfds;
	int i;
	int ret;
	int len;

	/* open the output devices */
	child_head = NULL;
	if (f_stdout) {
		n_childs--;
		struct devs *d = devs_prepare(DEVTYPE_CON, NULL);
		devs_add(&child_head, d);
	}
	for (i = 0; i < n_childs; i++) {
		struct devs *d = devs_prepare(DEVTYPE_PTY, NULL);
		devs_add(&child_head, d);
	}

	/* set timeout */
	timeout = NULL;
#ifdef USE_TIMEOUT
	if ((timeout = calloc(1, sizeof(*timeout))) == NULL)
		err(1, "ERROR: %s: calloc(timeout)", __FUNCTION__);
		timeout->tv_sec = 1;
		timeout->tv_usec = 0;
	}
#endif

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);
	FD_ZERO(&fdmask);
	FD_SET(fd_parent, &fdmask);
	nfds = fd_parent;
	nfds++;

	do {
		rfds = fdmask;
		efds = fdmask;

		ret = select(nfds, &rfds, &wfds, &efds, timeout);
		if (ret < 0) {
			switch (errno) {
			case EINTR:
				warn("WARN: %s: select(EINTR)", __FUNCTION__);
				continue;
			default:
				err(1, "ERROR: %s: select()", __FUNCTION__);
			}
		}
#ifdef USE_TIMEOUT
		else if (ret == 0) {
			/* check fds */
			check_fd(fd_parent);
			struct devs *d;
			for (d = child_head; d != NULL; d = d->next)
				check_fd(d->fd);
			continue;
		}
#endif

		if (FD_ISSET(fd_parent, &rfds)) {
			char buf[1024];
			len = read(fd_parent, buf, sizeof(buf));
			if (len < 0) {
				switch (errno) {
				case EAGAIN:
					printf("DEBUG: %s: read:EAGAIN "
					    "on fd_parent\n", __FUNCTION__);
					/* skip it */
					break;
				}
				err(1, "ERROR: %s: read()", __FUNCTION__);
			}
			else if (len == 0) {
				printf("original port has been closed.\n");
				exit(0);
			}
			else if (len > 0) {
				ret = devs_write(child_head, buf, len);
			}
		}
	} while(1);

	return 0;
}

int
main(int argc, char *argv[])
{
	int ch;
	int fd_parent;

	prog_name = 1 + rindex(argv[0], '/');

	while ((ch = getopt(argc, argv, "n:b:o:Cxdh")) != -1) {
		switch (ch) {
		case 'n':
			n_childs = atoi(optarg);
			break;
		case 'b':
			n_speed = atoi(optarg);
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'C':
			f_stdout++;
			break;
		case 'x':
			f_hex++;
			break;
		case 'd':
			f_debug++;
			break;
		case 'h':
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (n_childs == 0)
		usage();
	if (!f_stdout && f_hex)
		usage();

	if (argc != 1)
		usage();

	devfile_init();

	/* open the original device */
	if (strcmp(argv[0], "con") == 0 ||
	    strcmp(argv[0], "stdin") == 0 ||
	    strcmp(argv[0], "-") == 0) {
		set_stdin(0);
		fd_parent = STDIN_FILENO;
	} else {
		fd_parent = dev_open(argv[0], n_speed);
	}

	signal(SIGHUP, sigh);

	run(fd_parent);

	return 0;
}
