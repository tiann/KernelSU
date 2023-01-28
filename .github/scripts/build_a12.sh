build_from_image(){
	export TITLE=kernel-aarch64-$(echo $1 | sed 's/Image-//g')
	echo "[+] title: $TITLE"
	echo "[+] Download prebuilt ramdisk"
	curl -Lo gki-kernel.zip https://dl.google.com/android/gki/gki-certified-boot-android12-5.10-2021-11_r1.zip
	unzip gki-kernel.zip && rm gki-kernel.zip

	echo "[+] Unpack prebuilt boot.img"
	BOOT_IMG=$(find . -maxdepth 1 -name "boot*.img")
	$UNPACK_BOOTIMG --boot_img=$BOOT_IMG
	rm $BOOT_IMG

	echo "[+] Building Image.gz"
	cat Image | $GZIP -n -f -9 > Image.gz

	echo "[+] Building boot.img"
	$MKBOOTIMG --header_version 4 --kernel Image --output boot.img --ramdisk out/ramdisk --os_version 12.0.0 --os_patch_level 2021-11
	$AVBTOOL add_hash_footer --partition_name boot --partition_size $((64*1024*1024)) --image boot.img --algorithm SHA256_RSA2048 --key ../kernel-build-tools/linux-x86/share/avb/testkey_rsa2048.pem

	echo "[+] Building boot-gz.img"
	$MKBOOTIMG --header_version 4 --kernel Image.gz --output boot-gz.img --ramdisk out/ramdisk --os_version 12.0.0 --os_patch_level 2021-11
	$AVBTOOL add_hash_footer --partition_name boot --partition_size $((64*1024*1024)) --image boot-gz.img --algorithm SHA256_RSA2048 --key ../kernel-build-tools/linux-x86/share/avb/testkey_rsa2048.pem

	echo "[+] Building boot-lz4.img"
	$MKBOOTIMG --header_version 4 --kernel Image.lz4 --output boot-lz4.img --ramdisk out/ramdisk --os_version 12.0.0 --os_patch_level 2021-11
	$AVBTOOL add_hash_footer --partition_name boot --partition_size $((64*1024*1024)) --image boot-lz4.img --algorithm SHA256_RSA2048 --key ../kernel-build-tools/linux-x86/share/avb/testkey_rsa2048.pem

    echo "[+] Compress images"
    for image in boot*.img; do
        $GZIP -n -f -9 $image
    done

	python3 $GITHUB_WORKSPACE/KernelSU/scripts/ksubot.py
}

for dir in Image*; do
	if [ -d "$dir" ]; then
		echo "----- Building $dir -----"
		cd $dir
		build_from_image $dir
		cd ..
	fi
done