// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// NOTE: old_bf_read is guaranteed to return zeros if it overflows.

#ifndef SOURCE_TIER1_BITBUF_H_
#define SOURCE_TIER1_BITBUF_H_

#include "mathlib/mathlib.h"
#include "mathlib/vector.h"
#include "tier0/include/basetypes.h"
#include "tier0/include/dbg.h"
#include "tier0/include/platform.h"

// Forward declarations.
class Vector;
class QAngle;

// You can define a handler function that will be called in case of
// out-of-range values and overruns here.
//
// NOTE: the handler is only called in debug mode.
//
// Call SetBitBufErrorHandler to install a handler.
typedef enum {
  BITBUFERROR_VALUE_OUT_OF_RANGE =
      0,                       // Tried to write a value with too few bits.
  BITBUFERROR_BUFFER_OVERRUN,  // Was about to overrun a buffer.

  BITBUFERROR_NUM_ERRORS
} BitBufErrorType;

typedef void (*BitBufErrorHandler)(BitBufErrorType errorType,
                                   const char *pDebugName);

#if defined(_DEBUG)
extern void InternalBitBufErrorHandler(BitBufErrorType errorType,
                                       const char *pDebugName);
#define CallErrorHandler(errorType, pDebugName) \
  InternalBitBufErrorHandler(errorType, pDebugName);
#else
#define CallErrorHandler(errorType, pDebugName)
#endif

// Use this to install the error handler. Call with NULL to uninstall your error
// handler.
void SetBitBufErrorHandler(BitBufErrorHandler fn);

// Helpers.

inline int BitByte(int bits) {
  // return SOURCE_PAD_NUMBER( bits, 8 ) >> 3;
  return (bits + 7) >> 3;
}

// Used for serialization

class old_bf_write {
 public:
  old_bf_write();

  // nMaxBits can be used as the number of bits in the buffer.
  // It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
  old_bf_write(void *pData, int nBytes, int nMaxBits = -1);
  old_bf_write(const char *pDebugName, void *pData, int nBytes,
               int nMaxBits = -1);

  // Start writing to the specified buffer.
  // nMaxBits can be used as the number of bits in the buffer.
  // It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
  void StartWriting(void *pData, int nBytes, int iStartBit = 0,
                    int nMaxBits = -1);

  // Restart buffer writing.
  void Reset();

  // Get the base pointer.
  unsigned char *GetBasePointer() { return m_pData; }

  // Enable or disable assertion on overflow. 99% of the time, it's a bug that
  // we need to catch, but there may be the occasional buffer that is allowed to
  // overflow gracefully.
  void SetAssertOnOverflow(bool bAssert);

  // This can be set to assign a name that gets output if the buffer overflows.
  const char *GetDebugName();
  void SetDebugName(const char *pDebugName);

  // Seek to a specific position.
 public:
  void SeekToBit(int bitPos);

  // Bit functions.
 public:
  void WriteOneBit(int nValue);
  void WriteOneBitNoCheck(int nValue);
  void WriteOneBitAt(int iBit, int nValue);

  // Write signed or unsigned. Range is only checked in debug.
  void WriteUBitLong(unsigned int data, int numbits, bool bCheckRange = true);
  void WriteSBitLong(int data, int numbits);

  // Tell it whether or not the data is unsigned. If it's signed,
  // cast to unsigned before passing in (it will cast back inside).
  void WriteBitLong(unsigned int data, int numbits, bool bSigned);

  // Write a list of bits in.
  bool WriteBits(const void *pIn, int nBits);

  // writes an unsigned integer with variable bit length
  void WriteUBitVar(unsigned int data);

  // Copy the bits straight out of pIn. This seeks pIn forward by nBits.
  // Returns an error if this buffer or the read buffer overflows.
  bool WriteBitsFromBuffer(class bf_read *pIn, int nBits);

  void WriteBitAngle(float fAngle, int numbits);
  void WriteBitCoord(const float f);
  void WriteBitCoordMP(const float f, bool bIntegral, bool bLowPrecision);
  void WriteBitFloat(float val);
  void WriteBitVec3Coord(const Vector &fa);
  void WriteBitNormal(float f);
  void WriteBitVec3Normal(const Vector &fa);
  void WriteBitAngles(const QAngle &fa);

  // Byte functions.
 public:
  void WriteChar(int val);
  void WriteByte(int val);
  void WriteShort(int val);
  void WriteWord(int val);
  void WriteLong(long val);
  void WriteLongLong(int64_t val);
  void WriteFloat(float val);
  bool WriteBytes(const void *pBuf, int nBytes);

  // Returns false if it overflows the buffer.
  bool WriteString(const char *pStr);

  // Status.
 public:
  // How many bytes are filled in?
  int GetNumBytesWritten();
  int GetNumBitsWritten();
  int GetMaxNumBits();
  int GetNumBitsLeft();
  int GetNumBytesLeft();
  unsigned char *GetData();

  // Has the buffer overflowed?
  bool CheckForOverflow(int nBits);
  inline bool IsOverflowed() const { return m_bOverflow; }

  inline void SetOverflowFlag();

 public:
  // The current buffer.
  unsigned char *m_pData;
  int m_nDataBytes;
  int m_nDataBits;

  // Where we are in the buffer.
  int m_iCurBit;

 private:
  // Errors?
  bool m_bOverflow;

  bool m_bAssertOnOverflow;
  const char *m_pDebugName;
};

// Inlined methods

// How many bytes are filled in?
inline int old_bf_write::GetNumBytesWritten() { return BitByte(m_iCurBit); }

inline int old_bf_write::GetNumBitsWritten() { return m_iCurBit; }

inline int old_bf_write::GetMaxNumBits() { return m_nDataBits; }

inline int old_bf_write::GetNumBitsLeft() { return m_nDataBits - m_iCurBit; }

inline int old_bf_write::GetNumBytesLeft() { return GetNumBitsLeft() >> 3; }

inline unsigned char *old_bf_write::GetData() { return m_pData; }

inline bool old_bf_write::CheckForOverflow(int nBits) {
  if (m_iCurBit + nBits > m_nDataBits) {
    SetOverflowFlag();
    CallErrorHandler(BITBUFERROR_BUFFER_OVERRUN, GetDebugName());
  }

  return m_bOverflow;
}

inline void old_bf_write::SetOverflowFlag() {
  if (m_bAssertOnOverflow) {
    Assert(false);
  }

  m_bOverflow = true;
}

inline void old_bf_write::WriteOneBitNoCheck(int nValue) {
  if (nValue)
    m_pData[m_iCurBit >> 3] |= (1 << (m_iCurBit & 7));
  else
    m_pData[m_iCurBit >> 3] &= ~(1 << (m_iCurBit & 7));

  ++m_iCurBit;
}

inline void old_bf_write::WriteOneBit(int nValue) {
  if (!CheckForOverflow(1)) WriteOneBitNoCheck(nValue);
}

inline void old_bf_write::WriteOneBitAt(int iBit, int nValue) {
  if (iBit + 1 > m_nDataBits) {
    SetOverflowFlag();
    CallErrorHandler(BITBUFERROR_BUFFER_OVERRUN, GetDebugName());
    return;
  }

  if (nValue)
    m_pData[iBit >> 3] |= (1 << (iBit & 7));
  else
    m_pData[iBit >> 3] &= ~(1 << (iBit & 7));
}

inline void old_bf_write::WriteUBitLong(unsigned int curData, int numbits,
                                        bool bCheckRange) {
#ifdef _DEBUG
  // Make sure it doesn't overflow.
  if (bCheckRange && numbits < 32) {
    if (curData >= (unsigned long)(1 << numbits)) {
      CallErrorHandler(BITBUFERROR_VALUE_OUT_OF_RANGE, GetDebugName());
    }
  }
  Assert(numbits >= 0 && numbits <= 32);
#endif

  extern unsigned long g_BitWriteMasks[32][33];

  // Bounds checking..
  if ((m_iCurBit + numbits) > m_nDataBits) {
    m_iCurBit = m_nDataBits;
    SetOverflowFlag();
    CallErrorHandler(BITBUFERROR_BUFFER_OVERRUN, GetDebugName());
    return;
  }

  int nBitsLeft = numbits;
  int iCurBit = m_iCurBit;

  // Mask in a dword.
  unsigned int iDWord = iCurBit >> 5;
  Assert((iDWord * 4 + sizeof(long)) <= (unsigned int)m_nDataBytes);

  unsigned long iCurBitMasked = iCurBit & 31;

  unsigned long dword = LoadLittleDWord((unsigned long *)m_pData, iDWord);

  dword &= g_BitWriteMasks[iCurBitMasked][nBitsLeft];
  dword |= curData << iCurBitMasked;

  // write to stream (lsb to msb ) properly
  StoreLittleDWord((unsigned long *)m_pData, iDWord, dword);

  // Did it span a dword?
  int nBitsWritten = 32 - iCurBitMasked;
  if (nBitsWritten < nBitsLeft) {
    nBitsLeft -= nBitsWritten;
    curData >>= nBitsWritten;

    // read from stream (lsb to msb) properly
    dword = LoadLittleDWord((unsigned long *)m_pData, iDWord + 1);

    dword &= g_BitWriteMasks[0][nBitsLeft];
    dword |= curData;

    // write to stream (lsb to msb) properly
    StoreLittleDWord((unsigned long *)m_pData, iDWord + 1, dword);
  }

  m_iCurBit += numbits;
}

// This is useful if you just want a buffer to write into on the stack.

template <int SIZE>
class old_bf_write_static : public old_bf_write {
 public:
  inline old_bf_write_static() : old_bf_write(m_StaticData, SIZE) {}

  char m_StaticData[SIZE];
};

// Used for unserialization

class old_bf_read {
 public:
  old_bf_read();

  // nMaxBits can be used as the number of bits in the buffer.
  // It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
  old_bf_read(const void *pData, int nBytes, int nBits = -1);
  old_bf_read(const char *pDebugName, const void *pData, int nBytes,
              int nBits = -1);

  // Start reading from the specified buffer.
  // pData's start address must be dword-aligned.
  // nMaxBits can be used as the number of bits in the buffer.
  // It must be <= nBytes*8. If you leave it at -1, then it's set to nBytes * 8.
  void StartReading(const void *pData, int nBytes, int iStartBit = 0,
                    int nBits = -1);

  // Restart buffer reading.
  void Reset();

  // Enable or disable assertion on overflow. 99% of the time, it's a bug that
  // we need to catch, but there may be the occasional buffer that is allowed to
  // overflow gracefully.
  void SetAssertOnOverflow(bool bAssert);

  // This can be set to assign a name that gets output if the buffer overflows.
  const char *GetDebugName();
  void SetDebugName(const char *pName);

  void ExciseBits(int startbit, int bitstoremove);

  // Bit functions.
 public:
  // Returns 0 or 1.
  int ReadOneBit();

 protected:
  unsigned int CheckReadUBitLong(int numbits);  // For debugging.
  int ReadOneBitNoCheck();  // Faster version, doesn't check bounds and is
                            // inlined.
  bool CheckForOverflow(int nBits);

 public:
  // Get the base pointer.
  const unsigned char *GetBasePointer() { return m_pData; }

  SOURCE_FORCEINLINE int TotalBytesAvailable(void) const {
    return m_nDataBytes;
  }

  // Read a list of bits in..
  void ReadBits(void *pOut, int nBits);

  float ReadBitAngle(int numbits);

  unsigned int ReadUBitLong(int numbits);
  unsigned int PeekUBitLong(int numbits);
  int ReadSBitLong(int numbits);

  // reads an unsigned integer with variable bit length
  unsigned int ReadUBitVar();

  // You can read signed or unsigned data with this, just cast to
  // a signed int if necessary.
  unsigned int ReadBitLong(int numbits, bool bSigned);

  float ReadBitCoord();
  float ReadBitCoordMP(bool bIntegral, bool bLowPrecision);
  float ReadBitFloat();
  float ReadBitNormal();
  void ReadBitVec3Coord(Vector &fa);
  void ReadBitVec3Normal(Vector &fa);
  void ReadBitAngles(QAngle &fa);

  // Byte functions (these still read data in bit-by-bit).
 public:
  int ReadChar();
  int ReadByte();
  int ReadShort();
  int ReadWord();
  long ReadLong();
  int64_t ReadLongLong();
  float ReadFloat();
  bool ReadBytes(void *pOut, int nBytes);

  // Returns false if bufLen isn't large enough to hold the
  // string in the buffer.
  //
  // Always reads to the end of the string (so you can read the
  // next piece of data waiting).
  //
  // If bLine is true, it stops when it reaches a '\n' or a 0-terminator.
  //
  // pStr is always 0-terminated (unless bufLen is 0).
  //
  // pOutNumChars is set to the number of characters left in pStr when the
  // routine is complete (this will never exceed bufLen-1).
  //
  bool ReadString(char *pStr, int bufLen, bool bLine = false,
                  int *pOutNumChars = NULL);

  // Reads a string and allocates memory for it. If the string in the buffer
  // is > 2048 bytes, then pOverflow is set to true (if it's not NULL).
  char *ReadAndAllocateString(bool *pOverflow = 0);

  // Status.
 public:
  int GetNumBytesLeft();
  int GetNumBytesRead();
  int GetNumBitsLeft();
  int GetNumBitsRead() const;

  // Has the buffer overflowed?
  inline bool IsOverflowed() const { return m_bOverflow; }

  inline bool Seek(int iBit);  // Seek to a specific bit.
  inline bool SeekRelative(
      int iBitDelta);  // Seek to an offset from the current position.

  // Called when the buffer is overflowed.
  inline void SetOverflowFlag();

 public:
  // The current buffer.
  const unsigned char *m_pData;
  int m_nDataBytes;
  int m_nDataBits;

  // Where we are in the buffer.
  int m_iCurBit;

 private:
  // used by varbit reads internally
  inline int CountRunOfZeros();

  // Errors?
  bool m_bOverflow;

  // For debugging..
  bool m_bAssertOnOverflow;

  const char *m_pDebugName;
};

// Inlines.

inline int old_bf_read::GetNumBytesRead() { return BitByte(m_iCurBit); }

inline int old_bf_read::GetNumBitsLeft() { return m_nDataBits - m_iCurBit; }

inline int old_bf_read::GetNumBytesLeft() { return GetNumBitsLeft() >> 3; }

inline int old_bf_read::GetNumBitsRead() const { return m_iCurBit; }

inline void old_bf_read::SetOverflowFlag() {
  if (m_bAssertOnOverflow) {
    Assert(false);
  }

  m_bOverflow = true;
}

inline bool old_bf_read::Seek(int iBit) {
  if (iBit < 0 || iBit > m_nDataBits) {
    SetOverflowFlag();
    m_iCurBit = m_nDataBits;
    return false;
  } else {
    m_iCurBit = iBit;
    return true;
  }
}

// Seek to an offset from the current position.
inline bool old_bf_read::SeekRelative(int iBitDelta) {
  return Seek(m_iCurBit + iBitDelta);
}

inline bool old_bf_read::CheckForOverflow(int nBits) {
  if (m_iCurBit + nBits > m_nDataBits) {
    SetOverflowFlag();
    CallErrorHandler(BITBUFERROR_BUFFER_OVERRUN, GetDebugName());
  }

  return m_bOverflow;
}

inline int old_bf_read::ReadOneBitNoCheck() {
  int value = m_pData[m_iCurBit >> 3] & (1 << (m_iCurBit & 7));
  ++m_iCurBit;
  return !!value;
}

inline int old_bf_read::ReadOneBit() {
  return (!CheckForOverflow(1)) ? ReadOneBitNoCheck() : 0;
}

inline float old_bf_read::ReadBitFloat() {
  long val;

  static_assert(sizeof(float) == sizeof(long));
  static_assert(sizeof(float) == 4);

  if (CheckForOverflow(32)) return 0.0f;

  int bit = m_iCurBit & 0x7;
  int byte = m_iCurBit >> 3;
  val = m_pData[byte] >> bit;
  val |= ((int)m_pData[byte + 1]) << (8 - bit);
  val |= ((int)m_pData[byte + 2]) << (16 - bit);
  val |= ((int)m_pData[byte + 3]) << (24 - bit);
  if (bit != 0) val |= ((int)m_pData[byte + 4]) << (32 - bit);
  m_iCurBit += 32;
  return *((float *)&val);
}

inline unsigned int old_bf_read::ReadUBitLong(int numbits) {
  extern unsigned long g_ExtraMasks[32];

  if ((m_iCurBit + numbits) > m_nDataBits) {
    m_iCurBit = m_nDataBits;
    SetOverflowFlag();
    return 0;
  }

  Assert(numbits > 0 && numbits <= 32);

  // Read the current dword.
  int idword1 = m_iCurBit >> 5;
  unsigned int dword1 = LoadLittleDWord((unsigned long *)m_pData, idword1);

  dword1 >>= (m_iCurBit & 31);  // Get the bits we're interested in.

  m_iCurBit += numbits;
  unsigned int ret = dword1;

  // Does it span this dword?
  if ((m_iCurBit - 1) >> 5 == idword1) {
    if (numbits != 32) ret &= g_ExtraMasks[numbits];
  } else {
    int nExtraBits = m_iCurBit & 31;
    unsigned int dword2 =
        LoadLittleDWord((unsigned long *)m_pData, idword1 + 1);

    dword2 &= g_ExtraMasks[nExtraBits];

    // No need to mask since we hit the end of the dword.
    // Shift the second dword's part into the high bits.
    ret |= (dword2 << (numbits - nExtraBits));
  }

  return ret;
}

class CBitBuffer {
 public:
  char const *m_pDebugName;
  bool m_bOverflow;
  int m_nDataBits;
  size_t m_nDataBytes;

  void SetDebugName(char const *pName) { m_pDebugName = pName; }

  CBitBuffer() {
    m_bOverflow = false;
    m_pDebugName = NULL;
    m_nDataBits = -1;
    m_nDataBytes = 0;
  }

  SOURCE_FORCEINLINE void SetOverflowFlag() { m_bOverflow = true; }

  SOURCE_FORCEINLINE bool IsOverflowed(void) const { return m_bOverflow; }

  static const uint32_t s_nMaskTable[33];  // 0 1 3 7 15 ..
};

class CBitWrite : public CBitBuffer {
  uint32_t m_nOutBufWord;
  int m_nOutBitsAvail;
  uint32_t *m_pDataOut;
  uint32_t *m_pBufferEnd;
  uint32_t *m_pData;
  bool m_bFlushed;

 public:
  void StartWriting(void *pData, int nBytes, int iStartBit = 0,
                    int nMaxBits = -1);

  CBitWrite(void *pData, int nBytes, int nBits = -1) {
    m_bFlushed = false;
    StartWriting(pData, nBytes, 0, nBits);
  }

  CBitWrite(const char *pDebugName, void *pData, int nBytes, int nBits = -1) {
    m_bFlushed = false;
    SetDebugName(pDebugName);
    StartWriting(pData, nBytes, 0, nBits);
  }

  CBitWrite() { m_bFlushed = false; }

  ~CBitWrite() {
    TempFlush();
    Assert((!m_pData) || m_bFlushed);
  }
  SOURCE_FORCEINLINE int GetNumBitsLeft(void) const {
    return m_nOutBitsAvail + (32 * (m_pBufferEnd - m_pDataOut - 1));
  }

  SOURCE_FORCEINLINE void Reset() {
    m_bOverflow = false;
    m_nOutBitsAvail = 32;
    m_pDataOut = m_pData;
    m_nOutBufWord = 0;
  }

  SOURCE_FORCEINLINE void TempFlush() {
    // someone wants to know how much data we have written, or the pointer to
    // it, so we'd better make sure we write our data
    if (m_nOutBitsAvail != 32) {
      if (m_pDataOut == m_pBufferEnd) {
        SetOverflowFlag();
      } else {
        *(m_pDataOut) =
            (*m_pDataOut & ~s_nMaskTable[32 - m_nOutBitsAvail]) | m_nOutBufWord;
        m_bFlushed = true;
      }
    }
  }

  SOURCE_FORCEINLINE unsigned char *GetBasePointer() {
    TempFlush();
    return reinterpret_cast<unsigned char *>(m_pData);
  }

  SOURCE_FORCEINLINE unsigned char *GetData() { return GetBasePointer(); }

  SOURCE_FORCEINLINE void Finish();
  SOURCE_FORCEINLINE void Flush();
  SOURCE_FORCEINLINE void FlushNoCheck();
  SOURCE_FORCEINLINE void WriteOneBit(int nValue);
  SOURCE_FORCEINLINE void WriteOneBitNoCheck(int nValue);
  SOURCE_FORCEINLINE void WriteUBitLong(unsigned int data, int numbits,
                                        bool bCheckRange = true);
  SOURCE_FORCEINLINE void WriteSBitLong(int data, int numbits);
  SOURCE_FORCEINLINE void WriteUBitVar(unsigned int data);
  SOURCE_FORCEINLINE void WriteBitFloat(float flValue);
  SOURCE_FORCEINLINE void WriteFloat(float flValue);
  bool WriteBits(const void *pInData, int nBits);
  void WriteBytes(const void *pBuf, int nBytes);
  void SeekToBit(int nSeekPos);

  SOURCE_FORCEINLINE int GetNumBitsWritten(void) const {
    return (32 - m_nOutBitsAvail) + (32 * (m_pDataOut - m_pData));
  }

  SOURCE_FORCEINLINE int GetNumBytesWritten(void) const {
    return (GetNumBitsWritten() + 7) >> 3;
  }

  SOURCE_FORCEINLINE void WriteLong(long val) { WriteSBitLong(val, 32); }

  SOURCE_FORCEINLINE void WriteChar(int val) {
    WriteSBitLong(val, sizeof(char) << 3);
  }

  SOURCE_FORCEINLINE void WriteByte(int val) {
    WriteUBitLong(val, sizeof(unsigned char) << 3, false);
  }

  SOURCE_FORCEINLINE void WriteShort(int val) {
    WriteSBitLong(val, sizeof(short) << 3);
  }

  SOURCE_FORCEINLINE void WriteWord(int val) {
    WriteUBitLong(val, sizeof(unsigned short) << 3);
  }

  bool WriteString(const char *pStr);

  void WriteLongLong(int64_t val);

  void WriteBitAngle(float fAngle, int numbits);
  void WriteBitCoord(const float f);
  void WriteBitCoordMP(const float f, bool bIntegral, bool bLowPrecision);
  void WriteBitVec3Coord(const Vector &fa);
  void WriteBitNormal(float f);
  void WriteBitVec3Normal(const Vector &fa);
  void WriteBitAngles(const QAngle &fa);

  // Copy the bits straight out of pIn. This seeks pIn forward by nBits.
  // Returns an error if this buffer or the read buffer overflows.
  bool WriteBitsFromBuffer(class bf_read *pIn, int nBits);
};

void CBitWrite::Finish() {
  if (m_nOutBitsAvail != 32) {
    if (m_pDataOut == m_pBufferEnd) {
      SetOverflowFlag();
    }
    *(m_pDataOut) = m_nOutBufWord;
  }
}

void CBitWrite::FlushNoCheck() {
  *(m_pDataOut++) = m_nOutBufWord;
  m_nOutBitsAvail = 32;
  m_nOutBufWord =
      0;  // ugh - I need this because of 32 bit writes. a<<=32 is a nop
}
void CBitWrite::Flush() {
  if (m_pDataOut == m_pBufferEnd) {
    SetOverflowFlag();
  } else
    *(m_pDataOut++) = m_nOutBufWord;
  m_nOutBufWord =
      0;  // ugh - I need this because of 32 bit writes. a<<=32 is a nop
  m_nOutBitsAvail = 32;
}
void CBitWrite::WriteOneBitNoCheck(int nValue) {
  m_nOutBufWord |= (nValue & 1) << (32 - m_nOutBitsAvail);
  if (--m_nOutBitsAvail == 0) {
    FlushNoCheck();
  }
}

void CBitWrite::WriteOneBit(int nValue) {
  m_nOutBufWord |= (nValue & 1) << (32 - m_nOutBitsAvail);
  if (--m_nOutBitsAvail == 0) {
    Flush();
  }
}

SOURCE_FORCEINLINE void CBitWrite::WriteUBitLong(unsigned int nData,
                                                 int nNumBits,
                                                 bool bCheckRange) {
#ifdef _DEBUG
  // Make sure it doesn't overflow.
  if (bCheckRange && nNumBits < 32) {
    Assert(nData <= (unsigned long)(1 << nNumBits));
  }
  Assert(nNumBits >= 0 && nNumBits <= 32);
#endif
  if (nNumBits <= m_nOutBitsAvail) {
    if (bCheckRange)
      m_nOutBufWord |= (nData) << (32 - m_nOutBitsAvail);
    else
      m_nOutBufWord |= (nData & s_nMaskTable[nNumBits])
                       << (32 - m_nOutBitsAvail);
    m_nOutBitsAvail -= nNumBits;
    if (m_nOutBitsAvail == 0) {
      Flush();
    }
  } else {
    // split dwords case
    int nOverflowBits = (nNumBits - m_nOutBitsAvail);
    m_nOutBufWord |= (nData & s_nMaskTable[m_nOutBitsAvail])
                     << (32 - m_nOutBitsAvail);
    Flush();
    m_nOutBufWord = (nData >> (nNumBits - nOverflowBits));
    m_nOutBitsAvail = 32 - nOverflowBits;
  }
}

SOURCE_FORCEINLINE void CBitWrite::WriteSBitLong(int nData, int nNumBits) {
  WriteUBitLong((uint32_t)nData, nNumBits, false);
}

SOURCE_FORCEINLINE void CBitWrite::WriteUBitVar(unsigned int data) {
  if ((data & 0xf) == data) {
    WriteUBitLong(0, 2);
    WriteUBitLong(data, 4);
  } else {
    if ((data & 0xff) == data) {
      WriteUBitLong(1, 2);
      WriteUBitLong(data, 8);
    } else {
      if ((data & 0xfff) == data) {
        WriteUBitLong(2, 2);
        WriteUBitLong(data, 12);
      } else {
        WriteUBitLong(0x3, 2);
        WriteUBitLong(data, 32);
      }
    }
  }
}

SOURCE_FORCEINLINE void CBitWrite::WriteBitFloat(float flValue) {
  WriteUBitLong(*((uint32_t *)&flValue), 32);
}

SOURCE_FORCEINLINE void CBitWrite::WriteFloat(float flValue) {
  // Pre-swap the float, since WriteBits writes raw data
  LittleFloat(&flValue, &flValue);
  WriteUBitLong(*((uint32_t *)&flValue), 32);
}

class CBitRead : public CBitBuffer {
  uint32_t m_nInBufWord;
  int m_nBitsAvail;
  uint32_t const *m_pDataIn;
  uint32_t const *m_pBufferEnd;
  uint32_t const *m_pData;

 public:
  CBitRead(const void *pData, int nBytes, int nBits = -1) {
    StartReading(pData, nBytes, 0, nBits);
  }

  CBitRead(const char *pDebugName, const void *pData, int nBytes,
           int nBits = -1) {
    SetDebugName(pDebugName);
    StartReading(pData, nBytes, 0, nBits);
  }

  CBitRead(void) : CBitBuffer() {}

  SOURCE_FORCEINLINE int Tell(void) const { return GetNumBitsRead(); }

  SOURCE_FORCEINLINE size_t TotalBytesAvailable(void) const {
    return m_nDataBytes;
  }

  SOURCE_FORCEINLINE int GetNumBitsLeft() const { return m_nDataBits - Tell(); }

  SOURCE_FORCEINLINE int GetNumBytesLeft() const {
    return GetNumBitsLeft() >> 3;
  }

  bool Seek(int nPosition);

  SOURCE_FORCEINLINE bool SeekRelative(int nOffset) {
    return Seek(GetNumBitsRead() + nOffset);
  }

  SOURCE_FORCEINLINE unsigned char const *GetBasePointer() {
    return reinterpret_cast<unsigned char const *>(m_pData);
  }

  void StartReading(const void *pData, int nBytes, int iStartBit = 0,
                    int nBits = -1);

  SOURCE_FORCEINLINE int GetNumBitsRead(void) const;

  SOURCE_FORCEINLINE void GrabNextDWord(bool bOverFlowImmediately = false);
  SOURCE_FORCEINLINE void FetchNext();
  SOURCE_FORCEINLINE unsigned int ReadUBitLong(int numbits);
  SOURCE_FORCEINLINE int ReadSBitLong(int numbits);
  SOURCE_FORCEINLINE unsigned int ReadUBitVar();
  SOURCE_FORCEINLINE unsigned int PeekUBitLong(int numbits);
  SOURCE_FORCEINLINE float ReadBitFloat();
  float ReadBitCoord();
  float ReadBitCoordMP(bool bIntegral, bool bLowPrecision);
  float ReadBitNormal();
  void ReadBitVec3Coord(Vector &fa);
  void ReadBitVec3Normal(Vector &fa);
  void ReadBitAngles(QAngle &fa);
  bool ReadBytes(void *pOut, int nBytes);
  float ReadBitAngle(int numbits);

  // Returns 0 or 1.
  SOURCE_FORCEINLINE int ReadOneBit();
  SOURCE_FORCEINLINE int ReadLong();
  SOURCE_FORCEINLINE int ReadChar();
  SOURCE_FORCEINLINE int ReadByte();
  SOURCE_FORCEINLINE int ReadShort();
  SOURCE_FORCEINLINE int ReadWord();
  SOURCE_FORCEINLINE float ReadFloat();
  void ReadBits(void *pOut, int nBits);

  // Returns false if bufLen isn't large enough to hold the
  // string in the buffer.
  //
  // Always reads to the end of the string (so you can read the
  // next piece of data waiting).
  //
  // If bLine is true, it stops when it reaches a '\n' or a 0-terminator.
  //
  // pStr is always 0-terminated (unless bufLen is 0).
  //
  // pOutN<umChars is set to the number of characters left in pStr when the
  // routine is complete (this will never exceed bufLen-1).
  //
  bool ReadString(char *pStr, int bufLen, bool bLine = false,
                  int *pOutNumChars = NULL);
  char *ReadAndAllocateString(bool *pOverflow = 0);

  int64_t ReadLongLong();
};

SOURCE_FORCEINLINE int CBitRead::GetNumBitsRead(void) const {
  if (!m_pData)  // pesky 0 ptr bitbufs. these happen.
    return 0;
  int nCurOfs = (32 - m_nBitsAvail) +
                (8 * sizeof(m_pData[0]) * (m_pDataIn - m_pData - 1));
  int nAdjust = 8 * (m_nDataBytes & 3);
  return std::min(nCurOfs + nAdjust, m_nDataBits);
}

SOURCE_FORCEINLINE void CBitRead::GrabNextDWord(bool bOverFlowImmediately) {
  if (m_pDataIn == m_pBufferEnd) {
    m_nBitsAvail = 1;  // so that next read will run out of words
    m_nInBufWord = 0;
    m_pDataIn++;  // so seek count increments like old
    if (bOverFlowImmediately) SetOverflowFlag();
  } else if (m_pDataIn > m_pBufferEnd) {
    SetOverflowFlag();
    m_nInBufWord = 0;
  } else {
    Assert(reinterpret_cast<int>(m_pDataIn) + 3 <
           reinterpret_cast<int>(m_pBufferEnd));
    m_nInBufWord = LittleDWord(*(m_pDataIn++));
  }
}

SOURCE_FORCEINLINE void CBitRead::FetchNext() {
  m_nBitsAvail = 32;
  GrabNextDWord(false);
}

int CBitRead::ReadOneBit() {
  int nRet = m_nInBufWord & 1;
  if (--m_nBitsAvail == 0) {
    FetchNext();
  } else
    m_nInBufWord >>= 1;
  return nRet;
}

unsigned int CBitRead::ReadUBitLong(int numbits) {
  if (m_nBitsAvail >= numbits) {
    unsigned int nRet = m_nInBufWord & s_nMaskTable[numbits];
    m_nBitsAvail -= numbits;
    if (m_nBitsAvail) {
      m_nInBufWord >>= numbits;
    } else {
      FetchNext();
    }
    return nRet;
  } else {
    // need to merge words
    unsigned int nRet = m_nInBufWord;
    numbits -= m_nBitsAvail;
    GrabNextDWord(true);
    if (m_bOverflow) return 0;
    nRet |= ((m_nInBufWord & s_nMaskTable[numbits]) << m_nBitsAvail);
    m_nBitsAvail = 32 - numbits;
    m_nInBufWord >>= numbits;
    return nRet;
  }
}

SOURCE_FORCEINLINE unsigned int CBitRead::PeekUBitLong(int numbits) {
  int nSaveBA = m_nBitsAvail;
  int nSaveW = m_nInBufWord;
  uint32_t const *pSaveP = m_pDataIn;
  unsigned int nRet = ReadUBitLong(numbits);
  m_nBitsAvail = nSaveBA;
  m_nInBufWord = nSaveW;
  m_pDataIn = pSaveP;
  return nRet;
}

SOURCE_FORCEINLINE int CBitRead::ReadSBitLong(int numbits) {
  int nRet = ReadUBitLong(numbits);
  // sign extend
  return (nRet << (32 - numbits)) >> (32 - numbits);
}

SOURCE_FORCEINLINE int CBitRead::ReadLong() {
  return (int)ReadUBitLong(sizeof(long) << 3);
}

SOURCE_FORCEINLINE float CBitRead::ReadFloat() {
  uint32_t nUval = ReadUBitLong(sizeof(long) << 3);
  return *((float *)&nUval);
}

SOURCE_FORCEINLINE unsigned int CBitRead::ReadUBitVar() {
  switch (ReadUBitLong(2)) {
    case 0:
      return ReadUBitLong(4);

    case 1:
      return ReadUBitLong(8);

    case 2:
      return ReadUBitLong(12);

    case 3:
      return ReadUBitLong(32);
  }
}

SOURCE_FORCEINLINE float CBitRead::ReadBitFloat() {
  uint32_t nvalue = ReadUBitLong(32);
  return *((float *)&nvalue);
}

int CBitRead::ReadChar() { return ReadSBitLong(sizeof(char) << 3); }

int CBitRead::ReadByte() { return ReadUBitLong(sizeof(unsigned char) << 3); }

int CBitRead::ReadShort() { return ReadSBitLong(sizeof(short) << 3); }

int CBitRead::ReadWord() { return ReadUBitLong(sizeof(unsigned short) << 3); }

#define WRAP_READ(bc)                                                         \
                                                                              \
  class bf_read : public bc {                                                 \
   public:                                                                    \
    SOURCE_FORCEINLINE bf_read(void) : bc() {}                                \
                                                                              \
    SOURCE_FORCEINLINE bf_read(const void *pData, int nBytes, int nBits = -1) \
        : bc(pData, nBytes, nBits) {}                                         \
                                                                              \
    SOURCE_FORCEINLINE bf_read(const char *pDebugName, const void *pData,     \
                               int nBytes, int nBits = -1)                    \
        : bc(pDebugName, pData, nBytes, nBits) {}                             \
  };

#define WRAP_WRITE(bc)                                                      \
                                                                            \
  class bf_write : public bc {                                              \
   public:                                                                  \
    SOURCE_FORCEINLINE bf_write(void) : bc() {}                             \
    SOURCE_FORCEINLINE bf_write(void *pData, int nBytes, int nMaxBits = -1) \
        : bc(pData, nBytes, nMaxBits) {}                                    \
                                                                            \
    SOURCE_FORCEINLINE bf_write(const char *pDebugName, void *pData,        \
                                int nBytes, int nMaxBits = -1)              \
        : bc(pDebugName, pData, nBytes, nMaxBits) {}                        \
  };

#ifdef OS_POSIX
WRAP_READ(old_bf_read);
#else
WRAP_READ(CBitRead);
#endif
WRAP_WRITE(old_bf_write);

#endif  // SOURCE_TIER1_BITBUF_H_
