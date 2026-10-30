---
description: Lesser-known KernelSU features, such as loading a custom shell rc file for su from /data/adb/ksu/.ksurc.
---

# Hidden features

## .ksurc

By default, `/system/bin/sh` loads `/system/etc/mkshrc`.

You can make su load customized rc file by creating a `/data/adb/ksu/.ksurc` file.
