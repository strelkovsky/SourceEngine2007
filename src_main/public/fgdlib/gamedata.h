// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef GAMEDATA_H
#define GAMEDATA_H

#include <fstream>
#include "GDClass.h"
#include "InputOutput.h"
#include "TokenReader.h"
#include "UtlString.h"
#include "UtlVector.h"

class MDkeyvalue;
class GameData;
class KeyValues;

enum TEXTUREFORMAT : int;

typedef void (*GameDataMessageFunc_t)(int level, const char *fmt, ...);

// FGD-based AutoMaterialExclusion data

struct FGDMatExlcusions_s {
  char szDirectory[SOURCE_MAX_PATH];  // Where we store the material exclusion
                               // directories
  bool bUserGenerated;  // If the user specified this ( default:  false -- FGD
                        // defined )
};

// FGD-based AutoVisGroup data

struct FGDVisGroupsBaseClass_s {
  char szClass[SOURCE_MAX_PATH];  // i.e. Scene Logic, Sounds, etc   "Custom\Point
                           // Entities\Lights"
  // CUtlStringList szEntities;  // i.e. func_viscluster
};

struct FGDAutoVisGroups_s {
  char szParent[SOURCE_MAX_PATH];  // i.e. Custom, SFM, etc
  // i.e. Scene Logic, Sounds, etc
  CUtlVector<FGDVisGroupsBaseClass_s> m_Classes;
};

#define MAX_DIRECTORY_SIZE 32

//-----------------------------------------------------------------------------
// Purpose: Contains the set of data that is loaded from a single FGD file.
//-----------------------------------------------------------------------------
class GameData {
 public:
  typedef enum {
    NAME_FIXUP_PREFIX = 0,
    NAME_FIXUP_POSTFIX,
    NAME_FIXUP_NONE
  } TNameFixup;

  GameData();
  ~GameData();

  BOOL Load(const char *pszFilename);

  GDclass *ClassForName(const char *pszName, int *piIndex = NULL);

  void ClearData();

  inline int GetMaxMapCoord(void);
  inline int GetMinMapCoord(void);

  inline int GetClassCount();
  inline GDclass *GetClass(int nIndex);

  GDclass *BeginInstanceRemap(const char *pszClassName,
                              const char *pszInstancePrefix, Vector &Origin,
                              QAngle &Angle);
  bool RemapKeyValue(const char *pszKey, const char *pszInValue,
                     char *pszOutValue, size_t maxOutValueSize, TNameFixup NameFixup);
  bool RemapNameField(const char *pszInValue, char *pszOutValue, size_t maxOutValueSize,
                      TNameFixup NameFixup);
  bool LoadFGDMaterialExclusions(TokenReader &tr);
  bool LoadFGDAutoVisGroups(TokenReader &tr);

  CUtlVector<FGDMatExlcusions_s> m_FGDMaterialExclusions;

  CUtlVector<FGDAutoVisGroups_s> m_FGDAutoVisGroups;

 private:
  bool ParseMapSize(TokenReader &tr);

  CUtlVector<GDclass *> m_Classes;

  int m_nMinMapCoord;  // Min & max map bounds as defined by the FGD.
  int m_nMaxMapCoord;
  // Instance Remapping
  Vector m_InstanceOrigin;    // the origin offset of the instance
  QAngle m_InstanceAngle;     // the rotation of the the instance
  matrix3x4_t m_InstanceMat;  // matrix of the origin and rotation of rendering
  char
      m_InstancePrefix[128];  // the prefix used for the instance name remapping
  GDclass *m_InstanceClass;   // the entity class that is being remapped
};

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int GameData::GetClassCount() { return m_Classes.Count(); }

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline GDclass *GameData::GetClass(int nIndex) {
  if (nIndex >= m_Classes.Count()) return NULL;

  return m_Classes.Element(nIndex);
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GameData::GetMinMapCoord() { return m_nMinMapCoord; }

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
int GameData::GetMaxMapCoord() { return m_nMaxMapCoord; }

void GDSetMessageFunc(GameDataMessageFunc_t pFunc);
bool GDError(TokenReader &tr, const char *error, ...);
bool GDSkipToken(TokenReader &tr, trtoken_t ttexpecting = TOKENNONE,
                 const char *pszExpecting = NULL);
bool GDGetToken(TokenReader &tr, char *pszStore, int nSize,
                trtoken_t ttexpecting = TOKENNONE,
                const char *pszExpecting = NULL);
bool GDGetTokenDynamic(TokenReader &tr, char **pszStore, trtoken_t ttexpecting,
                       const char *pszExpecting = NULL);

#endif  // GAMEDATA_H
