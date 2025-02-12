// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef ENGINESINGLEUSERFILTER_H
#define ENGINESINGLEUSERFILTER_H

#include "bitvec.h"
#include "const.h"
#include "irecipientfilter.h"
#include "tier1/UtlVector.h"

class CEngineRecipientFilter : public IRecipientFilter {
 public:  // IRecipientFilter interface:
  CEngineRecipientFilter();
  virtual int GetRecipientCount(void) const;
  virtual int GetRecipientIndex(int slot) const;
  virtual bool IsReliable() const { return m_bReliable; };
  virtual bool IsInitMessage() const { return m_bInit; };

 public:
  void Reset(void);

  void MakeInitMessage(void);
  void MakeReliable(void);

  void AddAllPlayers(void);
  void AddRecipientsByPVS(const Vector& origin);
  void AddRecipientsByPAS(const Vector& origin);
  void AddPlayersFromBitMask(CBitVec<ABSOLUTE_PLAYER_LIMIT>& playerbits);
  void AddPlayersFromFilter(const IRecipientFilter* filter);
  void AddRecipient(int playerindex);
  void RemoveRecipient(int playerindex);
  bool IncludesPlayer(int playerindex);

 private:
  bool m_bInit;
  bool m_bReliable;
  CUtlVector<int> m_Recipients;
};

// Purpose: Simple filter for doing MSG_ONE type stuff directly in engine
class CEngineSingleUserFilter : public IRecipientFilter {
 public:
  CEngineSingleUserFilter(int clientindex, bool bReliable = false) {
    m_nClientIndex = clientindex;
    m_bReliable = bReliable;
  }

  virtual bool IsReliable() const { return m_bReliable; }

  virtual int GetRecipientCount() const { return 1; }

  virtual int GetRecipientIndex(int slot) const { return m_nClientIndex; }

  virtual bool IsBroadcastMessage() const { return false; }

  virtual bool IsInitMessage() const { return false; }

 private:
  int m_nClientIndex;
  bool m_bReliable;
};

#endif  // ENGINESINGLEUSERFILTER_H
