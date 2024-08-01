#!/sbin/bash
# By SakuraKyuo

OUTFD=$1

function ui_print() {
  echo -e "ui_print $1\nui_print" >> $OUTFD
}

function ui_printfile() {
  while IFS='' read -r line || $BB [[ -n "$line" ]]; do
    ui_print "$line";
  done < $1;
}

function failed(){
	ui_printfile /dev/tmp/install/log
	ui_print "- KernelSU Patch Failed."
	ui_print "- Please feedback to the developer with the screenshots."
	exit
}

function boot_execute_ab(){
	./lib/arm64-v8a/libksud.so boot-patch -b boot.img --magiskboot ./lib/arm64-v8a/libmagiskboot.so >> /dev/tmp/install/log
	if [[ ! "$?" == 0 ]]; then
		failed
	fi
	ui_printfile /dev/tmp/install/log
	mv /dev/tmp/install/kernelsu_patched_* /dev/tmp/install/kernelsu_boot.img
	dd if=/dev/tmp/install/kernelsu_boot.img of=/dev/block/by-name/boot$slot
	ui_print "- KernelSU Patch Done"
	exit
}

function boot_execute(){
	./lib/arm64-v8a/libksud.so boot-patch -b boot.img --magiskboot ./lib/arm64-v8a/libmagiskboot.so >> /dev/tmp/install/log
	if [[ ! "$?" == 0 ]]; then
		failed
	fi
	ui_printfile /dev/tmp/install/log
	mv /dev/tmp/install/kernelsu_patched_* /dev/tmp/install/kernelsu_boot.img
	ui_printfile /dev/tmp/install/log
	dd if=/dev/tmp/install/kernelsu_boot.img of=/dev/block/by-name/boot
	ui_print "- KernelSU Patch Done"
	exit
}

function init_boot_execute_ab(){
	./lib/arm64-v8a/libksud.so boot-patch -b init_boot.img --magiskboot ./lib/arm64-v8a/libmagiskboot.so >> /dev/tmp/install/log
	if [[ ! "$?" == 0 ]]; then
		failed
	fi
	ui_printfile /dev/tmp/install/log
	mv /dev/tmp/install/kernelsu_patched_* /dev/tmp/install/kernelsu_boot.img
	dd if=/dev/tmp/install/kernelsu_boot.img of=/dev/block/by-name/init_boot$slot
	ui_print "- KernelSU Patch Done"
	exit
}

function init_boot_execute(){
	./lib/arm64-v8a/libksud.so boot-patch -b init_boot.img --magiskboot ./lib/arm64-v8a/libmagiskboot.so >> /dev/tmp/install/log
	if [[ ! "$?" == 0 ]]; then
		failed
	fi
	ui_printfile /dev/tmp/install/log
	mv /dev/tmp/install/kernelsu_patched_* /dev/tmp/install/kernelsu_boot.img
	dd if=/dev/tmp/install/kernelsu_boot.img of=/dev/block/by-name/init_boot
	ui_print "- KernelSU Patch Done"
	exit
}

function main(){

cd /dev/tmp/install

chmod a+x ./lib/arm64-v8a/libksud.so
chmod a+x ./lib/arm64-v8a/libmagiskboot.so

slot=$(getprop ro.boot.slot_suffix)

if [[ ! "$slot" == "" ]]; then

	ui_print ""
	ui_print "- You are using A/B device."

	# Get kernel
	ui_print ""
	dd if=/dev/block/by-name/init_boot$slot of=/dev/tmp/install/init_boot.img
	if [[ "$?" == 0 ]]; then
		ui_print "- Detected init_boot partition."
		init_boot_execute_ab
	fi
	dd if=/dev/block/by-name/boot$slot of=/dev/tmp/install/boot.img
	if [[ "$?" == 0 ]]; then
		ui_print "- Detected boot partition."
		boot_execute_ab
	fi

else

	ui_print "You are using A Only device."

	# Get kernel
	ui_print ""
	dd if=/dev/block/by-name/init_boot of=/dev/tmp/install/init_boot.img
	if [[ "$?" == 0 ]]; then
		ui_print "- Detected init_boot partition."
		init_boot_execute
	fi
	dd if=/dev/block/by-name/boot of=/dev/tmp/install/boot.img
	if [[ "$?" == 0 ]]; then
		ui_print "- Detected boot partition."
		boot_execute
	fi

fi

}

main
