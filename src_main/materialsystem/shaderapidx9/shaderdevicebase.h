// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef MATERIALSYSTEM_SHADERAPIDX9_SHADERDEVICEBASE_H_
#define MATERIALSYSTEM_SHADERAPIDX9_SHADERDEVICEBASE_H_

#include "base/include/base_types.h"

#include "IHardwareConfigInternal.h"
#include "bitmap/imageformat.h"
#include "hardwareconfig.h"
#include "materialsystem/imaterialsystem.h"
#include "shaderapi/IShaderDevice.h"

class KeyValues;

// Define this if you want to run with NVPERFHUD.
//#define NVPERFHUD 1

// The Base implementation of the shader device.
class CShaderDeviceMgrBase : public IShaderDeviceMgr {
 public:
  CShaderDeviceMgrBase();
  virtual ~CShaderDeviceMgrBase();

  // Methods of IAppSystem

  bool Connect(CreateInterfaceFn factory) override;
  void Disconnect() override;
  void *QueryInterface(const ch *pInterfaceName) override;

  // Methods of IShaderDeviceMgr

  bool GetRecommendedConfigurationInfo(i32 nAdapter, i32 nDXLevel,
                                       KeyValues *pCongifuration) override;
  void AddModeChangeCallback(ShaderModeChangeCallbackFunc_t func) override;
  void RemoveModeChangeCallback(ShaderModeChangeCallbackFunc_t func) override;

  // Reads in the hardware caps from the dxsupport.cfg file
  void ReadHardwareCaps(HardwareCaps_t &caps, i32 nDxLevel);

  // Reads in the max + preferred DX support level
  void ReadDXSupportLevels(HardwareCaps_t &caps);

  // Returns the hardware caps for a particular adapter
  const HardwareCaps_t &GetHardwareCaps(i32 nAdapter) const;

  // Invokes mode change callbacks
  void InvokeModeChangeCallbacks();

  // Factory to return from SetMode
  static void *ShaderInterfaceFactory(const ch *pInterfaceName,
                                      i32 *pReturnCode);

  // Returns only valid dx levels
  i32 GetClosestActualDXLevel(i32 nDxLevel) const;

 protected:
  struct AdapterInfo_t {
    HardwareCaps_t m_ActualCaps;
  };

 private:
  // Reads in the dxsupport.cfg keyvalues
  KeyValues *ReadDXSupportKeyValues();

  // Reads in ConVars + config variables
  void LoadConfig(KeyValues *pKeyValues, KeyValues *pConfiguration);

  // Loads the hardware caps, for cases in which the D3D caps lie or where we
  // need to augment the caps
  void LoadHardwareCaps(KeyValues *pGroup, HardwareCaps_t &caps);

  // Gets the recommended configuration associated with a particular dx level
  bool GetRecommendedConfigurationInfo(i32 nAdapter, i32 nDXLevel,
                                       i32 nVendorID, i32 nDeviceID,
                                       KeyValues *pConfiguration);

  // Returns the amount of video memory in bytes for a particular adapter
  virtual u64 GetVidMemBytes(u32 adapter_idx) const = 0;

  // Looks for override keyvalues in the dxsupport cfg keyvalues
  KeyValues *FindDXLevelSpecificConfig(KeyValues *pKeyValues, i32 nDxLevel);
  KeyValues *FindDXLevelAndVendorSpecificConfig(KeyValues *pKeyValues,
                                                i32 nDxLevel, i32 nVendorID);
  KeyValues *FindCPUSpecificConfig(KeyValues *pKeyValues,
                                   i32 cpu_frequency_in_mhz, bool is_amd);
  KeyValues *FindMemorySpecificConfig(KeyValues *pKeyValues, u32 nSystemRamMB);
  KeyValues *FindVidMemSpecificConfig(KeyValues *pKeyValues, u32 nVideoRamMB);
  KeyValues *FindCardSpecificConfig(KeyValues *pKeyValues, i32 nVendorID,
                                    i32 nDeviceID);

 protected:
  // Stores adapter info for all adapters.
  CUtlVector<AdapterInfo_t> adapters_;
  // Installed mode change callbacks.
  CUtlVector<ShaderModeChangeCallbackFunc_t> shader_mode_change_callbacks_;
  // Dx support config.
  KeyValues *dx_support_config_;
};

// The Base implementation of the shader device
class CShaderDeviceBase : public IShaderDevice {
 public:
  enum IPCMessage_t {
    RELEASE_MESSAGE = 0x5E740DE0,
    REACQUIRE_MESSAGE = 0x5E740DE1,
    EVICT_MESSAGE = 0x5E740DE2,
  };

  CShaderDeviceBase();
  virtual ~CShaderDeviceBase();

  // Methods of IShaderDevice

  ImageFormat GetBackBufferFormat() const override;
  i32 StencilBufferBits() const override;
  bool IsAAEnabled() const override;
  void GetWindowSize(i32 &nWidth, i32 &nHeight) const override;
  bool AddView(HWND hWnd) override;
  void RemoveView(HWND hWnd) override;
  void SetView(HWND hWnd) override;

  // Methods exposed to the rest of shader api
  virtual bool InitDevice(HWND hWnd, i32 nAdapter,
                          const ShaderDeviceInfo_t &mode) = 0;
  virtual void ShutdownDevice() = 0;
  virtual bool IsDeactivated() const = 0;

  virtual void OtherAppInitializing(bool initializing) {}
  virtual void EvictManagedResourcesInternal() {}

  // Inline methods
  HWND GetIPCHWnd() const { return m_hWndCookie; }
  void SendIPCMessage(IPCMessage_t message);

 protected:
  // IPC communication for multiple shaderapi apps
  void InstallWindowHook(HWND hWnd);
  void RemoveWindowHook(HWND hWnd);

  // Finds a child window
  i32 FindView(HWND hWnd) const;

  i32 m_nAdapter;
  HWND m_hWnd;
  HWND m_hWndCookie;
  bool m_bInitialized : 1;
  bool m_bIsMinimized : 1;

  // The current view hwnd
  HWND m_ViewHWnd;

  i32 m_nWindowWidth;
  i32 m_nWindowHeight;
};

struct IUnknown;

// Helper class to reduce code related to shader buffers
template <typename T>
class CShaderBuffer : public IShaderBuffer {
  static_assert(std::is_abstract<T>::value,
                "The interface should be abstract.");
  static_assert(std::is_base_of<IUnknown, T>::value,
                "The interface should derive from IUnknown.");

 public:
  CShaderBuffer(T *pBlob) : blob_{pBlob} {}

  usize GetSize() const override { return blob_ ? blob_->GetBufferSize() : 0; }

  const void *GetBits() const override {
    return blob_ ? blob_->GetBufferPointer() : nullptr;
  }

  void Release() override {
    if (blob_) blob_->Release();

    delete this;
  }

 private:
  T *blob_;
};

#endif  // MATERIALSYSTEM_SHADERAPIDX9_SHADERDEVICEBASE_H_
