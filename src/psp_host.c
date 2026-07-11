// SPDX-FileCopyrightText: 2026 Box3D PSP Port
// SPDX-License-Identifier: MIT

#include "psp_host.h"

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspge.h>
#include <psputils.h>

#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <math.h>

#define VRAM_ADDR        ( ( void* )0x04000000u )
#define VRAM_FB_STRIDE   512
#define VRAM_FB_SIZE     ( VRAM_FB_STRIDE * PSP_SCREEN_HEIGHT * 2 )
#define VRAM_DEPTH_SIZE  ( VRAM_FB_STRIDE * PSP_SCREEN_HEIGHT * 2 )

static unsigned int s_vram_offset = 0;

static void* vram_alloc( unsigned int size )
{
	void* p = ( void* )( s_vram_offset );
	s_vram_offset += size;
	s_vram_offset = ( s_vram_offset + 15u ) & ~15u;
	return p;
}

void* psp_host_vram_abs( void* offset )
{
	return ( void* )( ( unsigned char* )VRAM_ADDR + ( unsigned int )offset );
}

static void* vram_abs( void* offset )
{
	return psp_host_vram_abs( offset );
}

static unsigned int s_gu_list[ 262144 / 4 ] __attribute__( ( aligned( 16 ) ) );

static psp_host s_host;
static psp_overlay_fn s_overlay_fn = NULL;

void psp_host_set_overlay( psp_overlay_fn fn )
{
	s_overlay_fn = fn;
}

psp_host* psp_host_get( void )
{
	return &s_host;
}

static unsigned int psp_color_from_hex( b3HexColor hex )
{
	unsigned int r = ( unsigned int )hex & 0xFF;
	unsigned int g = ( unsigned int )( hex >> 8 ) & 0xFF;
	unsigned int b = ( unsigned int )( hex >> 16 ) & 0xFF;
	return 0xFF000000u | ( b << 16 ) | ( g << 8 ) | r;
}

static inline void psp_push_line( float x1, float y1, float z1,
                                  float x2, float y2, float z2,
                                  unsigned int color )
{
	if ( s_host.lineVertCount + 2 > PSP_LINE_VERT_MAX ) return;
	psp_vertex_colored* v1 = &s_host.lineVerts[ s_host.lineVertCount++ ];
	psp_vertex_colored* v2 = &s_host.lineVerts[ s_host.lineVertCount++ ];
	v1->x = x1; v1->y = y1; v1->z = z1; v1->color = color;
	v2->x = x2; v2->y = y2; v2->z = z2; v2->color = color;
}

static const float SUN_DIR[3]   = { -0.4f, -0.8f, 0.5f };
static const float SUN_COLOR[3] = { 0.9f, 0.88f, 0.82f };
static const float AMB_COLOR[3] = { 0.25f, 0.27f, 0.32f };

static unsigned int psp_light_vertex( float nx, float ny, float nz, unsigned int material )
{
	float mr = ( float )( material & 0xFF )         / 255.0f;
	float mg = ( float )( ( material >> 8 ) & 0xFF )  / 255.0f;
	float mb = ( float )( ( material >> 16 ) & 0xFF ) / 255.0f;

	float nlen = vfpu_sqrtf( nx * nx + ny * ny + nz * nz );
	if ( nlen > 0.0001f )
	{
		float inv = 1.0f / nlen;
		nx *= inv; ny *= inv; nz *= inv;
	}

	float ndotl = nx * ( -SUN_DIR[0] ) + ny * ( -SUN_DIR[1] ) + nz * ( -SUN_DIR[2] );
	if ( ndotl < 0.0f ) ndotl = 0.0f;

	float r = mr * ( AMB_COLOR[0] + SUN_COLOR[0] * ndotl );
	float g = mg * ( AMB_COLOR[1] + SUN_COLOR[1] * ndotl );
	float b = mb * ( AMB_COLOR[2] + SUN_COLOR[2] * ndotl );

	int ir = ( int )( r * 255.0f + 0.5f ); if ( ir > 255 ) ir = 255; if ( ir < 0 ) ir = 0;
	int ig = ( int )( g * 255.0f + 0.5f ); if ( ig > 255 ) ig = 255; if ( ig < 0 ) ig = 0;
	int ib = ( int )( b * 255.0f + 0.5f ); if ( ib > 255 ) ib = 255; if ( ib < 0 ) ib = 0;
	return 0xFF000000u | ( ib << 16 ) | ( ig << 8 ) | ir;
}

static inline void psp_push_solid_tri( float n1x, float n1y, float n1z,
                                       float x1,  float y1,  float z1,
                                       float n2x, float n2y, float n2z,
                                       float x2,  float y2,  float z2,
                                       float n3x, float n3y, float n3z,
                                       float x3,  float y3,  float z3 )
{
	if ( s_host.solidVertCount + 3 > PSP_TRI_VERT_MAX ) return;
	unsigned int c1 = psp_light_vertex( n1x, n1y, n1z, s_host.solidColor );
	unsigned int c2 = psp_light_vertex( n2x, n2y, n2z, s_host.solidColor );
	unsigned int c3 = psp_light_vertex( n3x, n3y, n3z, s_host.solidColor );
	psp_vertex_colored* v1 = &s_host.solidVerts[ s_host.solidVertCount++ ];
	psp_vertex_colored* v2 = &s_host.solidVerts[ s_host.solidVertCount++ ];
	psp_vertex_colored* v3 = &s_host.solidVerts[ s_host.solidVertCount++ ];
	v1->color = c1; v1->x = x1; v1->y = y1; v1->z = z1;
	v2->color = c2; v2->x = x2; v2->y = y2; v2->z = z2;
	v3->color = c3; v3->x = x3; v3->y = y3; v3->z = z3;
}

static inline void psp_push_solid_tri_flat( float nx, float ny, float nz,
                                            float x1, float y1, float z1,
                                            float x2, float y2, float z2,
                                            float x3, float y3, float z3 )
{
	psp_push_solid_tri( nx, ny, nz, x1, y1, z1,
	                    nx, ny, nz, x2, y2, z2,
	                    nx, ny, nz, x3, y3, z3 );
}

static inline void psp_push_solid_fan( float nx, float ny, float nz,
                                       const float ( *verts )[3], int count )
{
	if ( count < 3 ) return;
	for ( int i = 1; i < count - 1; ++i )
	{
		psp_push_solid_tri_flat( nx, ny, nz,
		                         verts[0][0],  verts[0][1],  verts[0][2],
		                         verts[i][0],   verts[i][1],   verts[i][2],
		                         verts[i+1][0], verts[i+1][1], verts[i+1][2] );
	}
}

static void psp_draw_segment( b3Pos p1, b3Pos p2, b3HexColor color, void* ctx )
{
	( void )ctx;
	unsigned int c = psp_color_from_hex( color );
	psp_push_line( p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, c );
}

static void psp_draw_point( b3Pos p, float size, b3HexColor color, void* ctx )
{
	( void )ctx; ( void )size;
	unsigned int c = psp_color_from_hex( color );
	float s = size > 0.0f ? size * 0.5f : 0.05f;
	psp_push_line( p.x - s, p.y, p.z, p.x + s, p.y, p.z, c );
	psp_push_line( p.x, p.y - s, p.z, p.x, p.y + s, p.z, c );
	psp_push_line( p.x, p.y, p.z - s, p.x, p.y, p.z + s, c );
}

static void psp_draw_transform( b3WorldTransform transform, void* ctx )
{
	( void )ctx;
	unsigned int cx = 0xFF0000FFu;
	unsigned int cy = 0xFF00FF00u;
	unsigned int cz = 0xFFFF0000u;
	float len = 0.5f;
	b3Vec3 p = transform.p;
	psp_push_line( p.x, p.y, p.z, p.x + len, p.y, p.z, cx );
	psp_push_line( p.x, p.y, p.z, p.x, p.y + len, p.z, cy );
	psp_push_line( p.x, p.y, p.z, p.x, p.y, p.z + len, cz );
}

static void psp_draw_sphere( b3Pos p, float radius, b3HexColor color, float alpha, void* ctx )
{
	( void )ctx;
	( void )alpha;
	unsigned int c = psp_color_from_hex( color );
	int segments = 12;
	for ( int i = 0; i < segments; ++i )
	{
		float a0 = ( float )i       * ( 2.0f * 3.14159265f / segments );
		float a1 = ( float )( i + 1 ) * ( 2.0f * 3.14159265f / segments );
		float s0, c0, s1, c1;
		vfpu_sincosf( a0, &s0, &c0 );
		vfpu_sincosf( a1, &s1, &c1 );
		psp_push_line( p.x + radius * c0, p.y + radius * s0, p.z,
		               p.x + radius * c1, p.y + radius * s1, p.z, c );
		psp_push_line( p.x + radius * c0, p.y, p.z + radius * s0,
		               p.x + radius * c1, p.y, p.z + radius * s1, c );
		psp_push_line( p.x, p.y + radius * c0, p.z + radius * s0,
		               p.x, p.y + radius * c1, p.z + radius * s1, c );
	}
}

static void psp_draw_capsule( b3Pos p1, b3Pos p2, float radius, b3HexColor color, float alpha, void* ctx )
{
	( void )alpha; ( void )ctx;
	unsigned int c = psp_color_from_hex( color );
	psp_push_line( p1.x, p1.y, p1.z, p2.x, p2.y, p2.z, c );
	psp_draw_sphere( p1, radius, color, 1.0f, ctx );
	psp_draw_sphere( p2, radius, color, 1.0f, ctx );
}

static void psp_draw_bounds( b3AABB aabb, b3HexColor color, void* ctx )
{
	( void )ctx;
	unsigned int c = psp_color_from_hex( color );
	float x0 = aabb.lowerBound.x, y0 = aabb.lowerBound.y, z0 = aabb.lowerBound.z;
	float x1 = aabb.upperBound.x, y1 = aabb.upperBound.y, z1 = aabb.upperBound.z;
	psp_push_line( x0, y0, z0, x1, y0, z0, c );
	psp_push_line( x1, y0, z0, x1, y1, z0, c );
	psp_push_line( x1, y1, z0, x0, y1, z0, c );
	psp_push_line( x0, y1, z0, x0, y0, z0, c );
	psp_push_line( x0, y0, z1, x1, y0, z1, c );
	psp_push_line( x1, y0, z1, x1, y1, z1, c );
	psp_push_line( x1, y1, z1, x0, y1, z1, c );
	psp_push_line( x0, y1, z1, x0, y0, z1, c );
	psp_push_line( x0, y0, z0, x0, y0, z1, c );
	psp_push_line( x1, y0, z0, x1, y0, z1, c );
	psp_push_line( x1, y1, z0, x1, y1, z1, c );
	psp_push_line( x0, y1, z0, x0, y1, z1, c );
}

static void psp_draw_box( b3Vec3 extents, b3WorldTransform transform, b3HexColor color, void* ctx )
{
	( void )ctx;
	unsigned int c = psp_color_from_hex( color );
	float hx = extents.x * 0.5f, hy = extents.y * 0.5f, hz = extents.z * 0.5f;
	b3Vec3 local[8] = {
		{ -hx, -hy, -hz }, { +hx, -hy, -hz }, { +hx, +hy, -hz }, { -hx, +hy, -hz },
		{ -hx, -hy, +hz }, { +hx, -hy, +hz }, { +hx, +hy, +hz }, { -hx, +hy, +hz },
	};
	b3Quat q = transform.q;
	float xx = q.v.x * q.v.x, yy = q.v.y * q.v.y, zz = q.v.z * q.v.z;
	float xy = q.v.x * q.v.y, xz = q.v.x * q.v.z, yz = q.v.y * q.v.z;
	float wx = q.s * q.v.x, wy = q.s * q.v.y, wz = q.s * q.v.z;
	float R[9] = {
		1.0f - 2.0f * ( yy + zz ), 2.0f * ( xy - wz ),       2.0f * ( xz + wy ),
		2.0f * ( xy + wz ),       1.0f - 2.0f * ( xx + zz ), 2.0f * ( yz - wx ),
		2.0f * ( xz - wy ),       2.0f * ( yz + wx ),       1.0f - 2.0f * ( xx + yy ),
	};
	b3Vec3 world[8];
	for ( int i = 0; i < 8; ++i )
	{
		world[i].x = transform.p.x + R[0] * local[i].x + R[1] * local[i].y + R[2] * local[i].z;
		world[i].y = transform.p.y + R[3] * local[i].x + R[4] * local[i].y + R[5] * local[i].z;
		world[i].z = transform.p.z + R[6] * local[i].x + R[7] * local[i].y + R[8] * local[i].z;
	}
	static const int edges[12][2] = {
		{0,1},{1,2},{2,3},{3,0},
		{4,5},{5,6},{6,7},{7,4},
		{0,4},{1,5},{2,6},{3,7},
	};
	for ( int i = 0; i < 12; ++i )
	{
		const b3Vec3* a = &world[ edges[i][0] ];
		const b3Vec3* b = &world[ edges[i][1] ];
		psp_push_line( a->x, a->y, a->z, b->x, b->y, b->z, c );
	}
}

static void psp_draw_string( b3Pos p, const char* s, b3HexColor color, void* ctx )
{
	( void )p; ( void )s; ( void )color; ( void )ctx;
}

static bool psp_draw_shape( void* userShape, b3WorldTransform transform, b3HexColor color, void* context )
{
	( void )userShape; ( void )transform; ( void )color; ( void )context;
	return true;
}

b3DebugDraw psp_host_make_debug_draw( void )
{
	b3DebugDraw draw = b3DefaultDebugDraw();
	draw.DrawShapeFcn    = psp_draw_shape;
	draw.DrawSegmentFcn  = psp_draw_segment;
	draw.DrawTransformFcn = psp_draw_transform;
	draw.DrawPointFcn    = psp_draw_point;
	draw.DrawSphereFcn   = psp_draw_sphere;
	draw.DrawCapsuleFcn  = psp_draw_capsule;
	draw.DrawBoundsFcn   = psp_draw_bounds;
	draw.DrawBoxFcn      = psp_draw_box;
	draw.DrawStringFcn   = psp_draw_string;
	draw.context         = &s_host;
	draw.drawShapes         = true;
	draw.drawJoints         = true;
	draw.drawBounds         = false;
	draw.drawContacts       = true;
	draw.drawContactNormals = true;
	return draw;
}

static void psp_rot_from_quat( const b3Quat* q, float R[9] )
{
	float xx = q->v.x * q->v.x, yy = q->v.y * q->v.y, zz = q->v.z * q->v.z;
	float xy = q->v.x * q->v.y, xz = q->v.x * q->v.z, yz = q->v.y * q->v.z;
	float wx = q->s * q->v.x, wy = q->s * q->v.y, wz = q->s * q->v.z;
	R[0] = 1.0f - 2.0f * ( yy + zz ); R[1] = 2.0f * ( xy - wz );       R[2] = 2.0f * ( xz + wy );
	R[3] = 2.0f * ( xy + wz );       R[4] = 1.0f - 2.0f * ( xx + zz ); R[5] = 2.0f * ( yz - wx );
	R[6] = 2.0f * ( xz - wy );       R[7] = 2.0f * ( yz + wx );       R[8] = 1.0f - 2.0f * ( xx + yy );
}

static inline void psp_transform_point( float out[3], const float R[9], const b3Vec3* p, const b3Vec3* local )
{
	float lx = local->x, ly = local->y, lz = local->z;
	out[0] = p->x + R[0] * lx + R[1] * ly + R[2] * lz;
	out[1] = p->y + R[3] * lx + R[4] * ly + R[5] * lz;
	out[2] = p->z + R[6] * lx + R[7] * ly + R[8] * lz;
}

static void psp_draw_hull( const b3HullData* hull, const b3Vec3* p, const float R[9], unsigned int color )
{
	const b3HullVertex* verts = b3GetHullVertices( hull );
	const b3Vec3* points = b3GetHullPoints( hull );
	const b3HullHalfEdge* edges = b3GetHullEdges( hull );
	if ( !verts || !points || !edges ) return;

	int edgeCount = hull->edgeCount;
	for ( int i = 0; i < edgeCount; ++i )
	{
		uint8_t twin = edges[i].twin;
		if ( i > ( int )twin ) continue;

		uint8_t a = edges[i].origin;
		uint8_t b = edges[twin].origin;
		if ( a >= hull->vertexCount || b >= hull->vertexCount ) continue;

		float wa[3], wb[3];
		psp_transform_point( wa, R, p, &points[a] );
		psp_transform_point( wb, R, p, &points[b] );
		psp_push_line( wa[0], wa[1], wa[2], wb[0], wb[1], wb[2], color );
	}
}

static void psp_draw_sphere_local( const b3Sphere* sphere, const b3Vec3* p, const float R[9], unsigned int color )
{
	float r = sphere->radius;
	b3Vec3 c = sphere->center;
	float wcx, wcy, wcz;
	{
		float tmp[3];
		psp_transform_point( tmp, R, p, &c );
		wcx = tmp[0]; wcy = tmp[1]; wcz = tmp[2];
	}

	int segments = 10;
	for ( int i = 0; i < segments; ++i )
	{
		float a0 = ( float )i       * ( 2.0f * 3.14159265f / segments );
		float a1 = ( float )( i + 1 ) * ( 2.0f * 3.14159265f / segments );
		float s0, c0, s1, c1;
		vfpu_sincosf( a0, &s0, &c0 );
		vfpu_sincosf( a1, &s1, &c1 );
		b3Vec3 la = { c.x + r * c0, c.y + r * s0, c.z };
		b3Vec3 lb = { c.x + r * c1, c.y + r * s1, c.z };
		b3Vec3 lc = { c.x + r * c0, c.y, c.z + r * s0 };
		b3Vec3 ld = { c.x + r * c1, c.y, c.z + r * s1 };
		b3Vec3 le = { c.x, c.y + r * c0, c.z + r * s0 };
		b3Vec3 lf = { c.x, c.y + r * c1, c.z + r * s1 };
		float wa[3], wb[3], wc[3], wd[3], we[3], wf[3];
		psp_transform_point( wa, R, p, &la );
		psp_transform_point( wb, R, p, &lb );
		psp_transform_point( wc, R, p, &lc );
		psp_transform_point( wd, R, p, &ld );
		psp_transform_point( we, R, p, &le );
		psp_transform_point( wf, R, p, &lf );
		psp_push_line( wa[0], wa[1], wa[2], wb[0], wb[1], wb[2], color );
		psp_push_line( wc[0], wc[1], wc[2], wd[0], wd[1], wd[2], color );
		psp_push_line( we[0], we[1], we[2], wf[0], wf[1], wf[2], color );
	}
}

static void psp_draw_capsule_local( const b3Capsule* cap, const b3Vec3* p, const float R[9], unsigned int color )
{
	float wa[3], wb[3];
	psp_transform_point( wa, R, p, &cap->center1 );
	psp_transform_point( wb, R, p, &cap->center2 );
	psp_push_line( wa[0], wa[1], wa[2], wb[0], wb[1], wb[2], color );

	b3Sphere s1 = { cap->center1, cap->radius };
	b3Sphere s2 = { cap->center2, cap->radius };
	psp_draw_sphere_local( &s1, p, R, color );
	psp_draw_sphere_local( &s2, p, R, color );
}

void psp_host_draw_body( b3BodyId bodyId, unsigned int color )
{
	b3WorldTransform tf = b3Body_GetTransform( bodyId );
	b3Vec3 p = tf.p;
	b3Quat q = tf.q;
	float R[9];
	psp_rot_from_quat( &q, R );

	b3ShapeId shapes[ 16 ];
	int n = b3Body_GetShapes( bodyId, shapes, 16 );
	if ( n > 16 ) n = 16;

	for ( int i = 0; i < n; ++i )
	{
		b3ShapeType type = b3Shape_GetType( shapes[i] );
		switch ( type )
		{
			case b3_hullShape:
			{
				const b3HullData* hull = b3Shape_GetHull( shapes[i] );
				if ( hull ) psp_draw_hull( hull, &p, R, color );
				break;
			}
			case b3_sphereShape:
			{
				b3Sphere s = b3Shape_GetSphere( shapes[i] );
				psp_draw_sphere_local( &s, &p, R, color );
				break;
			}
			case b3_capsuleShape:
			{
				b3Capsule c = b3Shape_GetCapsule( shapes[i] );
				psp_draw_capsule_local( &c, &p, R, color );
				break;
			}
			default:
				break;
		}
	}
}

void psp_host_set_solid_color( unsigned int color )
{
	s_host.solidColor = color;
}

psp_vertex_colored* psp_host_get_solid_verts( void )
{
	return s_host.solidVerts;
}

static void psp_draw_hull_solid( const b3HullData* hull, const b3Vec3* p, const float R[9] )
{
	const b3HullHalfEdge* edges = b3GetHullEdges( hull );
	const b3Vec3* points = b3GetHullPoints( hull );
	const b3HullFace* faces = b3GetHullFaces( hull );
	const b3Plane* planes = b3GetHullPlanes( hull );
	if ( !edges || !points || !faces || !planes ) return;

	for ( int f = 0; f < hull->faceCount; ++f )
	{
		uint8_t startEdge = faces[ f ].edge;
		b3Vec3 localN = planes[ f ].normal;
		float wnx = R[0] * localN.x + R[1] * localN.y + R[2] * localN.z;
		float wny = R[3] * localN.x + R[4] * localN.y + R[5] * localN.z;
		float wnz = R[6] * localN.x + R[7] * localN.y + R[8] * localN.z;

		uint8_t vertIdx[ 8 ];
		int nVerts = 0;
		uint8_t e = startEdge;
		int safety = 0;
		while ( nVerts < 8 && safety < 32 )
		{
			vertIdx[ nVerts++ ] = edges[ e ].origin;
			e = edges[ e ].next;
			if ( e == startEdge ) break;
			++safety;
		}
		if ( nVerts < 3 ) continue;

		float wverts[ 8 ][ 3 ];
		for ( int i = 0; i < nVerts; ++i )
		{
			const b3Vec3* lp = &points[ vertIdx[ i ] ];
			wverts[ i ][ 0 ] = p->x + R[0] * lp->x + R[1] * lp->y + R[2] * lp->z;
			wverts[ i ][ 1 ] = p->y + R[3] * lp->x + R[4] * lp->y + R[5] * lp->z;
			wverts[ i ][ 2 ] = p->z + R[6] * lp->x + R[7] * lp->y + R[8] * lp->z;
		}
		psp_push_solid_fan( wnx, wny, wnz, wverts, nVerts );
	}
}

static void psp_draw_sphere_solid_local( const b3Sphere* sphere, const b3Vec3* p, const float R[9] )
{
	float r = sphere->radius;
	b3Vec3 c = sphere->center;
	int rings = 6;
	int segs = 10;

	for ( int i = 0; i < rings; ++i )
	{
		float phi0 = 3.14159265f * ( float )i / ( float )rings - 1.5707963f;
		float phi1 = 3.14159265f * ( float )( i + 1 ) / ( float )rings - 1.5707963f;
		float sp0, cp0, sp1, cp1;
		vfpu_sincosf( phi0, &sp0, &cp0 );
		vfpu_sincosf( phi1, &sp1, &cp1 );

		for ( int j = 0; j < segs; ++j )
		{
			float th0 = 2.0f * 3.14159265f * ( float )j / ( float )segs;
			float th1 = 2.0f * 3.14159265f * ( float )( j + 1 ) / ( float )segs;
			float st0, ct0, st1, ct1;
			vfpu_sincosf( th0, &st0, &ct0 );
			vfpu_sincosf( th1, &st1, &ct1 );

			b3Vec3 l00 = { c.x + r * cp0 * ct0, c.y + r * sp0, c.z + r * cp0 * st0 };
			b3Vec3 l10 = { c.x + r * cp0 * ct1, c.y + r * sp0, c.z + r * cp0 * st1 };
			b3Vec3 l01 = { c.x + r * cp1 * ct0, c.y + r * sp1, c.z + r * cp1 * st0 };
			b3Vec3 l11 = { c.x + r * cp1 * ct1, c.y + r * sp1, c.z + r * cp1 * st1 };

			b3Vec3 n00 = { cp0 * ct0, sp0, cp0 * st0 };
			b3Vec3 n10 = { cp0 * ct1, sp0, cp0 * st1 };
			b3Vec3 n01 = { cp1 * ct0, sp1, cp1 * st0 };
			b3Vec3 n11 = { cp1 * ct1, sp1, cp1 * st1 };

			float wn00[3] = { R[0]*n00.x+R[1]*n00.y+R[2]*n00.z, R[3]*n00.x+R[4]*n00.y+R[5]*n00.z, R[6]*n00.x+R[7]*n00.y+R[8]*n00.z };
			float wn10[3] = { R[0]*n10.x+R[1]*n10.y+R[2]*n10.z, R[3]*n10.x+R[4]*n10.y+R[5]*n10.z, R[6]*n10.x+R[7]*n10.y+R[8]*n10.z };
			float wn01[3] = { R[0]*n01.x+R[1]*n01.y+R[2]*n01.z, R[3]*n01.x+R[4]*n01.y+R[5]*n01.z, R[6]*n01.x+R[7]*n01.y+R[8]*n01.z };
			float wn11[3] = { R[0]*n11.x+R[1]*n11.y+R[2]*n11.z, R[3]*n11.x+R[4]*n11.y+R[5]*n11.z, R[6]*n11.x+R[7]*n11.y+R[8]*n11.z };

			float w00[3] = { p->x+R[0]*l00.x+R[1]*l00.y+R[2]*l00.z, p->y+R[3]*l00.x+R[4]*l00.y+R[5]*l00.z, p->z+R[6]*l00.x+R[7]*l00.y+R[8]*l00.z };
			float w10[3] = { p->x+R[0]*l10.x+R[1]*l10.y+R[2]*l10.z, p->y+R[3]*l10.x+R[4]*l10.y+R[5]*l10.z, p->z+R[6]*l10.x+R[7]*l10.y+R[8]*l10.z };
			float w01[3] = { p->x+R[0]*l01.x+R[1]*l01.y+R[2]*l01.z, p->y+R[3]*l01.x+R[4]*l01.y+R[5]*l01.z, p->z+R[6]*l01.x+R[7]*l01.y+R[8]*l01.z };
			float w11[3] = { p->x+R[0]*l11.x+R[1]*l11.y+R[2]*l11.z, p->y+R[3]*l11.x+R[4]*l11.y+R[5]*l11.z, p->z+R[6]*l11.x+R[7]*l11.y+R[8]*l11.z };

			psp_push_solid_tri( wn00[0],wn00[1],wn00[2], w00[0],w00[1],w00[2],
			                    wn10[0],wn10[1],wn10[2], w10[0],w10[1],w10[2],
			                    wn11[0],wn11[1],wn11[2], w11[0],w11[1],w11[2] );
			psp_push_solid_tri( wn00[0],wn00[1],wn00[2], w00[0],w00[1],w00[2],
			                    wn11[0],wn11[1],wn11[2], w11[0],w11[1],w11[2],
			                    wn01[0],wn01[1],wn01[2], w01[0],w01[1],w01[2] );
		}
	}
}

static void psp_draw_capsule_solid_local( const b3Capsule* cap, const b3Vec3* p, const float R[9] )
{
	b3Sphere s1 = { cap->center1, cap->radius };
	b3Sphere s2 = { cap->center2, cap->radius };
	psp_draw_sphere_solid_local( &s1, p, R );
	psp_draw_sphere_solid_local( &s2, p, R );
}

void psp_host_draw_body_solid( b3BodyId bodyId )
{
	b3WorldTransform tf = b3Body_GetTransform( bodyId );
	b3Vec3 p = tf.p;
	b3Quat q = tf.q;
	float R[9];
	psp_rot_from_quat( &q, R );

	b3ShapeId shapes[ 16 ];
	int n = b3Body_GetShapes( bodyId, shapes, 16 );
	if ( n > 16 ) n = 16;

	for ( int i = 0; i < n; ++i )
	{
		b3ShapeType type = b3Shape_GetType( shapes[i] );
		switch ( type )
		{
			case b3_hullShape:
			{
				const b3HullData* hull = b3Shape_GetHull( shapes[i] );
				if ( hull ) psp_draw_hull_solid( hull, &p, R );
				break;
			}
			case b3_sphereShape:
			{
				b3Sphere s = b3Shape_GetSphere( shapes[i] );
				psp_draw_sphere_solid_local( &s, &p, R );
				break;
			}
			case b3_capsuleShape:
			{
				b3Capsule c = b3Shape_GetCapsule( shapes[i] );
				psp_draw_capsule_solid_local( &c, &p, R );
				break;
			}
			default:
				break;
		}
	}
}

void psp_host_draw_sphere_world( float cx, float cy, float cz, float radius, unsigned int color )
{
	int segments = 10;
	for ( int i = 0; i < segments; ++i )
	{
		float a0 = ( float )i       * ( 2.0f * 3.14159265f / segments );
		float a1 = ( float )( i + 1 ) * ( 2.0f * 3.14159265f / segments );
		float s0, c0, s1, c1;
		vfpu_sincosf( a0, &s0, &c0 );
		vfpu_sincosf( a1, &s1, &c1 );
		psp_push_line( cx + radius * c0, cy + radius * s0, cz,
		               cx + radius * c1, cy + radius * s1, cz, color );
		psp_push_line( cx + radius * c0, cy, cz + radius * s0,
		               cx + radius * c1, cy, cz + radius * s1, color );
		psp_push_line( cx, cy + radius * c0, cz + radius * s0,
		               cx, cy + radius * c1, cz + radius * s1, color );
	}
}

int psp_host_init( void )
{
	memset( &s_host, 0, sizeof( s_host ) );

	s_host.fb0   = vram_alloc( VRAM_FB_SIZE );
	s_host.fb1   = vram_alloc( VRAM_FB_SIZE );
	s_host.depth = vram_alloc( VRAM_DEPTH_SIZE );

	sceDisplaySetMode( 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT );
	sceDisplaySetFrameBuf( vram_abs( s_host.fb1 ), VRAM_FB_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_5551, PSP_DISPLAY_SETBUF_NEXTFRAME );

	sceGuInit();
	sceGuStart( GU_DIRECT, s_gu_list );
	sceGuDrawBuffer( PSP_FB_PIXEL_FMT, s_host.fb0, VRAM_FB_STRIDE );
	sceGuDispBuffer( PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, s_host.fb1, VRAM_FB_STRIDE );
	sceGuDepthBuffer( s_host.depth, VRAM_FB_STRIDE );
	sceGuOffset( 2048 - ( PSP_SCREEN_WIDTH / 2 ), 2048 - ( PSP_SCREEN_HEIGHT / 2 ) );
	sceGuViewport( 2048, 2048, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT );
	sceGuDepthRange( 65535, 0 );
	sceGuScissor( 0, 0, PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT );
	sceGuEnable( GU_SCISSOR_TEST );
	sceGuDepthFunc( GU_GEQUAL );
	sceGuEnable( GU_DEPTH_TEST );
	sceGuFrontFace( GU_CCW );
	sceGuShadeModel( GU_SMOOTH );
	sceGuEnable( GU_CULL_FACE );
	sceGuClear( GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT );
	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );
	sceDisplayWaitVblankStart();
	sceGuDisplay( GU_TRUE );

	sceCtrlSetSamplingCycle( 0 );
	sceCtrlSetSamplingMode( PSP_CTRL_MODE_ANALOG );

	s_host.camera.target    = ( psp_vec3 ){ 0.0f, 1.5f, 0.0f, 0.0f };
	s_host.camera.distance  = 14.0f;
	s_host.camera.yaw       = 0.6f;
	s_host.camera.pitch     = 0.45f;
	s_host.camera.fovY      = 60.0f * 0.01745329f;
	s_host.camera.nearZ     = 0.5f;
	s_host.camera.farZ      = 500.0f;

	s_host.lineVerts  = memalign( 16, sizeof( psp_vertex_colored ) * PSP_LINE_VERT_MAX );
	s_host.solidVerts = memalign( 16, sizeof( psp_vertex_colored ) * PSP_TRI_VERT_MAX );
	if ( !s_host.lineVerts || !s_host.solidVerts )
	{
		return -1;
	}

	s_host.frameIndex = 0;
	s_host.dt  = 1.0f / 60.0f;
	s_host.fps = 60.0f;
	s_host.quit = 0;

	return 0;
}

void psp_host_shutdown( void )
{
	sceGuDisplay( GU_FALSE );
	sceGuTerm();
	if ( s_host.lineVerts )  { free( s_host.lineVerts );  s_host.lineVerts  = NULL; }
	if ( s_host.solidVerts ) { free( s_host.solidVerts ); s_host.solidVerts = NULL; }
}

void psp_host_begin_frame( void )
{
	s_host.prevPad = s_host.pad;
	sceCtrlReadBufferPositive( &s_host.pad, 1 );

	if ( ( s_host.pad.Buttons & PSP_CTRL_START ) && ( s_host.pad.Buttons & PSP_CTRL_SELECT ) )
	{
		s_host.quit = 1;
	}

	psp_camera_update( &s_host.camera, &s_host.pad, &s_host.prevPad, s_host.dt );

	s_host.lineVertCount  = 0;
	s_host.solidVertCount = 0;
}

void psp_host_end_frame( void )
{
	sceGuStart( GU_DIRECT, s_gu_list );
	sceGuClearColor( 0xFF8C8CD0 );
	sceGuClearDepth( 0 );
	sceGuClear( GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT );

	psp_vec3 eye;
	psp_camera_eye( &s_host.camera, &eye );

	ScePspFVector3 eyeV    = { eye.x, eye.y, eye.z };
	ScePspFVector3 targetV = { s_host.camera.target.x, s_host.camera.target.y, s_host.camera.target.z };
	ScePspFVector3 upV     = { 0.0f, 1.0f, 0.0f };

	sceGumMatrixMode( GU_PROJECTION );
	sceGumLoadIdentity();
	sceGumPerspective( s_host.camera.fovY * 57.29578f,
	                   ( float )PSP_SCREEN_WIDTH / ( float )PSP_SCREEN_HEIGHT,
	                   s_host.camera.nearZ, s_host.camera.farZ );

	sceGumMatrixMode( GU_VIEW );
	sceGumLoadIdentity();
	sceGumLookAt( &eyeV, &targetV, &upV );

	sceGumMatrixMode( GU_MODEL );
	sceGumLoadIdentity();

	sceGuEnable( GU_CLIP_PLANES );

	{
		psp_vertex_colored skyVerts[ 4 ] __attribute__( ( aligned( 16 ) ) );
		unsigned int topC = 0xFFD0A050u;
		unsigned int botC = 0xFFC8E0F0u;
		skyVerts[0] = ( psp_vertex_colored ){ topC, 0.0f,   0.0f,   0.0f };
		skyVerts[1] = ( psp_vertex_colored ){ topC, 480.0f, 0.0f,   0.0f };
		skyVerts[2] = ( psp_vertex_colored ){ botC, 0.0f,   272.0f, 0.0f };
		skyVerts[3] = ( psp_vertex_colored ){ botC, 480.0f, 272.0f, 0.0f };

		sceGuDisable( GU_DEPTH_TEST );
		sceGuDisable( GU_LIGHTING );
		sceGuDisable( GU_CULL_FACE );
		sceGuDisable( GU_CLIP_PLANES );
		sceGuDrawArray( GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D,
		                4, 0, skyVerts );
		sceGuEnable( GU_DEPTH_TEST );
		sceGuEnable( GU_CLIP_PLANES );
		sceGuClearDepth( 0 );
		sceGuClear( GU_DEPTH_BUFFER_BIT );
	}

	if ( s_host.solidVertCount > 0 )
	{
		sceGuDisable( GU_TEXTURE_2D );
		sceGuDisable( GU_LIGHTING );
		sceGuDisable( GU_CULL_FACE );
		sceGuShadeModel( GU_SMOOTH );

		sceKernelDcacheWritebackRange( s_host.solidVerts,
			( unsigned int )( sizeof( psp_vertex_colored ) * s_host.solidVertCount ) );

		sceGumDrawArray( GU_TRIANGLES,
		                 GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		                 s_host.solidVertCount, 0, s_host.solidVerts );
	}

	if ( s_host.lineVertCount > 0 )
	{
		sceGuDisable( GU_TEXTURE_2D );
		sceGuDisable( GU_CULL_FACE );
		sceGuDisable( GU_LIGHTING );
		sceGuShadeModel( GU_SMOOTH );

		sceKernelDcacheWritebackRange( s_host.lineVerts,
			( unsigned int )( sizeof( psp_vertex_colored ) * s_host.lineVertCount ) );

		sceGumDrawArray( GU_LINES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_3D,
		                 s_host.lineVertCount, 0, s_host.lineVerts );
	}

	sceGuFinish();
	sceGuSync( GU_SYNC_FINISH, GU_SYNC_WHAT_DONE );

	pspDebugScreenSetBase( ( u32* )vram_abs( s_host.fb0 ) );
	pspDebugScreenSetXY( 1, 1 );
	pspDebugScreenPrintf( "Box3D PSP  | fps %.1f  bodies %d  contacts %d",
	                      s_host.fps, s_host.bodyCount, s_host.contactCount );
	pspDebugScreenSetXY( 1, 2 );
	pspDebugScreenPrintf( "lines %d  solid %d  frame %d",
	                      s_host.lineVertCount, s_host.solidVertCount, s_host.frameIndex );
	pspDebugScreenSetXY( 1, 3 );
	pspDebugScreenPrintf( "analog=look  d-pad=pan  L/R=dolly  X=add  O=reset" );

	if ( s_overlay_fn ) s_overlay_fn();

	void* tmp = s_host.fb0;
	s_host.fb0 = s_host.fb1;
	s_host.fb1 = tmp;
	sceDisplaySetFrameBuf( vram_abs( s_host.fb1 ), VRAM_FB_STRIDE, PSP_DISPLAY_PIXEL_FORMAT_5551, PSP_DISPLAY_SETBUF_NEXTFRAME );
	sceGuDrawBuffer( PSP_FB_PIXEL_FMT, s_host.fb0, VRAM_FB_STRIDE );
	sceGuDispBuffer( PSP_SCREEN_WIDTH, PSP_SCREEN_HEIGHT, s_host.fb1, VRAM_FB_STRIDE );

	sceDisplayWaitVblankStart();

	s_host.frameIndex++;
	if ( s_host.dt > 0.0f )
	{
		s_host.fps = s_host.fps * 0.9f + ( 1.0f / s_host.dt ) * 0.1f;
	}
}

void psp_camera_eye( const psp_camera* cam, psp_vec3* eye )
{
	float cp = vfpu_cosf( cam->pitch );
	eye->x = cam->target.x + cam->distance * cp * vfpu_cosf( cam->yaw );
	eye->y = cam->target.y + cam->distance * vfpu_sinf( cam->pitch );
	eye->z = cam->target.z + cam->distance * cp * vfpu_sinf( cam->yaw );
	eye->_w = 0.0f;
}

void psp_camera_view( const psp_camera* cam, psp_mat4* view )
{
	psp_vec3 eye;
	psp_camera_eye( cam, &eye );
	psp_vec3 up = { 0.0f, 1.0f, 0.0f, 0.0f };
	vfpu_mat4_lookat( view, &eye, &cam->target, &up );
}

void psp_camera_proj( const psp_camera* cam, psp_mat4* proj )
{
	float aspect = ( float )PSP_SCREEN_WIDTH / ( float )PSP_SCREEN_HEIGHT;
	vfpu_mat4_perspective( proj, cam->fovY, aspect, cam->nearZ, cam->farZ );
}

static void psp_analog_to_float( unsigned char v, float* out )
{
	int dv = ( int )v - 128;
	if ( dv > -32 && dv < 32 ) dv = 0;
	*out = ( float )dv / 128.0f;
}

void psp_camera_update( psp_camera* cam, const SceCtrlData* pad, const SceCtrlData* prev, float dt )
{
	( void )prev;

	float ax, ay;
	psp_analog_to_float( pad->Lx, &ax );
	psp_analog_to_float( pad->Ly, &ay );

	float lookSpeed = 1.5f;
	cam->yaw   -= ax * lookSpeed * dt;
	cam->pitch += ay * lookSpeed * dt;
	if ( cam->pitch > 1.55f ) cam->pitch = 1.55f;
	if ( cam->pitch < -1.55f ) cam->pitch = -1.55f;

	if ( pad->Buttons & ( PSP_CTRL_UP | PSP_CTRL_DOWN | PSP_CTRL_LEFT | PSP_CTRL_RIGHT ) )
	{
		float fwdX = -vfpu_sinf( cam->yaw );
		float fwdZ =  vfpu_cosf( cam->yaw );
		float rgtX =  vfpu_cosf( cam->yaw );
		float rgtZ =  vfpu_sinf( cam->yaw );
		float panSpeed = ( pad->Buttons & PSP_CTRL_SQUARE ) ? 8.0f : 3.0f;
		if ( pad->Buttons & PSP_CTRL_UP )    { cam->target.x += fwdX * panSpeed * dt; cam->target.z += fwdZ * panSpeed * dt; }
		if ( pad->Buttons & PSP_CTRL_DOWN )  { cam->target.x -= fwdX * panSpeed * dt; cam->target.z -= fwdZ * panSpeed * dt; }
		if ( pad->Buttons & PSP_CTRL_LEFT )  { cam->target.x -= rgtX * panSpeed * dt; cam->target.z -= rgtZ * panSpeed * dt; }
		if ( pad->Buttons & PSP_CTRL_RIGHT ) { cam->target.x += rgtX * panSpeed * dt; cam->target.z += rgtZ * panSpeed * dt; }
	}

	if ( pad->Buttons & PSP_CTRL_LTRIGGER ) cam->distance += 6.0f * dt;
	if ( pad->Buttons & PSP_CTRL_RTRIGGER ) cam->distance -= 6.0f * dt;
	if ( cam->distance < 2.0f )  cam->distance = 2.0f;
	if ( cam->distance > 60.0f ) cam->distance = 60.0f;
}
