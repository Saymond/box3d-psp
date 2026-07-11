// SPDX-FileCopyrightText: 2026 Box3D PSP Port
// SPDX-License-Identifier: MIT

#include "vfpu_math.h"
#include <math.h>

float vfpu_sqrtf( float x )
{
	float r;
	__asm__ volatile (
		"mtv      %1, S000\n"
		"vsqrt.s  S001, S000\n"
		"mfv      %0, S001\n"
		: "=r"( r )
		: "r"( x )
	);
	return r;
}

float vfpu_rsqrtf( float x )
{
	float r;
	__asm__ volatile (
		"mtv     %1, S000\n"
		"vrsq.s  S001, S000\n"
		"mfv     %0, S001\n"
		: "=r"( r )
		: "r"( x )
	);
	return r;
}

float vfpu_sinf( float x )
{
	float r;
	__asm__ volatile (
		"mtv    %1, S000\n"
		"vsin.s S001, S000\n"
		"mfv    %0, S001\n"
		: "=r"( r )
		: "r"( x )
	);
	return r;
}

float vfpu_cosf( float x )
{
	float r;
	__asm__ volatile (
		"mtv    %1, S000\n"
		"vcos.s S001, S000\n"
		"mfv    %0, S001\n"
		: "=r"( r )
		: "r"( x )
	);
	return r;
}

void vfpu_sincosf( float x, float* s, float* c )
{
	__asm__ volatile (
		"mtv     %2, S000\n"
		"vsin.s  S001, S000\n"
		"vcos.s  S002, S000\n"
		"mfv     %0, S001\n"
		"mfv     %1, S002\n"
		: "=r"( *s ), "=r"( *c )
		: "r"( x )
	);
}

void vfpu_mat4_identity( psp_mat4* m )
{
	static const float identity[16] __attribute__( ( aligned( 16 ) ) ) = {
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f,
	};
	const float* src = identity;
	float* dst = m->m;
	__asm__ volatile (
		"lv.q  C000,  0(%[src])\n"
		"lv.q  C010, 16(%[src])\n"
		"lv.q  C020, 32(%[src])\n"
		"lv.q  C030, 48(%[src])\n"
		"sv.q  C000,  0(%[dst])\n"
		"sv.q  C010, 16(%[dst])\n"
		"sv.q  C020, 32(%[dst])\n"
		"sv.q  C030, 48(%[dst])\n"
		: [dst] "+r"( dst )
		: [src] "r"( src )
		: "memory"
	);
}

void vfpu_mat4_translation( psp_mat4* out, float x, float y, float z )
{
	vfpu_mat4_identity( out );
	out->m[12] = x;
	out->m[13] = y;
	out->m[14] = z;
	out->m[15] = 1.0f;
}

void vfpu_mat4_mul( psp_mat4* out, const psp_mat4* a, const psp_mat4* b )
{
	const float* A = a->m;
	const float* B = b->m;
	float* O = out->m;
	for ( int c = 0; c < 4; ++c )
	{
		for ( int r = 0; r < 4; ++r )
		{
			float sum = 0.0f;
			for ( int k = 0; k < 4; ++k )
			{
				sum += A[k * 4 + r] * B[c * 4 + k];
			}
			O[c * 4 + r] = sum;
		}
	}
}

void vfpu_mat4_perspective( psp_mat4* out, float fovy, float aspect, float nearZ, float farZ )
{
	float s, c;
	vfpu_sincosf( fovy * 0.5f, &s, &c );
	float f = c / s;
	float nf = 1.0f / ( nearZ - farZ );

	vfpu_mat4_identity( out );
	out->m[0]  = f / aspect;
	out->m[5]  = f;
	out->m[10] = ( farZ + nearZ ) * nf;
	out->m[11] = -1.0f;
	out->m[14] = 2.0f * farZ * nearZ * nf;
	out->m[15] = 0.0f;
}

void vfpu_mat4_lookat( psp_mat4* out, const psp_vec3* eye, const psp_vec3* target, const psp_vec3* up )
{
	psp_vec3 f = { target->x - eye->x, target->y - eye->y, target->z - eye->z, 0.0f };
	float fn = vfpu_sqrtf( f.x * f.x + f.y * f.y + f.z * f.z );
	float inv = fn > 0.0f ? 1.0f / fn : 0.0f;
	f.x *= inv; f.y *= inv; f.z *= inv;

	psp_vec3 s = {
		f.y * up->z - f.z * up->y,
		f.z * up->x - f.x * up->z,
		f.x * up->y - f.y * up->x,
		0.0f,
	};
	float sn = vfpu_sqrtf( s.x * s.x + s.y * s.y + s.z * s.z );
	float sinv = sn > 0.0f ? 1.0f / sn : 0.0f;
	s.x *= sinv; s.y *= sinv; s.z *= sinv;

	psp_vec3 u = {
		s.y * f.z - s.z * f.y,
		s.z * f.x - s.x * f.z,
		s.x * f.y - s.y * f.x,
		0.0f,
	};

	vfpu_mat4_identity( out );
	out->m[0]  = s.x;  out->m[1]  = u.x;  out->m[2]  = -f.x;
	out->m[4]  = s.y;  out->m[5]  = u.y;  out->m[6]  = -f.y;
	out->m[8]  = s.z;  out->m[9]  = u.z;  out->m[10] = -f.z;
	out->m[12] = -( s.x * eye->x + u.x * eye->y + (-f.x) * eye->z );
	out->m[13] = -( s.y * eye->x + u.y * eye->y + (-f.y) * eye->z );
	out->m[14] = -( s.z * eye->x + u.z * eye->y + (-f.z) * eye->z );
	out->m[15] = 1.0f;
}

void vfpu_mat4_from_quat( psp_mat4* out, const psp_vec4* q )
{
	float x = q->x, y = q->y, z = q->z, w = q->w;
	float x2, y2, z2;
	__asm__ volatile (
		"mtv     %[x], S000\n"
		"mtv     %[y], S010\n"
		"mtv     %[z], S020\n"
		"vmul.s  S001, S000, S000\n"
		"vmul.s  S011, S010, S010\n"
		"vmul.s  S021, S020, S020\n"
		"mfv     %[x2], S001\n"
		"mfv     %[y2], S011\n"
		"mfv     %[z2], S021\n"
		: [x2] "=r"( x2 ), [y2] "=r"( y2 ), [z2] "=r"( z2 )
		: [x] "r"( x ), [y] "r"( y ), [z] "r"( z )
	);

	float sx = 2.0f * x, sy = 2.0f * y, sz = 2.0f * z;
	float sxx = sx * x, syy = sy * y, szz = sz * z;
	float sxy = sx * y, sxz = sx * z, syz = sy * z;
	float swx = 2.0f * w * x, swy = 2.0f * w * y, swz = 2.0f * w * z;

	vfpu_mat4_identity( out );
	out->m[0]  = 1.0f - ( syy + szz );
	out->m[1]  = sxy + swz;
	out->m[2]  = sxz - swy;
	out->m[4]  = sxy - swz;
	out->m[5]  = 1.0f - ( sxx + szz );
	out->m[6]  = syz + swx;
	out->m[8]  = sxz + swy;
	out->m[9]  = syz - swx;
	out->m[10] = 1.0f - ( sxx + syy );
}

void vfpu_mat4_from_transform( psp_mat4* out, const psp_vec4* q, const psp_vec3* p )
{
	vfpu_mat4_from_quat( out, q );
	out->m[12] = p->x;
	out->m[13] = p->y;
	out->m[14] = p->z;
	out->m[15] = 1.0f;
}

void vfpu_mat4_transform_point( psp_vec3* out, const psp_mat4* m, const psp_vec3* in )
{
	float x = in->x, y = in->y, z = in->z;
	out->x = m->m[0] * x + m->m[4] * y + m->m[8]  * z + m->m[12];
	out->y = m->m[1] * x + m->m[5] * y + m->m[9]  * z + m->m[13];
	out->z = m->m[2] * x + m->m[6] * y + m->m[10] * z + m->m[14];
	out->_w = 1.0f;
}

void vfpu_mat4_transform_dir( psp_vec3* out, const psp_mat4* m, const psp_vec3* in )
{
	float x = in->x, y = in->y, z = in->z;
	out->x = m->m[0] * x + m->m[4] * y + m->m[8]  * z;
	out->y = m->m[1] * x + m->m[5] * y + m->m[9]  * z;
	out->z = m->m[2] * x + m->m[6] * y + m->m[10] * z;
	out->_w = 0.0f;
}

void vfpu_quat_normalize( psp_vec4* q )
{
	float xyz_dot;
	__asm__ volatile (
		"lv.q     C000, 0(%[q])\n"
		"vdot.t   S001, C000, C000\n"
		"mfv      %[r], S001\n"
		: [r] "=r"( xyz_dot )
		: [q] "r"( q )
		: "memory"
	);
	float norm_sq = xyz_dot + q->w * q->w;
	if ( norm_sq > 0.0f )
	{
		float inv = 1.0f / vfpu_sqrtf( norm_sq );
		q->x *= inv;
		q->y *= inv;
		q->z *= inv;
		q->w *= inv;
	}
}
