#include "voxelize.h"
#include "vectormath.h"
#include "error.h"
#include "common.h"
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>


/* Used to add triangle to given voxel, reallocating memory if needed. Returns
 * 1 on success or 0 on failure (that means memory allocation error). */
static int rtVoxelAddTriangle(RT_Voxel *v, RT_Triangle *t, int32_t bufsize) {
  /* Array has not been allocated yet - do this now. */
  if(!v->t) {
    v->t = malloc(bufsize*sizeof(RT_Triangle*));
    if(!v->t) {
      errno = E_MEMORY;
      return 0;
    }
    memset(v->t, 0, bufsize*sizeof(RT_Triangle*));
    v->p = bufsize;  // number of space left

  /* There is no more place to keep another triangle - reallocate memory by
   * `bufsize` step and make `bufsize` number of place. */
  } else if(v->p == 0) {
    // create new buffer
    RT_Triangle **tmp = malloc((v->nt+bufsize)*sizeof(RT_Triangle*));
    if(!tmp) {
      errno = E_MEMORY;
      return 0;
    }
    memset(tmp, 0, (v->nt+bufsize)*sizeof(RT_Triangle*));

    // copy triangle list to newly allocated buffer and destroy old one
    memcpy(tmp, v->t, v->nt*sizeof(RT_Triangle*));
    free(v->t);

    // update attributes
    v->t = tmp;
    v->p = bufsize;
  }

  /* At this point triangle list is ready to keep another triangle. Add given
   * triangle to list of triangles of given voxel then. */
  v->t[v->nt++] = t;
  v->p--;  // decrease number of space left (if it reaches 0 memory is reallocated)

  return 1;
}


///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
RT_Udd* rtUddCreate(RT_Scene* scene) {
  int k;
  float ds[3], v, tmp;
  RT_Udd *res=NULL;

  // create result object
  res = malloc(sizeof(RT_Udd));
  if(!res) {
    errno = E_MEMORY;
    return NULL;
  }

  // calculate domain size
  for(k=0; k<3; k++) {
    scene->dmin[k] -= 0.001f; 
    scene->dmax[k] += 0.001f;
    ds[k] = scene->dmax[k] - scene->dmin[k] + 0.001;
  }
  RT_DEBUG("domain size: x=%.3f, y=%.3f, z=%.3f", ds[0], ds[1], ds[2]);
  RT_DEBUG("domain size min: x=%.3f, y=%.3f, z=%.3f", scene->dmin[0], scene->dmin[1], scene->dmin[2]);
  RT_DEBUG("domain size max: x=%.3f, y=%.3f, z=%.3f", scene->dmax[0], scene->dmax[1], scene->dmax[2]);
  
  // calculate grid size and size of single element of grid
  v = pow(scene->nt/(ds[0]*ds[1]*ds[2]), 0.33333f) + 0.001f;
  for(k=0; k<3; k++) {
    tmp = ceil(ds[k]*v);  // number of grid elements in k-direction
    res->nv[k] = tmp;
    res->s[k] = ds[k]/tmp;  // size of voxel in k-direction
  }
  RT_DEBUG("number of voxels: i=%d, j=%d, k=%d", res->nv[0], res->nv[1], res->nv[2]);
  RT_DEBUG("total number of voxels: %d", res->nv[0]*res->nv[1]*res->nv[2]);
  RT_DEBUG("total number of triangles: %d", scene->nt);
  RT_DEBUG("size of single voxel: i=%.3f, j=%.3f, k=%.3f", res->s[0], res->s[1], res->s[2]);

  // create voxel grid array
  tmp = res->nv[0] * res->nv[1] * res->nv[2];
  res->v = malloc(tmp*sizeof(RT_Voxel));
  if(!res->v) {
    free(res);
    errno = E_MEMORY;
    return NULL;
  }
  memset(res->v, 0, tmp*sizeof(RT_Voxel));

  return res;
}
///////////////////////////////////////////////////////////////
void rtUddDestroy(RT_Udd **self) {
  RT_Udd *ptr = *self;
  int32_t k, s=ptr->nv[0]*ptr->nv[1]*ptr->nv[2];
  if(ptr->v) {
    for(k=0; k<s; k++) {
      if(ptr->v[k].t) {
        free(ptr->v[k].t);
      }
    }
    free(ptr->v);
  }
  free(ptr);
  *self = NULL;
}
///////////////////////////////////////////////////////////////
void rtUddVoxelize(RT_Udd *self, RT_Scene *scene) {
  const int32_t BUFSIZE = 10;  // number of space to add after each reallocation
  int32_t i, j, k;
  float s1, s2;
  RT_Voxel *vptr=NULL;
  RT_Triangle *t=scene->t, *maxt=(RT_Triangle*)(scene->t + scene->nt), **tmp;

  // make temporary storage for assigned triangles
  tmp = malloc(scene->nt*sizeof(RT_Triangle*));
  if(!tmp) {
    errno = E_MEMORY;
    return;
  }
  memset(tmp, 0, scene->nt*sizeof(RT_Triangle*));
  
  // iterate through array of triangles
  while(t < maxt) {

    // calculate indices of voxels containing current triangle's vertices
    int32_t iidx[3], jidx[3], kidx[3];
    for(k=0; k<3; k++) {
      iidx[k] = (t->i[k] - scene->dmin[k]) / self->s[k];
      jidx[k] = (t->j[k] - scene->dmin[k]) / self->s[k];
      kidx[k] = (t->k[k] - scene->dmin[k]) / self->s[k];
    }
    
    // now calculate minimal and maximal indices of voxels that must be checked
    int32_t min[3], max[3];
    for(k=0; k<3; k++) {
      min[k] = MIN(iidx[k], jidx[k], kidx[k]);
      max[k] = MAX(iidx[k], jidx[k], kidx[k]);
    }

    // if minimal and maximal are equal, triangle is added to exactly one voxel
    if(min[0]==max[0] && min[1]==max[1] && min[2]==max[2]) {
      vptr = (RT_Voxel*)(self->v + rtVoxelArrayOffset(self, min[0], min[1], min[2]));
      rtVoxelAddTriangle(vptr, t, BUFSIZE);
      t++;
      continue;
    }

    //RT_DEBUG("imin=%d, jmin=%d, kmin=%d", min[0], min[1], min[2]);
    //RT_DEBUG("imax=%d, jmax=%d, kmax=%d", max[0], max[1], max[2]);

    // loop through grid array
    for(i=min[0]; i<=max[0]; i++) {
      for(j=min[1]; j<=max[1]; j++) {
        for(k=min[2]; k<=max[2]; k++) {

          /* Triangle is included in the voxel if at least one of following
           * alternatives is true:
           * 1) At least one of its vertices is inside voxel
           * 2) At least one of triangle segments intersects voxel
           * 3) Triangle plane intersects voxel and voxel is inside triangle */
          float x1 = scene->dmin[0] + i*self->s[0];
          float x2 = x1 + self->s[0];
          float y1 = scene->dmin[1] + j*self->s[1];
          float y2 = y1 + self->s[1];
          float z1 = scene->dmin[2] + k*self->s[2];
          float z2 = z1 + self->s[2];

          /* Vertices of current voxel can now be calculated as follows:
           * 1) Bottom vertices:
           *    (x1,y1,z1) - close left
           *    (x2,y1,z1) - close right
           *    (x1,y1,z2) - far left
           *    (x2,y1,z2) - far right 
           * 2) Upper vertices:
           *    Like above, but with y2 instead of y1. */
          RT_Vertex4f bcl={x1, y1, z1, 0.0f}, bcr={x2, y1, z1, 0.0f};
          RT_Vertex4f bfl={x1, y1, x2, 0.0f}, bfr={x2, y1, z2, 0.0f};
          RT_Vertex4f ucl={x1, y2, z1, 0.0f}, ucr={x2, y2, z1, 0.0f};
          RT_Vertex4f ufl={x1, y2, x2, 0.0f}, ufr={x2, y2, z2, 0.0f};
          
          /* Apply voxel's corner nodes to triangle's plane equation and check
           * signs of result. If at least one corner have different sign that
           * any other then triangle's plane intersects voxel. */
          s1 = rtVectorDotp(t->n, bcl) + t->d;
          s2 = rtVectorDotp(t->n, bcr) + t->d;
          if(s1*s2 > 0.0f) {  // if sign for both vertices is the same
            s2 = rtVectorDotp(t->n, bfl) + t->d;
            if(s1*s2 > 0.0f) {
              s2 = rtVectorDotp(t->n, bfr) + t->d;
              if(s1*s2 > 0.0f) {
                s2 = rtVectorDotp(t->n, ucl) + t->d;
                if(s1*s2 > 0.0f) {
                  s2 = rtVectorDotp(t->n, ucr) + t->d;
                  if(s1*s2 > 0.0f) {
                    s2 = rtVectorDotp(t->n, ufl) + t->d;
                    if(s1*s2 > 0.0f) {
                      s2 = rtVectorDotp(t->n, ufr) + t->d;
                      if(s1*s2 > 0.0f) {
                        /* If we are here, all corners of voxel are on the same
                         * side of triangle's plane. That means that current
                         * triangle is not added to vertex. */
                        continue;
                      }
                    }
                  }
                }
              }
            }
          }
          
          //TODO: add more processing to remove as much of voxels as possible

          // add triangle to current voxel
          vptr = (RT_Voxel*)(self->v + rtVoxelArrayOffset(self, i, j, k));
          rtVoxelAddTriangle(vptr, t, BUFSIZE);
        }
      }
    }

    t++;
  }
}
///////////////////////////////////////////////////////////////
int rtUddFindStartupVoxel(
    RT_Udd *self, RT_Scene *scene, 
    float *o, float *r, 
    int32_t *i, int32_t *j, int32_t *k) 
{
  // check if we are already inside domain
  if(rtVertexGetVoxel(scene, self, o, i, j, k))
    return 1;

  // define some more variables
  RT_Vertex4f tmpv;
  float dmin1=FLT_MAX, dmin2=FLT_MAX;
  float d;
  int a;

  /* Calculate distance from ray origin to all walls by solving simplified
   * ray->plane equation. Get two minimal distances (because only two are
   * needed to test whether ray enters domain). */
  for(a=0; a<3; a++) {
    if(r[a] != 0.0f) {
      d = (scene->dmin[a] - o[a]) / r[a];
      if(d > 0.0f) {
        if(d < dmin1) {
          dmin2 = dmin1;
          dmin1 = d;
        } else if(d < dmin2) {
          dmin2 = d;
        }
      }
      d = (scene->dmax[a] - o[a]) / r[a];
      if(d > 0.0f) {
        if(d < dmin1) {
          dmin2 = dmin1;
          dmin1 = d;
        } else if(d < dmin2) {
          dmin2 = d;
        }
      }
    }
  }
  
  /* Calculate intersection point at first minimal distance and check whether
   * it belongs to domain. */
  rtVectorRaypoint(tmpv, o, r, dmin1);
  if(rtVertexGetVoxel(scene, self, tmpv, i, j, k))
    return 1;

  /* Calculate intersection point at second minimal distance - if this check
   * fails, ray is not entering domain. */
  rtVectorRaypoint(tmpv, o, r, dmin2);
  if(rtVertexGetVoxel(scene, self, tmpv, i, j, k))
    return 1;

  return 0;
}
///////////////////////////////////////////////////////////////
RT_Triangle* rtUddTraverse(
  RT_Udd *self, RT_Scene *scene, 
  RT_Triangle *current,
  float *ipoint,
  float *o, float *r, 
  int32_t i, int32_t j, int32_t k)
{
  float dtx1, dtx2, dty1, dty2, dtz1, dtz2;
  float dtx, dty, dtz, tx, ty, tz;
  float tx_n, ty_n, tz_n;
  int32_t di, dj, dk;

  // calculate voxel's planes
  float x1 = scene->dmin[0] + i*self->s[0];
  float x2 = x1 + self->s[0];
  float y1 = scene->dmin[1] + j*self->s[1];
  float y2 = y1 + self->s[1];
  float z1 = scene->dmin[2] + k*self->s[2];
  float z2 = z1 + self->s[2];
  
  // calculate dtx, dty and dtz
  if(r[0] == 0.0f) {
    dtx = FLT_MAX;
    tx = 0.0f;
  } else {
    dtx1=(x1-o[0])/r[0];
    dtx2=(x2-o[0])/r[0];
    dtx = rtAbs(dtx2 - dtx1);
    tx = dtx1<dtx2? dtx1: dtx2;
  }

  if(r[1] == 0.0f) {
    dty = FLT_MAX;
    ty = 0.0f;
  } else {
    dty1=(y1-o[1])/r[1]; 
    dty2=(y2-o[1])/r[1];
    dty = rtAbs(dty2 - dty1);
    ty = dty1<dty2? dty1: dty2;
  }

  if(r[2] == 0.0f) {
    dtz = FLT_MAX;
    tz = 0.0f;
  } else {
    dtz1=(z1-o[2])/r[2]; 
    dtz2=(z2-o[2])/r[2];
    dtz = rtAbs(dtz2 - dtz1);
    tz = dtz1<dtz2? dtz1: dtz2;
  }
  
  // calculate di, dj and dk
  if(r[0] > 0.0f) {
    di = 1;
  } else {
    di = -1;
  }
  if(r[1] > 0.0f) {
    dj = 1;
  } else {
    dj = -1;
  }
  if(r[2] > 0.0f) {
    dk = 1;
  } else {
    dk = -1;
  }

  /* Traverse through grid array. */
  while(1) {
    // check intersections in current voxel
    RT_Voxel *voxel = (RT_Voxel*)(self->v + rtVoxelArrayOffset(self, i, j, k));
    if(voxel->nt > 0) {
      float d, dmin=MIN(tx+dtx, ty+dty, tz+dtz);
      RT_Triangle **t=voxel->t, **maxt=(RT_Triangle**)(voxel->t + voxel->nt);
      RT_Triangle *nearest=NULL;
      while(t < maxt) {
        if(*t != current) {
          if((*t)->isint(*t, o, r, &d, &dmin)) {
            if(d < dmin) {
              dmin = d;
              nearest = *t;
            }
          }
        }
        t++;
      }
      if(nearest) {
        rtVectorRaypoint(ipoint, o, r, dmin); //FIXME: move calculation of intersection point to intersection test function
        return nearest;
      }
    }
    //RT_DEBUG("%d %d %d", i, j, k)

    // proceed to next voxel
    if ((tx_n=tx+dtx) < (ty_n=ty+dty)) {
      if (tx_n < (tz_n=tz+dtz)) { 
        i+=di; tx=tx_n;
      } else {
        k+=dk; tz=tz_n;
      }
    } else {
      if (ty_n < (tz_n=tz+dtz)) {
        j+=dj; ty=ty_n;
      } else {
        k+=dk; tz=tz_n;
      }
    }

    // termination check
    if(i<0 || i>=self->nv[0]) return NULL;
    if(j<0 || j>=self->nv[1]) return NULL;
    if(k<0 || k>=self->nv[2]) return NULL;
  }
}

// vim: tabstop=2 shiftwidth=2 softtabstop=2
