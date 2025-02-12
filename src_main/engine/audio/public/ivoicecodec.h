// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Define the IVoiceCodec interface.

#ifndef IVOICECODEC_H
#define IVOICECODEC_H

#include "tier1/interface.h"

#define BYTES_PER_SAMPLE 2

// This interface is for voice codecs to implement.
//
// Codecs are guaranteed to be called with the exact output from Compress into
// Decompress (ie: data won't be stuck together and sent to Decompress).
//
// Decompress is not guaranteed to be called in any specific order relative to
// Compress, but Codecs maintain state between calls, so it is best to call
// Compress with consecutive voice data and decompress likewise. If you call it
// out of order, it will sound wierd.
//
// In the same vein, calling Decompress twice with the same data is a bad idea
// since the state will be expecting the next block of data, not the same block.
class IVoiceCodec {
 protected:
  virtual ~IVoiceCodec() {}

 public:
  // Initialize the object. The uncompressed format is always 8-bit signed mono.
  virtual bool Init(int quality) = 0;

  // Use this to delete the object.
  virtual void Release() = 0;

  // Compress the voice data.
  // 16-bit signed mono voice data.
  // The length of the pCompressed buffer. Don't exceed this.
  // Set to true on the last call to Compress (the user stopped talking).
  // Some codecs like big block sizes and will hang onto data you give them
  // in Compress calls. 	When you call with bFinal, the codec will give
  // you compressed data no matter what.  Return the number of bytes you filled
  // into pCompressed.
  virtual int Compress(const ch *pUncompressed, int nSamples, ch *pCompressed,
                       int maxCompressedBytes, bool bFinal) = 0;

  // Decompress voice data. pUncompressed is 16-bit signed mono.
  virtual int Decompress(const ch *pCompressed, int compressedBytes,
                         ch *pUncompressed, int maxUncompressedBytes) = 0;

  // Some codecs maintain state between Compress and Decompress calls. This
  // should clear that state.
  virtual bool ResetState() = 0;
};

#define MILES_VOICE_CODEC "vaudio_miles"
#define SPEEX_VOICE_CODEC "vaudio_speex"

#endif  // IVOICECODEC_H
