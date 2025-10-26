/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Parts of this file derived from Chromium's base/cpu.cc.

#include "rtc_base/system/arch.h"
#include "system_wrappers/include/cpu_features_wrapper.h"
#include "system_wrappers/include/field_trial.h"

#if defined(WEBRTC_ARCH_X86_FAMILY) && defined(_MSC_VER)
#include <intrin.h>
#endif

#include <cstring>

namespace webrtc {


// No CPU feature is available => straight C path.
int GetCPUInfoNoASM(CPUFeature feature) {
  (void)feature;
  return 0;
}

#if defined(WEBRTC_ARCH_X86_FAMILY)

#ifndef _MSC_VER
// Intrinsic for "cpuid".
#if defined(__pic__) && defined(__i386__)
static inline void __cpuid(int cpu_info[4], int info_type) {
  __asm__ volatile(
      "mov %%ebx, %%edi\n"
      "cpuid\n"
      "xchg %%edi, %%ebx\n"
      : "=a"(cpu_info[0]), "=D"(cpu_info[1]), "=c"(cpu_info[2]),
        "=d"(cpu_info[3])
      : "a"(info_type));
}
#else
static inline void __cpuid(int cpu_info[4], int info_type) {
  __asm__ volatile("cpuid\n"
                   : "=a"(cpu_info[0]), "=b"(cpu_info[1]), "=c"(cpu_info[2]),
                     "=d"(cpu_info[3])
                   : "a"(info_type), "c"(0));
}
#endif
#endif  // _MSC_VER
#endif  // WEBRTC_ARCH_X86_FAMILY

#if defined(WEBRTC_ARCH_X86_FAMILY)

#if defined(WEBRTC_ENABLE_AVX2)
// xgetbv returns the value of an Intel Extended Control Register (XCR).
// Currently only XCR0 is defined by Intel so `xcr` should always be zero.
// Enhanced with safety checks to avoid illegal instructions on unsupported CPUs.
static uint64_t xgetbv(uint32_t xcr) {
  // First check if XGETBV is supported by checking OSXSAVE bit
  int cpu_info[4];

  // Use the local __cpuid function defined in this file
#if defined(_MSC_VER)
  __cpuid(cpu_info, 1);
#else
  __cpuid(cpu_info, 1);
#endif

  // If OSXSAVE bit is not set, XGETBV is not supported
  if ((cpu_info[2] & 0x08000000) == 0) {
    return 0;  // Return safe default value
  }

  // Only execute XGETBV if it's supported
#if defined(_MSC_VER)
  try {
    return _xgetbv(xcr);
  } catch (...) {
    return 0;  // Return safe default value on any error
  }
#else
  uint32_t eax, edx;
  try {
    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(xcr));
    return (static_cast<uint64_t>(edx) << 32) | eax;
  } catch (...) {
    return 0;  // Return safe default value on any error
  }
#endif  // _MSC_VER
}
#endif  // WEBRTC_ENABLE_AVX2

// Actual feature detection for x86.
int GetCPUInfo(CPUFeature feature) {
  int cpu_info[4];
  __cpuid(cpu_info, 1);

  if (feature == kSSE2) {
    return 0 != (cpu_info[3] & 0x04000000);
  }

  if (feature == kSSE3) {
    return 0 != (cpu_info[2] & 0x00000001);
  }

#if defined(WEBRTC_ENABLE_AVX2)
  if (feature == kAVX2) {
    // Check if field trial disables AVX2 support
    try {
      if (webrtc::field_trial::IsEnabled("WebRTC-Avx2SupportKillSwitch")) {
        return 0;
      }
    } catch (...) {
      // If field trial system is not initialized, disable AVX2 for safety
      return 0;
    }

    // Perform comprehensive CPU capability check
    int cpu_info7[4];
    __cpuid(cpu_info7, 0);
    if (cpu_info7[0] < 7) {
      return 0;
    }

    __cpuid(cpu_info7, 7);

    // Check AVX support
    if (!(cpu_info[2] & 0x10000000)) {  // AVX bit
      return 0;
    }

    // Check XSAVE support
    if (!(cpu_info[2] & 0x04000000)) {  // XSAVE bit
      return 0;
    }

    // Check OSXSAVE bit
    if (!(cpu_info[2] & 0x08000000)) {  // OSXSAVE bit
      return 0;
    }

    // Check OS-level XSAVE support
    uint64_t xcr0_value = xgetbv(0);
    if ((xcr0_value & 0x00000006) != 6) {  // x87 + SSE state enabled
      return 0;
    }

    // Check CPU AVX2 support
    if (!(cpu_info7[1] & 0x00000020)) {  // AVX2 bit
      return 0;
    }

    // All checks passed - AVX2 is supported
    return 1;
  }
#endif  // WEBRTC_ENABLE_AVX2

  // Unknown feature
  return 0;
}
#else
// Default to straight C for other platforms.
int GetCPUInfo(CPUFeature feature) {
  (void)feature;
  return 0;
}
#endif

}  // namespace webrtc
