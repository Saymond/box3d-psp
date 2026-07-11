// SPDX-FileCopyrightText: 2026 Box3D PSP Port
// SPDX-License-Identifier: MIT

#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspgum.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "box3d/box3d.h"
#include "box3d/types.h"

#include "psp_host.h"
#include "vfpu_math.h"

PSP_MODULE_INFO( "Box3D_PSP", 0, 1, 0 );
PSP_MAIN_THREAD_ATTR( THREAD_ATTR_VFPU | THREAD_ATTR_USER );
PSP_MAIN_THREAD_STACK_SIZE_KB( 256 );

static int exit_callback( int arg1, int arg2, void* common )
{
	( void )arg1; ( void )arg2; ( void )common;
	sceKernelExitGame();
	return 0;
}

static int callback_thread( SceSize args, void* argp )
{
	( void )args; ( void )argp;
	int cbid = sceKernelCreateCallback( "Exit Callback", exit_callback, NULL );
	sceKernelRegisterExitCallback( cbid );
	sceKernelSleepThreadCB();
	return 0;
}

static int setup_callbacks( void )
{
	int thid = sceKernelCreateThread( "update_thread", callback_thread, 0x11, 0xFA0, 0, NULL );
	if ( thid >= 0 ) sceKernelStartThread( thid, 0, 0 );
	return thid;
}

typedef enum
{
	SCENE_STACKS = 0,
	SCENE_WALL,
	SCENE_PYRAMID,
	SCENE_CAR,
	SCENE_COUNT
} scene_id;

static const char* SCENE_NAMES[ SCENE_COUNT ] = {
	"STACKS",
	"WALL",
	"PYRAMID",
	"CAR",
};

#define MAX_BODIES 128

typedef struct demo_state
{
	b3WorldId world;
	b3BodyId  ground;
	b3BodyId  bodies[ MAX_BODIES ];
	int       bodyCount;
	int       scene;

	b3BodyId  carBody;
	float     carHeading;
	float     carSpeed;
	int       carSceneInit;
	b3BodyId  terrainTiles[ 64 ];
	int       terrainTileCount;
} demo_state;

static demo_state g_demo;

static float noise_hash( int x, int z )
{
	unsigned int h = ( unsigned int )( x * 374761393 + z * 668265263 );
	h = ( h ^ ( h >> 13 ) ) * 1274126177u;
	h = h ^ ( h >> 16 );
	return ( float )( h & 0xFFFF ) / 65535.0f;
}

static float terrain_height( float x, float z )
{
	float n1 = noise_hash( ( int )( x * 1.5f ), ( int )( z * 1.5f ) ) - 0.5f;
	float n2 = noise_hash( ( int )( x * 3.0f ) + 7, ( int )( z * 3.0f ) + 13 ) - 0.5f;
	return n1 * 0.20f + n2 * 0.08f;
}

static b3WorldId create_world( void )
{
	b3WorldDef worldDef = b3DefaultWorldDef();
	worldDef.gravity     = ( b3Vec3 ){ 0.0f, -10.0f, 0.0f };
	worldDef.workerCount = 1;
	worldDef.enableSleep = true;
	worldDef.enableContinuous = false;
	worldDef.capacity.staticShapeCount  = 256;
	worldDef.capacity.dynamicShapeCount = 512;
	worldDef.capacity.staticBodyCount   = 128;
	worldDef.capacity.dynamicBodyCount  = 128;
	worldDef.capacity.contactCount      = 1024;
	return b3CreateWorld( &worldDef );
}

static b3BodyId add_ground_box( b3WorldId world, float hx, float hy, float hz, b3Vec3 pos )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.position = pos;
	b3BodyId id = b3CreateBody( world, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	b3BoxHull hull = b3MakeBoxHull( hx, hy, hz );
	b3CreateHullShape( id, &shapeDef, &hull.base );
	return id;
}

static b3BodyId add_dynamic_box( b3WorldId world, float hx, float hy, float hz, b3Vec3 pos )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = pos;
	b3BodyId id = b3CreateBody( world, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = 1.0f;
	b3BoxHull hull = b3MakeBoxHull( hx, hy, hz );
	b3CreateHullShape( id, &shapeDef, &hull.base );
	return id;
}

static b3BodyId add_dynamic_sphere( b3WorldId world, float radius, b3Vec3 pos, float density )
{
	b3BodyDef bodyDef = b3DefaultBodyDef();
	bodyDef.type = b3_dynamicBody;
	bodyDef.position = pos;
	b3BodyId id = b3CreateBody( world, &bodyDef );
	b3ShapeDef shapeDef = b3DefaultShapeDef();
	shapeDef.density = density;
	b3Sphere sphere = { { 0.0f, 0.0f, 0.0f }, radius };
	b3CreateSphereShape( id, &shapeDef, &sphere );
	return id;
}

static void track_body( b3BodyId id )
{
	if ( g_demo.bodyCount < MAX_BODIES ) g_demo.bodies[ g_demo.bodyCount++ ] = id;
}

static void scene_stacks_init( void )
{
	g_demo.ground = add_ground_box( g_demo.world, 10.0f, 1.0f, 10.0f,
	                                ( b3Vec3 ){ 0.0f, -1.0f, 0.0f } );
	g_demo.bodyCount = 0;

	for ( int i = 0; i < 5; ++i )
		track_body( add_dynamic_box( g_demo.world, 0.45f, 0.45f, 0.45f,
		                             ( b3Vec3 ){ -3.0f, 0.5f + 1.0f * i, 0.0f } ) );

	for ( int i = 0; i < 5; ++i )
		track_body( add_dynamic_sphere( g_demo.world, 0.45f,
		                                ( b3Vec3 ){ 3.0f, 0.5f + 1.0f * i, 0.0f }, 1.0f ) );

	for ( int row = 0; row < 3; ++row )
	{
		int cols = 3 - row;
		float y = 0.5f + 1.0f * row;
		for ( int c = 0; c < cols; ++c )
		{
			float x = -0.9f * ( cols - 1 ) * 0.5f + 0.9f * c;
			track_body( add_dynamic_box( g_demo.world, 0.4f, 0.4f, 0.4f,
			                             ( b3Vec3 ){ x, y, -3.5f } ) );
		}
	}
}

static void scene_wall_init( void )
{
	g_demo.ground = add_ground_box( g_demo.world, 12.0f, 1.0f, 12.0f,
	                                ( b3Vec3 ){ 0.0f, -1.0f, 0.0f } );
	g_demo.bodyCount = 0;

	const int wallW = 8;
	const int wallH = 6;
	const float brickW = 0.5f;
	const float brickH = 0.25f;
	const float brickD = 0.3f;
	const float gap = 0.02f;

	for ( int row = 0; row < wallH; ++row )
	{
		float y = brickH + ( brickH * 2.0f + gap ) * row;
		float xoff = ( row % 2 ) * ( brickW + gap );
		for ( int c = 0; c < wallW; ++c )
		{
			float x = -wallW * ( brickW + gap ) + ( brickW + gap ) * c + xoff;
			track_body( add_dynamic_box( g_demo.world, brickW, brickH, brickD,
			                             ( b3Vec3 ){ x, y, 0.0f } ) );
		}
	}
}

static void scene_pyramid_init( void )
{
	g_demo.ground = add_ground_box( g_demo.world, 14.0f, 1.0f, 14.0f,
	                                ( b3Vec3 ){ 0.0f, -1.0f, 0.0f } );
	g_demo.bodyCount = 0;

	const float boxH = 0.5f;
	const float gap = 0.005f;
	const float step = boxH * 2.0f + gap;

	for ( int row = 0; row < 5; ++row )
	{
		int cols = 5 - row;
		float y = boxH + step * row;
		for ( int xi = 0; xi < cols; ++xi )
		{
			for ( int zi = 0; zi < cols; ++zi )
			{
				float x = -step * ( cols - 1 ) * 0.5f + step * xi;
				float z = -step * ( cols - 1 ) * 0.5f + step * zi;
				track_body( add_dynamic_box( g_demo.world, boxH, boxH, boxH,
				                             ( b3Vec3 ){ x, y, z } ) );
			}
		}
	}
}

static void scene_car_init( void )
{
	const int gridSize = 8;
	const float tileSize = 1.5f;
	g_demo.terrainTileCount = 0;

	for ( int zi = 0; zi < gridSize; ++zi )
	{
		for ( int xi = 0; xi < gridSize; ++xi )
		{
			float x = -gridSize * tileSize * 0.5f + ( xi + 0.5f ) * tileSize;
			float z = -gridSize * tileSize * 0.5f + ( zi + 0.5f ) * tileSize;
			float h = terrain_height( x, z );
			b3BodyId tile = add_ground_box( g_demo.world,
			                                tileSize * 0.5f, 0.5f, tileSize * 0.5f,
			                                ( b3Vec3 ){ x, h - 0.5f, z } );
			if ( g_demo.terrainTileCount < 64 )
				g_demo.terrainTiles[ g_demo.terrainTileCount++ ] = tile;
		}
	}
	g_demo.ground = g_demo.terrainTiles[0];

	g_demo.bodyCount = 0;

	for ( int i = 0; i < 6; ++i )
	{
		float ang = ( float )i * ( 6.283f / 6.0f );
		float x = cosf( ang ) * 4.0f;
		float z = sinf( ang ) * 4.0f;
		float y = terrain_height( x, z ) + 0.5f;
		track_body( add_dynamic_box( g_demo.world, 0.4f, 0.4f, 0.4f,
		                             ( b3Vec3 ){ x, y, z } ) );
	}

	float startY = terrain_height( 0.0f, 0.0f ) + 0.5f;
	g_demo.carBody = add_dynamic_box( g_demo.world, 0.6f, 0.3f, 1.0f,
	                                  ( b3Vec3 ){ 0.0f, startY, 0.0f } );
	track_body( g_demo.carBody );
	g_demo.carHeading = 0.0f;
	g_demo.carSpeed = 0.0f;
	g_demo.carSceneInit = 1;
}

static void car_update( const SceCtrlData* pad, float dt )
{
	if ( !g_demo.carSceneInit ) return;

	float steer = 0.0f;
	if ( pad->Buttons & PSP_CTRL_LTRIGGER ) steer -= 1.0f;
	if ( pad->Buttons & PSP_CTRL_RTRIGGER ) steer += 1.0f;
	g_demo.carHeading += steer * 2.0f * dt;

	float throttle = 0.0f;
	if ( pad->Buttons & PSP_CTRL_UP    ) throttle += 1.0f;
	if ( pad->Buttons & PSP_CTRL_DOWN  ) throttle -= 1.0f;

	g_demo.carSpeed += throttle * 8.0f * dt;
	g_demo.carSpeed *= ( 1.0f - 1.5f * dt );
	if ( g_demo.carSpeed > 5.0f ) g_demo.carSpeed = 5.0f;
	if ( g_demo.carSpeed < -2.5f ) g_demo.carSpeed = -2.5f;

	float vx = cosf( g_demo.carHeading ) * g_demo.carSpeed;
	float vz = sinf( g_demo.carHeading ) * g_demo.carSpeed;

	b3Vec3 curVel = b3Body_GetLinearVelocity( g_demo.carBody );
	b3Body_SetLinearVelocity( g_demo.carBody, ( b3Vec3 ){ vx, curVel.y, vz } );

	b3Vec3 carPos2 = b3Body_GetPosition( g_demo.carBody );
	float halfH = g_demo.carHeading * 0.5f;
	b3Quat headingQ = { { 0.0f, sinf( halfH ), 0.0f }, cosf( halfH ) };
	b3Body_SetTransform( g_demo.carBody, carPos2, headingQ );

	b3Vec3 carPos = b3Body_GetPosition( g_demo.carBody );
	psp_host* h = psp_host_get();
	h->camera.target.x = carPos.x;
	h->camera.target.y = carPos.y + 1.0f;
	h->camera.target.z = carPos.z;
	h->camera.yaw = g_demo.carHeading + 3.14159f;
	h->camera.pitch = 0.3f;
	h->camera.distance = 6.0f;
}

static void launch_projectile( void )
{
	psp_host* h = psp_host_get();
	psp_vec3 eye;
	psp_camera_eye( &h->camera, &eye );

	b3Vec3 target = { h->camera.target.x, h->camera.target.y, h->camera.target.z };
	b3Vec3 fwd = { target.x - eye.x, target.y - eye.y, target.z - eye.z };
	float len = sqrtf( fwd.x * fwd.x + fwd.y * fwd.y + fwd.z * fwd.z );
	if ( len > 0.001f )
	{
		float inv = 1.0f / len;
		fwd.x *= inv; fwd.y *= inv; fwd.z *= inv;
	}
	else
	{
		fwd = ( b3Vec3 ){ 0.0f, 0.0f, -1.0f };
	}

	b3Vec3 spawn = { eye.x + fwd.x * 0.5f, eye.y + fwd.y * 0.5f, eye.z + fwd.z * 0.5f };
	b3BodyId id = add_dynamic_sphere( g_demo.world, 0.2f, spawn, 5.0f );
	track_body( id );

	float speed = 20.0f;
	b3Body_SetLinearVelocity( id, ( b3Vec3 ){ fwd.x * speed, fwd.y * speed, fwd.z * speed } );
}

static void scene_init( int scene )
{
	if ( g_demo.world.index1 != 0 )
	{
		b3DestroyWorld( g_demo.world );
	}
	g_demo.world = create_world();
	g_demo.bodyCount = 0;
	g_demo.carSceneInit = 0;
	g_demo.terrainTileCount = 0;
	g_demo.scene = scene;

	switch ( scene )
	{
		case SCENE_STACKS:  scene_stacks_init();   break;
		case SCENE_WALL:    scene_wall_init();     break;
		case SCENE_PYRAMID: scene_pyramid_init();  break;
		case SCENE_CAR:     scene_car_init();      break;
	}
}

static void scene_overlay( void )
{
	pspDebugScreenSetXY( 1, 4 );
	pspDebugScreenPrintf( "scene: %s (START=next  SELECT=shoot  O=reset)", SCENE_NAMES[ g_demo.scene ] );
	pspDebugScreenSetXY( 1, 5 );
	if ( g_demo.scene == SCENE_CAR )
	{
		pspDebugScreenPrintf( "CAR: d-pad=drive  L/R=steer  (cam follows)" );
	}
	else
	{
		pspDebugScreenPrintf( "X=drop box  analog=look" );
	}
}

int main( void )
{
	setup_callbacks();
	pspDebugScreenInit();

	if ( psp_host_init() != 0 )
	{
		pspDebugScreenPrintf( "psp_host_init failed\n" );
		sceKernelSleepThread();
		return -1;
	}

	scene_init( SCENE_STACKS );

	psp_host_set_overlay( scene_overlay );

	b3DebugDraw draw = psp_host_make_debug_draw();
	uint64_t debugMask = B3_DEFAULT_MASK_BITS;

	psp_host* h = psp_host_get();

	unsigned long prevTicks = sceKernelGetSystemTimeLow();
	const float fixedDt = 1.0f / 60.0f;

	while ( !h->quit )
	{
		unsigned long nowTicks = sceKernelGetSystemTimeLow();
		float dt = ( float )( nowTicks - prevTicks ) * 1e-6f;
		prevTicks = nowTicks;
		if ( dt > 0.1f ) dt = 0.1f;
		if ( dt <= 0.0f ) dt = fixedDt;
		h->dt = dt;

		unsigned int pressed  = h->pad.Buttons & ~h->prevPad.Buttons;

		if ( pressed & PSP_CTRL_START )
		{
			int next = ( g_demo.scene + 1 ) % SCENE_COUNT;
			scene_init( next );
		}
		if ( pressed & PSP_CTRL_SELECT )
		{
			launch_projectile();
		}
		if ( pressed & PSP_CTRL_CIRCLE )
		{
			scene_init( g_demo.scene );
		}
		if ( pressed & PSP_CTRL_CROSS && g_demo.scene != SCENE_CAR )
		{
			float x = ( float )( ( rand() % 7 ) - 3 );
			float z = ( float )( ( rand() % 7 ) - 3 );
			track_body( add_dynamic_box( g_demo.world, 0.45f, 0.45f, 0.45f,
			                             ( b3Vec3 ){ x, 8.0f, z } ) );
		}

		psp_host_begin_frame();

		if ( g_demo.scene == SCENE_CAR )
		{
			car_update( &h->pad, dt );
		}

		b3World_Step( g_demo.world, fixedDt, 4 );

		b3Counters counters = b3World_GetCounters( g_demo.world );
		h->bodyCount    = counters.bodyCount;
		h->contactCount = counters.contactCount;

		unsigned int groundColor  = 0xFFB09060u;
		unsigned int wallColor    = 0xFFA07050u;
		unsigned int boxColor     = 0xFF60A0F0u;
		unsigned int sphereColor  = 0xFFC06030u;
		unsigned int carColor     = 0xFFE0E050u;
		unsigned int terrainColor = 0xFF7090A0u;

		if ( g_demo.scene == SCENE_CAR )
		{
			psp_host_set_solid_color( terrainColor );
			for ( int i = 0; i < g_demo.terrainTileCount; ++i )
				psp_host_draw_body_solid( g_demo.terrainTiles[i] );

			psp_host_set_solid_color( carColor );
			psp_host_draw_body_solid( g_demo.carBody );

			for ( int i = 0; i < g_demo.bodyCount; ++i )
			{
				if ( g_demo.bodies[i].index1 == g_demo.carBody.index1 ) continue;
				psp_host_set_solid_color( boxColor );
				psp_host_draw_body_solid( g_demo.bodies[i] );
			}
		}
		else
		{
			psp_host_set_solid_color( groundColor );
			psp_host_draw_body_solid( g_demo.ground );

			for ( int i = 0; i < g_demo.bodyCount; ++i )
			{
				b3ShapeId shapes[1];
				int nsh = b3Body_GetShapes( g_demo.bodies[i], shapes, 1 );
				unsigned int c = boxColor;
				if ( nsh > 0 && b3Shape_GetType( shapes[0] ) == b3_sphereShape )
					c = sphereColor;
				if ( g_demo.scene == SCENE_WALL ) c = wallColor;
				psp_host_set_solid_color( c );
				psp_host_draw_body_solid( g_demo.bodies[i] );
			}
		}

		b3World_Draw( g_demo.world, &draw, debugMask );

		psp_host_end_frame();
	}

	if ( g_demo.world.index1 != 0 ) b3DestroyWorld( g_demo.world );
	psp_host_shutdown();
	sceKernelExitGame();
	return 0;
}
