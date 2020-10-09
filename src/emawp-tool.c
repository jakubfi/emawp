//  Copyright (c) 2015 Jakub Filipowicz <jakubf@gmail.com>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include "emawp.h"

#define FL_Z 0b1000000000000000
#define FL_M 0b0100000000000000
#define FL_V 0b0010000000000000
#define FL_C 0b0001000000000000

char *retnames[] = {
	"OK",
	"UDFLOW",
	"OVFLOW",
	"DIV_OF",
	"FP_ERR",
};

enum types { FLOAT, HEX, BOTH };
enum operations { NONE, NORM, ADD, SUB, MUL, DIV };
char *opnames[] = { "none", "norm", "add", "sub", "mul", "div" };
int verbose = 0;

struct num {
	uint16_t r[4];
	double f;
	int type;
	int res;
} n[2];

// -----------------------------------------------------------------------
void errexit(char *s, ...)
{
	va_list ap;
	va_start(ap, s);
	printf("ERROR: ");
	vprintf(s, ap);
	printf("\n");
	va_end(ap);
	exit(1);
}

// -----------------------------------------------------------------------
void usage()
{
	printf(
		"Usage:\n\n"
		"   emawp [-v] <arg>\n"
		"   emawp [-v] -n <arg>\n"
		"   emawp [-v] -a|-s|-m|-d <arg> <arg>\n"
		"   emawp -h\n\n"
		"If no operation is specified, argument is converted and printed.\n"
		"Other operations are:\n\n"
		"   -n : normalize\n"
		"   -a : add\n"
		"   -s : subtract\n"
		"   -m : multiply\n"
		"   -d : divide\n"
		"   -h : print help\n"
		"\n"
		"Arguments can either be a hex triplet representing floating point number in\n"
		"MERA-400 internal format (eg. 0x4000 0x0000 0x0002), or plain floating point nummer.\n"
		"Argument types can also be mixed. \"-v\" switch makes computations verbose.\n\n"
	);
}

// -----------------------------------------------------------------------
void print_num(struct num *n, char *name)
{
	if (n->type == HEX) {
		awp_to_double(n->r, &n->f);
	} else if (n->type == FLOAT) {
		n->res = awp_from_double(n->r, n->f);
	}

	int width = (int) log10(n->f);
	if (width < 0) width = 0;

	printf("%4s:  %6s  %s%s%s%s  0x%04x 0x%04x 0x%04x  %s  %.*f\n",
		name,
		retnames[n->res],
		n->r[0] & FL_Z ? "Z" : "-",
		n->r[0] & FL_M ? "M" : "-",
		n->r[0] & FL_C ? "C" : "-",
		n->r[0] & FL_V ? "V" : "-",
		n->r[1],
		n->r[2],
		n->r[3],
		n->type == HEX ? "->" : "<-",
		42 - width, n->f
	);

	if (verbose) {
		signed char exp = n->r[3] & 0x00ff;
		int64_t m;
		m  = (int64_t) n->r[1] << 48;
		m |= (int64_t) n->r[2] << 32;
		m |= (int64_t) (n->r[3] & 0xff00) << 16;
		double m_f = ldexp(m, -63);
		printf("                                             = %.*f * 2^%i\n", 42, m_f, exp);
	}

	n->type = HEX;
}

// -----------------------------------------------------------------------
int main(int argc, char **argv)
{
	int option;
	int args_req = 2;
	int operation = NONE;

	while ((option = getopt(argc, argv,"nhasmdv")) != -1) {
		if ((operation != NONE) && (option != 'h') && (option != 'v')){
			errexit("Only one operation can be specified");
		}

		switch (option) {
			case 'h':
				usage();
				exit(0);
			case 'n':
				operation = NORM;
				args_req = 1;
				break;
			case 'a':
				operation = ADD;
				break;
			case 's':
				operation = SUB;
				break;
			case 'm':
				operation = MUL;
				break;
			case 'd':
				operation = DIV;
				break;
			case 'v':
				verbose = 1;
				break;
			default:
				exit(1);
		}
	}

	int npos = 0;
	int pos_args = argc-optind;

	if (pos_args == args_req) { // float input
		while (args_req > 0) {
			n[npos].type = FLOAT;
			n[npos].f = atof(argv[optind]);
			optind++;
			npos++;
			args_req--;
		}
	} else if (pos_args == 3*args_req) { // word input
		while (args_req > 0) {
			n[npos].type = HEX;
			for (int i=1 ; i<=3 ; i++) {
				if (optind >= argc) {
					errexit("Not enough positional arguments for a triplet");
				}
				if (!strncmp(argv[optind], "0x", 2)) {
					n[npos].r[i] = strtol(argv[optind], NULL, 16);
				} else if (!strncmp(argv[optind], "0b", 2)) {
					n[npos].r[i] = strtol(argv[optind]+2, NULL, 2);
				} else {
					n[npos].r[i] = strtol(argv[optind], NULL, 10);
				}
				optind++;
			}
			npos++;
			args_req--;
		}
	} else {
		errexit("Wrong number of positional arguments for operation");
	}

	print_num(n+0, "in1");
	if (npos == 2) {
		print_num(n+1, "in2");
	}

	switch (operation) {
		case NORM:
			awp_float_norm(n[0].r);
			break;
		case ADD:
			n[0].res = awp_float_add(n[0].r, n[1].r+1);
			break;
		case SUB:
			n[0].res = awp_float_sub(n[0].r, n[1].r+1);
			break;
		case MUL:
			n[0].res = awp_float_mul(n[0].r, n[1].r+1);
			break;
		case DIV:
			n[0].res = awp_float_div(n[0].r, n[1].r+1);
			break;
		case NONE:
		default:
			break;
	}

	if (operation != NONE) {
		print_num(n+0, opnames[operation]);
	}

	return 0;
}

// vim: tabstop=4 shiftwidth=4 autoindent
