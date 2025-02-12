// Copyright © 1996-2001, Valve LLC, All rights reserved.
//
// Purpose: implementation of the TokenLine class.

#include "TokenLine.h"

#include <cstring>

TokenLine::~TokenLine() {}

bool TokenLine::SetLine(const char* newLine) {
  m_tokenNumber = 0;

  if (!newLine || (strlen(newLine) >= (MAX_LINE_CHARS - 1))) {
    memset(m_fullLine, 0, MAX_LINE_CHARS);
    memset(m_tokenBuffer, 0, MAX_LINE_CHARS);
    return false;
  }

  strncpy_s(m_fullLine, newLine, MAX_LINE_CHARS - 1);
  m_fullLine[MAX_LINE_CHARS - 1] = '\0';

  strncpy_s(m_tokenBuffer, newLine, MAX_LINE_CHARS - 1);
  m_tokenBuffer[MAX_LINE_CHARS - 1] = '\0';

  // parse tokens
  char* charPointer = m_tokenBuffer;

  while (*charPointer && (m_tokenNumber < MAX_LINE_TOKENS)) {
    while (*charPointer && ((*charPointer <= 32) || (*charPointer > 126)))
      charPointer++;

    if (*charPointer) {
      m_token[m_tokenNumber] = charPointer;

      // special treatment for quotes
      if (*charPointer == '\"') {
        charPointer++;
        m_token[m_tokenNumber] = charPointer;
        while (*charPointer && (*charPointer != '\"')) charPointer++;

      } else {
        m_token[m_tokenNumber] = charPointer;
        while (*charPointer && ((*charPointer > 32) && (*charPointer <= 126)))
          charPointer++;
      }

      m_tokenNumber++;

      if (*charPointer) {
        *charPointer = 0;
        charPointer++;
      }
    }
  }

  return (m_tokenNumber != MAX_LINE_TOKENS);
}

char* TokenLine::GetLine() { return m_fullLine; }

char* TokenLine::GetToken(int i) {
  if (i >= m_tokenNumber) return NULL;
  return m_token[i];
}

// if the given parm is not present return NULL
// otherwise return the address of the following token, or an empty string
const char* TokenLine::CheckToken(char* parm) {
  for (int i = 0; i < m_tokenNumber; i++) {
    if (!m_token[i]) continue;
    if (!strcmp(parm, m_token[i])) {
      const char* ret = m_token[i + 1];
      // if this token doesn't exist, since index i was the last
      // return an empty string
      if ((i + 1) == m_tokenNumber) ret = "";
      return ret;
    }
  }

  return NULL;
}

int TokenLine::CountToken() {
  int c = 0;
  for (int i = 0; i < m_tokenNumber; i++) {
    if (m_token[i]) c++;
  }
  return c;
}

char* TokenLine::GetRestOfLine(int i) {
  if (i >= m_tokenNumber) return NULL;
  return m_fullLine + (m_token[i] - m_tokenBuffer);
}

TokenLine::TokenLine(char* string) { SetLine(string); }

TokenLine::TokenLine() {}
