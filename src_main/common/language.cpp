// Copyright © 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: languages definition

#include "language.h"
#include "tier0/include/dbg.h"
#include "tier1/strtools.h"

struct Language_t {
  const char *m_pchName;
  const char *m_pchShortName;
  const char *m_pchVGUILocalizationName;
  const char *m_pchICUName;  // used by osx; ISO-639-1 + ISO-3166-1 alpha-2.
                             // http://userguide.icu-project.org/locale/examples
  ELanguage m_ELanguage;
  int m_LanguageCodeID;
};

static const Language_t s_LanguageNames[] = {
    {"None", "none", "None", "none", k_Lang_None, 0},
    {"English", "english", "#GameUI_Language_English", "en_US", k_Lang_English,
     1033},
    {"German", "german", "#GameUI_Language_German", "de_DE", k_Lang_German,
     1031},
    {"French", "french", "#GameUI_Language_French", "fr_FR", k_Lang_French,
     1036},
    {"Italian", "italian", "#GameUI_Language_Italian", "it_IT", k_Lang_Italian,
     1040},
    {"Korean", "koreana", "#GameUI_Language_Korean", "ko_KR", k_Lang_Korean,
     1042},
    {"Spanish", "spanish", "#GameUI_Language_Spanish", "es_ES", k_Lang_Spanish,
     1034},
    {"Simplified_Chinese", "schinese", "#GameUI_Language_Simplified_Chinese",
     "zh_CN", k_Lang_Simplified_Chinese, 2052},
    {"Traditional_Chinese", "tchinese", "#GameUI_Language_Traditional_Chinese",
     "zh_TW", k_Lang_Traditional_Chinese, 1028},
    {"Russian", "russian", "#GameUI_Language_Russian", "ru_RU", k_Lang_Russian,
     1049},
    {"Thai", "thai", "#GameUI_Language_Thai", "th_TH", k_Lang_Thai, 1054},
    {"Japanese", "japanese", "#GameUI_Language_Japanese", "ja_JP",
     k_Lang_Japanese, 1041},
    {"Portuguese", "portuguese", "#GameUI_Language_Portuguese", "pt_PT",
     k_Lang_Portuguese, 2070},
    {"Polish", "polish", "#GameUI_Language_Polish", "pl_PL", k_Lang_Polish,
     1045},
    {"Danish", "danish", "#GameUI_Language_Danish", "da_DK", k_Lang_Danish,
     1030},
    {"Dutch", "dutch", "#GameUI_Language_Dutch", "nl_NL", k_Lang_Dutch, 1043},
    {"Finnish", "finnish", "#GameUI_Language_Finnish", "fi_FI", k_Lang_Finnish,
     1035},
    {"Norwegian", "norwegian", "#GameUI_Language_Norwegian", "no_NO",
     k_Lang_Norwegian, 1044},
    {"Swedish", "swedish", "#GameUI_Language_Swedish", "sv_SE", k_Lang_Swedish,
     1053},
    {"Romanian", "romanian", "#GameUI_Language_Romanian", "ro_RO",
     k_Lang_Romanian, 1048},
    {"Turkish", "turkish", "#GameUI_Language_Turkish", "tr_TR", k_Lang_Turkish,
     1055},
    {"Hungarian", "hungarian", "#GameUI_Language_Hungarian", "hu_HU",
     k_Lang_Hungarian, 1038},
    {"Czech", "czech", "#GameUI_Language_Czech", "cs_CZ", k_Lang_Czech, 1029},
    {"Brazilian", "brazilian", "#GameUI_Language_Brazilian", "pt_BR",
     k_Lang_Brazilian, 1046},
    {"Bulgarian", "bulgarian", "#GameUI_Language_Bulgarian", "bg_BG",
     k_Lang_Bulgarian, 1026},
    {"Greek", "greek", "#GameUI_Language_Greek", "el_GR", k_Lang_Greek, 1032},
};

//-----------------------------------------------------------------------------
// Purpose: find the language by name
//-----------------------------------------------------------------------------
ELanguage PchLanguageToELanguage(const char *pchShortName, ELanguage eDefault) {
  static_assert(std::size(s_LanguageNames) == k_Lang_MAX + 1);
  if (!pchShortName) return eDefault;

  for (usize iLang = 0; iLang < std::size(s_LanguageNames); ++iLang) {
    if (!_stricmp(pchShortName, s_LanguageNames[iLang].m_pchShortName)) {
      return s_LanguageNames[iLang].m_ELanguage;
    }
  }

  // return default
  return eDefault;
}

//-----------------------------------------------------------------------------
// Purpose: find the language by ICU short code
//-----------------------------------------------------------------------------
ELanguage PchLanguageICUCodeToELanguage(const char *pchICUCode,
                                        ELanguage eDefault) {
  static_assert(std::size(s_LanguageNames) == k_Lang_MAX + 1);
  if (!pchICUCode) return eDefault;

  // Match to no more than the param length so either a short 'en' or
  // full 'zh-Hant' can match
  usize nLen = strlen(pchICUCode);

  // we only have 5 character ICU codes so this should be enough room
  char rchCleanedCode[6];
  strcpy_s(rchCleanedCode, pchICUCode);
  if (nLen >= 3 && rchCleanedCode[2] == '-') {
    rchCleanedCode[2] = '_';
  }

  for (usize iLang = 0; iLang < std::size(s_LanguageNames); ++iLang) {
    if (!_strnicmp(rchCleanedCode, s_LanguageNames[iLang].m_pchICUName, nLen)) {
      return s_LanguageNames[iLang].m_ELanguage;
    }
  }

  // return default
  return eDefault;
}

//-----------------------------------------------------------------------------
// Purpose: return the short string name used for this language by SteamUI
//-----------------------------------------------------------------------------
const char *GetLanguageShortName(ELanguage eLang) {
  static_assert(std::size(s_LanguageNames) == k_Lang_MAX + 1);
  if (s_LanguageNames[eLang + 1].m_ELanguage == eLang) {
    return s_LanguageNames[eLang + 1].m_pchShortName;
  }

  Assert( !"enum ELanguage order mismatched from Language_t s_LanguageNames, fix it!" );
  return s_LanguageNames[0].m_pchShortName;
}

//-----------------------------------------------------------------------------
// Purpose: return the short string name used for this language by SteamUI
//-----------------------------------------------------------------------------
const char *GetLanguageName(ELanguage eLang) {
  static_assert(std::size(s_LanguageNames) == k_Lang_MAX + 1);
  if (s_LanguageNames[eLang + 1].m_ELanguage == eLang) {
    return s_LanguageNames[eLang + 1].m_pchName;
  }

  Assert( !"enum ELanguage order mismatched from Language_t s_LanguageNames, fix it!" );
  return s_LanguageNames[0].m_pchShortName;
}

//-----------------------------------------------------------------------------
// Purpose: return the ICU code used for this language by SteamUI
//-----------------------------------------------------------------------------
const char *GetLanguageICUName(ELanguage eLang) {
  static_assert(std::size(s_LanguageNames) == k_Lang_MAX + 1);
  if (s_LanguageNames[eLang + 1].m_ELanguage == eLang) {
    Assert(eLang + 1 < std::size(s_LanguageNames));
    return s_LanguageNames[eLang + 1].m_pchICUName;
  }

  Assert(!"enum ELanguage order mismatched from Language_t s_LanguageNames, fix it!");
  return s_LanguageNames[0].m_pchICUName;
}

//-----------------------------------------------------------------------------
// Purpose: return the short string name used for this language by SteamUI
//-----------------------------------------------------------------------------
const char *GetLanguageVGUILocalization(ELanguage eLang) {
  static_assert(std::size(s_LanguageNames) == k_Lang_MAX + 1);
  if (s_LanguageNames[eLang + 1].m_ELanguage == eLang) {
    return s_LanguageNames[eLang + 1].m_pchVGUILocalizationName;
  }

  Assert( !"enum ELanguage order mismatched from Language_t s_LanguageNames, fix it!" );
  return s_LanguageNames[0].m_pchVGUILocalizationName;
}
