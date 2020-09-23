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

// -----------------------------------------------------------------------
struct awp * awp_init(uint16_t *flags, uint16_t *r1, uint16_t *r2, uint16_t *r3)
{
	struct awp * awp = malloc(sizeof(struct awp));
	if (!awp) return NULL;

	awp->flags = flags;
	awp->r1 = r1;
	awp->r2 = r2;
	awp->r3 = r3;
	awp->v = 0;

	return awp;
}

// -----------------------------------------------------------------------
void awp_destroy(struct awp *awp)
{
	if (!awp) return;

	free(awp);
}

// -----------------------------------------------------------------------
// ---- 32-bit -----------------------------------------------------------
// -----------------------------------------------------------------------

// -----------------------------------------------------------------------
static void __awp_dword_set_Z(struct awp *awp, uint64_t z)
{
	// set Z if:
	//  * all 32 bits of result are 0

	if ((z & BITS_0_31) == 0) {
		FL_SET(*(awp->flags), FL_Z);
	} else {
		FL_CLR(*(awp->flags), FL_Z);
	}
}

// -----------------------------------------------------------------------
static void __awp_dword_update_V(struct awp *awp, uint64_t x, uint64_t y, uint64_t z)
{
	// update (not set!) V if:
	//  * both arguments were positive, and result is negative
	//  * OR both arguments were negative, and result is positive
	// store awp->v, M needs it

	if (
	   (  (x & BIT_0) &&  (y & BIT_0) && !(z & BIT_0) )
	|| ( !(x & BIT_0) && !(y & BIT_0) &&  (z & BIT_0) )
	) {
		FL_SET(*(awp->flags), FL_V);
		awp->v = 1;
	} else {
		awp->v = 0;
	}
}

// -----------------------------------------------------------------------
static void __awp_dword_set_C(struct awp *awp, uint64_t z)
{
	// set C if bit on position -1 is set

	if ((z & BIT_MINUS1)) {
		FL_SET(*(awp->flags), FL_C);
	} else {
		FL_CLR(*(awp->flags), FL_C);
	}
}

// -----------------------------------------------------------------------
static void __awp_dword_set_M(struct awp *awp, uint64_t z)
{
	// note: internal V needs to be set before setting M
	// set M if:
	//  * minus and no overflow (just a plain negative number)
	//  * OR not minus and overflow (number looks non-negative, but there is overflow, which means a negative number overflown)

	if (
	   ( (z & BIT_0) && !awp->v)
	|| (!(z & BIT_0) &&  awp->v)
	) {
		FL_SET(*(awp->flags), FL_M);
	} else {
		FL_CLR(*(awp->flags), FL_M);
	}
}

// -----------------------------------------------------------------------
int awp_dword_addsub(struct awp *awp, uint16_t b1, uint16_t b2, int op)
{
	uint32_t a = DWORD(*(awp->r1), *(awp->r2));
	uint32_t b = DWORD(b1, b2);
	uint64_t res = (uint64_t) a + (op * b);

	*(awp->r1) = DWORD_H(res);
	*(awp->r2) = DWORD_L(res);

	// AD and SD set all flags

	__awp_dword_update_V(awp, a, op*b, res);
	__awp_dword_set_M(awp, res);
	__awp_dword_set_C(awp, res);
	__awp_dword_set_Z(awp, res);

	return AWP_OK;
}

// -----------------------------------------------------------------------
int awp_dword_mul(struct awp *awp, int16_t b)
{
	int64_t res = (int16_t) *(awp->r2) * b;

	*(awp->r1) = DWORD_H(res);
	*(awp->r2) = DWORD_L(res);

	// MW does not touch C
	// MW may touch V, but it won't (?)

	awp->v = 0; // internal only
	__awp_dword_set_M(awp, res);
	__awp_dword_set_Z(awp, res);

	return AWP_OK;
}

// -----------------------------------------------------------------------
int awp_dword_div(struct awp *awp, int16_t b)
{
	// NOTE: in case of AWP_FP_ERR CPU registers stay unchanged
	if (b == 0) {
		return AWP_FP_ERR;
	}

	int32_t a = DWORD(*(awp->r1), *(awp->r2));

	int32_t res = a / b;

	// NOTE: in case of AWP_DIV_OF CPU registers stay unchanged
	if ((res > SHRT_MAX) || (res < SHRT_MIN)) {
		return AWP_DIV_OF;
	}

	*(awp->r2) = res;
	*(awp->r1) = a % b;

	// DW has does not touch V nor C

	awp->v = 0; // internal only
	__awp_dword_set_M(awp, res);
	__awp_dword_set_Z(awp, res);

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

// -----------------------------------------------------------------------
int awp_to_double(double *f, uint16_t d1, uint16_t d2, uint16_t d3)
{
	int64_t m;
	double m_f;
	int8_t exp;
	int ret = AWP_OK;

	exp = d3 & 0x00ff;
	m  = (int64_t) d1 << 48;
	m |= (int64_t) d2 << 32;
	m |= (int64_t) (d3 & 0xff00) << 16;

	if (m == 0) {
		*f = 0.0f;
	} else {
		// indicate that input is denormalized
		if (((m & FP_M_BIT_0) >> 1) == (m & FP_M_BIT_1)) {
			ret = AWP_FP_ERR;
		}

		// convert mantissa to floating point: m_f = m * 2^-(FP_BITS-1)
		m_f = ldexp(m, -(FP_BITS-1));

		// make final fp number: f = m_f * 2^exp
		*f = ldexp(m_f, exp);
	}

	return ret;
}

// -----------------------------------------------------------------------
int awp_from_double(uint16_t *d1, uint16_t *d2, uint16_t *d3, uint16_t *flags, double f, int round)
{
	int ret = AWP_OK;

	int exp;
	int64_t m_int;
	double m;

	// get m, exp
	m = frexp(f, &exp);
	// scale m to 64-bit
	m_int = ldexp(m, FP_BITS-1);

/*
	Normalize

	normalized mantissa for frexp() is: (-1, -0.5], [0.5, 1)
	normalized mantissa for AWP is:     [-1, -0.5), [0.5, 1)
	
	for example, -1 + -1 = -2
	
	-1  0x8000 0x0000 0x0000 +
	-1  0x8000 0x0000 0x0000
	=   0xc000 0x0000 0x0002 = -0.5 * 2^n     (without the fix)
	=   0x8000 0x0000 0x0001 = -1   * 2^(n-1) (with the fix)
*/
	if ((m_int != 0) && (((m_int & FP_M_BIT_0) >> 1) == (m_int & FP_M_BIT_1))) {
		m_int <<= 1;
		exp--;
	}

	// AWP rounds up for AF/SF if bit 40 (stored in M[-1]) of the result is set
	if (round && (m_int & FP_M_BIT_40)) {
		// if we would overflow, need to shift right first
		if ((m_int & FP_M_MAX) == FP_M_MAX) {
			m_int >>= 1;
			exp++;
			m_int += FP_M_BIT_39 >> 1;
		} else {
			m_int += FP_M_BIT_39;
		}
	}

	// check for overflow/underflow
	if (exp > 127) {
		ret = AWP_FP_OF;
	} else if ((exp < -128) && (m_int != 0)) {
		ret = AWP_FP_UF;
	}

	*d1 = m_int >> 48;
	*d2 = m_int >> 32;
	*d3 = (m_int >> 16) & 0xff00;
	*d3 |= exp & 0xff;

	// fp operations don't touch V
	// set Z and M
	if ((m_int & FP_M_MASK) == 0) {
		FL_SET(*flags, FL_Z);
		FL_CLR(*flags, FL_M);
	} else if ((m_int & FP_M_BIT_0)) {
		FL_CLR(*flags, FL_Z);
		FL_SET(*flags, FL_M);
	} else {
		FL_CLR(*flags, FL_Z);
		FL_CLR(*flags, FL_M);
	}

	if (round && (m_int & FP_M_BIT_40)) {
		// set C to M[-1] (used only by AF/SF)
		FL_SET(*flags, FL_C);
	} else {
		FL_CLR(*flags, FL_C);
	}

	return ret;
}

struct awpf {
	int64_t m;
	int16_t e;
} awpf;

// -----------------------------------------------------------------------
int awp_float_from_words(struct awpf *f, uint16_t w1, uint16_t w2, uint16_t w3)
{
	f->m  = (int64_t) w1 << 48;
	f->m |= (int64_t) w2 << 32;
	f->m |= (int64_t) (w3 & 0xff00) << 16;
	f->e = (int8_t) (w3 & 0xff);

	if (f->m == 0) {
		f->e = 0;
		return 0;
	} else {
		return (w1 ^ (w1<<1)) & 0x8000 ? 0 : -1;
	}
}

// -----------------------------------------------------------------------
void awp_float_to_words(struct awpf *f, struct awp *awp)
{
	// set Z and M
	if ((f->m & FP_M_MASK) == 0) {
		f->e = 0; // by definition
		FL_SET(*awp->flags, FL_Z);
		FL_CLR(*awp->flags, FL_M);
	} else if ((f->m & FP_M_BIT_0)) {
		FL_CLR(*awp->flags, FL_Z);
		FL_SET(*awp->flags, FL_M);
	} else {
		FL_CLR(*awp->flags, FL_Z);
		FL_CLR(*awp->flags, FL_M);
	}

	*awp->r1 = f->m >> 48;
	*awp->r2 = f->m >> 32;
	*awp->r3 = ((f->m >> 16) & 0xff00) | ( f->e & 0xff);
}
void awp_norm(struct awpf *f);

// -----------------------------------------------------------------------
int awp_float_norm(struct awp *awp)
{
	struct awpf f;

	awp_float_from_words(&f, *awp->r1, *awp->r2, *awp->r3);
	awp_norm(&f);
	awp_float_to_words(&f, awp);

	// NOTE: normalization always clears C
	FL_CLR(*(awp->flags), FL_C);

	// check for overflow/underflow
	if (f.e > 127) {
		return AWP_FP_OF;
	} else if ((f.e < -128) && (f.m != 0)) {
		return AWP_FP_UF;
	} else {
		return AWP_OK;
	}
}

// -----------------------------------------------------------------------
void awp_denorm(struct awpf *f, int x)
{
	f->m >>= x;
	f->e += x;
}

// -----------------------------------------------------------------------
void awp_norm(struct awpf *f)
{
	while ((f->m != 0) && !((f->m ^ (f->m << 1)) & FP_M_BIT_0)) {
		f->m <<= 1;
		f->e -= 1;
	}
}

// -----------------------------------------------------------------------
int awp_float_addsub(struct awp *awp, uint16_t d1, uint16_t d2, uint16_t d3, int op)
{
	struct awpf af1, af2;

	if (awp_float_from_words(&af1, *awp->r1, *awp->r2, *awp->r3) || awp_float_from_words(&af2, d1, d2, d3)) {
		return AWP_FP_ERR;
	}

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
	// arithmetic is done in 64 bits, so there's plenty of space
	awp_denorm(&af1, 1);
	awp_denorm(&af2, 1);

	af1.m = af1.m + op * af2.m;
	awp_norm(&af1);

	// rounding up if bit 40 is set. this also sets the carry flag
	if (af1.m & FP_M_BIT_40) {
		awp_denorm(&af1, 1);
		af1.m += FP_M_BIT_40;
		awp_norm(&af1);
	}

	// NOTE: AF/SF never set V

	// TODO: store and UF/OF check order

	// set C to M[-1]
	if ((af1.m & FP_M_BIT_40)) {
		FL_SET(*awp->flags, FL_C);
	} else {
		FL_CLR(*awp->flags, FL_C);
	}

	awp_float_to_words(&af1, awp);

	// check for overflow/underflow
	if (af1.e > 127) {
		return AWP_FP_OF;
	} else if ((af1.e < -128) && (af1.m != 0)) {
		return AWP_FP_UF;
	} else {
		return AWP_OK;
	}
}

// -----------------------------------------------------------------------
int awp_float_mul(struct awp *awp, uint16_t d1, uint16_t d2, uint16_t d3)
{
	struct awpf af1, af2;

	if (awp_float_from_words(&af1, *awp->r1, *awp->r2, *awp->r3) || awp_float_from_words(&af2, d1, d2, d3)) {
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
	// we multiply one additional bit because of the initial denormalization
	int64_t m = 0;
	af1.e += af2.e;
	af2.m >>= 23;
	for (int i=0 ; i<41 ; i++) {
		m >>= 1;
		m += af1.m * (af2.m & 1);
		af2.m >>= 1;
	}
	if (sign & 1) {
		af1.m = -m;
	} else {
		af1.m = m;
	}

	awp_norm(&af1);

	// rounding up if bit 40 is set. this also sets the carry flag contrary to what doc says
	if (af1.m & FP_M_BIT_40) {
		awp_denorm(&af1, 1);
		af1.m += FP_M_BIT_40;
		awp_norm(&af1);
		FL_SET(*awp->flags, FL_C);
	} else {
		FL_CLR(*awp->flags, FL_C);
	}

	// TODO: store and UF/OF check order

	awp_float_to_words(&af1, awp);

	// check for overflow/underflow
	if (af1.e > 127) {
		return AWP_FP_OF;
	} else if ((af1.e < -128) && (af1.m != 0)) {
		return AWP_FP_UF;
	} else {
		return AWP_OK;
	}
}

// -----------------------------------------------------------------------
int awp_float_div(struct awp *awp, uint16_t d1, uint16_t d2, uint16_t d3)
{
	struct awpf af1, af2;

	if (awp_float_from_words(&af1, *awp->r1, *awp->r2, *awp->r3) || awp_float_from_words(&af2, d1, d2, d3)) {
		return AWP_FP_ERR;
	}

	// div by 0
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
	FL_CLR(*awp->flags, FL_C);

	// TODO: store and UF/OF check order

	awp_float_to_words(&af1, awp);

	// check for overflow/underflow
	if (af1.e > 127) {
		return AWP_FP_OF;
	} else if ((af1.e < -128) && (af1.m != 0)) {
		return AWP_FP_UF;
	} else {
		return AWP_OK;
	}
}

// vim: tabstop=4 shiftwidth=4 autoindent
