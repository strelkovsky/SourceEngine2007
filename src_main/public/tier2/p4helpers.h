// Copyright � 1996-2018, Valve Corporation, All rights reserved.

#ifndef SOURCE_TIER2_P4HELPERS_H_
#define SOURCE_TIER2_P4HELPERS_H_

#ifdef _WIN32
#pragma once
#endif

#include "base/include/base_types.h"
#include "tier1/smartptr.h"
#include "tier1/utlstring.h"

// Class representing file operations
class CP4File {
 public:
  explicit CP4File(char const *szFilename) : m_sFilename(szFilename) {}
  virtual ~CP4File() {}

 public:
  // Opens the file for edit
  virtual bool Edit();

  // Opens the file for add
  virtual bool Add();

  // Is the file in perforce?
  virtual bool IsFileInPerforce();

 protected:
  // The filename that this class instance represents
  CUtlString m_sFilename;
};

// An override of CP4File performing no Perforce interaction
class CP4File_Dummy : public CP4File {
 public:
  explicit CP4File_Dummy(ch const *szFilename) : CP4File(szFilename) {}

 public:
  bool Edit() override { return true; }
  bool Add() override { return true; }
  bool IsFileInPerforce() override { return false; }
};

// Class representing a factory for creating other helper objects
class CP4Factory {
 public:
  CP4Factory() : m_bDummyMode{false} {}
  ~CP4Factory() {}

 public:
  // Sets whether dummy objects are created by the factory.
  // Returns the old state of the dummy mode.
  bool SetDummyMode(bool bDummyMode);

 public:
  // Sets the name of the changelist to open files under,
  // NULL for "Default" changelist.
  void SetOpenFileChangeList(const ch *szChangeListName);

 public:
  // Creates a file access object for the given filename.
  CP4File *AccessFile(ch const *szFilename) const;

 protected:
  // Whether the factory is in the "dummy mode" and is creating dummy objects
  bool m_bDummyMode;
};

// Default p4 factory
extern CP4Factory *g_p4factory;

// CP4AutoEditFile - edits the file upon construction
class CP4AutoEditFile {
 public:
  explicit CP4AutoEditFile(ch const *szFilename)
      : m_spImpl(g_p4factory->AccessFile(szFilename)) {
    m_spImpl->Edit();
  }

  CP4File *File() const { return m_spImpl.Get(); }

 protected:
  CPlainAutoPtr<CP4File> m_spImpl;
};

// CP4AutoAddFile - adds the file upon construction
class CP4AutoAddFile {
 public:
  explicit CP4AutoAddFile(ch const *szFilename)
      : m_spImpl(g_p4factory->AccessFile(szFilename)) {
    m_spImpl->Add();
  }

  CP4File *File() const { return m_spImpl.Get(); }

 protected:
  CPlainAutoPtr<CP4File> m_spImpl;
};

// CP4AutoEditAddFile - edits the file upon construction / adds upon destruction
class CP4AutoEditAddFile {
 public:
  explicit CP4AutoEditAddFile(ch const *szFilename)
      : m_spImpl(g_p4factory->AccessFile(szFilename)) {
    m_spImpl->Edit();
  }
  ~CP4AutoEditAddFile() { m_spImpl->Add(); }

  CP4File *File() const { return m_spImpl.Get(); }

 protected:
  CPlainAutoPtr<CP4File> m_spImpl;
};

#endif  // SOURCE_TIER2_P4HELPERS_H_
