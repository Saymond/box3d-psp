// SPDX-FileCopyrightText: 2026 Box3D PSP Port
// SPDX-License-Identifier: MIT

#pragma once

#include <psptypes.h>
#include <pspge.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>

#include "box3d/box3d.h"
#include "box3d/types.h"

#include "vfpu_math.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PSP_SCREEN_WIDTH   480
#define PSP_SCREEN_HEIGHT  272
#define PSP_FB_PIXEL_FMT   GU_PSM_5551
#define PSP_DEPTH_FMT      GU_PSM_5551

typedef struct psp_vertex_colored
{
	unsigned int color;
	float        x, y, z;
} __attribute__( ( aligned( 16 ) ) ) psp_vertex_colored;

#define PSP_LINE_VERT_MAX  (1u << 13)
#define PSP_TRI_VERT_MAX   (1u << 15)

typedef struct psp_camera
{
	psp_vec3 target;
	float    distance;
	float    yaw;
	float    pitch;
	float    fovY;
	float    nearZ;
	float    farZ;
} psp_camera;

void psp_camera_eye( const psp_camera* cam, psp_vec3* eye );
void psp_camera_view( const psp_camera* cam, psp_mat4* view );
void psp_camera_proj( const psp_camera* cam, psp_mat4* proj );
void psp_camera_update( psp_camera* cam, const SceCtrlData* pad, const SceCtrlData* prev, float dt );

typedef struct psp_host
{
	void* fb0;
	void* fb1;
	void* depth;

	SceCtrlData pad;
	SceCtrlData prevPad;

	psp_camera camera;

	psp_vertex_colored* lineVerts;
	int                 lineVertCount;
	psp_vertex_colored* solidVerts;
	int                 solidVertCount;
	unsigned int        solidColor;

	int   frameIndex;
	float dt;
	float fps;
	int   bodyCount;
	int   contactCount;

	int   quit;
} psp_host;

psp_host* psp_host_get( void );
int  psp_host_init( void );
void psp_host_shutdown( void );

void psp_host_begin_frame( void );
void psp_host_end_frame( void );

typedef void (*psp_overlay_fn)( void );
void psp_host_set_overlay( psp_overlay_fn fn );

b3DebugDraw psp_host_make_debug_draw( void );

void psp_host_draw_body( b3BodyId bodyId, unsigned int color );
void psp_host_draw_body_solid( b3BodyId bodyId );
void psp_host_set_solid_color( unsigned int color );
void psp_host_draw_sphere_world( float cx, float cy, float cz, float radius, unsigned int color );

static inline unsigned int psp_color_rgba( float r, float g, float b, float a )
{
	unsigned int ur = ( unsigned int )( r * 255.0f + 0.5f );
	unsigned int ug = ( unsigned int )( g * 255.0f + 0.5f );
	unsigned int ub = ( unsigned int )( b * 255.0f + 0.5f );
	unsigned int ua = ( unsigned int )( a * 255.0f + 0.5f );
	if ( ur > 255 ) ur = 255;
	if ( ug > 255 ) ug = 255;
	if ( ub > 255 ) ub = 255;
	if ( ua > 255 ) ua = 255;
	return ( ua << 24 ) | ( ub << 16 ) | ( ug << 8 ) | ur;
}

#ifdef __cplusplus
}
#endif
