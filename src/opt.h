#ifndef OPT_H_
#define OPT_H_

struct options {
	unsigned long print_interval;
	unsigned long frame_interval;
	float fps_pos_x, fps_pos_y;
	unsigned int shot_key, vid_key;
	char *shot_fname;

	int capture_shot, capture_vid;
};

#endif	/* OPT_H_ */
