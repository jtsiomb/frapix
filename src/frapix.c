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
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

static int init(void);
static void overlay(void);
static unsigned long get_msec(void);

static void *so;
static void (*swap_buffers)(Display*, GLXDrawable);

static unsigned long print_interval = 1000;
static unsigned long frame_interval = 0;
static int cur_fps;
static float fps_pos_x, fps_pos_y;

void glXSwapBuffers(Display *dpy, GLXDrawable drawable)
{
	unsigned long msec, interv;
	static unsigned long prev_frame, prev_print;
	static unsigned long frames;

	if(!swap_buffers && init() == -1) {
		return;
	}

	msec = get_msec();
	
	overlay();
	swap_buffers(dpy, drawable);
	
	interv = msec - prev_print;
	if(interv >= print_interval) {
		cur_fps = 1000 * frames / interv;
		prev_print = msec;
		frames = 0;
	} else {
		frames++;
	}

	interv = msec - prev_frame;
	if(interv < frame_interval) {
		struct timespec ts;

		ts.tv_sec = 0;
		ts.tv_nsec = (frame_interval - interv) * 1000000;

		if(nanosleep(&ts, 0) == -1) {
			perror("nanosleep failed");
		}
	}

	prev_frame = msec;
}

static int init(void)
{
	char *env;

	if(!so) {
		if(!(so = dlopen("libGL.so", RTLD_LAZY))) {
			fprintf(stderr, "failed to open libGL.so: %s\n", dlerror());
			return -1;
		}
	}
	if(!swap_buffers) {
		if(!(swap_buffers = dlsym(so, "glXSwapBuffers"))) {
			fprintf(stderr, "symbol glXSwapBuffers not found: %s\n", dlerror());
			return -1;
		}
	}

	if((env = getenv("FRAPIX_FPS_UPDATE_RATE"))) {
		if(!isdigit(*env)) {
			fprintf(stderr, "FRAPIX_FPS_UPDATE_RATE must be set to the period of fps update in milliseconds\n");
		} else {
			print_interval = atoi(env);
		}
	}

	if((env = getenv("FRAPIX_FPS_LIMIT"))) {
		printf("env: %s\n", env);
		if(!isdigit(*env)) {
			fprintf(stderr, "FRAPIX_FPS_LIMIT must be followed by the maximum fps number\n");
		} else {
			int max_fps = atoi(env);
			frame_interval = 1000 / max_fps;

			printf("set fps limit: %d fps (interval: %ld msec)\n", max_fps, frame_interval);
		}
	}

	fps_pos_x = 0.95;
	fps_pos_y = 0.925;

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

	glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_TRANSFORM_BIT | GL_VIEWPORT_BIT);

	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();

	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glTranslatef(fps_pos_x, fps_pos_y, 0);

	glDisable(GL_LIGHTING);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_1D);
	glDisable(GL_TEXTURE_2D);
	glColorMask(1, 1, 1, 1);

	/* TODO also disable shaders */

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
