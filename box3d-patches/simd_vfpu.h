// SPDX-FileCopyrightText: 2026 Box3D PSP Port
// SPDX-License-Identifier: MIT
//
// VFPU SIMD shim for Box3D. Implements the b3V32 API using Allegrex VFPU
// instructions (vadd.t, vsub.t, vmul.t, vdiv.t, vmin.t, vmax.t, vcrs.t,
// vabs.t, vneg.t, lv.q, sv.q).

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct b3V32
{
	float x, y, z, w;
} __attribute__( ( aligned( 16 ) ) ) b3V32;

typedef union b3128
{
	b3V32 v;
	float f[4];
} b3128;

static const b3V32 b3_zeroV __attribute__( ( aligned( 16 ) ) ) = { 0.0f, 0.0f, 0.0f, 0.0f };
static const b3V32 b3_halfV __attribute__( ( aligned( 16 ) ) ) = { 0.5f, 0.5f, 0.5f, 0.0f };
static const b3V32 b3_oneV  __attribute__( ( aligned( 16 ) ) ) = { 1.0f, 1.0f, 1.0f, 0.0f };

#define B3_VFPU_BINOP( INST, NAME )                                              \
	static inline b3V32 NAME( b3V32 a, b3V32 b )                                  \
	{                                                                             \
		b3V32 r;                                                                  \
		const b3V32* pa = &a;                                                     \
		const b3V32* pb = &b;                                                     \
		b3V32* pr = &r;                                                           \
		__asm__ volatile (                                                        \
			"lv.q    C000, 0(%[a])\n"                                             \
			"lv.q    C010, 0(%[b])\n"                                             \
			INST " C020, C000, C010\n"                                            \
			"sv.q    C020, 0(%[r])\n"                                             \
			: [r] "=r"( pr )                                                      \
			: [a] "r"( pa ), [b] "r"( pb )                                        \
			: "memory"                                                            \
		);                                                                        \
		return r;                                                                 \
	}

B3_VFPU_BINOP( "vadd.t", b3AddV )
B3_VFPU_BINOP( "vsub.t", b3SubV )
B3_VFPU_BINOP( "vmul.t", b3MulV )
B3_VFPU_BINOP( "vdiv.t", b3DivV )
B3_VFPU_BINOP( "vmin.t", b3MinV )
B3_VFPU_BINOP( "vmax.t", b3MaxV )
B3_VFPU_BINOP( "vcrs.t", b3CrossV )

#undef B3_VFPU_BINOP

static inline b3V32 b3NegV( b3V32 a )
{
	b3V32 r;
	const b3V32* pa = &a;
	b3V32* pr = &r;
	__asm__ volatile (
		"lv.q    C000, 0(%[a])\n"
		"vneg.t  C020, C000\n"
		"sv.q    C020, 0(%[r])\n"
		: [r] "=r"( pr )
		: [a] "r"( pa )
		: "memory"
	);
	return r;
}

static inline b3V32 b3AbsV( b3V32 a )
{
	b3V32 r;
	const b3V32* pa = &a;
	b3V32* pr = &r;
	__asm__ volatile (
		"lv.q    C000, 0(%[a])\n"
		"vabs.t  C020, C000\n"
		"sv.q    C020, 0(%[r])\n"
		: [r] "=r"( pr )
		: [a] "r"( pa )
		: "memory"
	);
	return r;
}

static inline b3V32 b3LoadV( const float* src )
{
	b3V32 r;
	r.x = src[0];
	r.y = src[1];
	r.z = src[2];
	r.w = 0.0f;
	return r;
}

static inline b3V32 b3ZeroV( void )
{
	b3V32 r;
	b3V32* pr = &r;
	__asm__ volatile (
		"vzero.q C020\n"
		"sv.q    C020, 0(%[r])\n"
		: [r] "=r"( pr )
		:
		: "memory"
	);
	return r;
}

static inline float b3GetXV( b3V32 a ) { return a.x; }
static inline float b3GetYV( b3V32 a ) { return a.y; }
static inline float b3GetZV( b3V32 a ) { return a.z; }

static inline float b3GetV( b3V32 a, int index )
{
	b3128 b;
	b.v = a;
	return b.f[index];
}

static inline b3V32 b3SplatV( float x )
{
	b3V32 r;
	r.x = x; r.y = x; r.z = x; r.w = 0.0f;
	return r;
}

static inline b3V32 b3ModifiedCrossV( b3V32 a, b3V32 b )
{
	b3V32 r;
	r.x = a.y * b.z + a.z * b.y;
	r.y = a.z * b.x + a.x * b.z;
	r.z = a.x * b.y + a.y * b.x;
	r.w = 0.0f;
	return r;
}

static inline bool b3AnyLess3V( b3V32 a, b3V32 b )      { return a.x <  b.x || a.y <  b.y || a.z <  b.z; }
static inline bool b3AnyLessEq3V( b3V32 a, b3V32 b )    { return a.x <= b.x || a.y <= b.y || a.z <= b.z; }
static inline bool b3AnyGreater3V( b3V32 a, b3V32 b )   { return a.x >  b.x || a.y >  b.y || a.z >  b.z; }
static inline bool b3AllLessEq3V( b3V32 a, b3V32 b )    { return a.x <= b.x && a.y <= b.y && a.z <= b.z; }

#ifdef __cplusplus
}
#endif
