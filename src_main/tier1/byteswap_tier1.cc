// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Low level byte swapping routines.

#include "tier1/byteswap.h"

// Copy a single field from the input buffer to the output buffer, swapping the
// bytes if necessary
void CByteswap::SwapFieldToTargetEndian(void *pOutputBuffer, void *pData,
                                        typedescription_t *pField) {
  switch (pField->fieldType) {
    case FIELD_CHARACTER:
      SwapBufferToTargetEndian<char>((char *)pOutputBuffer, (char *)pData,
                                     pField->fieldSize);
      break;

    case FIELD_BOOLEAN:
      SwapBufferToTargetEndian<bool>((bool *)pOutputBuffer, (bool *)pData,
                                     pField->fieldSize);
      break;

    case FIELD_SHORT:
      SwapBufferToTargetEndian<short>((short *)pOutputBuffer, (short *)pData,
                                      pField->fieldSize);
      break;

    case FIELD_FLOAT:
      SwapBufferToTargetEndian<u32>((u32 *)pOutputBuffer, (u32 *)pData,
                                    pField->fieldSize);
      break;

    case FIELD_INTEGER:
      SwapBufferToTargetEndian<int>((int *)pOutputBuffer, (int *)pData,
                                    pField->fieldSize);
      break;

    case FIELD_VECTOR:
      SwapBufferToTargetEndian<u32>((u32 *)pOutputBuffer, (u32 *)pData,
                                    pField->fieldSize * 3);
      break;

    case FIELD_VECTOR2D:
      SwapBufferToTargetEndian<u32>((u32 *)pOutputBuffer, (u32 *)pData,
                                    pField->fieldSize * 2);
      break;

    case FIELD_QUATERNION:
      SwapBufferToTargetEndian<u32>((u32 *)pOutputBuffer, (u32 *)pData,
                                    pField->fieldSize * 4);
      break;

    case FIELD_EMBEDDED: {
      typedescription_t *pEmbed = pField->td->dataDesc;
      for (int i = 0; i < pField->fieldSize; ++i) {
        SwapFieldsToTargetEndian(
            (u8 *)pOutputBuffer + pEmbed->fieldOffset[TD_OFFSET_NORMAL],
            (u8 *)pData + pEmbed->fieldOffset[TD_OFFSET_NORMAL], pField->td);

        pOutputBuffer = (u8 *)pOutputBuffer + pField->fieldSizeInBytes;
        pData = (u8 *)pData + pField->fieldSizeInBytes;
      }
    } break;

    default:
      assert(0);
  }
}

// Write a block of fields. Works a bit like the saverestore code.
void CByteswap::SwapFieldsToTargetEndian(void *pOutputBuffer, void *pBaseData,
                                         datamap_t *pDataMap) {
  // deal with base class first
  if (pDataMap->baseMap) {
    SwapFieldsToTargetEndian(pOutputBuffer, pBaseData, pDataMap->baseMap);
  }

  typedescription_t *pFields = pDataMap->dataDesc;
  int fieldCount = pDataMap->dataNumFields;

  for (int i = 0; i < fieldCount; ++i) {
    typedescription_t *pField = &pFields[i];
    SwapFieldToTargetEndian(
        (u8 *)pOutputBuffer + pField->fieldOffset[TD_OFFSET_NORMAL],
        (u8 *)pBaseData + pField->fieldOffset[TD_OFFSET_NORMAL], pField);
  }
}
