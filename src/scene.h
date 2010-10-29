#ifndef __SCENE_H
#define __SCENE_H

#include "types.h"
#include "imagelib.h"
#include <stdio.h>

//// TYPES ////////////////////////////////////////////////////

typedef float SCN_Vertex[4];

//// ENUMS ////////////////////////////////////////////////////

typedef enum _SCN_ProjectionPlane {
    PP_XOY,
    PP_XOZ,
    PP_ZOY
} SCN_ProjectionPlane;

//// STRUCTURES ///////////////////////////////////////////////

/* Definition of surface. A surface is a group of triangles that have the same
 * properties. 
 
 @attr: R, G, B: surface color (components are normalized: 0..1)
 @attr: kd: diffusion factor (0..1) 
 @attr: ks: mirroring factor (0..1) 
 @attr: g: glitter factor (0..1) 
 @attr: ka: ambient light factor 
 @attr: kt: ??? 
 @attr: eta: refractive index 
 @attr: kr: refraction factor (0..1) */
typedef struct _SCN_Surface {
    IML_Color color;
    float kd, ks, g, ka;
    float kt, eta, kr;
} SCN_Surface;

/* Definition of triangle. 

 @attr: i, j, k: pointers to SCN_Vertex objects that create this triangle 
 @attr: s: pointer to surface represented by this triangle */
typedef struct _SCN_Triangle {
    float i[4], j[4], k[4];  //triangle's vertices (x, y, z, w)
    SCN_Surface *s;  //pointer to surface description of this triangle
    /* speedup & helper attributes (initialized just before raytracing process) */
    int32_t sid;  //surface index (used only to assign `s` pointer while loading surface description)
    float n[4];  // normal vector
    float ij[4], ik[4];  // vectors: i->j, i->k
    #if INT_ALG == 2
    float d;  // d coefficient of triangle's plane equation: (i dotp n) + d = 0
    SCN_ProjectionPlane pplane;
    float ijA, ijB, ijC; // ij line coefficients: Ax+By+C=0
    float jkA, jkB, jkC; // jk line coefficients
    float ikA, ikB, ikC; // ik line coefficients
    float minx, maxx, miny, maxy;
    #endif
} SCN_Triangle;

/* Definition of single light. 

 @attr: x, y, z: light position in scene's space 
 @attr: p: total luminous flux 
 @attr: R, G, B: light color */
typedef struct _SCN_Light {
    float p[4];
    float flux;
    IML_Color color;
} SCN_Light;

/* Groups vertices and triangles in one place creating scene. This object will
 * contain only geometrical definition of scene. Full scene definition
 * requires also lights and surface to be desribed. 
 
 @attr: vsize: number of vertices
 @attr: tsize: number of triangles 
 @attr: v: array of vertices 
 @attr: t: array of triangles */
typedef struct _SCN_Scene {
    int32_t tsize;   //number of triangles in scene
    int32_t lsize;  //number of lights
    int32_t ssize;  //number of surfaces
    SCN_Triangle *t; //array of triangles
    SCN_Light *l;   //array of lights
    SCN_Surface *s;  //array of surfaces
} SCN_Scene;

/* Definition of camera. All coordinates (except screen size) are in scene's
 * space.

 @attr: vx, vy, vz: observer's "eye" location
 @attr: ul_x, ul_y, ul_z: coordinates of screen's upper-left corner 
 @attr: bl_x, bl_y, bl_z: coordinates of screen's bottom-left corner 
 @attr: ur_x, ur_y, ur_z: coordinates of screen's upper-right corner */
typedef struct _SCN_Camera {
    /* observer location */
    float ob[4];
    //float vx, vy, vz;
    /* screen location (in scene's space): ul - upper left, bl - bottom left, ur - upper right */
    float ul[4], bl[4], ur[4];
    /* screen size (resolution) */
    int32_t sw, sh;
} SCN_Camera;

//// FUNCTIONS ////////////////////////////////////////////////

/* Loads scene geometry description from given *.brp file and creates SCN_Scene
 * object.
 
 @param: filename: absolute path to geometry file */
SCN_Scene* scn_scene_load(const char *filename);

/* Loads lights data from given file and fills given given SCN_Scene object
 * with light data. Returns `self` if operation is successfull or NULL
 * otherwise.
 
 @param: self: pointer to SCN_Scene object which must be created by previously
    call to `scn_scene_load` function
 @param: filename: name of file containing lights data */
SCN_Scene* scn_scene_load_lights(SCN_Scene* self, const char *filename);

/* Loads surface data from file and fills SCN_Scene object with surface data.
 * Returns `self` if operation is successfull or NULL otherwise.

 @param: self: pointer to SCN_Scene object, previously created by call to
    `scn_scene_load` function 
 @param: filename: absolute path of file to load */
SCN_Scene* scn_scene_load_surface(SCN_Scene* self, const char *filename);

/* Loads camera configuration from given file. 

 @param: filename: absolute path to camera configuration file */
SCN_Camera* scn_camera_load(const char *filename);

/* Releases memory occupied by camera object `self`. 

 @param: self: pointer to SCN_Camera object previously created by call to
    `scn_camera_load` */
void scn_camera_destroy(SCN_Camera *self);

/* Releases memory occupied by scene object `self`.

 @param: self: pointer to SCN_Scene previously created by call to
    `scn_scene_load` */
void scn_scene_destroy(SCN_Scene *self);

#endif
