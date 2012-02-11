/* Extended Module Player
 * Copyright (C) 1996-2012 Claudio Matsuoka and Hipolito Carraro Jr
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

/*
 * Based on the AIX XMMS output plugin by Peter Alm, Thomas Nilsson
 * and Olle Hallnas.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/audio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common.h"
#include "driver.h"
#include "mixer.h"

static int audio_fd;
static audio_control control;
static audio_change change;

static int init(struct context_data *);
static int setaudio(struct xmp_options *);
static void bufdump(struct context_data *, void *, int);
static void shutdown(struct context_data *);

static void dummy()
{
}

static char *help[] = {
	"gain=val", "Audio output gain (0 to 255)",
	/* "buffer=val", "Audio buffer size (default is 32768)", */
	NULL
};

struct xmp_drv_info drv_bsd = {
	"aix",			/* driver ID */
	"AIX PCM audio",	/* driver description */
	help,			/* help */
	init,			/* init */
	shutdown,		/* shutdown */
	dummy,			/* starttimer */
	dummy,			/* stoptimer */
	bufdump,		/* bufdump */
	NULL
};

#define AUDIO_MIN_GAIN 0
#define AUDIO_MAX_GAIN 100

static int setaudio(struct xmp_options *o)
{
	audio_init ainit;
	int gain = 128;
	int bsize = 32 * 1024;
	char *token, **parm;

	parm_init();
	chkparm1("gain", gain = strtoul(token, NULL, 0));
	/* chkparm1 ("buffer", bsize = strtoul(token, NULL, 0)); */
	parm_end();

	if (gain < AUDIO_MIN_GAIN)
		gain = AUDIO_MIN_GAIN;
	if (gain > AUDIO_MAX_GAIN)
		gain = AUDIO_MAX_GAIN;

	init.mode = PCM;	/* audio format */
	init.srate = o->freq;	/* sample rate */
	init.operation = PLAY;	/* PLAY or RECORD */
	init.channels = o->outfmt & XMP_FORMAT_MONO ? 1 : 2;
	init.bits_per_sample = o->resol;	/* bits per sample */
	init.flags = BIG_ENDIAN | TWOS_COMPLEMENT;

	if (ioctl(audio_fd, AUDIO_INIT, &init) < 0) {
		close(audio_fd);
		return XMP_ERR_DINIT;
	}

	/* full blast; range: 0-0x7fffffff */
	change.volume = 0x7fffffff * (1.0 * gain / 200.0);
	change.monitor = AUDIO_IGNORE;	/* monitor what's recorded ? */
	change.input = AUDIO_IGNORE;	/* input to record from */
	change.output = OUTPUT_1;	/* line-out */
	change.balance = 0x3FFFFFFF;

	control.ioctl_request = AUDIO_CHANGE;
	control.request_info = (char *)&change;
	if (ioctl(audio_fd, AUDIO_CONTROL, &control) < 0) {
		close(audio_fd);
		return XMP_ERR_DINIT;
	}

	/* start playback - won't actually start until write() calls occur */
	control.ioctl_request = AUDIO_START;
	control.position = 0;
	if (ioctl(audio_fd, AUDIO_CONTROL, &control) < 0) {
		close(audio_fd);
		return XMP_ERR_DINIT;
	}

	return 0;
}

static int init(struct context_data *ctx)
{
	if ((audio_fd = open("/dev/paud0/1", O_WRONLY)) == -1)
		return XMP_ERR_DINIT;

	if (setaudio(&ctx->o) != 0)
		return XMP_ERR_DINIT;

	return 0;
}

/* Build and write one tick (one PAL frame or 1/50 s in standard vblank
 * timed mods) of audio data to the output device.
 */
static void bufdump(struct context_data *ctx, void *b, int i)
{
	int j;

	while (i) {
		if ((j = write(audio_fd, b, i)) > 0) {
			i -= j;
			(char *)b += j;
		} else
			break;
	};
}

static void shutdown(struct context_data *ctx)
{
	control.ioctl_request = AUDIO_STOP;
	ioctl(audio_fd, AUDIO_CONTROL, &control);
	close(audio_fd);
}