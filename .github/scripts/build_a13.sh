#!/bin/bash
set -euo pipefail

build_from_image() {
	export TITLE
	TITLE=kernel-aarch64-${1//Image-/}

	echo "[+] title: $TITLE"
	echo '[+] Building Image.gz'
	$GZIP -n -k -f -9 Image >Image.gz

	echo '[+] Building boot.img'
	$MKBOOTIMG --header_version 4 --kernel Image --output boot.img
	$AVBTOOL add_hash_footer --partition_name boot --partition_size $((64 * 1024 * 1024)) --image boot.img --algorithm SHA256_RSA2048 --key ../kernel-build-tools/linux-x86/share/avb/testkey_rsa2048.pem

	echo '[+] Building boot-gz.img'
	$MKBOOTIMG --header_version 4 --kernel Image.gz --output boot-gz.img
	$AVBTOOL add_hash_footer --partition_name boot --partition_size $((64 * 1024 * 1024)) --image boot-gz.img --algorithm SHA256_RSA2048 --key ../kernel-build-tools/linux-x86/share/avb/testkey_rsa2048.pem

	echo '[+] Building boot-lz4.img'
	$MKBOOTIMG --header_version 4 --kernel Image.lz4 --output boot-lz4.img
	$AVBTOOL add_hash_footer --partition_name boot --partition_size $((64 * 1024 * 1024)) --image boot-lz4.img --algorithm SHA256_RSA2048 --key ../kernel-build-tools/linux-x86/share/avb/testkey_rsa2048.pem

	echo '[+] Compress images'
	for image in boot*.img; do
		$GZIP -n -f -9 "$image"
        mv "$image".gz "${1//Image-/}"-"$image".gz
	done

	echo '[+] Images to upload'
	find . -type f -name "*.gz"

	# find . -type f -name "*.gz" -exec python3 "$GITHUB_WORKSPACE"/KernelSU/scripts/ksubot.py {} +
}

for dir in Image*; do
	if [ -d "$dir" ]; then
		echo "----- Building $dir -----"
		cd "$dir"
		build_from_image "$dir"
		cd ..
	fi
done
