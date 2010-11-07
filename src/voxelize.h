/*
  Module that performs uniform domain division of scene.
*/
#ifndef __VOXELIZE_H
#define __VOXELIZE_H

#include "scene.h"


//// STRUCTURES ///////////////////////////////////////////////

/* Structure that represents single voxel. */
typedef struct _RT_Voxel {
  int32_t nt;       // number of triangles in this voxel
  int32_t p;        // number of free indices left in `t` array (when 0, `t` must be reallocated)
  RT_Triangle **t;  // array of triangle pointers
} RT_Voxel;

/* Structure that groups all voxels in one place. "UDD" stands for "Uniform
 * Domain Division". */
typedef struct _RT_Udd {
  float s[3];     // size of single voxel (x, y, z)
  int32_t nv[3];  // voxel grid size (nv[0]*nv[1]*nv[2] is number of items in `v` array)
  RT_Voxel *v;    // array of voxels mapped from 3D array to 1D array
} RT_Udd;


//// INLINE FUNCTIONS /////////////////////////////////////////

/* Calculate (i,j,k) voxel indices for given vertex `v`. Return 1 if vertex is
 * inside domain or 0 if it is not. */
static inline int rtVertexGetVoxel(
    const RT_Scene *scene, const RT_Udd *udd, float *v,
    int32_t *i, int32_t *j, int32_t *k)
{
  *i = (v[0] - scene->dmin[0]) / udd->s[0];
  *j = (v[1] - scene->dmin[1]) / udd->s[1];
  *k = (v[2] - scene->dmin[2]) / udd->s[2];
  if(*i < 0 || *i >= udd->nv[0]) return 0;
  if(*j < 0 || *j >= udd->nv[1]) return 0;
  if(*k < 0 || *k >= udd->nv[2]) return 0;
  return 1;
}

/* Calculates mapping from 3D coords into 1D array offset value. Used to get 1D
 * index of voxel at position (i,j,k). */
static inline int32_t rtVoxelArrayOffset(const RT_Udd *udd, int32_t i, int32_t j, int32_t k) {
  return (i*udd->nv[1] + j)*udd->nv[2] + k;
}


//// FUNCTIONS ////////////////////////////////////////////////

/* Initializes Uniform Domain Division structure. */
RT_Udd* rtUddCreate(RT_Scene *scene);

/* Releases memory occupied by RT_Udd object. */
void rtUddDestroy(RT_Udd **self);

/* Performs scene voxelization (fills voxels with triangles). */
void rtUddVoxelize(RT_Udd *self, RT_Scene *scene);

/* Calculates indices of startup voxel for given ray origin `o` and normalized
 * ray vector `r`. Returns 1 if ray enters domain or 0 otherwise. */
int rtUddFindStartupVoxel(
  RT_Udd *self, RT_Scene *scene, 
  float *o, float *r, 
  int32_t *i, int32_t *j, int32_t *k
);


#endif

// vim: tabstop=2 shiftwidth=2 softtabstop=2
