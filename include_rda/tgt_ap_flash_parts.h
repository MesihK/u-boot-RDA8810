#ifndef __TGT_AP_FLASH_PARTS_H__
#define __TGT_AP_FLASH_PARTS_H__

#define MTDPARTS_DEF			\
		"2M@0(bootloader),"	\
		"2M(factorydata),"	\
		"2M(misc),"	        \
		"4M(modem),"            \
		MTDPARTS_ANDROID_DEF

#define MTDPARTS_ANDROID_DEF		\
		"-(rootfs)"

/*
kernel  need handle mtd from 0, so define a dummy partions whose
size is bootloader+factorydata+modem+boot+recovery
*/
#define MTDPARTS_KERNEL_DEF		\
		"10M@0(dummy),"		\
		MTDPARTS_ANDROID_DEF
#endif

