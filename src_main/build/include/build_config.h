// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// This file adds defines about the platform we're currently building on.
//  Operating System:
//    OS_WIN / OS_MACOSX / OS_LINUX / OS_POSIX (MACOSX or LINUX)
//  Compiler:
//    COMPILER_MSVC / COMPILER_GCC / COMPILER_CLANG
//  Processor:
//    ARCH_CPU_X86 / ARCH_CPU_X86_64 / ARCH_CPU_X86_FAMILY (X86 or X86_64)
//    ARCH_CPU_32_BITS / ARCH_CPU_64_BITS
#ifndef BUILD_INCLUDE_BUILD_CONFIG_H_
#define BUILD_INCLUDE_BUILD_CONFIG_H_
#if defined(__APPLE__)
#include <TargetConditionals.h>
#endif
// A set of macros to use for platform detection.
#if defined(__APPLE__)
#define OS_MACOSX 1
#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#define OS_IOS 1
#endif  // defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE
#elif defined(ANDROID)
#define OS_ANDROID 1
#elif defined(__linux__)
#define OS_LINUX 1
#elif defined(_WIN32)
#define OS_WIN 1
#elif defined(__FreeBSD__)
#define OS_FREEBSD 1
#elif defined(__OpenBSD__)
#define OS_OPENBSD 1
#elif defined(__sun)
#define OS_SOLARIS 1
#else
#error Please add support for your platform in build/include/build_config.h
#endif
#if defined(USE_OPENSSL) && defined(USE_NSS)
#error Cannot use both OpenSSL and NSS
#endif
// For access to standard BSD features, use OS_BSD instead of a
// more specific macro.
#if defined(OS_FREEBSD) || defined(OS_OPENBSD)
#define OS_BSD 1
#endif
// For access to standard POSIXish features, use OS_POSIX instead of a
// more specific macro.
#if defined(OS_MACOSX) || defined(OS_LINUX) || defined(OS_FREEBSD) || \
    defined(OS_OPENBSD) || defined(OS_SOLARIS) || defined(OS_ANDROID)
#define OS_POSIX 1
#endif
#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_ANDROID)
#define USE_X11 1  // Use X for graphics.
#endif
// Use tcmalloc
#if (defined(OS_WIN) || defined(OS_LINUX)) && !defined(NO_TCMALLOC)
#define USE_TCMALLOC 1
#endif
// Compiler detection.
#if defined(__clang__)
#define COMPILER_CLANG 1
#elif defined(__GNUC__)
#define COMPILER_GCC 1
#elif defined(_MSC_VER)
#define COMPILER_MSVC 1
#else
#error Please add support for your compiler in build/include/build_config.h
#endif
// Processor architecture detection.  For more info on what's defined, see:
//   https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros
//   http://www.agner.org/optimize/calling_conventions.pdf
//   or with gcc, run: "echo | gcc -E -dM -"
#if defined(_M_X64) || defined(__x86_64__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86_64 1
#define ARCH_CPU_64_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(_M_IX86) || defined(__i386__)
#define ARCH_CPU_X86_FAMILY 1
#define ARCH_CPU_X86 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__ARMEL__)
#define ARCH_CPU_ARM_FAMILY 1
#define ARCH_CPU_ARMEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#elif defined(__MIPSEL__)
#define ARCH_CPU_MIPS_FAMILY 1
#define ARCH_CPU_MIPSEL 1
#define ARCH_CPU_32_BITS 1
#define ARCH_CPU_LITTLE_ENDIAN 1
#else
#error Please add support for your architecture in build/include/build_config.h
#endif
// Type detection for wchar_t.
#if defined(OS_WIN)
#define WCHAR_T_IS_UTF16
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fffffff || __WCHAR_MAX__ == 0xffffffff)
#define WCHAR_T_IS_UTF32
#elif defined(OS_POSIX) && defined(COMPILER_GCC) && defined(__WCHAR_MAX__) && \
    (__WCHAR_MAX__ == 0x7fff || __WCHAR_MAX__ == 0xffff)
// On Posix, we'll detect short wchar_t, but projects aren't guaranteed to
// compile in this mode (in particular, Chrome doesn't). This is intended for
// other projects using base who manage their own dependencies and make sure
// short wchar works for them.
#define WCHAR_T_IS_UTF16
#else
#error Please add support for your compiler in build/include/build_config.h
#endif
#if defined(__ARMEL__) && !defined(OS_IOS)
#define WCHAR_T_IS_UNSIGNED 1
#elif defined(__MIPSEL__)
#define WCHAR_T_IS_UNSIGNED 0
#endif
#if defined(OS_ANDROID)
// The compiler thinks std::string::const_iterator and "const char*" are
// equivalent types.
#define STD_STRING_ITERATOR_IS_CHAR_POINTER
// The compiler thinks base::string16::const_iterator and "char16*" are
// equivalent types.
#define BASE_STRING16_ITERATOR_IS_CHAR16_POINTER
#endif
#endif  // BUILD_INCLUDE_BUILD_CONFIG_H_
