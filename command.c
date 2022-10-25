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
#include <unistd.h>
#include <ctype.h>

#include "config.h"
#include "resource.h"
#include "translate.h"
#include "command.h"

static struct res_fork *cmd_rf;
static restype_t cmd_res_type;
static int cmd_res_id;
static struct resource *cmd_res;

int cmd_parseresource(const char *spec)
{
	char *colon;

	if(!spec) {
		fprintf(stderr, "Please specify a resource to act on.\n");
		return 1;
	}

	colon = strrchr(spec, ':');
	if(!colon || colon > spec+4) {
		fprintf(stderr, "Incorrect resource specification\n");
		return 1;
	}
	memset(cmd_res_type, ' ', sizeof(cmd_res_type));
	memcpy(cmd_res_type, spec, colon - spec);
	cmd_res_id = atoi(colon + 1);

	return 0;
}

int cmd_selectresource(const char *spec)
{
	if(cmd_parseresource(spec))
		return 1;

	cmd_res = res_lookup(cmd_rf, cmd_res_type, cmd_res_id);
	if(!cmd_res) {
		fprintf(stderr, "No such resource.\n");
		return 1;
	}

	return 0;
}


static char *cmd_ifname, *cmd_ofname;

int cmd_read(char **argv)
{
	struct res_fork *rf;

	if(argv[1]) {
		cmd_ifname = realloc(cmd_ifname, strlen(argv[1]) + 1);
		strcpy(cmd_ifname, argv[1]);
	}
	if(!cmd_ifname) {
		fprintf(stderr, "Read which file ?\n");
		return 1;
	}

	rf = res_read(cmd_ifname);
	if(!rf) {
		fprintf(stderr, "Error while reading %s\n", argv[1]);
		return 1;
	}

	res_delfork(cmd_rf);
	cmd_rf = rf;
	return 0;
}

int cmd_write(char **argv)
{
	const char *fname;
	int ret;

	if(argv[1]) {
		cmd_ofname = realloc(cmd_ofname, strlen(argv[1]) + 1);
		strcpy(cmd_ofname, argv[1]);
	}
	fname = cmd_ofname ? cmd_ofname : cmd_ifname;
	if(!fname) {
		fprintf(stderr, "Write to which file ?\n");
		return 1;
	}

	ret = res_write(cmd_rf, fname);
	if(ret < 0) {
		fprintf(stderr, "Write error\n");
		return 1;
	}

	return 0;
}

int cmd_create(char **argv)
{
	if(!argv[1]) {
		fprintf(stderr, "%s <resource>\n", argv[0]);
		return 1;
	}

	if(cmd_parseresource(argv[1]))
		return 1;

	cmd_res = res_lookup(cmd_rf, cmd_res_type, cmd_res_id);
	if(cmd_res) {
		fprintf(stderr, "Resource already exists.\n");
		return 1;
	}

	cmd_res = res_new(cmd_rf, cmd_res_type, cmd_res_id);
	return 0;
}

int cmd_delete(char **argv)
{
	if(!argv[1]) {
		fprintf(stderr, "%s <resource>\n", argv[0]);
		return 1;
	}

	if(cmd_selectresource(argv[1]))
		return 1;

	res_delete(cmd_res);
	return 0;
}

int cmd_rename(char **argv)
{
	if(!argv[2]) {
		fprintf(stderr, "%s <resource> <name>\n", argv[0]);
		return 1;
	}

	if(cmd_selectresource(argv[1]))
		return 1;

	res_rename(cmd_res, argv[2], strlen(argv[2]));
	return 0;
}

int cmd_chattr(char **argv)
{
	if(!argv[2]) {
		fprintf(stderr, "%s <resource> <attributes>\n", argv[0]);
		return 1;
	}

	if(cmd_selectresource(argv[1]))
		return 1;

	res_chattr(cmd_res, argv[2]);
	return 0;
}

int cmd_ls(char **argv)
{
	res_ls(stdout, cmd_rf);
	return 0;
}

int cmd_hexdump(char **argv)
{
	int i, j;
	unsigned char *data;
	int len;

	if(cmd_selectresource(argv[1]))
		return 1;

	res_getdata(cmd_res, (void **) &data, &len);
	
	for(i=0 ; i < len ; i += 16) {
		printf("%8x ", i);
		for(j=0 ; j < 16 ; j++) {
			if(i+j < len)
				printf(" %02x", data[i+j]);
			else
				printf("   ");
		}
		printf("  |");
		for(j=0 ; j < 16 && i+j < len ; j++)
			printf("%c", isprint(data[i+j]) ? data[i+j] : '.');
		printf("|\n");
	}

	return 0;
}


int cmd_export(char **argv)
{
	struct translator *tr;
	FILE *f;
	char *c;
	int what, r;
	
	what = (strcmp(argv[0], "export") == 0);
	
	if(!argv[2]) {
		fprintf(stderr, "%s <resource> <file> [<type>]\n", argv[0]);
		return 1;
	}
	if(!argv[3]) {
		c = strrchr(argv[2], '.');
		argv[3] = c ? c+1 : NULL;
	}

	if(cmd_selectresource(argv[1]))
		return 1;
	
	tr = tr_lookup(cmd_res, argv[3]);
	if(!tr) {
		fprintf(stderr, "I don't know how to translate this.\n");
		return 1;
	}

	f = (strcmp(argv[2], "-") == 0) ? (what ? stdout : stdin)
					: fopen(argv[2], what ? "w" : "r");
	if(!f) {
		perror(argv[2]);
		return 1;
	}

	r = (what ? tr_export : tr_import)(tr, cmd_res, f);
	if(strcmp(argv[2], "-") != 0)
		fclose(f);

	return r;
}

/* Export the resource data to a temporary file and return its name */
static char *cmd_edit_export(struct translator *tr)
{
	char filename[] = "/tmp/rsrce.XXXXXX", *nfname, *ret = NULL;
	const char *ext;
	int fd, r;
	FILE *f = NULL;
	
	fd  = mkstemp(filename);
	if(fd < 0) {
		perror(filename);
		return NULL;
	}

	ext = tr_ext(tr);
	nfname = malloc(strlen(filename) + strlen(ext) + 2);
	sprintf(nfname, "%s.%s", filename, ext);

	if((r = rename(filename, nfname)) < 0) 
		perror(nfname);
	else if((f = fdopen(fd, "w")) == NULL)
		perror("fdopen");
	else if(tr_export(tr, cmd_res, f) < 0)
		fprintf(stderr,"Couldn't translate resource data\n");
	else
		ret = nfname;

	if(!ret) {
		unlink(r < 0 ? filename : nfname);
		free(nfname);
	}
	if(f) fclose(f); else close(fd);

	return ret;
}

/* Invoke the external editor */
static int cmd_edit_edit(const char *fname)
{
	char *cmd;
	int ret;

	/* FIXME: We should fork and exec rather than system in order to avoid
	 * shell interference with funny filenames. */
	cmd = malloc(strlen(CFG_EDITOR) + strlen(fname) + 2);
	sprintf(cmd, "%s %s", CFG_EDITOR, fname);
	ret = system(cmd);
	free(cmd);

	return ret;
}

/* Re-import the resource data */
static int cmd_edit_import(struct translator *tr, const char *filename)
{
	FILE *f;
	int ret;

	f = fopen(filename, "r");
	if(!f) {
		perror(filename);
		return -1;
	}

	ret = tr_import(tr, cmd_res, f);
	fclose(f);

	return ret;
}

int cmd_edit(char **argv)
{
	struct translator *tr;
	int ret;
	char *fname;
	
	if(cmd_selectresource(argv[1]) != 0)
		return 1;

	tr = tr_lookup(cmd_res, argv[2]);
	if(!tr) {
		fprintf(stderr, "I don't know how to translate this.\n");
		return 1;
	}

	fname = cmd_edit_export(tr);
	if(!fname)
		return 1;
	ret = (cmd_edit_edit(fname) == 0 && cmd_edit_import(tr, fname) == 0);
	unlink(fname);
	free(fname);

	return ret ? 0 : 1;
}

int cmd_help(char **argv);

int cmd_exit(char **argv)
{
	exit(0);
}

struct command {
	const char *cmd;
	int (*f)(char **argv);
	const char *help;
} cmd_table[] = {
	{"read", 	cmd_read,	"Read the resource fork from a file"},
	{"write",	cmd_write,	"Write the resource fork to a file"},
	{"create",	cmd_create,	"Create resource"},
	{"delete",	cmd_delete,	"Delete resource"},
	{"rename",	cmd_rename,	"Change the resource name"},
	{"chattr",	cmd_chattr,	"Change the resource attributes"},
	{"ls",		cmd_ls,		"List resources"},
	{"hexdump",	cmd_hexdump,	"Show an hexdump of the resource data"},
	{"import",	cmd_export,	"Import resource data from a file"},
	{"export",	cmd_export,	"Export resource data to a file"},
	{"edit",	cmd_edit,	"Edit resource data"},
	{"help",	cmd_help,	"This short help listing"},
	{"exit",	cmd_exit,	"Exit (don't forget to write first!)"},
	{NULL,		NULL,		NULL}
};

int cmd_help(char **argv)
{
	int i;

	for(i=0 ; cmd_table[i].cmd ; i++) {
		printf("%10s -- %s\n", cmd_table[i].cmd, cmd_table[i].help);
	}

	return 0;
}


int cmd_exec(char **argv)
{
	int i;

	if(!argv[0])
		return 0;

	for(i=0 ; cmd_table[i].cmd ; i++) {
		if(strcmp(cmd_table[i].cmd, argv[0]) == 0)
			return cmd_table[i].f(argv);
	}

	fprintf(stderr, "%s: no such command\n", argv[0]);
	return 1;
}

void cmd_init(const char *ifname, const char *ofname)
{
	cmd_ifname = ifname ? strdup(ifname) : NULL;
	cmd_ofname = ofname ? strdup(ofname) : NULL;
	cmd_rf = cmd_ifname ? res_read(cmd_ifname) : res_newfork();
	if(!cmd_rf) {
		exit(1);
	}
}
