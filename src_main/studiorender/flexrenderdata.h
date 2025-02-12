// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef FLEXRENDERDATA_H
#define FLEXRENDERDATA_H

#include "mathlib/vector.h"
#include "studio.h"
#include "tier1/utlvector.h"

struct mstudiomesh_t;

// Used by flex vertex data cache
struct CachedPosNormTan_t {
  Vector m_Position;
  Vector m_Normal;
  Vector4D m_TangentS;

  CachedPosNormTan_t() {}

  CachedPosNormTan_t(CachedPosNormTan_t const& src) {
    VectorCopy(src.m_Position, m_Position);
    VectorCopy(src.m_Normal, m_Normal);
    Vector4DCopy(src.m_TangentS, m_TangentS);
    Assert(m_TangentS.w == 1.0f || m_TangentS.w == -1.0f);
  }
};

// Used by world (decal) vertex data cache
struct CachedPosNorm_t {
  Vector4DAligned m_Position;
  Vector4DAligned m_Normal;

  CachedPosNorm_t() {}

  CachedPosNorm_t(CachedPosNorm_t const& src) {
    Vector4DCopy(src.m_Position, m_Position);
    Vector4DCopy(src.m_Normal, m_Normal);
  }
};

// Stores flex vertex data and world (decal) vertex data for the lifetime of the
// model rendering
class CCachedRenderData {
 public:
  // Constructor
  CCachedRenderData();

  // Call this when we start to render a new model
  void StartModel();

  // Used to hook ourselves into a particular body part, model, and mesh
  void SetBodyPart(int bodypart);
  void SetModel(int model);
  void SetMesh(int mesh);

  // For faster setup in the decal code
  // For faster setup in the decal code
  void SetBodyModelMesh(int body, int model, int mesh) {
    m_Body = body;
    m_Model = model;
    m_Mesh = mesh;

    // At this point, we should have all 3 defined.
    CacheDict_t& dict = m_CacheDict[m_Body][m_Model][m_Mesh];

    if (dict.m_Tag == m_CurrentTag) {
      m_pFirstFlexIndex = &m_pFlexIndex[dict.m_FirstIndex];
      m_pFirstThinFlexIndex = &m_pThinFlexIndex[dict.m_FirstIndex];
      m_pFirstWorldIndex = &m_pWorldIndex[dict.m_FirstIndex];
    } else {
      m_pFirstFlexIndex = 0;
      m_pFirstThinFlexIndex = 0;
      m_pFirstWorldIndex = 0;
    }
  }

  // Used to set up a flex computation
  bool IsFlexComputationDone() const;

  // Used to set up a computation (for world or flex data)
  void SetupComputation(mstudiomesh_t* pMesh, bool flexComputation = false);

  // Is a particular vertex flexed?
  // Checks to see if the vertex is defined
  bool IsVertexFlexed(int vertex) const {
    return (m_pFirstFlexIndex &&
            (m_pFirstFlexIndex[vertex].m_Tag == m_CurrentTag));
  }
  bool IsThinVertexFlexed(int vertex) const {
    return (m_pFirstThinFlexIndex &&
            (m_pFirstThinFlexIndex[vertex].m_Tag == m_CurrentTag));
  }

  // Checks to see if the vertex is defined
  // Checks to see if the vertex is defined
  bool IsVertexPositionCached(int vertex) const {
    return (m_pFirstWorldIndex &&
            (m_pFirstWorldIndex[vertex].m_Tag == m_CurrentTag));
  }

  // Gets a flexed vertex
  // Gets an existing flexed vertex associated with a vertex
  CachedPosNormTan_t* GetFlexVertex(int vertex) {
    Assert(m_pFirstFlexIndex);
    Assert(m_pFirstFlexIndex[vertex].m_Tag == m_CurrentTag);
    return &m_pFlexVerts[m_pFirstFlexIndex[vertex].m_VertexIndex];
  }

  // Gets a flexed vertex
  CachedPosNorm_t* GetThinFlexVertex(int vertex) {
    Assert(m_pFirstThinFlexIndex);
    Assert(m_pFirstThinFlexIndex[vertex].m_Tag == m_CurrentTag);
    return &m_pThinFlexVerts[m_pFirstThinFlexIndex[vertex].m_VertexIndex];
  }

  // Creates a new flexed vertex to be associated with a vertex
  CachedPosNormTan_t* CreateFlexVertex(int vertex);

  // Creates a new flexed vertex to be associated with a vertex
  CachedPosNorm_t* CreateThinFlexVertex(int vertex);

  // Renormalizes the normals and tangents of the flex verts
  void RenormalizeFlexVertices(bool bHasTangentData);

  // Gets a decal vertex
  // Gets an existing world vertex associated with a vertex
  CachedPosNorm_t* GetWorldVertex(int vertex) {
    Assert(m_pFirstWorldIndex);
    Assert(m_pFirstWorldIndex[vertex].m_Tag == m_CurrentTag);
    return &m_pWorldVerts[m_pFirstWorldIndex[vertex].m_VertexIndex];
  }

  // Creates a new decal vertex to be associated with a vertex
  CachedPosNorm_t* CreateWorldVertex(int vertex);

 private:
  // Used to create the flex render data. maps
  struct CacheIndex_t {
    u16 m_Tag;
    u16 m_VertexIndex;
  };

  // A dictionary for the cached data
  struct CacheDict_t {
    u16 m_FirstIndex;
    u16 m_IndexCount;
    u16 m_Tag;
    u16 m_FlexTag;

    CacheDict_t() : m_FirstIndex{0}, m_IndexCount{0}, m_Tag{0}, m_FlexTag{0} {}
  };

  typedef CUtlVector<CacheDict_t> CacheMeshDict_t;
  typedef CUtlVector<CacheMeshDict_t> CacheModelDict_t;
  typedef CUtlVector<CacheModelDict_t> CacheBodyPartDict_t;

  // Flex data, allocated for the lifespan of rendering
  // Can't use UtlVector due to alignment issues
  int m_FlexVertexCount;
  CachedPosNormTan_t m_pFlexVerts[MAXSTUDIOFLEXVERTS + 1];

  // Flex data, allocated for the lifespan of rendering
  // Can't use UtlVector due to alignment issues
  int m_ThinFlexVertexCount;
  CachedPosNorm_t m_pThinFlexVerts[MAXSTUDIOFLEXVERTS + 1];

  // World data, allocated for the lifespan of rendering
  // Can't use UtlVector due to alignment issues
  int m_WorldVertexCount;
  CachedPosNorm_t m_pWorldVerts[MAXSTUDIOVERTS + 1];

  // Maps actual mesh vertices into flex cache + world cache indices
  int m_IndexCount;
  CacheIndex_t m_pFlexIndex[MAXSTUDIOVERTS + 1];
  CacheIndex_t m_pThinFlexIndex[MAXSTUDIOVERTS + 1];
  CacheIndex_t m_pWorldIndex[MAXSTUDIOVERTS + 1];

  CacheBodyPartDict_t m_CacheDict;

  // The flex tag
  u16 m_CurrentTag;

  // the current body, model, and mesh
  int m_Body;
  int m_Model;
  int m_Mesh;

  // mapping for the current mesh to flex data
  CacheIndex_t* m_pFirstFlexIndex;
  CacheIndex_t* m_pFirstThinFlexIndex;
  CacheIndex_t* m_pFirstWorldIndex;
};

#endif  // FLEXRENDERDATA_H
