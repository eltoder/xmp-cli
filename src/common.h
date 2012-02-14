
struct options {
	int start;		/* start order */
	int amplify;		/* amplification factor */
	int freq;		/* sampling rate */
	int format;		/* sample format */
	int time;		/* max. replay time */
	int mix;		/* channel separation */
	int loop;		/* loop module */
	int random;		/* play in random order */
	int load_only;		/* load module and exit */
	int verbose;
	int silent;		/* silent output */
	char *out_file;		/* output file name */
	char *ins_path;		/* instrument path */
	char mute[XMP_MAX_CHANNELS];
};

struct control {
	int skip;
	int loop;
	int pause;
};

int set_tty(void);
int reset_tty(void);
