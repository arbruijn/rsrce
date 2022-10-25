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
#ifndef __RESOURCE_H__
#define __RESOURCE_H__

#include <stdio.h>
#include <stdint.h>

typedef char restype_t[4];
struct res_fork;
struct resource;

struct resource *res_new(struct res_fork *f, restype_t type, int16_t id);
void res_getdata(struct resource *r, void **dp, int *len);
void res_setdata(struct resource *r, void *p, int len);
void res_gettype(struct resource *r, restype_t type);
void res_rename(struct resource *r, char *name, int nlen);
void res_chattr(struct resource *r, const char *spec);
void res_delete(struct resource *r);

struct res_fork *res_newfork();
struct res_fork *res_read(const char *fname);
int res_write(struct res_fork *f, const char *fname);
struct resource *res_lookup(struct res_fork *f, restype_t type, int16_t id);
void res_ls(FILE *s, struct res_fork *f);
void res_delfork(struct res_fork *f);

#endif

