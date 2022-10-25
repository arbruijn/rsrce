/* rsrce -- a Macintosh resource fork editor
 * Copyright (C) 2004  Jeremie Koenig
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <netinet/in.h>
#include "resource.h"
#include "translate.h"


#define BUFSZ 512

int tr_raw_export(FILE *f, void *data, int len)
{
	return (fwrite(data, 1, len, f) == len) ? 0 : -1;
}

int tr_raw_import(FILE *f, void **data, int *len)
{
	int sz, n;
	
	sz = *len;
	*len = 0;
	
	for(;;) {
		if(*len + BUFSZ > sz) {
			sz += BUFSZ;
			*data = realloc(*data, sz);
		}
		n = fread(*data + *len, 1, sz - *len, f);
		if(n == 0) return feof(f) ? 0 : -1;
		*len += n;
	}
}

int tr_str_export(FILE *f, void *data, int len)
{
	return (fwrite(data+1, 1, len-1, f) == len-1 && fputc('\n', f) == '\n')
		? 0 : -1;
}

int tr_str_import(FILE *f, void **data, int *len)
{
	int sz, c;

	*data = realloc(*data, sz = BUFSZ);
	*len = 1;

	while((c = fgetc(f)) >= 0 && c != '\n') {
		if(*len >= sz) *data = realloc(*data, sz += BUFSZ);
		((unsigned char *) *data)[(*len)++] = c;
	}
	* (unsigned char *) *data = *len - 1;

	return (feof(f) || c == '\n') ? 0 : -1;
}

int tr_strhash_export(FILE *f, void *data, int len)
{
	uint8_t *buf = data;
	int strn, pos;

	strn = (len >= 2) ? htons(* (uint16_t *) data) : 1;
	pos = sizeof (uint16_t);

	while(strn--) {
		if(pos + 1 > len || pos + 1 + buf[pos] > len) {
			fprintf(stderr,"W: unexpected end of resource data\n");
			return 0;
		}
		if(tr_str_export(f, buf + pos, 1 + buf[pos]) != 0)
			return -1;
		pos += 1 + buf[pos];
	}

	return 0;
}

int tr_strhash_import(FILE *f, void **data, int *len)
{
	void *sd = NULL;
	int sl = 0, sn;
	
	*len = sizeof(unsigned short);
	*data = realloc(*data, *len);

	for(sn = 0; !tr_str_import(f, &sd,&sl) && (!feof(f) || sl > 1); sn++) {
		*data = realloc(*data, *len + sl);
		memcpy(*data + *len, sd, sl);
		*len += sl;
	}
	* (unsigned short *) *data = htons(sn);

	return feof(f) ? 0 : -1;
}

int tr_cmdl_export(FILE *f, void *data, int len)
{
	const char empty[] = "";

	if(!len) {
		data = (void *) empty;
		len  = sizeof(len);
	}
	if(!memchr(data, '\0', len)) {
		fprintf(stderr, "No terminating null byte.\n");
		return -1;
	}
	fprintf(f, "%s\n", (char*) data);

	return 0;
}

int tr_cmdl_import(FILE *f, void **data, int *len)
{
	char buf[512], *c;

	c = fgets(buf, sizeof(buf), f);
	if(!c) {
		perror(NULL);
		return -1;
	}
	c = strchr(buf, '\n');
	if(c) *(c++) = '\0';

	*len  = strlen(buf) + 1;
	*data = realloc(*data, *len);
	strcpy(*data, buf);
	return 0;
}

struct translator {
	restype_t type;
	const char *ext;
	int (*export)(FILE *f, void *data, int len);
	int (*import)(FILE *f, void **data, int *len);
} tr_table[] = {
	{"STR ", "txt", tr_str_export,		tr_str_import		},
	{"STR#", "txt", tr_strhash_export,	tr_strhash_import	},
	{"CMDL", "txt", tr_cmdl_export,		tr_cmdl_import		},
	{"",     "bin", tr_raw_export,		tr_raw_import		},
	{"",     NULL,  NULL,			NULL			}
};

struct translator *tr_lookup(struct resource *r, const char *ext)
{
	struct translator *tr;
	restype_t type;

	res_gettype(r, type);
	for(tr = tr_table ; tr->export ; tr++) {
		if(tr->type[0] && memcmp(tr->type, type, sizeof type))
			continue;
		if(ext && tr->ext && strcmp(tr->ext, ext))
			continue;

		return tr;
	}
	return NULL;
}

const char *tr_ext(struct translator *tr)
{
	return tr->ext;
}

int tr_export(struct translator *tr, struct resource *r, FILE *out)
{
	void *data;
	int len;

	res_getdata(r, &data, &len);
	return tr->export(out, data, len);
}

int tr_import(struct translator *tr, struct resource *r, FILE *in)
{
	void *data;
	int len, ret;

	data = NULL;
	len = 0;
	ret = tr->import(in, &data, &len);

	if(!ret)
		res_setdata(r, data, len);
	else
		fprintf(stderr, "Couldn't import resource data\n");
	if(data)
		free(data);

	return ret;
}

