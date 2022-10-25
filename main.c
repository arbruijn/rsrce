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
#include <ctype.h>
#include <unistd.h>

#include "config.h"
#include "command.h"


#define MAXCMDSZ 1023
#define MAXARGC 7


/* Command parser */

static char backslashed(char c)
{
	static const char bsc[] = "abfnrtv", bse[] = "\a\b\f\n\r\t\v";
	char *bsi;
	return (bsi = strchr(bsc, c)) ? bse[bsi - bsc] : c;
}

static int parse_command(FILE *f)
{
	char buf[MAXCMDSZ+1], *argv[MAXARGC+1];
	int ok = 1, pos = 0, argc = 0, c;

	enum {
		BOA, UNQUOTED, QUOTED, BACKSLASHED, COMMENT, EOC
	} state = BOA;

	while(state != EOC && (c = fgetc(f)) > 0) {
		if(pos >= MAXCMDSZ || argc >= MAXARGC)
			ok = 0, pos = 0, argc = 0;

		switch(state) {
		    case BOA:
			if(c == '#')	{ state = COMMENT; break; }
			if(c == '\n')	{ state = EOC; break; }
			if(isspace(c))	{ break; }

			argv[argc++] = buf + pos;
			state = UNQUOTED;

		    case UNQUOTED:
			if(c == '\n')	{ state = EOC; break; }
			if(isspace(c))	{ buf[pos++]='\0'; state=BOA; break; }

			if(c == '\\')	{ state = BACKSLASHED; break; }
			if(c == '\'')	{ state = QUOTED; break; }
		    case QUOTED:
			if(c == '\'')	{ state = UNQUOTED; break; }

			buf[pos++] = c;
			break;

		    case BACKSLASHED:
			buf[pos++] = backslashed(c);
			state = UNQUOTED; break;

		    case COMMENT:
			if(c == '\n')	{ state = EOC; break; }
			break;
		}
	}
	buf[pos] = '\0';
	argv[argc] = NULL;

	if(!ok) {
		fprintf(stderr, "Max lenght of commands: %d!\n", MAXCMDSZ);
		return -1;
	}

	return cmd_exec(argv);
}


/* Command line options */

static struct {
	FILE *cmdin;
	int e;
	const char *ifname, *ofname;
} opts;

static void usage(const char *myname)
{
	fprintf(stderr, "Usage: %s [-e] [-f <script>] "
			"[-o <output-file>] [<input-file>]\n", myname);
}

static void do_cmdline(int argc, char **argv)
{
	int opt;

	opts.cmdin = stdin;
	opts.ifname = opts.ofname = NULL;
	opts.e = 0;

	opterr = 1;
	while(opt = getopt(argc, argv, "ef:o:"), opt != EOF)
		switch(opt) {
			case 'e':
				opts.e = 1;
				break;
			case 'f':
				opts.cmdin = fopen(optarg, "r");
				if(!opts.cmdin) {
					perror(optarg);
					exit(1);
				}
				break;
			case 'o':
				opts.ofname = optarg;
				break;
			default:
				usage(argv[0]);
				exit(2);
		}

	opts.ifname = argv[optind];
}

int main(int argc, char **argv)
{
	int tty;
	int r;

	do_cmdline(argc, argv);

	cmd_init(opts.ifname, opts.ofname);
	tty = isatty(fileno(opts.cmdin));

	while(!feof(opts.cmdin)) {
		if(tty) fputs(CFG_PROMPT, stdout);
		r = parse_command(opts.cmdin);
		if(opts.e && r) exit(1);
	}
	if(tty) printf("\n");

	return 0;
}


