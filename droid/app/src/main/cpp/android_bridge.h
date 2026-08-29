#ifndef UKU_ANDROID_BRIDGE_H
#define UKU_ANDROID_BRIDGE_H

#include <stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

void android_bridge_init(void);
void android_bridge_apply_system_theme(void);
void android_bridge_set_soft_keyboard(int visible);
int android_bridge_bottom_reserved(void);
int android_bridge_share_text(const char *text, const char *title);
int android_bridge_share_file(const char *path, const char *mime_type,
                              const char *title, const char *extra_text);
int android_bridge_scan_qr(void);
int android_bridge_take_qr_scan_result(char *out, size_t out_size);

#if defined(__cplusplus)
}
#endif

#endif
