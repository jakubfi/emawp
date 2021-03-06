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

#include <stdlib.h>
#include <inttypes.h>
#include <math.h>
#include <fenv.h>
#include <limits.h>

#include "emawp.h"

#define FL_Z 0b1000000000000000
#define FL_M 0b0100000000000000
#define FL_V 0b0010000000000000
#define FL_C 0b0001000000000000

#define FL_SET(flags, flag) (flags) |= (flag)
#define FL_CLR(flags, flag) (flags) &= ~(flag)
#define FL_GET(flags, flag) ((flags & flag) ? 1 : 0)

#define DWORD(x, y) (uint32_t) ((x) << 16) | (uint16_t) (y)
#define DWORD_H(z)  (uint16_t) ((z) >> 16)
#define DWORD_L(z)  (uint16_t) (z)

// all constant names use MERA-400 0...n bit numbering

#define BIT_0		0x080000000L
#define BIT_MINUS1	0x100000000L
#define BITS_0_31	0x0ffffffffL

#define FP_BITS 64

#define FP_M_MASK   0b1111111111111111111111111111111111111111000000000000000000000000L
#define FP_M_MAX    0b0111111111111111111111111111111111111111000000000000000000000000L
#define FP_M_BIT_0  0b1000000000000000000000000000000000000000000000000000000000000000L
#define FP_M_BIT_1  0b0100000000000000000000000000000000000000000000000000000000000000L
#define FP_M_BIT_39 0b0000000000000000000000000000000000000001000000000000000000000000L
#define FP_M_BIT_40 0b0000000000000000000000000000000000000000100000000000000000000000L

enum awp_addsub_op {
	AWP_OP_SUB = -1,
	AWP_OP_ADD = 1,
};

// -----------------------------------------------------------------------
// ---- 32-bit -----------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
static void __awp_dword_set_Z(uint16_t *r0, uint64_t z)
{
	// set Z if:
	//  * all 32 bits of result are 0

	if ((z & BITS_0_31) == 0) {
		FL_SET(*r0, FL_Z);
	} else {
		FL_CLR(*r0, FL_Z);
	}
}

// -----------------------------------------------------------------------
static int __awp_dword_update_V(uint16_t *r0, uint64_t a, uint64_t b, uint64_t z)
{
	// update (not set!) V if:
	//  * both arguments were positive, and result is negative
	//  * OR both arguments were negative, and result is positive
	// store awp->v, M needs it

	if (
	   (  (a & BIT_0) &&  (b & BIT_0) && !(z & BIT_0) )
	|| ( !(a & BIT_0) && !(b & BIT_0) &&  (z & BIT_0) )
	) {
		FL_SET(*r0, FL_V);
		return 1;
	} else {
		return 0;
	}
}

// -----------------------------------------------------------------------
static void __awp_dword_set_C(uint16_t *r0, uint64_t z)
{
	// set C if bit on position -1 is set

	if ((z & BIT_MINUS1)) {
		FL_SET(*r0, FL_C);
	} else {
		FL_CLR(*r0, FL_C);
	}
}

// -----------------------------------------------------------------------
static void __awp_dword_set_M(uint16_t *r0, uint64_t z, int v)
{
	// note: internal V needs to be set before setting M
	// set M if:
	//  * minus and no overflow (just a plain negative number)
	//  * OR not minus and overflow (number looks non-negative, but there is overflow, which means a negative number overflown)

	if (
	   ( (z & BIT_0) && !v)
	|| (!(z & BIT_0) &&  v)
	) {
		FL_SET(*r0, FL_M);
	} else {
		FL_CLR(*r0, FL_M);
	}
}

// -----------------------------------------------------------------------
int awp_dword_add(uint16_t *r, uint16_t *n)
{
	uint32_t a = DWORD(r[1], r[2]);
	uint32_t b = DWORD(n[0], n[1]);
	uint64_t res = (uint64_t) a + b;

	int64_t sa = (int32_t) a;
	int64_t sb = (int32_t) b;

	r[1] = DWORD_H(res);
	r[2] = DWORD_L(res);

	// AD and SD set all flags

	int v = __awp_dword_update_V(r, a, b, res);
	__awp_dword_set_M(r, res, v);
	__awp_dword_set_C(r, res);
	if (sa + sb == 0) {
		FL_SET(*r, FL_Z);
	} else {
		FL_CLR(*r, FL_Z);
	}

	return AWP_OK;
}

// -----------------------------------------------------------------------
int awp_dword_sub(uint16_t *r, uint16_t *n)
{
	uint32_t a = DWORD(r[1], r[2]);
	uint32_t b = DWORD(n[0], n[1]);
	uint64_t res = (uint64_t) a + (uint64_t) -b;

	r[1] = DWORD_H(res);
	r[2] = DWORD_L(res);

	if ((res & BIT_MINUS1) || (b == 0)) {
		FL_SET(*r, FL_C);
	} else {
		FL_CLR(*r, FL_C);
	}

	int diff_bit_zero = ((a^b) & BIT_0) >> 31;
	if (FL_GET(*r, FL_C) == diff_bit_zero) {
		FL_SET(*r, FL_M);
	} else {
		FL_CLR(*r, FL_M);
	}

	if (((res & BIT_0) >> 31) ^ FL_GET(*r, FL_M)) {
		FL_SET(*r, FL_V);
	} else {
		// V is never cleared
	}

	__awp_dword_set_Z(r, res);

	return AWP_OK;
}

// -----------------------------------------------------------------------
int awp_dword_mul(uint16_t *r, int16_t n)
{
	int64_t res = (int16_t) r[2] * n;

	r[1] = DWORD_H(res);
	r[2] = DWORD_L(res);

	// MW does not touch C
	// MW may touch V, but doesn't (?)

	__awp_dword_set_M(r, res, 0);
	__awp_dword_set_Z(r, res);

	return AWP_OK;
}

// -----------------------------------------------------------------------
int awp_dword_div(uint16_t *r, int16_t n)
{
	// NOTE: in case of AWP_FP_ERR CPU registers stay unchanged
	if (n == 0) {
		return AWP_FP_ERR;
	}

	int32_t a = DWORD(r[1], r[2]);

	int64_t res = (int64_t) a / n;
	// dirty workaround for an edge case in the H/W
	if ((a == 2147483647) && (n == -32768)) {
		res = 1;
	}

	// NOTE: in case of AWP_DIV_OF CPU registers stay unchanged
	if ((res > SHRT_MAX) || (res < SHRT_MIN)) {
		return AWP_DIV_OF;
	}

	r[2] = res;
	r[1] = a % n;

	// NOTE: DW does not touch V nor C

	__awp_dword_set_M(r, res, 0);
	__awp_dword_set_Z(r, res);

	return AWP_OK;
}

// -----------------------------------------------------------------------
// ---- Floating Point ---------------------------------------------------
// -----------------------------------------------------------------------

// NOTE: normalized floating-point 0 has all 48 bits set to 0
// NOTE: no floating-point operation touches V
// NOTE: in case of denormalized input (except denormalized 0)
//       or division by 0 no CPU registers are updated
// NOTE: in case of underflow or overflow CPU registers are updated anyway

struct awpf {
	int64_t m;
	int e;
} awpf;

// -----------------------------------------------------------------------
int awp_load_float(struct awpf *af, uint16_t *data)
{
	af->m  = (int64_t) data[0] << 48;
	af->m |= (int64_t) data[1] << 32;
	af->m |= (int64_t) (data[2] & 0xff00) << 16;
	af->e = (int8_t) (data[2] & 0xff);

	if (af->m == 0) {
		af->e = 0;
		return 0;
	} else {
		if ((data[0] ^ (data[0]<<1)) & 0x8000) {
			return 0;
		} else {
			return 1;
		}
	}
}

// -----------------------------------------------------------------------
int awp_store_float(struct awpf *af, uint16_t *data, uint16_t *flags)
{
	if (flags) {
		// set Z and M
		if ((af->m & FP_M_MASK) == 0) {
			af->e = 0; // by definition
			FL_SET(*flags, FL_Z);
			FL_CLR(*flags, FL_M);
		} else if ((af->m & FP_M_BIT_0)) {
			FL_CLR(*flags, FL_Z);
			FL_SET(*flags, FL_M);
		} else {
			FL_CLR(*flags, FL_Z);
			FL_CLR(*flags, FL_M);
		}
	}

	data[0] = af->m >> 48;
	data[1] = af->m >> 32;
	data[2] = ((af->m >> 16) & 0xff00) | (af->e & 0xff);

	// check for overflow/underflow
	// TODO: store and UF/OF check order
	if (af->e > 127) {
		return AWP_FP_OF;
	} else if (af->e < -128) {
		return AWP_FP_UF;
	} else {
		return AWP_OK;
	}
}

// -----------------------------------------------------------------------
void awp_denorm(struct awpf *af, int shift)
{
	af->m >>= shift;
	af->e += shift;
}

// -----------------------------------------------------------------------
void awp_norm(struct awpf *af)
{
	while ((af->m != 0) && !((af->m ^ (af->m << 1)) & FP_M_BIT_0)) {
		af->m <<= 1;
		af->e -= 1;
	}
}

// -----------------------------------------------------------------------
int awp_to_double(uint16_t *r, double *fp)
{
	struct awpf af;

	if (awp_load_float(&af, r+1)) {
		return AWP_FP_ERR;
	}

	// convert mantissa to floating point: m_f = m * 2^-(FP_BITS-1)
	double m_f = ldexp(af.m, -(FP_BITS-1));

	// make final fp number: f = m_f * 2^exp
	*fp = ldexp(m_f, af.e);

	return AWP_OK;
}

// -----------------------------------------------------------------------
int awp_from_double(uint16_t *r, double f)
{
	struct awpf af;

	// get m, exp
	double m = frexp(f, &af.e);
	// scale m to 64-bit
	af.m = ldexp(m, FP_BITS-1);

	awp_norm(&af);

	return awp_store_float(&af, r+1, r);
}

// -----------------------------------------------------------------------
int awp_float_norm(uint16_t *r)
{
	struct awpf f;

	awp_load_float(&f, r+1);
	awp_norm(&f);
	// NOTE: normalization always clears C
	FL_CLR(r[0], FL_C);

	return awp_store_float(&f, r+1, r);
}

// -----------------------------------------------------------------------
int awp_float_addsub(uint16_t *r, uint16_t *n, int op)
{
	struct awpf af1, af2;

	if (awp_load_float(&af1, r+1) || awp_load_float(&af2, n)) {
		return AWP_FP_ERR;
	}

	// denormalize the smaller one to match exponents
	int ediff = af1.e - af2.e;
	if (ediff < 0) {
		if (ediff <= -40) {
			af1.m = 0;
			af1.e = af2.e;
		} else {
			awp_denorm(&af1, -ediff);
		}
	} else if (ediff > 0) {
		if (ediff >= 40) {
			af2.m = 0;
			af2.e = af1.e;
		} else {
			awp_denorm(&af2, ediff);
		}
	}

	// denormalize in case operation would overflow
	// (hardware has register and ALU positions -1 for that)
	// arithmetic is done in 64 bits, so there's plenty of space
	awp_denorm(&af1, 1);
	awp_denorm(&af2, 1);

	// add (or subtract)
	af1.m = af1.m + op * af2.m;
	awp_norm(&af1);

	// rounding up if bit 40 is set
	if (af1.m & FP_M_BIT_40) {
		awp_denorm(&af1, 1);
		af1.m += FP_M_BIT_40;
		awp_norm(&af1);
	}

	// NOTE: AF/SF never set V

	// set C to M[-1]
	if ((af1.m & FP_M_BIT_40)) FL_SET(r[0], FL_C);
	else FL_CLR(r[0], FL_C);

	return awp_store_float(&af1, r+1, r);
}

// -----------------------------------------------------------------------
int awp_float_add(uint16_t *r, uint16_t *n)
{
	return awp_float_addsub(r, n, AWP_OP_ADD);
}

// -----------------------------------------------------------------------
int awp_float_sub(uint16_t *r, uint16_t *n)
{
	return awp_float_addsub(r, n, AWP_OP_SUB);
}

// -----------------------------------------------------------------------
int awp_float_mul(uint16_t *r, uint16_t *n)
{
	struct awpf af1, af2;

	if (awp_load_float(&af1, r+1) || awp_load_float(&af2, n)) {
		return AWP_FP_ERR;
	}

	// remove signs, store the final sign
	awp_denorm(&af1, 1);
	awp_denorm(&af2, 1);
	int sign = 0;
	if (af1.m < 0) {
		sign++;
		af1.m = -af1.m;
	}
	if (af2.m < 0) {
		sign++;
		af2.m = -af2.m;
	}

	// multiply
	int64_t m = 0;
	af1.e += af2.e;
	af2.m >>= 23;
	// multiply one additional bit because of the initial denormalization
	for (int i=0 ; i<41 ; i++) {
		m >>= 1;
		if (af2.m & 1) m += af1.m;
		af2.m >>= 1;
	}
	if (sign & 1) {
		af1.m = -m;
	} else {
		af1.m = m;
	}

	awp_norm(&af1);

	// rounding up if bit 40 is set.
	if (af1.m & FP_M_BIT_40) {
		awp_denorm(&af1, 1);
		af1.m += FP_M_BIT_40;
		awp_norm(&af1);
	}

	// set C to M[-1]
	if ((af1.m & FP_M_BIT_40)) FL_SET(r[0], FL_C);
	else FL_CLR(r[0], FL_C);

	// NOTE: MF never set V

	return awp_store_float(&af1, r+1, r);
}

// -----------------------------------------------------------------------
int awp_float_div(uint16_t *r, uint16_t *n)
{
	struct awpf af1, af2;

	if (awp_load_float(&af1, r+1) || awp_load_float(&af2, n)) {
		return AWP_FP_ERR;
	}

	// NOTE: in case of AWP_FP_ERR CPU registers stay unchanged
	if (af2.m == 0) {
		return AWP_FP_ERR;
	}

	// remove signs, store the final sign
	awp_denorm(&af1, 2);
	awp_denorm(&af2, 1);

	int sign = 1;
	if (af1.m < 0) {
		sign *= -1;
		af1.m = -af1.m;
	}
	if (af2.m < 0) {
		sign *= -1;
		af2.m = -af2.m;
	}

	// divide
	af1.e -= af2.e;
	int64_t rem = af1.m;
	af1.m = 0;
	for (int i=0 ; i<41 ; i++) {
		rem -= af2.m;
		af1.m <<= 1;
		if (rem < 0) {
			rem += af2.m;
		} else {
			af1.m |= FP_M_BIT_40;
		}
		rem <<= 1;
	}
	af1.m *= sign;

	awp_norm(&af1);

	// division always clears C
	FL_CLR(r[0], FL_C);

	// NOTE: DF never set V

	return awp_store_float(&af1, r+1, r);
}

// vim: tabstop=4 shiftwidth=4 autoindent
