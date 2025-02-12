// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef ISHADERDEVICE_H
#define ISHADERDEVICE_H

#include "appframework/IAppSystem.h"
#include "base/include/macros.h"
#include "bitmap/imageformat.h"
#include "materialsystem/imaterial.h"
#include "shaderapi/ishaderdynamic.h"
#include "tier1/interface.h"
#include "tier1/utlbuffer.h"

struct MaterialAdapterInfo_t;
class IMesh;
class KeyValues;

// Describes how to set the mode
constexpr inline int SHADER_DISPLAY_MODE_VERSION{1};

struct ShaderDisplayMode_t {
  ShaderDisplayMode_t() {
    memset(this, 0, sizeof(ShaderDisplayMode_t));
    m_nVersion = SHADER_DISPLAY_MODE_VERSION;
  }

  int m_nVersion;
  int m_nWidth;  // 0 when running windowed means use desktop resolution
  int m_nHeight;
  ImageFormat m_Format;         // use ImageFormats (ignored for windowed mode)
  int m_nRefreshRateNumerator;  // Refresh rate. Use 0 in numerator +
                                // denominator for a default setting.
  int m_nRefreshRateDenominator;  // Refresh rate = numerator / denominator.
};

// Describes how to set the device
constexpr inline int SHADER_DEVICE_INFO_VERSION{1};

struct ShaderDeviceInfo_t {
  ShaderDeviceInfo_t() {
    memset(this, 0, sizeof(ShaderDeviceInfo_t));
    m_nVersion = SHADER_DEVICE_INFO_VERSION;
    m_DisplayMode.m_nVersion = SHADER_DISPLAY_MODE_VERSION;
  }

  int m_nVersion;
  ShaderDisplayMode_t m_DisplayMode;
  int m_nBackBufferCount;  // valid values are 1 or 2 [2 results in triple
                           // buffering]
  int m_nAASamples;        // Number of AA samples to use
  int m_nAAQuality;        // AA quality level
  int m_nDXLevel;          // 0 means use recommended DX level for this adapter
  int m_nWindowedSizeLimitWidth;  // Used if m_bLimitWindowedSize is set,
                                  // defines max bounds for the back buffer
  int m_nWindowedSizeLimitHeight;

  bool m_bWindowed : 1;
  bool m_bResizing : 1;  // Only is meaningful when using windowed mode; means
                         // the window can be resized.
  bool m_bUseStencil : 1;
  bool m_bLimitWindowedSize : 1;  // In windowed mode, should we prevent the
                                  // back buffer from getting too large?
  bool m_bWaitForVSync : 1;       // Would we not present until vsync?
  bool m_bScaleToOutputResolution : 1;  // 360 ONLY: sets up hardware scaling
  bool m_bProgressive : 1;              // 360 ONLY: interlaced or progressive
  bool m_bUsingMultipleWindows : 1;  // Forces D3DPresent to use _COPY instead
};

// Info for non-interactive mode
struct ShaderNonInteractiveInfo_t {
  ShaderAPITextureHandle_t m_hTempFullscreenTexture;
  int m_nPacifierCount;
  ShaderAPITextureHandle_t m_pPacifierTextures[64];
  float m_flNormalizedX;
  float m_flNormalizedY;
  float m_flNormalizedSize;
};

// For vertex/index buffers. What type is it?
// NOTE: mirror this with a similarly named enum at the material system level
// for backwards compatibility.
enum ShaderBufferType_t {
  SHADER_BUFFER_TYPE_STATIC = 0,
  SHADER_BUFFER_TYPE_DYNAMIC,
  SHADER_BUFFER_TYPE_STATIC_TEMP,
  SHADER_BUFFER_TYPE_DYNAMIC_TEMP,

  SHADER_BUFFER_TYPE_COUNT,
};

constexpr inline bool IsDynamicBufferType(ShaderBufferType_t type) {
  return (type == SHADER_BUFFER_TYPE_DYNAMIC ||
          type == SHADER_BUFFER_TYPE_DYNAMIC_TEMP);
}

// Handle to a vertex, pixel, and geometry shader
SOURCE_DECLARE_POINTER_HANDLE(VertexShaderHandle_t);
SOURCE_DECLARE_POINTER_HANDLE(GeometryShaderHandle_t);
SOURCE_DECLARE_POINTER_HANDLE(PixelShaderHandle_t);

constexpr inline VertexShaderHandle_t VERTEX_SHADER_HANDLE_INVALID{nullptr};
constexpr inline GeometryShaderHandle_t GEOMETRY_SHADER_HANDLE_INVALID{nullptr};
constexpr inline PixelShaderHandle_t PIXEL_SHADER_HANDLE_INVALID{nullptr};

// A shader buffer returns a block of memory which must be released when done
// with it
the_interface IShaderBuffer {
 public:
  virtual size_t GetSize() const = 0;
  virtual const void *GetBits() const = 0;
  virtual void Release() = 0;
};

// Helper wrapper for IShaderBuffer for reading precompiled shader files
// NOTE: This is meant to be instanced on the stack; so don't call Release!
class CUtlShaderBuffer : public IShaderBuffer {
 public:
  CUtlShaderBuffer(CUtlBuffer &buffer) : buffer_{&buffer} {}

  size_t GetSize() const override { return buffer_->TellMaxPut(); }
  const void *GetBits() const override { return buffer_->Base(); }
  void Release() override { Assert(0); }

 private:
  const CUtlBuffer *buffer_;
};

// Mode chance callback
using ShaderModeChangeCallbackFunc_t = void (*)();

SOURCE_FORWARD_DECLARE_HANDLE(HWND);

// Methods related to discovering and selecting devices
#define SHADER_DEVICE_MGR_INTERFACE_VERSION "ShaderDeviceMgr002"

the_interface IShaderDeviceMgr : public IAppSystem {
 public:
  // Gets the number of adapters...
  virtual int GetAdapterCount() const = 0;

  // Returns info about each adapter
  virtual void GetAdapterInfo(int nAdapter, MaterialAdapterInfo_t &info)
      const = 0;

  // Gets recommended congifuration for a particular adapter at a particular dx
  // level
  virtual bool GetRecommendedConfigurationInfo(int nAdapter, int nDXLevel,
                                               KeyValues *pConfiguration) = 0;

  // Returns the number of modes
  virtual int GetModeCount(int nAdapter) const = 0;

  // Returns mode information..
  virtual void GetModeInfo(ShaderDisplayMode_t * pInfo, int nAdapter, int nMode)
      const = 0;

  // Returns the current mode info for the requested adapter
  virtual void GetCurrentModeInfo(ShaderDisplayMode_t * pInfo, int nAdapter)
      const = 0;

  // Initialization, shutdown
  virtual bool SetAdapter(int nAdapter, int nFlags) = 0;

  // Sets the mode
  // Use the returned factory to get at an IShaderDevice and an IShaderRender
  // and any other interfaces we decide to create.
  // A returned factory of nullptr indicates the mode was not set properly.
  virtual CreateInterfaceFn SetMode(HWND hWnd, int nAdapter,
                                    const ShaderDeviceInfo_t &mode) = 0;

  // Installs a callback to get called
  virtual void AddModeChangeCallback(ShaderModeChangeCallbackFunc_t func) = 0;
  virtual void RemoveModeChangeCallback(
      ShaderModeChangeCallbackFunc_t func) = 0;
};

// Methods related to control of the device
#define SHADER_DEVICE_INTERFACE_VERSION "ShaderDevice002"

class IShaderDevice {
 public:
  // Releases/reloads resources when other apps want some memory
  virtual void ReleaseResources() = 0;
  virtual void ReacquireResources() = 0;

  // returns the backbuffer format and dimensions
  virtual ImageFormat GetBackBufferFormat() const = 0;
  virtual void GetBackBufferDimensions(int &width, int &height) const = 0;

  // Returns the current adapter in use
  virtual int GetCurrentAdapter() const = 0;

  // Are we using graphics?
  virtual bool IsUsingGraphics() const = 0;

  // Use this to spew information about the 3D layer
  virtual void SpewDriverInfo() const = 0;

  // What's the bit depth of the stencil buffer?
  virtual int StencilBufferBits() const = 0;

  // Are we using a mode that uses MSAA
  virtual bool IsAAEnabled() const = 0;

  // Does a page flip
  virtual void Present() = 0;

  // Returns the window size
  virtual void GetWindowSize(int &nWidth, int &nHeight) const = 0;

  // Gamma ramp control
  virtual void SetHardwareGammaRamp(float fGamma, float fGammaTVRangeMin,
                                    float fGammaTVRangeMax,
                                    float fGammaTVExponent,
                                    bool bTVEnabled) = 0;

  // Creates/ destroys a child window
  virtual bool AddView(HWND hWnd) = 0;
  virtual void RemoveView(HWND hWnd) = 0;

  // Activates a view
  virtual void SetView(HWND hWnd) = 0;

  // Shader compilation
  virtual IShaderBuffer *CompileShader(const char *pProgram, size_t nBufLen,
                                       const char *pShaderVersion) = 0;

  // Shader creation, destruction
  virtual VertexShaderHandle_t CreateVertexShader(
      IShaderBuffer *pShaderBuffer) = 0;
  virtual void DestroyVertexShader(VertexShaderHandle_t hShader) = 0;

  virtual GeometryShaderHandle_t CreateGeometryShader(
      IShaderBuffer *pShaderBuffer) = 0;
  virtual void DestroyGeometryShader(GeometryShaderHandle_t hShader) = 0;

  virtual PixelShaderHandle_t CreatePixelShader(
      IShaderBuffer *pShaderBuffer) = 0;
  virtual void DestroyPixelShader(PixelShaderHandle_t hShader) = 0;

  // Utility methods to make shader creation simpler
  // NOTE: For the utlbuffer version, use a binary buffer for a compiled shader
  // and a text buffer for a source-code (.fxc) shader
  VertexShaderHandle_t CreateVertexShader(const char *pProgram, size_t nBufLen,
                                          const char *pShaderVersion);
  VertexShaderHandle_t CreateVertexShader(CUtlBuffer &buf,
                                          const char *pShaderVersion = nullptr);

  GeometryShaderHandle_t CreateGeometryShader(const char *pProgram,
                                              size_t nBufLen,
                                              const char *pShaderVersion);
  GeometryShaderHandle_t CreateGeometryShader(
      CUtlBuffer &buf, const char *pShaderVersion = nullptr);

  PixelShaderHandle_t CreatePixelShader(const char *pProgram, size_t nBufLen,
                                        const char *pShaderVersion);
  PixelShaderHandle_t CreatePixelShader(CUtlBuffer &buf,
                                        const char *pShaderVersion = nullptr);

  // NOTE: Deprecated!! Use CreateVertexBuffer/CreateIndexBuffer instead
  // Creates/destroys Mesh
  virtual IMesh *CreateStaticMesh(VertexFormat_t vertexFormat,
                                  const char *pTextureBudgetGroup,
                                  IMaterial *pMaterial = nullptr) = 0;
  virtual void DestroyStaticMesh(IMesh *mesh) = 0;

  // Creates/destroys static vertex + index buffers
  virtual IVertexBuffer *CreateVertexBuffer(ShaderBufferType_t type,
                                            VertexFormat_t fmt,
                                            int nVertexCount,
                                            const char *pBudgetGroup) = 0;
  virtual void DestroyVertexBuffer(IVertexBuffer *pVertexBuffer) = 0;

  virtual IIndexBuffer *CreateIndexBuffer(ShaderBufferType_t bufferType,
                                          MaterialIndexFormat_t fmt,
                                          int nIndexCount,
                                          const char *pBudgetGroup) = 0;
  virtual void DestroyIndexBuffer(IIndexBuffer *pIndexBuffer) = 0;

  // Do we need to specify the stream here in the case of locking multiple
  // dynamic VBs on different streams?
  virtual IVertexBuffer *GetDynamicVertexBuffer(int nStreamID,
                                                VertexFormat_t vertexFormat,
                                                bool bBuffered = true) = 0;
  virtual IIndexBuffer *GetDynamicIndexBuffer(MaterialIndexFormat_t fmt,
                                              bool bBuffered = true) = 0;

  // A special path used to tick the front buffer while loading on the 360
  virtual void EnableNonInteractiveMode(
      MaterialNonInteractiveMode_t mode,
      ShaderNonInteractiveInfo_t *pInfo = nullptr) = 0;
  virtual void RefreshFrontBufferNonInteractive() = 0;
};

// Inline methods of IShaderDevice
inline VertexShaderHandle_t IShaderDevice::CreateVertexShader(
    CUtlBuffer &buf, const char *pShaderVersion) {
  // NOTE: Text buffers are assumed to have source-code shader files
  // Binary buffers are assumed to have compiled shader files
  if (buf.IsText()) {
    Assert(pShaderVersion);
    return CreateVertexShader((const char *)buf.Base(), buf.TellMaxPut(),
                              pShaderVersion);
  }

  CUtlShaderBuffer shader_buffer{buf};
  return CreateVertexShader(&shader_buffer);
}

inline VertexShaderHandle_t IShaderDevice::CreateVertexShader(
    const char *pProgram, size_t nBufLen, const char *pShaderVersion) {
  VertexShaderHandle_t vertex_shader{VERTEX_SHADER_HANDLE_INVALID};
  IShaderBuffer *shader_buffer{
      CompileShader(pProgram, nBufLen, pShaderVersion)};

  if (shader_buffer) {
    vertex_shader = CreateVertexShader(shader_buffer);
    shader_buffer->Release();
  }

  return vertex_shader;
}

inline GeometryShaderHandle_t IShaderDevice::CreateGeometryShader(
    CUtlBuffer &buf, const char *pShaderVersion) {
  // NOTE: Text buffers are assumed to have source-code shader files
  // Binary buffers are assumed to have compiled shader files
  if (buf.IsText()) {
    Assert(pShaderVersion);
    return CreateGeometryShader((const char *)buf.Base(), buf.TellMaxPut(),
                                pShaderVersion);
  }

  CUtlShaderBuffer shader_buffer{buf};
  return CreateGeometryShader(&shader_buffer);
}

inline GeometryShaderHandle_t IShaderDevice::CreateGeometryShader(
    const char *pProgram, size_t nBufLen, const char *pShaderVersion) {
  GeometryShaderHandle_t geometry_shader{GEOMETRY_SHADER_HANDLE_INVALID};
  IShaderBuffer *shader_buffer{
      CompileShader(pProgram, nBufLen, pShaderVersion)};

  if (shader_buffer) {
    geometry_shader = CreateGeometryShader(shader_buffer);
    shader_buffer->Release();
  }

  return geometry_shader;
}

inline PixelShaderHandle_t IShaderDevice::CreatePixelShader(
    CUtlBuffer &buf, const char *pShaderVersion) {
  // NOTE: Text buffers are assumed to have source-code shader files
  // Binary buffers are assumed to have compiled shader files
  if (buf.IsText()) {
    Assert(pShaderVersion);
    return CreatePixelShader((const char *)buf.Base(), buf.TellMaxPut(),
                             pShaderVersion);
  }

  CUtlShaderBuffer shader_buffer{buf};
  return CreatePixelShader(&shader_buffer);
}

inline PixelShaderHandle_t IShaderDevice::CreatePixelShader(
    const char *pProgram, size_t nBufLen, const char *pShaderVersion) {
  PixelShaderHandle_t pixel_shader{PIXEL_SHADER_HANDLE_INVALID};
  IShaderBuffer *shader_buffer{
      CompileShader(pProgram, nBufLen, pShaderVersion)};

  if (shader_buffer) {
    pixel_shader = CreatePixelShader(shader_buffer);
    shader_buffer->Release();
  }

  return pixel_shader;
}

#endif  // ISHADERDEVICE_H
