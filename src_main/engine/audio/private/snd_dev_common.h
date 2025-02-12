// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Device Common Routines.

#ifndef SND_DEV_COMMON_H
#define SND_DEV_COMMON_H

#include "snd_device.h"

class CAudioDeviceBase : public IAudioDevice {
 public:
  virtual bool IsActive() { return false; }
  virtual bool Init() { return false; }
  virtual void Shutdown() {}
  virtual void Pause() {}
  virtual void UnPause() {}
  virtual float MixDryVolume() { return 0; }
  virtual bool Should3DMix() { return m_bSurround; }
  virtual void StopAllSounds() {}

  virtual int PaintBegin(float, int soundtime, int paintedtime) { return 0; }
  virtual void PaintEnd() {}

  virtual void SpatializeChannel(int volume[CCHANVOLUMES / 2], int master_vol,
                                 const Vector &sourceDir, float gain,
                                 float mono);
  virtual void ApplyDSPEffects(int idsp, portable_samplepair_t *pbuffront,
                               portable_samplepair_t *pbufrear,
                               portable_samplepair_t *pbufcenter,
                               int samplecount);
  virtual int GetOutputPosition() { return 0; }
  virtual void ClearBuffer() {}
  virtual void UpdateListener(const Vector &position, const Vector &forward,
                              const Vector &right, const Vector &up) {}

  virtual void MixBegin(int sampleCount);
  virtual void MixUpsample(int sampleCount, int filtertype);

  virtual void Mix8Mono(channel_t *pChannel, ch *pData, int outputOffset,
                        int inputOffset, fixedint rateScaleFix, int outCount,
                        int timecompress);
  virtual void Mix8Stereo(channel_t *pChannel, ch *pData, int outputOffset,
                          int inputOffset, fixedint rateScaleFix, int outCount,
                          int timecompress);
  virtual void Mix16Mono(channel_t *pChannel, short *pData, int outputOffset,
                         int inputOffset, fixedint rateScaleFix, int outCount,
                         int timecompress);
  virtual void Mix16Stereo(channel_t *pChannel, short *pData, int outputOffset,
                           int inputOffset, fixedint rateScaleFix, int outCount,
                           int timecompress);

  virtual void ChannelReset(int entnum, int channelIndex, float distanceMod) {}
  virtual void TransferSamples(int end) {}

  virtual const ch *DeviceName() { return nullptr; }
  virtual int DeviceChannels() { return 0; }
  virtual int DeviceSampleBits() { return 0; }
  virtual int DeviceSampleBytes() { return 0; }
  virtual int DeviceDmaSpeed() { return 1; }
  virtual int DeviceSampleCount() { return 0; }

  virtual bool IsSurround() { return m_bSurround; }
  virtual bool IsSurroundCenter() { return m_bSurroundCenter; }
  virtual bool IsHeadphone() { return m_bHeadphone; }

  bool m_bSurround;
  bool m_bSurroundCenter;
  bool m_bHeadphone;
};

#endif  // SND_DEV_COMMON_H
