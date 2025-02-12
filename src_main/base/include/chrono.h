// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#ifndef BASE_INCLUDE_CHRONO_H_
#define BASE_INCLUDE_CHRONO_H_

#include "build/include/build_config.h"

#ifdef COMPILER_MSVC
#include <intrin.h>
#endif

#include <type_traits>

#include "base/include/base_types.h"
#include "base/include/check.h"              // DCHECK
#include "base/include/compiler_specific.h"  // SOURCE_FORCEINLINE
#include "base/include/cpu_instruction_set.h"
#include "base/include/macros.h"  // implicit_cast

#ifdef OS_WIN
#include "base/include/windows/windows_errno_info.h"  // source::windows::windows_errno_result
#include "base/include/windows/windows_light.h"
#endif

// x86-64 needs asm versions, since __cpuid generates unneeded code.
#if defined(COMPILER_MSVC) && defined(ARCH_CPU_X86_64)
extern "C" u64 start_tsc();
extern "C" u64 end_tsc();
#endif

// TODO(d.rattman): RDTSC / RDTSCP correct check & comments.

namespace source::chrono {
// Small timer with almost instruction-like precision.
// See
// https://www.intel.com/content/dam/www/public/us/en/documents/white-papers/ia-32-ia-64-benchmark-code-execution-paper.pdf
class CpuTscTimer {
 public:
  // Time any callable arg with void as return type. |cpu_clocks| is CPU
  // timestamp counter cycles elapsed.
  template <typename T, typename... Args>
  static inline typename std::enable_if_t<
      std::is_void_v<std::invoke_result_t<T, Args...>>>
  TimeIt(u64 &cpu_clocks, T it, Args &&... args) noexcept(noexcept(T)) {
    const u64 start_tsc_value{start_tsc()};

    it(std::forward<Args>(args)...);

    cpu_clocks = end_tsc() - start_tsc_value;
  }

  // Time any callable arg with non-void as return type. |cpu_clocks| is CPU
  // timestamp counter cycles elapsed.
  template <typename T, typename... Args>
  static inline typename std::enable_if_t<
      !std::is_void_v<std::invoke_result_t<T, Args...>>,
      std::invoke_result_t<T, Args...>>
  TimeIt(u64 &cpu_clocks, T it, Args &&... args) noexcept(noexcept(T)) {
    const u64 start_tsc_value{start_tsc()};
    const auto ret = it(std::forward<Args>(args)...);

    cpu_clocks = end_tsc() - start_tsc_value;

    return ret;
  }

  // Is timing supported?
  static inline bool IsSupported() noexcept {
    return source::CpuInstructionSet::HasRdtsc() &&
           source::CpuInstructionSet::HasRdtscp();
  }

 private:
  static SOURCE_FORCEINLINE u64 start_tsc() noexcept {
#ifndef ARCH_CPU_X86_64
    u32 high_tsc_part, low_tsc_part;

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
    // Warm up CPU instruction cache (3 times).
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%eax", "%ebx", "%ecx",
          "%edx");
    asm volatile(
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%eax", "%ebx", "%ecx",
          "%edx");
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%eax", "%ebx", "%ecx",
          "%edx");
    asm volatile(
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%eax", "%ebx", "%ecx",
          "%edx");
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%eax", "%ebx", "%ecx",
          "%edx");
    asm volatile(
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%eax", "%ebx", "%ecx",
          "%edx");

    // Measure.
    asm volatile(
        "cpuid\n\t"  // Complete every preceding instruction of the code before
                     // continuing the program execution.  (eax, ebx, ecx and
                     // edx modified).
        "rdtsc\n\t"  // Read TSC value (edx has high part, eax has low part).
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%eax", "%ebx", "%ecx",
          "%edx");
#elif defined(COMPILER_MSVC)
    // Warm up CPU instruction cache (3 times).
    // clang-format off
    __asm { 
      push   ebx                ; cpuid overwrites ebx and MSVC can not autoclobber so?
      cpuid                     ; Complete every preceding instruction of the code before
                                ; continuing the program execution.  (eax, ebx, ecx and
                                ; edx modified).
      rdtsc                     ; Read TSC value (edx has high part, eax has low part).
      mov    high_tsc_part,edx
      mov    low_tsc_part,eax
      pop    ebx                ; Need to restore old ebx. Unfortunately, taken into
                                ; measurement.
    }
    __asm {
      push   ebx                ; cpuid overwrites ebx and MSVC can not autoclobber so?  Unfortunately,
                                ; taken into measurement.
      rdtscp                    ; Sync all instructions before.  Read TSC value (edx has high part, eax
                                ; has low part, ecx has CPU id).
      mov    high_tsc_part,edx
      mov    low_tsc_part,eax
      cpuid                     ; Disallow next instructions executed before cpuid and logically before
                                ; rdtscp.  (eax, ebx, ecx and edx modified).
      pop    ebx                ; Need to restore old ebx.
    }
    __asm { 
      push   ebx                ; cpuid overwrites ebx and MSVC can not autoclobber so?
      cpuid                     ; Complete every preceding instruction of the code before
                                ; continuing the program execution.  (eax, ebx, ecx and
                                ; edx modified).
      rdtsc                     ; Read TSC value (edx has high part, eax has low part).
      mov    high_tsc_part,edx
      mov    low_tsc_part,eax
      pop    ebx                ; Need to restore old ebx. Unfortunately, taken into
                                ; measurement.
    }
    __asm {
      push   ebx                ; cpuid overwrites ebx and MSVC can not autoclobber so?  Unfortunately,
                                ; taken into measurement.
      rdtscp                    ; Sync all instructions before.  Read TSC value (edx has high part, eax
                                ; has low part, ecx has CPU id).
      mov    high_tsc_part,edx
      mov    low_tsc_part,eax
      cpuid                     ; Disallow next instructions executed before cpuid and logically before
                                ; rdtscp.  (eax, ebx, ecx and edx modified).
      pop    ebx                ; Need to restore old ebx.
    }
    __asm { 
      push   ebx                ; cpuid overwrites ebx and MSVC can not autoclobber so?
      cpuid                     ; Complete every preceding instruction of the code before
                                ; continuing the program execution.  (eax, ebx, ecx and
                                ; edx modified).
      rdtsc                     ; Read TSC value (edx has high part, eax has low part).
      mov    high_tsc_part,edx
      mov    low_tsc_part,eax
      pop    ebx                ; Need to restore old ebx. Unfortunately, taken into
                                ; measurement.
    }
    __asm {
      push   ebx                ; cpuid overwrites ebx and MSVC can not autoclobber so?  Unfortunately,
                                ; taken into measurement.
      rdtscp                    ; Sync all instructions before.  Read TSC value (edx has high part, eax
                                ; has low part, ecx has CPU id).
      mov    high_tsc_part,edx
      mov    low_tsc_part,eax
      cpuid                     ; Disallow next instructions executed before cpuid and logically before
                                ; rdtscp.  (eax, ebx, ecx and edx modified).
      pop    ebx                ; Need to restore old ebx.
    }
    // clang-format on

    // Measure.
    __asm { 
      push   ebx                ; cpuid overwrites ebx and MSVC can not autoclobber so?
      cpuid                     ; Complete every preceding instruction of the code before
                                ; continuing the program execution.  (eax, ebx, ecx and
                                ; edx modified).
      rdtsc                     ; Read TSC value (edx has high part, eax has low part).
      mov    high_tsc_part,edx
      mov    low_tsc_part,eax
      pop    ebx                ; Need to restore old ebx. Unfortunately, taken into
                                ; measurement.
    }
#endif

    // Construct full u64 TSC value.  Unfortunately, taken into measurement.
    return (implicit_cast<u64>(high_tsc_part) << 32) |
           implicit_cast<u64>(low_tsc_part);
#else  // ARCH_CPU_X86_64
#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
    u32 high_tsc_part, low_tsc_part;

    // Warm up CPU instruction cache (3 times).
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%rax", "%rbx", "%rcx",
          "%rdx");
    asm volatile(
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%rax", "%rbx", "%rcx",
          "%rdx");
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%rax", "%rbx", "%rcx",
          "%rdx");
    asm volatile(
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%rax", "%rbx", "%rcx",
          "%rdx");
    asm volatile(
        "cpuid\n\t"
        "rdtsc\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%rax", "%rbx", "%rcx",
          "%rdx");
    asm volatile(
        "rdtscp\n\t"
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        "cpuid\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%rax", "%rbx", "%rcx",
          "%rdx");

    // Measure.
    asm volatile(
        "cpuid\n\t"  // Complete every preceding instruction of the code before
                     // continuing the program execution.  (rax, rbx, rcx and
                     // rdx modified).
        "rdtsc\n\t"  // Read TSC value (edx has high part, eax has low part).
        "mov %%edx, %0\n\t"
        "mov %%eax, %1\n\t"
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%rax", "%rbx", "%rcx",
          "%rdx");

    // Construct full u64 TSC value.  Unfortunately, taken into measurement.
    return (implicit_cast<u64>(high_tsc_part) << 32) |
           implicit_cast<u64>(low_tsc_part);
#elif defined(COMPILER_MSVC)
    return ::start_tsc();
#endif
#endif  // !ARCH_CPU_X86_64
  }

  static SOURCE_FORCEINLINE u64 end_tsc() noexcept {
#ifndef ARCH_CPU_X86_64
    u32 high_tsc_part, low_tsc_part;

#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
    asm volatile(
        "rdtscp\n\t"  // Guarantee the execution of all the code we wanted to
                      // measure is completed.  Read TSC value (edx has high
                      // part, eax has low part, ecx has CPU id).
        "mov %%edx, %0\n\t"  // edx depends on rdtscp, it can't be out-of-order
                             // executed.
        "mov %%eax, %1\n\t"  // eax depends on rdtscp, it can't be out-of-order
                             // executed.
        "cpuid\n\t"  // Disallow next instructions executed before cpuid and
                     // logically before rdtscp.
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%eax", "%ebx", "%ecx",
          "%edx");
#elif defined(COMPILER_MSVC)
    __asm {
      push   ebx                ; cpuid overwrites ebx and MSVC can not autoclobber so?  Unfortunately,
                                ; taken into measurement.
      rdtscp                    ; Sync all instructions before.  Read TSC value (edx has high part, eax
                                ; has low part, ecx has CPU id).
      mov    high_tsc_part,edx
      mov    low_tsc_part,eax
      cpuid                     ; Disallow next instructions executed before cpuid and logically before
                                ; rdtscp.  (eax, ebx, ecx and edx modified).
      pop    ebx                ; Need to restore old ebx.
    }
#endif

    return (implicit_cast<u64>(high_tsc_part) << 32) |
           implicit_cast<u64>(low_tsc_part);
#else  // ARCH_CPU_X86_64
#if defined(COMPILER_CLANG) || defined(COMPILER_GCC)
    u32 high_tsc_part, low_tsc_part;

    asm volatile(
        "rdtscp\n\t"  // Guarantee the execution of all the code we wanted to
                      // measure is completed.  Read TSC value (edx has high
                      // part, eax has low part, ecx has CPU id).
        "mov %%edx, %0\n\t"  // edx depends on rdtscp, it can't be out-of-order
                             // executed.
        "mov %%eax, %1\n\t"  // eax depends on rdtscp, it can't be out-of-order
                             // executed.
        "cpuid\n\t"  // Disallow next instructions executed before cpuid and
                     // logically before rdtscp.
        : "=r"(high_tsc_part), "=r"(low_tsc_part)::"%rax", "%rbx", "%rcx",
          "%rdx");

    return (implicit_cast<u64>(high_tsc_part) << 32) |
           implicit_cast<u64>(low_tsc_part);
#elif defined(COMPILER_MSVC)
    return ::end_tsc();
#endif
#endif  // !ARCH_CPU_X86_64
  }

  CpuTscTimer() = delete;
  CpuTscTimer(const CpuTscTimer &c) = delete;
  CpuTscTimer &operator=(const CpuTscTimer &c) = delete;
  CpuTscTimer(const CpuTscTimer &&c) = delete;
  CpuTscTimer &operator=(const CpuTscTimer &&c) = delete;
};

// Small High Precision Event Timer with mostly nanosecond precision.  May be
// slow to read.
class HpetTimer {
 public:
#ifdef OS_WIN
  // Retrieves the current value of the performance counter, which is a high
  // resolution (<1us) time stamp that can be used for time-interval
  // measurements.  On systems that run Windows XP or later, the function will
  // always succeed.  The performance counter is monotonic.  See
  // https://msdn.microsoft.com/en-us/library/dn553408(v=vs.85).aspx
  static SOURCE_FORCEINLINE i64 Stamp() noexcept {
    alignas(8) LARGE_INTEGER ticks;
#ifndef NDEBUG
    const BOOL is_ok =
#endif
        ::QueryPerformanceCounter(&ticks);

    // On systems that run Windows XP or later, the function will always
    // succeed.  But we still check.  See
    // https://msdn.microsoft.com/en-us/library/ms644904(v=vs.85).aspx
    DCHECK_LAZY_EXIT(is_ok != FALSE, []() {
      return source::windows::windows_errno_code_last_error();
    });

    return ticks.QuadPart;
  }

  // Retrieves the frequency of the performance counter.  The frequency of the
  // performance counter is fixed at system boot and is consistent across all
  // processors.  Therefore, the frequency need only be queried upon application
  // initialization, and the result can be cached.
  static SOURCE_FORCEINLINE i64 Frequency() noexcept {
    alignas(8) static LARGE_INTEGER hpet_frequency;
    if (hpet_frequency.QuadPart != 0) return hpet_frequency.QuadPart;

#ifndef NDEBUG
    const BOOL is_ok =
#endif
        ::QueryPerformanceFrequency(&hpet_frequency);

    // On systems that run Windows XP or later, the function will always
    // succeed.  But we still check.  See
    // https://msdn.microsoft.com/en-us/library/ms644905(v=vs.85).aspx
    DCHECK_LAZY_EXIT(is_ok != FALSE, []() {
      return source::windows::windows_errno_code_last_error();
    });

    return hpet_frequency.QuadPart;
  }
#endif

  // Time any callable arg with void as return type.  |elapsed_seconds| is
  // HPET seconds elapsed.
  template <typename T, typename... Args>
  static inline typename std::enable_if_t<
      std::is_void_v<std::invoke_result_t<T, Args...>>>
  TimeIt(f64 &elapsed_seconds, T it, Args &&... args) noexcept(noexcept(T)) {
    alignas(8) const i64 start_hpetc_value{Stamp()};

    it(std::forward<Args>(args)...);

    elapsed_seconds =
        implicit_cast<f64>(Stamp() - start_hpetc_value) / Frequency();
  }

  // Time any callable arg with non-void as return type.  |elapsed_seconds| is
  // HPET seconds elapsed.
  template <typename T, typename... Args>
  static inline typename std::enable_if_t<
      !std::is_void_v<std::invoke_result_t<T, Args...>>,
      std::invoke_result_t<T, Args...>>
  TimeIt(f64 &elapsed_seconds, T it, Args &&... args) noexcept(noexcept(T)) {
    alignas(8) const i64 start_hpetc_value{Stamp()};
    const auto ret = it(std::forward<Args>(args)...);

    elapsed_seconds =
        implicit_cast<f64>(Stamp() - start_hpetc_value) / Frequency();

    return ret;
  }

 private:
  HpetTimer() = delete;
  HpetTimer(const HpetTimer &c) = delete;
  HpetTimer &operator=(const HpetTimer &c) = delete;
  HpetTimer(const HpetTimer &&c) = delete;
  HpetTimer &operator=(const HpetTimer &&c) = delete;
};
}  // namespace source::chrono

#endif  // BASE_INCLUDE_CHRONO_H_
