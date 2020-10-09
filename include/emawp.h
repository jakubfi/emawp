//  Copyright (c) 2012-2015 Jakub Filipowicz <jakubf@gmail.com>
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

#ifndef AWP_H
#define AWP_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum awp_errors {
	AWP_OK,
	AWP_FP_UF,
	AWP_FP_OF,
	// NOTE: for results below, CPU registers stay unchanged
	AWP_DIV_OF,
	AWP_FP_ERR,
};

int awp_dword_add(uint16_t *r, uint16_t *n);
int awp_dword_sub(uint16_t *r, uint16_t *n);
int awp_dword_mul(uint16_t *r, int16_t n);
int awp_dword_div(uint16_t *r, int16_t n);

int awp_to_double(uint16_t *r, double *f);
int awp_from_double(uint16_t *r, double f);

int awp_float_norm(uint16_t *r);
int awp_float_add(uint16_t *r, uint16_t *n);
int awp_float_sub(uint16_t *r, uint16_t *n);
int awp_float_mul(uint16_t *r, uint16_t *n);
int awp_float_div(uint16_t *r, uint16_t *n);

#ifdef __cplusplus
}
#endif

#endif

// vim: tabstop=4 shiftwidth=4 autoindent
