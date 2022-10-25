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
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <netinet/in.h>

#include "resource.h"
#include "rsrc-fmt.h"


struct resource {
	struct res_fork *f;
	int ti;
	struct resource *next;

	int16_t id;
	uint8_t attr;
	char *name, *data;
	int namelen, datalen;
};

struct res_fork {
	uint16_t attr;
	int rlen;			/* Reserved space at beginning */
	int tn;
	struct {
		restype_t type;
		struct resource *rlist;
	} *tt;
};


static int res_lookup_type(struct res_fork *f, restype_t type)
{
	int i;
	for(i=0; (i < f->tn) && memcmp(f->tt[i].type, type, sizeof(restype_t)); i++);
	return (i < f->tn) ? i : -1;
}

static struct resource **res_lookup_rp(struct res_fork *f, int ti, int id)
{
	struct resource **rp;
	for(rp = &f->tt[ti].rlist; *rp && (*rp)->id != id; rp = &(*rp)->next);
	return rp;
}

struct resource *res_lookup(struct res_fork *f, restype_t type, int16_t id)
{
	int ti = res_lookup_type(f, type);
	return ti >= 0 ? *res_lookup_rp(f, ti, id) : NULL;
}


static int res_getti(struct res_fork *f, restype_t type)
{
	int ti;
	
	ti = res_lookup_type(f, type);
	if(ti < 0) {
		ti = f->tn++;
		f->tt = realloc(f->tt, f->tn * sizeof *f->tt);
		memcpy(f->tt[ti].type, type, sizeof(restype_t));
		f->tt[ti].rlist = NULL;
	}

	return ti;
}

struct resource *res_new(struct res_fork *f, restype_t type, int16_t id)
{
	struct resource *r, **rp;
	int ti = res_getti(f, type);

	/* append to the type's resource list's tail */
	rp = res_lookup_rp(f, ti, -1);
	if(*rp) {
		fprintf(stderr, "W: reusing duplicate resource %.4s:%d!\n",
				type, id);
		return *rp;
	}

	r = malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));
	r->f = f;
	r->ti = ti;
	r->id = id;

	*rp = r;
	return r;
}

void res_getdata(struct resource *r, void **dp, int *len)
{
	*dp  = r->data;
	*len = r->datalen;
}

void res_setdata(struct resource *r, void *p, int len)
{
	if(r->data) free(r->data);

	r->datalen = len;
	if(len) {
		r->data = malloc(len);
		memcpy(r->data, p, len);
	} else
		r->data = NULL;
}

void res_gettype(struct resource *r, restype_t type)
{
	memcpy(type, r->f->tt[r->ti].type, sizeof(restype_t));
}

void res_rename(struct resource *r, char *name, int nlen)
{
	if(r->name) free(r->name);

	r->namelen = (nlen >= 0) ? nlen : strlen(name);
	r->name = malloc(r->namelen + 1);
	memcpy(r->name, name, r->namelen);
	r->name[r->namelen] = '\0';
}

const char * const res_attrch = "7spLPlw0";
void res_chattr(struct resource *r, const char *spec)
{
	int v, op, mask;
	char *pos;

	v = 0;
	op = 0xff;
	while(*spec) {
		pos = strchr(res_attrch, *spec);
		if(pos) {
			mask = 0x80 >> (pos - res_attrch);
			v &= ~mask;
			v |= op & mask;
			r->attr = v;
		} else if(*spec == '+') {
			v = r->attr;
			op = 0xff;
		} else if(*spec == '-') {
			v = r->attr;
			op = 0;
		} else {
			r->attr = v;
			fprintf(stderr, "Unknown attribute '%c'\n", *spec);
		}
		spec++;
	}
}

static void res_deltype(struct res_fork *f, int ti)
{
	struct resource *r;

	memmove(f->tt + ti, f->tt + ti + 1, (--f->tn - ti) * sizeof *f->tt);
	f->tt = realloc(f->tt, f->tn * sizeof *f->tt);

	while(ti < f->tn) {
		for(r = f->tt[ti].rlist; r; r = r->next)
			r->ti = ti;
		ti++;
	}
}

void res_delete(struct resource *r)
{
	struct resource **rp;

	rp = res_lookup_rp(r->f, r->ti, r->id);
	assert(*rp);
	*rp = r->next;

	if(!r->f->tt[r->ti].rlist)
		res_deltype(r->f, r->ti);

	free(r);
}

static void res_printinfo(struct resource *r, FILE *s)
{	
	int i;

	fprintf(s, "%.4s %11d ", r->f->tt[r->ti].type, r->id);
	for(i=0 ; i<8 ; i++)
		fprintf(s, "%c", ((r->attr << i) & 0x80) ? res_attrch[i] : '-');
	fprintf(s, " %10d", r->datalen);
	if(r->name)
		fprintf(s, " %s", r->name);
	fprintf(s, "\n");
}

void res_ls(FILE *s, struct res_fork *f)
{
	struct resource *r;
	int ti;

	for(ti = 0; ti < f->tn; ti++)
		for(r = f->tt[ti].rlist ; r ; r = r->next)
			res_printinfo(r, s);
}


struct res_fork *res_newfork()
{
	struct res_fork *f;

	f = malloc(sizeof(*f));
	memset(f, 0, sizeof *f);
	f->rlen = sizeof(struct reshdr);

	return f;
}

void res_delfork(struct res_fork *f)
{
	while(f->tn) res_delete(f->tt[0].rlist);
	free(f);
}


/*** Reading and writing ***/


struct res_parsecontext {
	struct res_fork *f;
	char *buf, *dbase, *tbase, *nbase;
	int len;
};

#define CHECKPTR(p, m) \
if((char *)(p) < c->buf || (char *)(p) + sizeof(*(p)) > c->buf + c->len) { \
	fprintf(stderr, "Wild " m " offset!\n"); \
	return -1; \
}

#define BUFSZ 4096
#define MAXSZ (100*1024*1024)

static int res_readbuf(struct res_parsecontext *c, FILE *stream)
{
	int n;

	do {
		c->buf = realloc(c->buf, c->len + BUFSZ);
		n = fread(c->buf + c->len, 1, BUFSZ, stream);
		c->len += n;
	} while(n == BUFSZ && c->len < MAXSZ);

	if(c->len >= MAXSZ) {
		fprintf(stderr, "This resource fork is rather huge !\n");
		return -1;
	} else if(!feof(stream)) {
		perror(NULL);
		return -1;
	}

	return 0;
}

static int res_parse_reflist(struct res_parsecontext *c, struct restype *t)
{
	struct resource *r;
	struct resref *ref;
	struct resname *name;
	struct resdata *data;
	int i;

	ref = (struct resref *) (c->tbase + ntohs(t->refofs));
	for(i=0 ; i < ntohs(t->rnum) + 1 ; i++) {
		CHECKPTR(ref+i, "reference");
		r = res_new(c->f, t->type, ntohs(ref[i].id));
		r->attr = ref[i].attr;
		
		if(ntohs(ref[i].nameofs) != 0xffff) {
			name = (struct resname *)
				(c->nbase + ntohs(ref[i].nameofs));
			CHECKPTR(name, "name");
			CHECKPTR(name->name + name->len - 1, "name");
			res_rename(r, name->name, name->len);
		}

		data = (struct resdata *)(c->dbase + ntoh3(ref[i].dataofs));
		CHECKPTR(data, "data");
		CHECKPTR(data->data + ntohl(data->len) - 1, "data");
		res_setdata(r, data->data, ntohl(data->len));
	}

	return 0;
}

static int res_parse_typelist(struct res_parsecontext *c)
{
	uint16_t *tnp = (uint16_t *) c->tbase, tn = ntohs(*tnp) + 1;
	struct restype *l = (struct restype *) (tnp + 1);
	int i;

	for(i=0 ; i < tn ; i++) {
		if(res_parse_reflist(c, l + i) < 0)
			return -1;
	}

	return 0;
}

static int res_parsebuf(struct res_parsecontext *c)
{
	struct reshdr *rh;
	struct resmaphdr *mh;

	rh = (struct reshdr *) c->buf;
	mh = (struct resmaphdr *) (c->buf + ntohl(rh->mofs));
	CHECKPTR(mh, "resource map");
	c->f->rlen = ntohl(rh->dofs) < ntohl(rh->mofs)
		? ntohl(rh->dofs) : ntohl(rh->mofs);

	c->dbase = c->buf + ntohl(rh->dofs);
	c->tbase = (char *) mh + ntohs(mh->tlistofs);
	c->nbase = (char *) mh + ntohs(mh->nlistofs);
	return res_parse_typelist(c);
}

struct res_fork *res_read(const char *fname)
{
	FILE *stream;
	struct res_parsecontext c;
	int ok;

	stream = fopen(fname, "r");
	if(!stream) {
		perror(fname);
		return NULL;
	}

	memset(&c, 0, sizeof c);
	c.f = res_newfork();
	ok = (res_readbuf(&c, stream) >= 0) && (res_parsebuf(&c) >= 0);
	free(c.buf);
	fclose(stream);

	if(!ok) {
		res_delfork(c.f);
		return NULL;
	}

	return c.f;
}


static int res_write_data(struct resource *r, FILE *stream)
{
	uint32_t len = htonl(r->datalen);

	if(fwrite(&len, sizeof len, 1, stream) != 1)
		return -1;
	if(fwrite(r->data, 1, r->datalen, stream) != r->datalen)
		return -1;

	return 0;
}

/* TODO: split this into intelligible parts */
/* FIXME: stream is not closed when an error occurs */
int res_write(struct res_fork *f, const char *fname)
{
	int mlen, dlen, nlen, rlen;
	int rnum, dofs, nofs, rofs;

	struct reshdr rh;
	struct resmaphdr mh;
	struct restype type;
	struct resref ref;

	FILE *stream;
	struct resource *r;
	int ti, i;

	stream = fopen(fname, "w");
	if(!stream) {
		perror(fname);
		return -1;
	}

	/* compute the lenght of parts */
	dlen = 0;
	mlen = sizeof(struct resmaphdr);
	rlen = 0;
	nlen = 0;
	for(ti = 0; ti < f->tn; ti++) {
		mlen += sizeof(struct restype);
		for(r = f->tt[ti].rlist ; r ; r = r->next) {
			dlen += sizeof(struct resdata) + r->datalen;
			rlen += sizeof(struct resref);
			if(r->name)
				nlen += sizeof(struct resname) + r->namelen;
		}
	}

	/* write the resource fork header */
	rh.dofs = htonl(f->rlen);
	rh.mofs = htonl(f->rlen + dlen);
	rh.dlen = htonl(dlen);
	rh.mlen = htonl(mlen + rlen + nlen);
	if(fwrite(&rh, sizeof rh, 1, stream) != 1)
		return -1;

	/* pad the reserved zone with zeros */
	for(i = sizeof rh; i < f->rlen; i++)
		if(fputc(0, stream) == EOF)
			return -1;

	/* write the resource data */
	for(ti = 0; ti < f->tn; ti++)
		for(r = f->tt[ti].rlist; r; r = r->next)
			if(res_write_data(r, stream))
				return -1;

	/* write the resource map header */
	memset(&mh.reserved, 0, sizeof mh.reserved);
	mh.attr = htons(f->attr);
	mh.tlistofs = htons((char *) &mh.tnum - (char *) &mh);
	mh.nlistofs = htons(mlen + rlen);
	mh.tnum = htons(f->tn - 1);
	if(fwrite(&mh, sizeof mh, 1, stream) != 1)
		return -1;

	/* write the type list */
	rofs = sizeof mh.tnum + f->tn * sizeof type;
	for(ti = 0; ti < f->tn; ti++) {
		/* count resources */
		for(r = f->tt[ti].rlist, rnum = 0; r; r = r->next, rnum++);

		memcpy(type.type, &f->tt[ti].type, sizeof(restype_t));
		type.rnum = htons(rnum - 1);
		type.refofs = htons(rofs);
		if(fwrite(&type, sizeof type, 1, stream) != 1)
			return -1;

		rofs += rnum * sizeof ref;
	}

	/* write the reference lists */
	nofs = dofs = 0;
	for(ti = 0; ti < f->tn; ti++)
		for(r = f->tt[ti].rlist; r; r = r->next) {
			ref.id = htons(r->id);
			ref.nameofs = htons(r->name ? nofs : -1);
			ref.dataofs = hton3(dofs);
			ref.attr = r->attr;
			ref.reserved = htonl(0);
			if(fwrite(&ref, sizeof ref, 1, stream) != 1)
				return -1;

			nofs += r->name ? sizeof (struct resname) + r->namelen : 0;
			dofs += sizeof (struct resdata) + r->datalen;
		}

	/* write the name list */
	for(ti = 0; ti < f->tn; ti++)
		for(r = f->tt[ti].rlist; r; r = r->next) {
			if(!r->name) continue;

			if(fputc(r->namelen, stream) != r->namelen)
				return -1;
			if(fwrite(r->name, 1, r->namelen, stream) != r->namelen)
				return -1;
		}

	fclose(stream);
	return 0;
}

