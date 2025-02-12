// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Outputs directly into a binary format

#ifndef DMSERIALIZERBINARY_H
#define DMSERIALIZERBINARY_H

//-----------------------------------------------------------------------------
// Forward declarations
//-----------------------------------------------------------------------------
class IDataModel;

//-----------------------------------------------------------------------------
// Installation methods for standard serializers
//-----------------------------------------------------------------------------
void InstallBinarySerializer(IDataModel *pFactory);

#endif  // DMSERIALIZERBINARY_H
