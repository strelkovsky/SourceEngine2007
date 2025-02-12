// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: This module implements the IVoiceServer interface.

#include "ivoiceserver.h"
#include "quakedef.h"
#include "server.h"

#include "tier0/include/memdbgon.h"

class CVoiceServer : public IVoiceServer {
 public:
  virtual bool GetClientListening(int iReceiver, int iSender) {
    // Make into client indices..
    --iReceiver;
    --iSender;

    if (iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 ||
        iSender >= sv.GetClientCount())
      return false;

    return sv.GetClient(iSender)->IsHearingClient(iReceiver);
  }

  virtual bool SetClientListening(int iReceiver, int iSender, bool bListen) {
    // Make into client indices..
    --iReceiver;
    --iSender;

    if (iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 ||
        iSender >= sv.GetClientCount())
      return false;

    CGameClient *cl = sv.Client(iSender);

    cl->m_VoiceStreams.Set(iReceiver, bListen ? 1 : 0);

    return true;
  }

  virtual bool SetClientProximity(int iReceiver, int iSender,
                                  bool bUseProximity) {
    // Make into client indices..
    --iReceiver;
    --iSender;

    if (iReceiver < 0 || iReceiver >= sv.GetClientCount() || iSender < 0 ||
        iSender >= sv.GetClientCount())
      return false;

    CGameClient *cl = sv.Client(iSender);

    cl->m_VoiceProximity.Set(iReceiver, bUseProximity);

    return true;
  }
};

EXPOSE_SINGLE_INTERFACE(CVoiceServer, IVoiceServer,
                        INTERFACEVERSION_VOICESERVER);
