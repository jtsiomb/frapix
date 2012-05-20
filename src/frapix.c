/*
frapix - framerate monitoring and screen capture for UNIX OpenGL programs.
Copyright (C) 2009 John Tsiombikas <nuclear@member.fsf.org>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>
#ifndef __USE_GNU
/* otherwise RTLD_NEXT won't be defined in recent glibc versions */
#define __USE_GNU
#endif
#include <dlfcn.h>
#include <sys/mman.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <imago2.h>
#include "opt.h"

int frapix_init_keyb(struct options *o);

static int init(void);
static void overlay(void);
static unsigned long get_msec(void);

static void (*swap_buffers)(Display*, GLXDrawable);

/* TODO load these */
static void (*glUseProgramObjectARB)(unsigned int);
static unsigned int (*glGetHandleARB)(unsigned int);

static struct options *opt;

static int cur_fps;
static int cap_num;

#define OVERHEAD	4
void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
	unsigned long msec, interv;
	static unsigned long prev_frame, prev_print;
	static unsigned long frames;

	if(!swap_buffers && init() == -1) {
		return;
	}

	if(opt->capture_shot || opt->capture_vid) {
		char fname[64];
		int xsz, ysz;
		static int img_xsz = -1, img_ysz = -1;
		static uint32_t *img;

		int vp[4];
		glGetIntegerv(GL_VIEWPORT, vp);
		xsz = vp[2];
		ysz = vp[3];

		if(opt->capture_shot) {
			printf("grabbing image %dx%d\n", xsz, ysz);
		}

		if(xsz != img_xsz || ysz != img_ysz) {
			free(img);
			if(!(img = malloc(xsz * ysz * sizeof *img))) {
				perror("frapix: failed to allocate memory");
			}
			img_xsz = xsz;
			img_ysz = ysz;
		}

		glReadPixels(0, 0, xsz, ysz, GL_RGBA, GL_UNSIGNED_BYTE, img);

		sprintf(fname, opt->shot_fname, cap_num++);

		/*set_image_option(IMG_OPT_COMPRESS, 1);
		set_image_option(IMG_OPT_INVERT, 1);
		if(save_image(fname, img, xsz, ysz, IMG_FMT_TGA) == -1) {
			fprintf(stderr, "frapix: failed to save image: %s\n", opt->shot_fname);
		}*/
		if(img_save_pixels(fname, img, xsz, ysz, IMG_FMT_RGBA32) == -1) {
			fprintf(stderr, "frapix: failed to save image: %s\n", opt->shot_fname);
		}
		opt->capture_shot = 0;
	}

	overlay();
	swap_buffers(dpy, drawable);

	msec = get_msec();

	interv = msec - prev_print;
	if(interv >= opt->print_interval && interv > 0) {
		cur_fps = 1000 * frames / interv;
		prev_print = msec;
		frames = 0;
	} else {
		frames++;
	}

	interv = msec - prev_frame + OVERHEAD;
	if(interv < opt->frame_interval) {
		struct timespec ts;

		ts.tv_sec = 0;
		ts.tv_nsec = (opt->frame_interval - interv) * 1000000;

		if(nanosleep(&ts, 0) == -1) {
			perror("nanosleep failed");
		}
	}

	prev_frame = get_msec();
}

static int init(void)
{
	char *env;
	int fd;

	if(!swap_buffers) {
		if(!(swap_buffers = dlsym(RTLD_NEXT, "glXSwapBuffers"))) {
			fprintf(stderr, "symbol glXSwapBuffers not found: %s\n", dlerror());
			return -1;
		}
	}

	/* allocate shared memory for option struct */
	if((fd = open("/dev/zero", O_RDWR)) == -1) {
		perror("frapix: failed to open /dev/zero");
		return -1;
	}
	if((opt = mmap(0, sizeof *opt, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) == (void*)-1) {
		perror("frapix: failed to mmap /dev/zero");
		return -1;
	}

	opt->print_interval = 1000;
	opt->frame_interval = 0;
	opt->fps_pos_x = 0.95;
	opt->fps_pos_y = 0.925;
	opt->shot_key = XK_F12;
	opt->vid_key = XK_F11;
	opt->shot_fname = strdup("/tmp/frapix%04d.tga");

	if((env = getenv("FRAPIX_FPS_UPDATE_RATE"))) {
		if(!isdigit(*env)) {
			fprintf(stderr, "FRAPIX_FPS_UPDATE_RATE must be set to the period of fps update in milliseconds\n");
		} else {
			opt->print_interval = atoi(env);
		}
	}

	if((env = getenv("FRAPIX_FPS_LIMIT"))) {
		printf("env: %s\n", env);
		if(!isdigit(*env)) {
			fprintf(stderr, "FRAPIX_FPS_LIMIT must be followed by the maximum fps number\n");
		} else {
			int max_fps = atoi(env);
			if(max_fps) {
				opt->frame_interval = 1000 / max_fps;

				printf("set fps limit: %d fps (interval: %ld msec)\n", max_fps, opt->frame_interval);
			}
		}
	}

	if(frapix_init_keyb(opt) == -1) {
		return -1;
	}

	return 0;
}

static int count_digits(int x)
{
	int dig = 0;
	if(!x) return 1;

	while(x) {
		x /= 10;
		dig++;
	}
	return dig;
}

static int diglist[10];

#define XSZ		0.025
#define YSZ		0.05
#define DX		0.0075

static struct {
	float x, y;
} led[7][4] = {
	{{-XSZ - DX, 0}, {-XSZ + DX, 0}, {-XSZ + DX, YSZ + DX}, {-XSZ - DX, YSZ + DX}},
	{{-XSZ - DX, -YSZ - DX}, {-XSZ + DX, -YSZ - DX}, {-XSZ + DX, 0}, {-XSZ - DX, 0}},
	{{-XSZ - DX, -YSZ - DX}, {XSZ + DX, -YSZ - DX}, {XSZ + DX, -YSZ + DX}, {-XSZ - DX, -YSZ + DX}},
	{{XSZ - DX, -YSZ - DX}, {XSZ + DX, -YSZ - DX}, {XSZ + DX, 0}, {XSZ - DX, 0}},
	{{XSZ - DX, 0}, {XSZ + DX, 0}, {XSZ + DX, YSZ + DX}, {XSZ - DX, YSZ + DX}},
	{{-XSZ - DX, YSZ - DX}, {XSZ + DX, YSZ - DX}, {XSZ + DX, YSZ + DX}, {-XSZ - DX, YSZ + DX}},
	{{-XSZ - DX, -DX}, {XSZ + DX, -DX}, {XSZ + DX, +DX}, {-XSZ - DX, +DX}}
};

static int seglut[10][7] = {
	{1, 1, 1, 1, 1, 1, 0},	/* 0 */
	{0, 0, 0, 1, 1, 0, 0},	/* 1 */
	{0, 1, 1, 0, 1, 1, 1},	/* 2 */
	{0, 0, 1, 1, 1, 1, 1},	/* 3 */
	{1, 0, 0, 1, 1, 0, 1},	/* 4 */
	{1, 0, 1, 1, 0, 1, 1},	/* 5 */
	{1, 1, 1, 1, 0, 1, 1},	/* 6 */
	{0, 0, 0, 1, 1, 1, 0},	/* 7 */
	{1, 1, 1, 1, 1, 1, 1},	/* 8 */
	{1, 0, 1, 1, 1, 1, 1}	/* 9 */
};


static void draw_digit(int x)
{
	int i;

	if(x < 0 || x > 9) return;

	if(!diglist[x]) {
		diglist[x] = glGenLists(1);
		glNewList(diglist[x], GL_COMPILE);

		glBegin(GL_QUADS);
		for(i=0; i<7; i++) {
			if(seglut[x][i]) {
				glVertex2f(led[i][0].x, led[i][0].y);
				glVertex2f(led[i][1].x, led[i][1].y);
				glVertex2f(led[i][2].x, led[i][2].y);
				glVertex2f(led[i][3].x, led[i][3].y);
			}
		}
		glEnd();

		glEndList();
	}

	glCallList(diglist[x]);
}

static void overlay(void)
{
	int i, fps, fps_digits = count_digits(cur_fps);
	unsigned int sdr = 0;

	glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT | GL_VIEWPORT_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(opt->fps_pos_x, opt->fps_pos_y, 0);

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);
	glColorMask(1, 1, 1, 1);

	/* TODO also disable shaders */
	if(glUseProgramObjectARB && glGetHandleARB) {
		sdr = glGetHandleARB(GL_PROGRAM_OBJECT_ARB);
		glUseProgramObjectARB(0);
	}

	glColor3f(1.0f, 1.0f, 0.0f);

	fps = cur_fps;
	for(i=0; i<fps_digits; i++) {
		draw_digit(fps % 10);
		fps /= 10;
		glTranslatef(-XSZ * 3.0, 0, 0);
	}

	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	if(glUseProgramObjectARB && sdr) {
		glUseProgramObjectARB(sdr);
	}

	glPopAttrib();
}

static unsigned long get_msec(void)
{
	struct timeval tv;
	static struct timeval tv0;

	gettimeofday(&tv, 0);

	if(tv0.tv_sec == 0 && tv0.tv_usec == 0) {
		tv0 = tv;
		return 0;
	}
	return (tv.tv_sec - tv0.tv_sec) * 1000 + (tv.tv_usec - tv0.tv_usec) / 1000;
}
