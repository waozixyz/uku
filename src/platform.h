#ifndef UKU_PLATFORM_H
#define UKU_PLATFORM_H

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
#define FLINT_ANDROID_BUILD 1
#else
#define FLINT_ANDROID_BUILD 0
#endif

#endif
