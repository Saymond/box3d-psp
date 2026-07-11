// SPDX-FileCopyrightText: 2026 Box3D PSP Port
// SPDX-License-Identifier: MIT
//
// VFPU math: 4x4 matrices, quaternions, scalar transcendentals.
// Column-major layout matches GU's ScePspFMatrix4.

#pragma once

#include <psptypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct psp_mat4
{
	float m[16];
} __attribute__( ( aligned( 16 ) ) ) psp_mat4;

typedef struct psp_vec3
{
	float x, y, z, _w;
} __attribute__( ( aligned( 16 ) ) ) psp_vec3;

typedef struct psp_vec4
{
	float x, y, z, w;
} __attribute__( ( aligned( 16 ) ) ) psp_vec4;

void vfpu_mat4_identity( psp_mat4* m );
void vfpu_mat4_mul( psp_mat4* out, const psp_mat4* a, const psp_mat4* b );
void vfpu_mat4_perspective( psp_mat4* out, float fovy, float aspect, float nearZ, float farZ );
void vfpu_mat4_lookat( psp_mat4* out, const psp_vec3* eye, const psp_vec3* target, const psp_vec3* up );
void vfpu_mat4_translation( psp_mat4* out, float x, float y, float z );
void vfpu_mat4_from_quat( psp_mat4* out, const psp_vec4* q );
void vfpu_mat4_from_transform( psp_mat4* out, const psp_vec4* q, const psp_vec3* p );
void vfpu_mat4_transform_point( psp_vec3* out, const psp_mat4* m, const psp_vec3* in );
void vfpu_mat4_transform_dir( psp_vec3* out, const psp_mat4* m, const psp_vec3* in );

void vfpu_quat_normalize( psp_vec4* q );

static inline psp_vec4 vfpu_quat_from_b3( float x, float y, float z, float w )
{
	psp_vec4 q = { x, y, z, w };
	return q;
}

float vfpu_sqrtf( float x );
float vfpu_rsqrtf( float x );
float vfpu_sinf( float x );
float vfpu_cosf( float x );
void  vfpu_sincosf( float x, float* s, float* c );

#ifdef __cplusplus
}
#endif
