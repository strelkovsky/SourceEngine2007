// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Fast SSE-ONLY ray tracing module. Based upon various "real time ray tracing"
// research.

#ifndef SOURCE_RAYTRACE_RAYTRACE_H_
#define SOURCE_RAYTRACE_RAYTRACE_H_

#include <cassert>
#include "base/include/base_types.h"
#include "bspfile.h"
#include "mathlib/lightdesc.h"
#include "mathlib/mathlib.h"
#include "mathlib/ssemath.h"
#include "mathlib/vector.h"
#include "tier1/utlvector.h"

// #define DEBUG_RAYTRACE 1

class FourRays {
 public:
  FourVectors origin;
  FourVectors direction;

  inline void Check() const {
  // in order to be valid to trace as a group, all four rays must have the
  // same signs in all of their direction components
#ifndef NDEBUG
    for (int c = 1; c < 4; c++) {
      Assert(direction.X(0) * direction.X(c) >= 0);
      Assert(direction.Y(0) * direction.Y(c) >= 0);
      Assert(direction.Z(0) * direction.Z(c) >= 0);
    }
#endif
  }

  // returns direction sign mask for 4 rays. returns -1 if the rays can not be
  // traced as a bundle.
  int CalculateDirectionSignMask() const;
};

// The format a triangle is stored in for intersections. size of this structure
// is important. This structure can be in one of two forms. Before the ray
// tracing environment is set up, the ProjectedEdgeEquations hold the
// coordinates of the 3 vertices, for facilitating bounding box checks needed
// while building the tree. afterwards, they are changed into the projected ege
// equations for intersection purposes.
enum triangleflags {
  FCACHETRI_TRANSPARENT = 0x01,
  FCACHETRI_NEGATIVE_NORMAL = 0x02,
};

struct TriIntersectData_t {
  // This structure is 16longs=64 bytes for cache line packing.
  f32 m_flNx, m_flNy, m_flNz;  // plane equation
  f32 m_flD;

  i32 m_nTriangleID;  // id of the triangle.

  f32 m_ProjectedEdgeEquations[6];  // A,B,C for each edge equation.  a
                                    // point is inside the triangle if
                                    // a*c1+b*c2+c is negative for all 3
                                    // edges.

  u8 m_nCoordSelect0,
      m_nCoordSelect1;  // the triangle is projected onto a 2d
                        // plane for edge testing. These are
                        // the indices (0..2) of the
                        // coordinates preserved in the
                        // projection

  u8 m_nFlags;   // triangle flags
  u8 m_unused0;  // no longer used
};

struct TriGeometryData_t {
  i32 m_nTriangleID;         // id of the triangle.
  f32 m_VertexCoordData[9];  // can't use a vector in a union

  u8 m_nFlags;              // triangle flags
  signed char m_nTmpData0;  // used by kd-tree builder
  signed char m_nTmpData1;  // used by kd-tree builder

  // accessors to get around union annoyance
  SOURCE_FORCEINLINE Vector &Vertex(int idx) {
    return *(reinterpret_cast<Vector *>(m_VertexCoordData + 3 * idx));
  }
};

struct CacheOptimizedTriangle {
  union {
    TriIntersectData_t m_IntersectData;
    TriGeometryData_t m_GeometryData;
  } m_Data;

  // accessors to get around union annoyance
  SOURCE_FORCEINLINE Vector &Vertex(int idx) {
    return *(reinterpret_cast<Vector *>(
        m_Data.m_GeometryData.m_VertexCoordData + 3 * idx));
  }

  SOURCE_FORCEINLINE const Vector &Vertex(int idx) const {
    return *(reinterpret_cast<const Vector *>(
        m_Data.m_GeometryData.m_VertexCoordData + 3 * idx));
  }

  void ChangeIntoIntersectionFormat(void);  // change information storage format
                                            // for computing intersections.

  int ClassifyAgainstAxisSplit(int split_plane,
                               f32 split_value);  // PLANECHECK_xxx below
};

#define PLANECHECK_POSITIVE 1
#define PLANECHECK_NEGATIVE -1
#define PLANECHECK_STRADDLING 0

#define KDNODE_STATE_XSPLIT 0  // this node is an x split
#define KDNODE_STATE_YSPLIT 1  // this node is a ysplit
#define KDNODE_STATE_ZSPLIT 2  // this node is a zsplit
#define KDNODE_STATE_LEAF 3    // this node is a leaf

struct CacheOptimizedKDNode {
  // this is the cache intensive data structure. "Tricks" are used to fit it
  // into 8 bytes:
  //
  // A) the right child is always stored after the left child, which means we
  // only need one pointer B) The type of node (KDNODE_xx) is stored in the
  // lower 2 bits of the pointer. C) for leaf nodes, we store the number of
  // triangles in the leaf in the same place as the floating
  //    point splitting parameter is stored in a non-leaf node

  i32 Children;             // child idx, or'ed with flags above
  f32 SplittingPlaneValue;  // for non-leaf nodes, the nodes on the
                            // "high" side of the splitting plane
                            // are on the right

#ifdef DEBUG_RAYTRACE
  Vector vecMins;
  Vector vecMaxs;
#endif

  inline int NodeType() const { return Children & 3; }

  inline i32 TriangleIndexStart() const {
    assert(NodeType() == KDNODE_STATE_LEAF);
    return Children >> 2;
  }

  inline int LeftChild() const {
    assert(NodeType() != KDNODE_STATE_LEAF);
    return Children >> 2;
  }

  inline int RightChild() const { return LeftChild() + 1; }

  inline int NumberOfTrianglesInLeaf() const {
    assert(NodeType() == KDNODE_STATE_LEAF);
    return *((i32 *)&SplittingPlaneValue);
  }

  inline void SetNumberOfTrianglesInLeafNode(int n) {
    *((i32 *)&SplittingPlaneValue) = n;
  }

 protected:
};

struct RayTracingSingleResult {
  Vector surface_normal;  // surface normal at intersection
  i32 HitID;              // -1=no hit. otherwise, triangle index
  f32 HitDistance;        // distance to intersection
  f32 ray_length;         // leng of initial ray
};

struct RayTracingResult {
  FourVectors surface_normal;  // surface normal at intersection
  alignas(16) i32 HitIds[4];   // -1=no hit. otherwise, triangle index
  fltx4 HitDistance;           // distance to intersection
};

struct RayTraceLight {
  FourVectors Position;
  FourVectors Intensity;
};

#define RTE_FLAGS_FAST_TREE_GENERATION 1
#define RTE_FLAGS_DONT_STORE_TRIANGLE_COLORS 2  // saves memory if not needed
#define RTE_FLAGS_DONT_STORE_TRIANGLE_MATERIALS 4

enum RayTraceLightingMode_t {
  DIRECT_LIGHTING,               // just dot product lighting
  DIRECT_LIGHTING_WITH_SHADOWS,  // with shadows
  GLOBAL_LIGHTING                // global light w/ shadows
};

class RayStream {
  friend class RayTracingEnvironment;

  RayTracingSingleResult *PendingStreamOutputs[8][4];
  int n_in_stream[8];
  FourRays PendingRays[8];

 public:
  RayStream() { memset(n_in_stream, 0, sizeof(n_in_stream)); }
};

// When transparent triangles are in the list, the caller can provide a callback
// that will get called at each triangle allowing the callback to stop
// processing if desired. UNDONE: This is not currently SIMD - it really only
// supports single rays Also for efficiency FourRays really needs some kind of
// active mask for the cases where rays get unbundled
the_interface ITransparentTriangleCallback {
 public:
  virtual bool VisitTriangle_ShouldContinue(
      const TriIntersectData_t &triangle, const FourRays &rays, fltx4 *hitMask,
      fltx4 *b0, fltx4 *b1, fltx4 *b2, i32 hitID) = 0;
};

class RayTracingEnvironment {
 public:
  u32 Flags;  // RTE_FLAGS_xxx above
  Vector m_MinBound;
  Vector m_MaxBound;

  FourVectors BackgroundColor;  //< color where no intersection
  CUtlVector<CacheOptimizedKDNode>
      OptimizedKDTree;  //< the packed kdtree. root is 0
  CUtlBlockVector<CacheOptimizedTriangle>
      OptimizedTriangleList;          //< the packed triangles
  CUtlVector<i32> TriangleIndexList;  //< the list of triangle indices.
  CUtlVector<LightDesc_t> LightList;  //< the list of lights
  CUtlVector<Vector> TriangleColors;  //< color of tries
  CUtlVector<i32> TriangleMaterials;  //< material index of tries

 public:
  RayTracingEnvironment() : OptimizedTriangleList{1024} {
    BackgroundColor.DuplicateVector(Vector{1, 0, 0});  // red
    Flags = 0;
  }

  // call AddTriangle to set up the world
  void AddTriangle(i32 id, const Vector &v1, const Vector &v2, const Vector &v3,
                   const Vector &color);

  // Adds a triangle with flags & material
  void AddTriangle(i32 id, const Vector &v1, const Vector &v2, const Vector &v3,
                   const Vector &color, uint16_t flags, i32 materialIndex);

  void AddQuad(i32 id, const Vector &v1, const Vector &v2, const Vector &v3,
               const Vector &v4,  // specify vertices in cw or ccw order
               const Vector &color);

  // for ease of testing.
  void AddAxisAlignedRectangularSolid(int id, Vector mincoord, Vector Maxcoord,
                                      const Vector &color);

  // SetupAccelerationStructure to prepare for tracing
  void SetupAccelerationStructure(void);

  // lowest level intersection routine - fire 4 rays through the scene. all 4
  // rays must pass the Check() function, and t extents must be initialized.
  // skipid can be set to exclude a particular id (such as the origin surface).
  // This function finds the closest intersection.
  void Trace4Rays(const FourRays &rays, fltx4 TMin, fltx4 TMax,
                  int DirectionSignMask, RayTracingResult *rslt_out,
                  i32 skip_id = -1,
                  ITransparentTriangleCallback *pCallback = NULL);

  // higher level intersection routine that handles computing the mask and
  // handling rays which do not match in direciton sign
  void Trace4Rays(const FourRays &rays, fltx4 TMin, fltx4 TMax,
                  RayTracingResult *rslt_out, i32 skip_id = -1,
                  ITransparentTriangleCallback *pCallback = NULL);

  // compute virtual light sources to model inter-reflection
  void ComputeVirtualLightSources(void);

  // high level interface - pass viewing parameters, rendering flags, and a
  // destination frame buffer, and get a ray traced scene in 32-bit rgba format
  void RenderScene(int width,
                   int height,  // width and height of desired rendering
                   int stride,  // actual width in pixels of target buffer
                   u32 *output_buffer,   // pointer to destination
                   Vector CameraOrigin,  // eye position
                   Vector ULCorner,      // word space coordinates of upper left
                                         // monitor corner
                   Vector URCorner,      // top right corner
                   Vector LLCorner,      // lower left
                   Vector LRCorner,      // lower right
                   RayTraceLightingMode_t lightmode = DIRECT_LIGHTING);

  // raytracing stream - lets you trace an array of rays by feeding them to
  // this function. results will not be returned until FinishStream is called.
  // This function handles sorting the rays by direction, tracing them 4 at a
  // time, and de-interleaving the results.

  void AddToRayStream(RayStream &s, Vector const &start, Vector const &end,
                      RayTracingSingleResult *rslt_out);

  inline void FlushStreamEntry(RayStream &s, int msk);

  // call this when you are done. handles all cleanup. After this is called,
  // all rslt ptrs previously passed to AddToRaySteam will have been filled in.
  void FinishRayStream(RayStream &s);

  int MakeLeafNode(int first_tri, int last_tri);

  f32 CalculateCostsOfSplit(int split_plane, i32 const *tri_list, int ntris,
                            Vector MinBound, Vector MaxBound, f32 &split_value,
                            int &nleft, int &nright, int &nboth);

  void RefineNode(int node_number, i32 const *tri_list, int ntris,
                  Vector MinBound, Vector MaxBound, int depth);

  void CalculateTriangleListBounds(i32 const *tris, int ntris, Vector &minout,
                                   Vector &maxout);

  void AddInfinitePointLight(Vector position,    // light center
                             Vector intensity);  // rgb amount

  // use the global variables set by LoadBSPFile to populated the
  // RayTracingEnvironment with faces.
  void InitializeFromLoadedBSP(void);

  void AddBSPFace(int id, dface_t const &face);

  // MakeRoomForTriangles - a hint telling it how many triangles we are going to
  // add so that the utl vectors used can be pre-allocated
  void MakeRoomForTriangles(int ntriangles);

  const CacheOptimizedTriangle &GetTriangle(i32 triID) const {
    return OptimizedTriangleList[triID];
  }

  i32 GetTriangleMaterial(i32 triID) const { return TriangleMaterials[triID]; }
  const Vector &GetTriangleColor(int triID) const {
    return TriangleColors[triID];
  }
};

#endif  // SOURCE_RAYTRACE_RAYTRACE_H_
