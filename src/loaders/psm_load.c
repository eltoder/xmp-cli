/* Epic Megagames PSM loader for xmp
 * Copyright (C) 2005 Claudio Matsuoka and Hipolito Carraro Jr
 * Based on the PSM loader from Modplug by Olivier Lapicque
 *
 * This file is part of the Extended Module Player and is distributed
 * under the terms of the GNU General Public License. See doc/COPYING
 * for more information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "load.h"
#include "iff.h"


struct psm_hdr {
	int8 songname[8];		/* "MAINSONG" */
	uint8 reserved1;
	uint8 reserved2;
	uint8 channels;
} PACKED;

struct psm_pat {
	uint32 size;
	uint32 name;
	uint16 rows;
	uint16 reserved1;
	uint8 data;
} PACKED;

struct psm_ins {
	uint8 flags;
	int8 songname[8];
	uint32 smpid;
	int8 samplename[34];
	uint32 reserved1;
	uint8 reserved2;
	uint8 insno;
	uint8 reserved3;
	uint32 length;
	uint32 loopstart;
	uint32 loopend;
	uint16 reserved4;
	uint8 defvol;
	uint32 reserved5;
	uint32 samplerate;
	uint8 reserved6[19];
} PACKED;


static int cur_pat;
static int cur_ins;
uint32 *pnam;
uint32 *pord;


static void get_sdft(int size, uint16 *buffer)
{
}

static void get_titl(int size, uint16 *buffer)
{
	strncpy(xmp_ctl->name, (char *)buffer, size > 32 ? 32 : size);
}

static void get_dsmp_cnt(int size, uint16 *buffer)
{
	xxh->ins++;
	xxh->smp = xxh->ins;
}

static void get_pbod_cnt(int size, uint16 *buffer)
{
	xxh->pat++;
}


static void get_dsmp(int size, void *buffer)
{
	int i;
	struct psm_ins *pi;

	pi = (struct psm_ins *)buffer;

	L_ENDIAN32(pi->length);
	L_ENDIAN32(pi->loopstart);
	L_ENDIAN32(pi->loopend);
	L_ENDIAN32(pi->samplerate);
	L_ENDIAN32(pi->smpid);

	i = cur_ins;
	xxi[i] = calloc(sizeof (struct xxm_instrument), 1);
	xxs[i].len = pi->length;
	xxih[i].nsm = !!(xxs[i].len);
	xxs[i].lps = pi->loopstart;
	xxs[i].lpe = pi->loopend;
	xxs[i].flg = pi->loopend > 2 ? WAVE_LOOPING : 0;
	xxi[i][0].vol = pi->defvol;
	xxi[i][0].pan = 0x80;
	xxi[i][0].sid = i;
	c2spd_to_note(pi->samplerate, &xxi[i][0].xpo, &xxi[i][0].fin);

	strncpy ((char *)xxih[i].name, pi->samplename, 32);
	str_adj ((char *)xxih[i].name);

	if ((V(1)) && (strlen ((char *) xxih[i].name) || (xxs[i].len > 1)))
	    report ("\n[%2X] %-32.32s %05x %05x %05x %c V%02x %5d", i,
		xxih[i].name, xxs[i].len, xxs[i].lps, xxs[i].lpe, xxs[i].flg
		& WAVE_LOOPING ? 'L' : ' ', xxi[i][0].vol, pi->samplerate);

	xmp_drv_loadpatch (NULL, i, xmp_ctl->c4rate, XMP_SMP_NOLOAD,
		&xxs[i], (uint8 *)buffer + sizeof(struct psm_hdr));

	cur_ins++;
}


static void get_pbod (int size, void *buffer)
{
	int i, r;
	struct xxm_event *event;
	struct psm_pat *pp = (struct psm_pat *)buffer;
	char *p;
	uint8 f, c, c2;
	int rows, pos, len;

	i = cur_pat;
	pnam[i] = pp->name;

	PATTERN_ALLOC(i);
	xxp[i]->rows = pp->rows;
	TRACK_ALLOC(i);

	p = &pp->data;
	len = pp->size;
	rows = xxp[i]->rows;
	pos = 0;

	size -= sizeof(struct psm_pat) - 1;

	for (r = 0; r < rows && pos < size; ) {
		f = p[pos++];
		if (f == 0x00)
			break;
		c = p[pos++];

		if (((f & 0xf0) == 0x10) && (c <= c2)) {
			if ((pos + 1 < len) && (!(p[pos] & 0x0f))
		    	    /*&& (p[pos + 1] < that->m_nChannels)*/) {
		    		r++;
		    		c2 = c;
		    		continue;
			}
		}

		if ((pos >= len) || (r >= rows))
			break;

		if (!(f & 0xf0)) {
			r++;
			c2 = c;
			continue;
		}

		event = &EVENT(i, c, r);

		if ((f & 0x40) && (pos + 1 < len)) {
			uint8 note = p[pos++];
			uint8 ins = p[pos++];

			if (note && (note < 0x80))
				note = (note >> 4) * 12 + (note & 0x0f) + 12 + 1;
			event->note = note;
			event->ins = ins;
		}

		if ((f & 0x20) && (pos < len)) {
			event->vol = p[pos++] / 2;
		}

		if ((f & 0x10) && (pos + 1 < len)) {
			uint8 fxt = p[pos++];
			uint8 fxp = p[pos++];

			switch (fxt) {
			case 0x01:		/* 01: fine volslide up */
				fxt = FX_VOLSLIDE;
				fxp |= 0x0f;
				break;
			case 0x04: 		/* 04: fine volslide down */
				fxt = FX_VOLSLIDE;
				fxp >>= 4;
				fxp |= 0xf0;
				break;
		    	case 0x0C:		/* 0C: portamento up */
				fxt = FX_PORTA_UP;
				fxp = (fxp + 1) / 2;
				break;
			case 0x0E:		/* 0E: portamento down */
				fxt = FX_PORTA_DN;
				fxp = (fxp + 1) / 2;
				break;
			case 0x33:		/* 33: Position Jump */
				fxt = FX_JUMP;
				break;
		    	case 0x34:		/* 34: Pattern break */
				fxt = FX_BREAK;
				break;
			case 0x3D:		/* 3D: speed */
				fxt = FX_TEMPO;
				break;
			case 0x3E:		/* 3E: tempo */
				fxt = FX_S3M_TEMPO;
				break;
			default:
				fxt = fxp = 0;
		}

		event->fxt = fxt;
		event->fxp = fxp;
	    }
	    c2 = c;
	}

	cur_pat++;
}

static void get_song(int size, char *buffer)
{
	struct psm_hdr *ph = (struct psm_hdr *)buffer;

	xxh->chn = ph->channels;
	if (*xmp_ctl->name == 0)
		strncpy(xmp_ctl->name, ph->songname, 8);
}

static void get_song_2(int size, char *buffer)
{
	char *p;
	uint32 oplh_size;

	xxh->len = 0;

	p = buffer + 29;
	oplh_size = *(uint32 *)p;
	L_ENDIAN32(oplh_size);

	/* Get patterns by moving to the end of the OPLH chunk and
	 * get back checking for 0x01. Certainly not the way it
	 * was designed to work, but enough to play jjrabit psms.
	 * Must fix this later!
	 */
	for (p = buffer + 33 + oplh_size - 9; *p == 1; p -= 5);

	for (p += 5; *p == 0x01; p += 5) {
		uint32 pat = *(uint32 *)(p + 1);
		pord[xxh->len++] = pat;
	}
}

int psm_load(FILE *f)
{
	char magic[4];
	int offset;
	int i, j;

	LOAD_INIT ();

	/* Check magic */
	fread(magic, 1, 4, f);
	if (strncmp (magic, "PSM ", 4))
		return -1;

	strcpy (xmp_ctl->type, "Epic Megagames (PSM)");

	fseek(f, 8, SEEK_CUR);		/* skip file size and FILE */
	xxh->smp = xxh->ins = 0;
	cur_pat = 0;
	cur_ins = 0;
	offset = ftell(f);

	/* IFF chunk IDs */
	iff_register("TITL", get_titl);
	iff_register("SDFT", get_sdft);
	iff_register("SONG", get_song);
	iff_register("DSMP", get_dsmp_cnt);
	iff_register("PBOD", get_pbod_cnt);
	iff_setflag(IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof (f))
		iff_chunk(f);

	iff_release();

	xxh->trk = xxh->pat * xxh->chn;
	pnam = malloc(xxh->pat * sizeof(uint32));	/* pattern names */
	pord = malloc(255 * sizeof(uint32));		/* pattern orders */

	MODULE_INFO();
	INSTRUMENT_INIT();
	PATTERN_INIT();

	if (V(0)) {
	    report("Stored patterns: %d\n", xxh->pat);
	    report("Stored samples : %d", xxh->smp);
	}

	fseek(f, offset, SEEK_SET);

	iff_register("SONG", get_song_2);
	iff_register("DSMP", get_dsmp);
	iff_register("PBOD", get_pbod);
	iff_setflag(IFF_LITTLE_ENDIAN);

	/* Load IFF chunks */
	while (!feof (f))
		iff_chunk(f);

	iff_release ();

	for (i = 0; i < xxh->len; i++) {
		for (j = 0; j < xxh->pat; j++) {
			if (pord[i] == pnam[j]) {
				xxo[i] = j;
				break;
			}
		}

		if (j == xxh->pat)
			break;
	}

	free(pnam);
	free(pord);

	if (V(0))
		report("\n");

	return 0;
}

