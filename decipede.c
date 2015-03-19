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

struct xxx {
	int fd;
	struct devs *dev;	/* pointer to the devs */
	struct xxx *next;
};

struct devs *devs_head = NULL;

int n_childs = 1;
int f_stdout = 0;
int f_debug = 0;

char *prog_name = NULL;

void
usage()
{
	printf(
"Usage: %s [-dh] [-n num] [-C] (dev)\n"
"\n"
"    It reads data from the device specified at the end of parameters.\n"
"    And, it writes the data into some pseudo terminal that it created\n"
"    when it had started.\n"
"    You can use a special word \"con\" to write the data into the standard\n"
"    output.\n"
"\n"
"    -n: specifies the number of pseudo devices to be created. (default: 1)\n"
"    -C: writes data into the console as the one of the pseudo devices.\n"
"\n"
	, prog_name);

	exit(0);
}

int
check_fd(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		err(1, "ERROR: %s: fcntl(GETFL)", __FUNCTION__);
	if (f_debug)
		printf("DEBUG: flags(%d)=%d\n", fd, flags);

	return 0;
}

int
set_non_block(int fd)
{
	int flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		err(1, "ERROR: %s: fcntl(GETFL)", __FUNCTION__);
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
		err(1, "ERROR: %s: fcntl(SETFL)", __FUNCTION__);

	return 0;
}

int
set_non_icanon(int fd)
{
	struct termios new;

	/* child's termios */
	if (tcgetattr(fd, &new) < 0)
		err(1, "ERROR: %s: tcgetattr(stdin)", __FUNCTION__);

//	new.c_iflag &= ~( INLCR | IGNCR | ICRNL | ISTRIP );
//	new.c_lflag &= ~ECHO;
//	new.c_lflag |= BRKINT;
	new.c_lflag &= ~ICANON;
	new.c_cc[VMIN] = 0;
	new.c_cc[VTIME] = 0;

	if (tcsetattr(fd, TCSANOW, &new) == -1)
		err(1, "tcsetattr(stdin)");

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
			err(1, "tcsetattr(stdin)");
		return 0;
	}

	/* set terminal */
	set_non_icanon(fd);

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
dev_open(char *name)
{
	int fd;
	int mode;

	mode = O_RDWR;
	mode |= O_NOCTTY;
	mode |= O_NONBLOCK;

	if ((fd = open(name, mode)) == -1)
		err(1, "ERROR: %s: open()", __FUNCTION__);

	set_non_block(fd);	/* is it verbose ? */

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
devs_open_pty(char **name, int *fd, int *fd2)
{
	struct termios pty_term;
	int brate = B115200;

	/* child's termios */
	if (tcgetattr(STDIN_FILENO, &pty_term) < 0)
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
	else
		printf("%s\n", *name);
//	close(*fd2);
//	set_non_icanon(*fd2);
//	set_non_block(*fd2);

#ifdef __linux__
	if (f_debug)
		printf("F_GETPIPE_SZ=%d\n", fcntl(*fd, F_GETPIPE_SZ, 0));
#endif

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
devs_write_do(struct devs *d, char *buf, int datalen)
{
	int wlen;

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
		err(1, "calloc(timeout)");
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

	while ((ch = getopt(argc, argv, "n:Cdh")) != -1) {
		switch (ch) {
		case 'n':
			n_childs = atoi(optarg);
			break;
		case 'C':
			f_stdout++;
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

	if (argc != 1)
		usage();

	/* open the original device */
	if (strcmp(argv[0], "con") == 0 ||
	    strcmp(argv[0], "stdout") == 0 ||
	    strcmp(argv[0], "-") == 0) {
		set_stdin(0);
		fd_parent = STDIN_FILENO;
	} else {
		fd_parent = dev_open(argv[0]);
	}

	signal(SIGHUP, sigh);

	run(fd_parent);

	return 0;
}
