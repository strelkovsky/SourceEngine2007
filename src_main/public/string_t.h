// Copyright � 1996-2018, Valve Corporation, All rights reserved.
//
// Purpose: Defines the more complete set of operations on the string_t defined
// These should be used instead of direct manipulation to allow more flexibility
// in future ports or optimization.

#ifndef STRING_T_H
#define STRING_T_H

#ifndef NO_STRING_T

#ifdef WEAK_STRING_T

typedef int string_t;

// Purpose: The correct way to specify the nullptr string as a constant.
#define NULL_STRING 0

// Purpose: Given a string_t, make a C string. By convention the result string
//  pointer should be considered transient and should not be stored.
#define STRING(offset) ((offset) ? reinterpret_cast<const char *>(offset) : "")

// Purpose: Given a C string, obtain a string_t
#define MAKE_STRING(str) ((*str != 0) ? reinterpret_cast<int>(str) : 0)

#define IDENT_STRINGS(s1, s2) (*((void **)&(s1)) == *((void **)&(s2)))

#else  // Strong string_t

struct string_t {
 public:
  bool operator!() const { return pszValue == nullptr; }
  bool operator==(const string_t &rhs) const {
    return pszValue == rhs.pszValue;
  }
  bool operator!=(const string_t &rhs) const { return !(*this == rhs); }
  bool operator<(const string_t &rhs) const {
    return ((void *)pszValue < (void *)rhs.pszValue);
  }

  const char *ToCStr() const { return (pszValue) ? pszValue : ""; }

 protected:
  const char *pszValue;
};

// string_t is used in unions, hence, no constructor allowed
struct castable_string_t : public string_t {
  castable_string_t() { pszValue = nullptr; }
  castable_string_t(const char *pszFrom) {
    pszValue = (pszFrom && *pszFrom) ? pszFrom : nullptr;
  }
};

// Purpose: The correct way to specify the nullptr string as a constant.
#define NULL_STRING \
  castable_string_t {}

// Purpose: Given a string_t, make a C string. By convention the result string
//  pointer should be considered transient and should not be stored.
#define STRING(string_t_obj) (string_t_obj).ToCStr()

// Purpose: Given a C string, obtain a string_t
#define MAKE_STRING(c_str) castable_string_t(c_str)

#define IDENT_STRINGS(s1, s2) (*((void **)&(s1)) == *((void **)&(s2)))

#endif

#else  // NO_STRING_T

typedef const char *string_t;
#define NULL_STRING 0
#define STRING(c_str) (c_str)
#define MAKE_STRING(c_str) (c_str)
#define IDENT_STRINGS(s1, s2) (*((void **)&(s1)) == *((void **)&(s2)))

#endif  // NO_STRING_T

#endif  // STRING_T_H
