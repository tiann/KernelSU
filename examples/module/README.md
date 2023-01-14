# ksu_config

This is a example kernel module for KernelSU, making it easy to change expected_* to test self-signed manager.

## Usage

See current config

```
cat /sys/module/ksu_config/parameters/expected_hash
cat /sys/module/ksu_config/parameters/expected_size
```

Modify config

```
echo 12345 > /sys/module/ksu_config/parameters/expected_hash
echo 12345 > /sys/module/ksu_config/parameters/expected_size
```

> You can use tools/check_v2.c to get the signature of your apk