#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
#include <X11/Xlib.h>
#include "opt.h"

static int keyb_handler(void);
static void sig_handler(int s);
static void cleanup(void);

static struct options *opt;
static int keyb_pid;
static int exiting;

int frapix_init_keyb(struct options *o)
{
	int pid;

	opt = o;
	signal(SIGCHLD, sig_handler);

	if((pid = fork()) == -1) {
		perror("frapix: failed to fork keyboard process");
		return -1;
	} else if(pid) {
		keyb_pid = pid;
		atexit(cleanup);
	} else {
		keyb_handler();
		_exit(0);
	}
	return 0;
}

static int keyb_handler(void)
{
	Display *dpy;
	int scr;
	Window win, root;
	/*XSetWindowAttributes xattr;*/

	if(!(dpy = XOpenDisplay(0))) {
		fprintf(stderr, "frapix: failed to connect to the X server\n");
		return -1;
	}
	scr = DefaultScreen(dpy);
	root = RootWindow(dpy, scr);

	win = XCreateSimpleWindow(dpy, root, 0, 0, 16, 16, 0, 0, 0);
	XSelectInput(dpy, win, KeyPress);

	/* TODO handle X errors */
	XGrabKey(dpy, XKeysymToKeycode(dpy, opt->shot_key), AnyModifier, root, True, GrabModeAsync, GrabModeAsync);

	for(;;) {
		XEvent xev;
		XNextEvent(dpy, &xev);

		switch(xev.type) {
		case KeyPress:
			if(xev.xkey.keycode == XKeysymToKeycode(dpy, opt->shot_key)) {
				printf("foo\n");
				opt->capture_shot = 1;
			}
			break;

		default:
			break;
		}
	}
}

static void sig_handler(int s)
{
	if(s == SIGCHLD) {
		wait(0);
		if(!exiting) {
			fprintf(stderr, "frapix: keyboard process died aparently!\n");
		}
	}
}

static void cleanup(void)
{
	exiting = 1;
	kill(keyb_pid, SIGTERM);
}
