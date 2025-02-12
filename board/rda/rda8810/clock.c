#include <common.h>
#include <errno.h>
#include <asm/arch/rda_iomap.h>
#include <asm/io.h>
#include <asm/arch/reg_sysctrl.h>
#include <asm/arch/hwcfg.h>
#include <asm/arch/ispi.h>
#include <asm/arch/rda_sys.h>
#include <asm/arch/defs_mdcom.h>
#include <asm/arch/spl_board_info.h>
#include "clock_config.h"
#include "debug.h"
#include "ddr3.h"
#include "tgt_ap_clock_config.h"

#if (PMU_VBUCK1_VAL < 0 || PMU_VBUCK1_VAL > 15)
#error "Invalid PMU_VBUCK1_VAL"
#endif
#if (PMU_VBUCK3_VAL < 0 || PMU_VBUCK3_VAL > 15)
#error "Invalid PMU_VBUCK3_VAL"
#endif
#ifdef PMU_VBUCK4_VAL
#if (PMU_VBUCK4_VAL < 0 || PMU_VBUCK4_VAL > 15)
#error "Invalid PMU_VBUCK4_VAL"
#endif
#endif

//#define DO_SIMPLE_DDR_TEST
//#define DO_DDR_PLL_DEBUG

#ifdef _TGT_AP_DDR_AUTO_CALI_ENABLE
extern UINT8 g_debug_ddr3;
static UINT8 rda2_result[8] = {0};

extern ulong __ddrcal_start;
extern ulong __start;

static u16 * spl_text_start_addr;
static u16 * ddr_cal_val_start_addr;
#endif

enum {
	AP_CPU_CLK_IDX = 0,
	AP_BUS_CLK_IDX,
	AP_MEM_CLK_IDX,
	AP_USB_CLK_IDX,
};

static int pll_enabled(int idx)
{
	if ((hwp_sysCtrlAp->Cfg_Pll_Ctrl[idx] &
			(SYS_CTRL_AP_AP_PLL_ENABLE_MASK |
			 SYS_CTRL_AP_AP_PLL_LOCK_RESET_MASK)) ==
			(SYS_CTRL_AP_AP_PLL_ENABLE_ENABLE |
			 SYS_CTRL_AP_AP_PLL_LOCK_RESET_NO_RESET))
		return 1;
	else
		return 0;
}

static int usb_in_use = 0;

static void check_usb_usage(void)
{
	unsigned int mask = SYS_CTRL_AP_BUS_SEL_FAST_SLOW |
		SYS_CTRL_AP_PLL_LOCKED_BUS_MASK |
		SYS_CTRL_AP_PLL_LOCKED_USB_MASK;
	unsigned int reg = SYS_CTRL_AP_BUS_SEL_FAST_FAST |
		SYS_CTRL_AP_PLL_LOCKED_BUS_LOCKED |
		SYS_CTRL_AP_PLL_LOCKED_USB_LOCKED;

	if ((hwp_sysCtrlAp->Sel_Clock & mask) == reg &&
			pll_enabled(AP_BUS_CLK_IDX) &&
			pll_enabled(AP_USB_CLK_IDX))
		usb_in_use = 1;
	else
		usb_in_use = 0;
}

#ifdef DO_SIMPLE_DDR_TEST

void mem_test_write(void)
{
	unsigned short flag, mask;
	unsigned int i, a, b;
	volatile unsigned int *addr;

	printf("write ddr test!!!!!!\n");

 	addr = (volatile unsigned int *)(0x83000000);
	while((uint32)addr < 0x85000000)
	{
	    for (i=0; i<16; i++) {
		flag = 0x0101 << i;
	        mask = ~flag;
	        a = mask<<16 | mask;
	        b = mask<<16 |flag ;
	        *(addr++) = a;
	        *(addr++) = a;
	        *(addr++) = b;
	        *(addr++) = a;
	    }
	}
}

void mem_test_read(void)
{
	unsigned short flag, mask;
	unsigned int i, temp, a, b, i_err;
	volatile unsigned int *addr;

	printf("read ddr!!!!\n");

	addr = (volatile unsigned int *)(0x83000000);
	i_err=0;

	while((uint32)addr < 0x85000000)
	{
	    for (i=0; i<16; i++) {
	        flag = 0x0101 << i;
	        mask = ~flag;
	        a = mask<<16 | mask;
	        b = mask<<16 |flag ;
	        temp = *(addr++);
	        if ( temp != a) {
		    i_err=i_err+1;
		    if (i_err > 2 )  break;
	            printf("addr: 0x");
	            print_u32((UINT32)addr);
	            printf("\n");
	            printf("error value: 0x");
	            print_u32(temp);
	            printf("\n");
	            printf("right value: 0x");
	            print_u32(a);
	            printf("\n");
	        }
	        temp = *(addr++);
	        if ( temp != a) {
		    i_err=i_err+1;
		    if (i_err > 2 )  break;
	            printf("addr: 0x");
	            print_u32((UINT32)addr);
	            printf("\n");
	            printf("error value: 0x");
	            print_u32(temp);
	            printf("\n");
	            printf("right value: 0x");
	            print_u32(a);
	            printf("\n");
	        }
	        temp = *(addr++);
	        if ( temp != b) {
		    i_err=i_err+1;
		    if (i_err > 2 )  break;
	            printf("addr: 0x");
	            print_u32((UINT32)addr);
	            printf("\n");
	            printf("error value: 0x");
	            print_u32(temp);
	            printf("\n");
	            printf("right value: 0x");
	            print_u32(b);
	            printf("\n");
	        }
	        temp = *(addr++);
	        if ( temp != a) {
		    i_err=i_err+1;
		    if (i_err > 2 )  break;
	            printf("addr: 0x");
	            print_u32((UINT32)addr);
	            printf("\n");
	            printf("error value: 0x");
	            print_u32(temp);
	            printf("\n");
	            printf("right value: 0x");
	            print_u32(a);
	            printf("\n");
	        }
	    }
	}
	printf("test complete!\n");
}

#endif /* DO_SIMPLE_DDR_TEST */

#ifndef CONFIG_RDA_FPGA

// PMU bit fields
#define PMU_SET_BITFIELD(dword, bitfield, value) \
	(((dword) & ~(bitfield ## _MASK)) | (bitfield(value)))

#define RDA_PMU_VBUCK1_BIT_ACT(n)        (((n)&0xf)<<12)
#define RDA_PMU_VBUCK1_BIT_ACT_MASK      (0xf<<12)
#define RDA_PMU_VBUCK1_BIT_ACT_SHIFT     (12)

#define RDA_PMU_VBUCK4_BIT_ACT_SHIFT     (4)
#define RDA_PMU_VBUCK4_BIT_ACT_MASK      (0xf<<4)
#define RDA_PMU_VBUCK4_BIT_ACT(n)        (((n)&0xf)<<4)

#define RDA_PMU_VBUCK3_BIT_ACT_SHIFT     (12)
#define RDA_PMU_VBUCK3_BIT_ACT_MASK      (0xf<<12)
#define RDA_PMU_VBUCK3_BIT_ACT(n)        (((n)&0xf)<<12)

struct pll_freq {
	UINT32 freq_mhz;
	UINT16 major;
	UINT16 minor;
	UINT16 with_div;
	UINT16 div;
};

typedef enum {
	PLL_REG_CPU_BASE = 0x00,
	PLL_REG_BUS_BASE = 0x20,
	PLL_REG_MEM_BASE = 0x60,
	PLL_REG_USB_BASE = 0x80,
} PLL_REG_BASE_INDEX_t;

typedef enum {
	PLL_REG_OFFSET_01H = 1,
	PLL_REG_OFFSET_02H = 2,
	PLL_REG_OFFSET_DIV = 3,
	PLL_REG_OFFSET_04H = 4,
	PLL_REG_OFFSET_MAJOR = 5,
	PLL_REG_OFFSET_MINOR = 6,
	PLL_REG_OFFSET_07H = 7,
} PLL_REG_OFFSET_INDEX_t;

/* MajorMinor = 2^29 * (freq / 26), only hign 32bits */
static struct pll_freq pll_freq_table[] = {
	/* MHz Major   Minor   div */
	{1600, 0x7B13, 0xB138, 0, 0x0000},
	{1200, 0x5C4E, 0xC4EC, 0, 0x0000},
	{1150, 0x5876, 0x2762, 0, 0x0000},
	{1100, 0x549D, 0x89D8, 0, 0x0000},
	{1050, 0x50C4, 0xEC4E, 0, 0x0000},
	{1020, 0x4E76, 0x2762, 0, 0x0000},
	{1010, 0x4DB1, 0x3B13, 0, 0x0000},
	{1000, 0x4CEC, 0x4EC4, 0, 0x0000},
	{ 988, 0x4C00, 0x0000, 0, 0x0000},
	{ 800, 0x3D89, 0xD89C, 0, 0x0000},
	{ 780, 0x3C00, 0x0000, 0, 0x0000},
	{ 750, 0x39B1, 0x3B13, 0, 0x0000},
	{ 600, 0x2E27, 0x6274, 0, 0x0000},
	{ 520, 0x2800, 0x0000, 0, 0x0000},
	{ 519, 0x27EC, 0x4EC4, 1, 0x0007},
	{ 500, 0x2676, 0x2762, 1, 0x0007},
	{ 480, 0x24EC, 0x4EC4, 0, 0x0000},
	{ 455, 0x2300, 0x0000, 1, 0x0007},
	{ 416, 0x2000, 0x0000, 1, 0x0007},
	/* 800M div 1, PLL:800M  DDR:400M for mem pll only */
	{ 400, 0x1EC4, 0xEC4C, 1, 0x0007},
	{ 355, 0x369D, 0x89D8, 1, 0x0007},
	/* 702M div 1, PLL:702M  DDR:351M for mem pll only */
	{ 351, 0x3600, 0x0000, 1, 0x0007},
	{ 333, 0x333B, 0x13B1, 1, 0x0007},
	/* 624M div 1, PLL:624M  DDR:312M for mem pll only */
	{ 312, 0x3000, 0x0000, 1, 0x0007},
	{ 290, 0x2C9D, 0x89D8, 1, 0x0007},
	/* 520M div 1, PLL:520M  DDR:260M for mem pll only */
	{ 260, 0x2800, 0x0000, 1, 0x0007},
	/* 800M div 2, PLL:400M  DDR:200M for mem pll only */
	{ 200, 0x3D89, 0xD89C, 1, 0x0006},
	/* DDR:156M for mem pll only */
	{ 156, 0x3000, 0x0000, 1, 0x0006},
	/* 800M div 4, PLL:200M  DDR:100M for mem pll only */
	{ 100, 0x3D89, 0xD89C, 1, 0x0005},
	/* 800M div 8, PLL:100M  DDR:50M for mem pll only */
	{  50, 0x3D89, 0xD89C, 1, 0x0004},
};

static  struct clock_config *g_clock_config;

#ifdef DO_DDR_PLL_DEBUG
static struct clock_config clock_debug_config;
static UINT32 ddrfreq = 400, ddr32bit = 0;
#endif

unsigned int * const g_reg_cache =
	(unsigned int *)RDA_MDCOM_CHN_AT_BUF_ADD_WRITE;
unsigned short g_reg_cnt = 0;
const unsigned short g_reg_to_save[] = {
	 0x65,  0x69, 0x100, 0x120, 0x140, 0x160,
	0x107, 0x127, 0x147, 0x167, 0x18a, };

#define AP_ISPI_REG_CACHE_FLAG      (0xac1561ca)

static void reg_cache_start(void)
{
	*g_reg_cache = AP_ISPI_REG_CACHE_FLAG;
}

static void reg_cache_save(unsigned int idx, unsigned int value)
{
	int i;

	for (i = 0; i < g_reg_cnt; i++) {
		if ((*(g_reg_cache + 2 + i) >> 16) == idx) {
			*(g_reg_cache + 2 + i) = (idx << 16) | (value & 0xFFFF);
			return;
		}
	}

	*(g_reg_cache + 2 + g_reg_cnt) = (idx << 16) | (value & 0xFFFF);
	++g_reg_cnt;
}

static void reg_cache_end(void)
{
	*(g_reg_cache + 1) = g_reg_cnt;
	*(g_reg_cache + 2 + g_reg_cnt) = AP_ISPI_REG_CACHE_FLAG;
}

static void ispi_reg_write_and_save(unsigned int idx, unsigned int value)
{
	int i;

	ispi_reg_write(idx, value);

	for (i = 0; i < ARRAY_SIZE(g_reg_to_save); i++) {
		if (idx == g_reg_to_save[i])
			reg_cache_save(idx, value);
	}
}

static void sys_shutdown_pll(void)
{
	int i;

	hwp_sysCtrlAp->REG_DBG = AP_CTRL_PROTECT_UNLOCK;

	if (usb_in_use) {
		hwp_sysCtrlAp->Sel_Clock = SYS_CTRL_AP_SLOW_SEL_RF_RF
			| SYS_CTRL_AP_CPU_SEL_FAST_SLOW
			| SYS_CTRL_AP_BUS_SEL_FAST_FAST
			| SYS_CTRL_AP_TIMER_SEL_FAST_FAST;
	} else {
		hwp_sysCtrlAp->Sel_Clock = SYS_CTRL_AP_SLOW_SEL_RF_RF
			| SYS_CTRL_AP_CPU_SEL_FAST_SLOW
			| SYS_CTRL_AP_BUS_SEL_FAST_SLOW
			| SYS_CTRL_AP_TIMER_SEL_FAST_FAST;
	}

	for (i = 0; i < 3; i++) {
		/* In download mode, rom code has been set ap bus*/
		if (usb_in_use) {
			if (i == AP_BUS_CLK_IDX) // ap bus
				continue;
		}
		hwp_sysCtrlAp->Cfg_Pll_Ctrl[i] =
			SYS_CTRL_AP_AP_PLL_ENABLE_POWER_DOWN |
			SYS_CTRL_AP_AP_PLL_LOCK_RESET_RESET;
	}
}

static const unsigned short pll_patch_reg_base[] =
{ PLL_REG_CPU_BASE, PLL_REG_BUS_BASE, PLL_REG_MEM_BASE, PLL_REG_USB_BASE };
static char pll_patch_multi2[ARRAY_SIZE(pll_patch_reg_base)] = { 0, };

void pll_patch_reg_set(unsigned int mask, unsigned int offset,
		unsigned int value)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(pll_patch_reg_base); i++) {
		if ((mask & (1 << i)) == 0) {
			continue;
		}

		if (offset == PLL_REG_OFFSET_02H) {
			// Keep the original refmulti2 flag
			value |= pll_patch_multi2[i] ? (1 << 8) : 0;
			if (i == AP_USB_CLK_IDX && rda_metal_id_get() >= 7)
				value ^= (1 << 8);
		}
		ispi_reg_write_and_save(pll_patch_reg_base[i] + offset, value);
	}
}

unsigned int pll_patch_reg_check(unsigned int mask, unsigned int check,
		unsigned int count)
{
	int i, j;
	unsigned int value;
	unsigned int lockMask = mask;

	for (j = 0; j < count; j++) {
		for (i = 0; i < ARRAY_SIZE(pll_patch_reg_base); i++) {
			if ((mask & (1 << i)) == 0) {
				continue;
			}

			value = ispi_reg_read(pll_patch_reg_base[i] + PLL_REG_OFFSET_04H);
			if ((value & check) != check) {
				lockMask &= ~(1 << i);
				if (lockMask == 0) {
					goto _exit;
				}
			}
		}
	}

_exit:
	return lockMask;
}

static void pll_setup_patch(unsigned int mask)
{
	unsigned int cnt;
	unsigned int lockMask;
	unsigned int vcore;

	if (mask == 0)
		return;

	/* AP ISPI read requires low vcore & high vpad */
	ispi_open(1);
	vcore = pmu_reg_read(0x2F);
	pmu_reg_write(0x05, 0x4FF8);
	pmu_reg_write(0x2F, 0x8444);
	ispi_open(0);
	udelay(5000);

	lockMask = pll_patch_reg_check(mask, 0xE000, 10);
	mask &= ~lockMask;
	if (mask == 0) {
		goto _exit;
	}

	pll_patch_reg_set(mask, PLL_REG_OFFSET_07H, 0x0010);
	pll_patch_reg_set(mask, PLL_REG_OFFSET_02H, 0x02c9);

	cnt = 0;
	do {
		if (cnt < 5 || (cnt % 100) == 0)
			printf("Applying PLL enable patch: mask=0x%x\n", mask);

		pll_patch_reg_set(mask, PLL_REG_OFFSET_01H, (cnt & 0x1) ? 0x7e70 : 0x7e7f);

		lockMask = pll_patch_reg_check(mask, 0xA000, 1);
		if (lockMask) {
			pll_patch_reg_set(lockMask, PLL_REG_OFFSET_01H, 0x7e78);
			pll_patch_reg_set(lockMask, PLL_REG_OFFSET_02H, 0x0209);
			udelay(1000);
			lockMask = pll_patch_reg_check(lockMask, 0xE000, 10);
			mask &= ~lockMask;
			if (mask == 0)
				break;
		}

		cnt++;
	} while (1);

	printf("PLL enable patch done: %d\n", cnt);

_exit:
	ispi_open(1);
	pmu_reg_write(0x2F, vcore);
	ispi_open(0);
	udelay(5000);
}

static void sys_setup_pll(void)
{
	int i;
	UINT32 mask;
	UINT32 locked;
	int cnt = 10; //10us, according to IC, the pll must be locked
	UINT32 pllMask = 0;

	hwp_sysCtrlAp->REG_DBG = AP_CTRL_PROTECT_UNLOCK;

	for (i = 0; i < 3; i++) {
		/* In download mode, rom code has been set ap bus*/
		if (usb_in_use) {
			if (i == AP_BUS_CLK_IDX) // ap bus
				continue;
		}

		pllMask |= (1 << i);

		if (AP_MEM_CLK_IDX == i)
			hwp_sysCtrlAp->Cfg_Pll_Ctrl[i] =
				SYS_CTRL_AP_AP_PLL_ENABLE_ENABLE |
				SYS_CTRL_AP_AP_PLL_LOCK_RESET_NO_RESET |
				SYS_CTRL_AP_AP_PLL_LOCK_NUM_LOW(1)|
				SYS_CTRL_AP_AP_PLL_LOCK_NUM_HIGH(30);
		else
			hwp_sysCtrlAp->Cfg_Pll_Ctrl[i] =
				SYS_CTRL_AP_AP_PLL_ENABLE_ENABLE |
				SYS_CTRL_AP_AP_PLL_LOCK_RESET_NO_RESET |
				SYS_CTRL_AP_AP_PLL_LOCK_NUM_LOW(6)|
				SYS_CTRL_AP_AP_PLL_LOCK_NUM_HIGH(30);
	}

	pll_setup_patch(pllMask);

	mask = SYS_CTRL_AP_PLL_LOCKED_CPU_MASK
	    | SYS_CTRL_AP_PLL_LOCKED_BUS_MASK
	    | SYS_CTRL_AP_PLL_LOCKED_MEM_MASK
	    //| SYS_CTRL_AP_PLL_LOCKED_USB_MASK
	    ;
	locked = SYS_CTRL_AP_PLL_LOCKED_CPU_LOCKED
	    | SYS_CTRL_AP_PLL_LOCKED_BUS_LOCKED
	    | SYS_CTRL_AP_PLL_LOCKED_MEM_LOCKED
	    //| SYS_CTRL_AP_PLL_LOCKED_USB_LOCKED
	    ;

	while (((hwp_sysCtrlAp->Sel_Clock & mask) != locked) && cnt) {
		udelay(1);
		cnt--;
	}
	if (cnt == 0) {
		printf("WARNING, cannot lock cpu/bus/mem pll 0x%08x ",
			hwp_sysCtrlAp->Sel_Clock);
		printf("but we run anyway ...\n");
	}

	for (i = 0; i < 3; i++) {
		hwp_sysCtrlAp->Cfg_Pll_Ctrl[i] |=
		    SYS_CTRL_AP_AP_PLL_CLK_FAST_ENABLE_ENABLE;
	}

	hwp_sysCtrlAp->Sel_Clock = SYS_CTRL_AP_SLOW_SEL_RF_RF
	    | SYS_CTRL_AP_CPU_SEL_FAST_FAST
	    | SYS_CTRL_AP_BUS_SEL_FAST_FAST
	    | SYS_CTRL_AP_TIMER_SEL_FAST_FAST;
}

static void sys_setup_clk(void)
{
	// Disable some power-consuming clocks
#ifdef CONFIG_VPU_TEST
	hwp_sysCtrlAp->Clk_APO_Enable = SYS_CTRL_AP_ENABLE_APOC_VPU;
	hwp_sysCtrlAp->Clk_MEM_Enable = SYS_CTRL_AP_ENABLE_MEM_VPU;
#else
	hwp_sysCtrlAp->Clk_APO_Disable = SYS_CTRL_AP_DISABLE_APOC_VPU;
	hwp_sysCtrlAp->Clk_MEM_Disable = SYS_CTRL_AP_DISABLE_MEM_VPU;
#endif

	// Init clock gating mode
	hwp_sysCtrlAp->Clk_CPU_Mode = 0;
#ifdef CONFIG_VPU_TEST
	hwp_sysCtrlAp->Clk_AXI_Mode = SYS_CTRL_AP_MODE_AXI_DMA_MANUAL | SYS_CTRL_AP_MODE_APB0_CONF_MANUAL;
#else
	hwp_sysCtrlAp->Clk_AXI_Mode = SYS_CTRL_AP_MODE_AXI_DMA_MANUAL;
#endif
	hwp_sysCtrlAp->Clk_AXIDIV2_Mode = 0;
	hwp_sysCtrlAp->Clk_GCG_Mode = SYS_CTRL_AP_MODE_GCG_GOUDA_MANUAL
				| SYS_CTRL_AP_MODE_GCG_CAMERA_MANUAL;
	hwp_sysCtrlAp->Clk_AHB1_Mode = 0;
	hwp_sysCtrlAp->Clk_APB1_Mode = 0;
	hwp_sysCtrlAp->Clk_APB2_Mode = 0;
#ifdef CONFIG_VPU_TEST
	hwp_sysCtrlAp->Clk_MEM_Mode = SYS_CTRL_AP_MODE_CLK_MEM_MANUAL;
#else
	hwp_sysCtrlAp->Clk_MEM_Mode = 0;
#endif
	hwp_sysCtrlAp->Clk_APO_Mode = SYS_CTRL_AP_MODE_APOC_VPU_MANUAL;

	// Init module frequency
	hwp_sysCtrlAp->Cfg_Clk_AP_CPU = g_clock_config->CLK_CPU;
	hwp_sysCtrlAp->Cfg_Clk_AP_AXI = g_clock_config->CLK_AXI;
	hwp_sysCtrlAp->Cfg_Clk_AP_GCG = g_clock_config->CLK_GCG;

	if (!usb_in_use) {
		hwp_sysCtrlAp->Cfg_Clk_AP_AHB1 = g_clock_config->CLK_AHB1;
	}

	hwp_sysCtrlAp->Cfg_Clk_AP_APB1 = g_clock_config->CLK_APB1;
	hwp_sysCtrlAp->Cfg_Clk_AP_APB2 = g_clock_config->CLK_APB2;
	hwp_sysCtrlAp->Cfg_Clk_AP_MEM = g_clock_config->CLK_MEM;
	hwp_sysCtrlAp->Cfg_Clk_AP_GPU = g_clock_config->CLK_GPU;
	hwp_sysCtrlAp->Cfg_Clk_AP_VPU = g_clock_config->CLK_VPU;
	hwp_sysCtrlAp->Cfg_Clk_AP_VOC = g_clock_config->CLK_VOC;
	hwp_sysCtrlAp->Cfg_Clk_AP_SFLSH = g_clock_config->CLK_SFLSH;
}

static void print_sys_reg(char *name, UINT32 value)
{
#if 0
	printf("clk %s = %lx\n", name, value);
#endif
}

static void sys_dump_clk(void)
{
	print_sys_reg("CPU", hwp_sysCtrlAp->Cfg_Clk_AP_CPU);
	print_sys_reg("AXI", hwp_sysCtrlAp->Cfg_Clk_AP_AXI);
	print_sys_reg("GCG", hwp_sysCtrlAp->Cfg_Clk_AP_GCG);
	print_sys_reg("AHB1", hwp_sysCtrlAp->Cfg_Clk_AP_AHB1);
	print_sys_reg("APB1", hwp_sysCtrlAp->Cfg_Clk_AP_APB1);
	print_sys_reg("APB2", hwp_sysCtrlAp->Cfg_Clk_AP_APB2);
	print_sys_reg("MEM", hwp_sysCtrlAp->Cfg_Clk_AP_MEM);
	print_sys_reg("GPU", hwp_sysCtrlAp->Cfg_Clk_AP_GPU);
	print_sys_reg("VPU", hwp_sysCtrlAp->Cfg_Clk_AP_VPU);
	print_sys_reg("VOC", hwp_sysCtrlAp->Cfg_Clk_AP_VOC);
	print_sys_reg("SFLSH", hwp_sysCtrlAp->Cfg_Clk_AP_SFLSH);
}

static int pll_freq_set(UINT32 reg_base, UINT32 freq_mhz)
{
	int i;
	struct pll_freq *freq;
	unsigned int major, minor;
	unsigned short value_02h;

	/* find pll_freq */
	for (i = 0; i < ARRAY_SIZE(pll_freq_table); i++) {
		if (pll_freq_table[i].freq_mhz == freq_mhz)
			break;
	}
	if (i >= ARRAY_SIZE(pll_freq_table)) {
		printf("pll_freq_set, fail to find freq\n");
		return -1;
	}

	freq = &pll_freq_table[i];
	if (freq->with_div && (reg_base == PLL_REG_MEM_BASE)) {
		ispi_reg_write_and_save(reg_base + PLL_REG_OFFSET_DIV,
				freq->div);
		// Calculate the real MEM PLL freq
		freq_mhz *= (1 << (8 - (freq->div & 0x7)));
	}
	if (freq_mhz >= 800 || reg_base == PLL_REG_USB_BASE) {
		// Apply multi2 option for PLL over 800M
		for (i = 0; i < ARRAY_SIZE(pll_patch_reg_base); i++) {
			if (pll_patch_reg_base[i] == reg_base)
				break;
		}
		if (i >= ARRAY_SIZE(pll_patch_reg_base)) {
			printf("pll_freq_set: fail to find reg base: 0x%x",
					(unsigned int)reg_base);
			return -1;
		}
		pll_patch_multi2[i] = 1;
		value_02h = 0x0309;
		// Div PLL freq by 2
		minor = ((freq->major & 0xFFFF) << 14) |
			((freq->minor >> 2) & 0x3FFF);
		minor >>= 1;
		// Recalculate the divider
		major = (minor >> 14) & 0xFFFF;
		minor = (minor << 2) & 0xFFFF;
	} else {
		value_02h = 0x0209;
		major = freq->major;
		minor = freq->minor;
	}
	if (reg_base == PLL_REG_USB_BASE && rda_metal_id_get() >= 7)
		value_02h ^= (1 << 8);
	ispi_reg_write_and_save(reg_base + PLL_REG_OFFSET_02H, value_02h);
	ispi_reg_write_and_save(reg_base + PLL_REG_OFFSET_MAJOR, major);
	ispi_reg_write_and_save(reg_base + PLL_REG_OFFSET_MINOR, minor);
	ispi_reg_write_and_save(reg_base + PLL_REG_OFFSET_07H, 0x0010);

	return 0;
}

static void pll_setup_freq(void)
{
	pll_freq_set(PLL_REG_CPU_BASE, g_clock_config->PLL_FREQ_CPU);
	// Always configure BUS PLL, even when it is being used
	pll_freq_set(PLL_REG_BUS_BASE, g_clock_config->PLL_FREQ_BUS);
	pll_freq_set(PLL_REG_MEM_BASE, g_clock_config->PLL_FREQ_MEM);
	pll_freq_set(PLL_REG_USB_BASE, g_clock_config->PLL_FREQ_USB);
}

static void print_pll_freq(char *name, UINT32 value)
{
	printf("pll freq %s = %d\n", name, (int)value);
}

static void sys_dump_pll_freq(void)
{
	print_pll_freq("CPU", g_clock_config->PLL_FREQ_CPU);
	print_pll_freq("BUS", g_clock_config->PLL_FREQ_BUS);
	print_pll_freq("MEM", g_clock_config->PLL_FREQ_MEM);
	//print_pll_freq("USB", g_clock_config->PLL_FREQ_USB);
}

#if 0
static void pmu_get_efuse_dcdc(int buckVoltLow[])
{
	static const u8 setBitCnt[8] = { 0, 1, 1, 2, 1, 2, 2, 3, };
	u16 dcdc;
	int i;

	pmu_reg_write(0x51, 0x02ed);
	udelay(2000);
	pmu_reg_write(0x51, 0x02fd);
	dcdc = pmu_reg_read(0x52);
	pmu_reg_write(0x51, 0x0200);

	for (i = 0; i < 4; i++) {
		buckVoltLow[i] = (setBitCnt[dcdc & 0x7] > 1);
		dcdc >>= 3;
	}
}
#endif

static void v_mmc_set(void)
{
	//ispi_open(1);

	u16 v_mmc = pmu_reg_read(6); //6 - reg RDA_ADDR_LDO_ACTIVE_SETTING4
	v_mmc |= 7<<3; //7 - 3.2v
	pmu_reg_write(6, v_mmc);

	u16 v_mmc_is_low = pmu_reg_read(4); //4 - reg RDA_ADDR_LDO_ACTIVE_SETTING2
	v_mmc_is_low &= ~(1<<8);
	pmu_reg_write(4, v_mmc_is_low);

	//ispi_open(0);
}

static void pmu_setup_init(void)
{
	u32 value;
	int buck_volt_low[4] = { 0, };

	rda_nand_iodrive_set();

#ifdef CONFIG_RDA_PDL
	enable_charger(0);
#endif

	ispi_open(1);

	// Disable vibrator
	if (rda_metal_id_get() < 9)
		pmu_reg_write(0x03, 0x9FDF);
	else
		pmu_reg_write(0x03, 0x9FFF);
	// Enable bandgap chopper mode
	pmu_reg_write(0x0F, 0x1E90);
	// Enable high AC throttling
	pmu_reg_write(0x12, 0x1218);
#ifdef TARGET_TABLET_MODE
	// Charger current (cc = 6, pre = 7)
	pmu_reg_write(0x13, 0x1B70);
#else
	// Charger current (cc = 2, pre = 7)
	pmu_reg_write(0x13, 0x1970);
#endif
	// Vcore DCDC freq
	pmu_reg_write(0x2D, 0x96BA);
	pmu_reg_write(0x2E, 0x12AA);
#if 0
	// Get efuse dcdc
	pmu_get_efuse_dcdc(buck_volt_low);
#endif
	// Vcore voltage
	value = 0x9444;
	value = PMU_SET_BITFIELD(value, RDA_PMU_VBUCK1_BIT_ACT,
			PMU_VBUCK1_VAL +
			((PMU_VBUCK1_VAL < 15 && buck_volt_low[0]) ? 1 : 0));
	pmu_reg_write(0x2F, value);
	// DDR PWM mode
	pmu_reg_write(0x0D, 0x92D0);
	// DDR voltage
	value = 0xAAB5;
	value = PMU_SET_BITFIELD(value, RDA_PMU_VBUCK3_BIT_ACT,
			PMU_VBUCK3_VAL +
			((PMU_VBUCK3_VAL < 15 && buck_volt_low[2]) ? 1 : 0));
#ifdef PMU_VBUCK4_VAL
	value = PMU_SET_BITFIELD(value, RDA_PMU_VBUCK4_BIT_ACT,
			PMU_VBUCK4_VAL +
			((PMU_VBUCK4_VAL < 15 && buck_volt_low[3]) ? 1 : 0));
	if (rda_metal_id_get() >= 9)
		value ^= RDA_PMU_VBUCK4_BIT_ACT(8);
	// vBuck4 in low voltage range
	if (rda_metal_id_get() < 9)
		pmu_reg_write(0x36, 0x6E44);
	else
		pmu_reg_write(0x36, 0x6E45);
#else
	if (rda_metal_id_get() < 9)
		pmu_reg_write(0x36, 0x6E55);
	else
		pmu_reg_write(0x36, 0x6E54);
#endif
	pmu_reg_write(0x2A, value);
	// DDR power parameters
	pmu_reg_write(0x4A, 0x96AA);
	pmu_reg_write(0x4B, 0x96AA);

	// Set v_mmc to 3V
	v_mmc_set();

	ispi_open(0);
	printf("PMU vbuck1 = %u, vbuck3 = %u\n", PMU_VBUCK1_VAL, PMU_VBUCK3_VAL);
}

static void setup_ddr_vtt(int vtt)
{
	if (rda_metal_id_get() >= 2) {
		printf("setup ddr vtt to %d\n", vtt);
		ispi_reg_write_and_save(0x69, 0x0008 | (vtt & 0x07) );
	}
}

/*
 * we initialize usb clock, but this won't cause the usb clock jitter,
 * because wo don't setup the usb pll
 */
static void usb_clock_pre_init(void)
{
	if (rda_metal_id_get() < 2) {
		ispi_reg_write_and_save(0x81, 0x7a68);
		ispi_reg_write_and_save(0x82, 0x0308);
		ispi_reg_write_and_save(0x83, 0xfae7);
		ispi_reg_write_and_save(0x85, 0x1276);
		ispi_reg_write_and_save(0x86, 0x2762);
		ispi_reg_write_and_save(0x89, 0x7441);
	} else {
		ispi_reg_write_and_save(0x83, 0x72ef);
		ispi_reg_write_and_save(0x89, 0x7400);
	}
}

static void pll_setup_init(void)
{
	setup_ddr_vtt(DDR_VTT_VAL);

	if (g_clock_config->DDR_CHAN_1_VALID) {
		ispi_reg_write_and_save(0x100, g_clock_config->DDR_TIMING_100H);
		ispi_reg_write_and_save(0x101, g_clock_config->DDR_TIMING_101H);
		ispi_reg_write_and_save(0x102, g_clock_config->DDR_TIMING_102H);
		ispi_reg_write_and_save(0x103, g_clock_config->DDR_TIMING_103H);
		ispi_reg_write_and_save(0x104, g_clock_config->DDR_TIMING_104H);
		ispi_reg_write_and_save(0x105, g_clock_config->DDR_TIMING_105H);
		ispi_reg_write_and_save(0x106, g_clock_config->DDR_TIMING_106H);
		ispi_reg_write_and_save(0x107, g_clock_config->DDR_TIMING_107H);
		ispi_reg_write_and_save(0x108, g_clock_config->DDR_TIMING_108H);
		ispi_reg_write_and_save(0x109, g_clock_config->DDR_TIMING_109H);
		ispi_reg_write_and_save(0x10A, g_clock_config->DDR_TIMING_10AH);
		ispi_reg_write_and_save(0x10B, g_clock_config->DDR_TIMING_10BH);
	}

	if (g_clock_config->DDR_CHAN_2_VALID) {
		ispi_reg_write_and_save(0x120, g_clock_config->DDR_TIMING_120H);
		ispi_reg_write_and_save(0x121, g_clock_config->DDR_TIMING_121H);
		ispi_reg_write_and_save(0x122, g_clock_config->DDR_TIMING_122H);
		ispi_reg_write_and_save(0x123, g_clock_config->DDR_TIMING_123H);
		ispi_reg_write_and_save(0x124, g_clock_config->DDR_TIMING_124H);
		ispi_reg_write_and_save(0x125, g_clock_config->DDR_TIMING_125H);
		ispi_reg_write_and_save(0x126, g_clock_config->DDR_TIMING_126H);
		ispi_reg_write_and_save(0x127, g_clock_config->DDR_TIMING_127H);
		ispi_reg_write_and_save(0x128, g_clock_config->DDR_TIMING_128H);
		ispi_reg_write_and_save(0x129, g_clock_config->DDR_TIMING_129H);
		ispi_reg_write_and_save(0x12A, g_clock_config->DDR_TIMING_12AH);
		ispi_reg_write_and_save(0x12B, g_clock_config->DDR_TIMING_12BH);
	}

	if (g_clock_config->DDR_CHAN_3_VALID) {
		ispi_reg_write_and_save(0x140, g_clock_config->DDR_TIMING_140H);
		ispi_reg_write_and_save(0x141, g_clock_config->DDR_TIMING_141H);
		ispi_reg_write_and_save(0x142, g_clock_config->DDR_TIMING_142H);
		ispi_reg_write_and_save(0x143, g_clock_config->DDR_TIMING_143H);
		ispi_reg_write_and_save(0x144, g_clock_config->DDR_TIMING_144H);
		ispi_reg_write_and_save(0x145, g_clock_config->DDR_TIMING_145H);
		ispi_reg_write_and_save(0x146, g_clock_config->DDR_TIMING_146H);
		ispi_reg_write_and_save(0x147, g_clock_config->DDR_TIMING_147H);
		ispi_reg_write_and_save(0x148, g_clock_config->DDR_TIMING_148H);
		ispi_reg_write_and_save(0x149, g_clock_config->DDR_TIMING_149H);
		ispi_reg_write_and_save(0x14A, g_clock_config->DDR_TIMING_14AH);
		ispi_reg_write_and_save(0x14B, g_clock_config->DDR_TIMING_14BH);
	}

	if (g_clock_config->DDR_CHAN_4_VALID) {
		ispi_reg_write_and_save(0x160, g_clock_config->DDR_TIMING_160H);
		ispi_reg_write_and_save(0x161, g_clock_config->DDR_TIMING_161H);
		ispi_reg_write_and_save(0x162, g_clock_config->DDR_TIMING_162H);
		ispi_reg_write_and_save(0x163, g_clock_config->DDR_TIMING_163H);
		ispi_reg_write_and_save(0x164, g_clock_config->DDR_TIMING_164H);
		ispi_reg_write_and_save(0x165, g_clock_config->DDR_TIMING_165H);
		ispi_reg_write_and_save(0x166, g_clock_config->DDR_TIMING_166H);
		ispi_reg_write_and_save(0x167, g_clock_config->DDR_TIMING_167H);
		ispi_reg_write_and_save(0x168, g_clock_config->DDR_TIMING_168H);
		ispi_reg_write_and_save(0x169, g_clock_config->DDR_TIMING_169H);
		ispi_reg_write_and_save(0x16A, g_clock_config->DDR_TIMING_16AH);
		ispi_reg_write_and_save(0x16B, g_clock_config->DDR_TIMING_16BH);
	}

	ispi_reg_write_and_save(0x180, g_clock_config->DDR_TIMING_180H);
	ispi_reg_write_and_save(0x181, g_clock_config->DDR_TIMING_181H);
	ispi_reg_write_and_save(0x182, g_clock_config->DDR_TIMING_182H);
	ispi_reg_write_and_save(0x183, g_clock_config->DDR_TIMING_183H);
	ispi_reg_write_and_save(0x184, g_clock_config->DDR_TIMING_184H);
	ispi_reg_write_and_save(0x185, g_clock_config->DDR_TIMING_185H);
	ispi_reg_write_and_save(0x186, g_clock_config->DDR_TIMING_186H);
	ispi_reg_write_and_save(0x187, g_clock_config->DDR_TIMING_187H);
	ispi_reg_write_and_save(0x188, g_clock_config->DDR_TIMING_188H);
	ispi_reg_write_and_save(0x189, g_clock_config->DDR_TIMING_189H);
	ispi_reg_write_and_save(0x18A, g_clock_config->DDR_TIMING_18AH);
	ispi_reg_write_and_save(0x18B, g_clock_config->DDR_TIMING_18BH);
	ispi_reg_write_and_save(0x18C, g_clock_config->DDR_TIMING_18CH);

	usb_clock_pre_init();

	udelay(5000);
}

static void pll_setup_mem(void)
{
}

static void pll_setup_mem_cal(void)
{
}

#if 0
static void print_pll_reg(UINT32 index, UINT32 value)
{
	printf("pll reg %lx = %lx\n", index, value);
}
#endif

static void pll_dump_reg(void)
{
#if 0
	print_pll_reg(0x005, ispi_reg_read(0x005));
	print_pll_reg(0x006, ispi_reg_read(0x006));
	print_pll_reg(0x063, ispi_reg_read(0x063));
	print_pll_reg(0x065, ispi_reg_read(0x065));
	print_pll_reg(0x066, ispi_reg_read(0x066));

	if (g_clock_config->DDR_CHAN_1_VALID)
	{
		print_pll_reg(0x100, ispi_reg_read(0x100));
		print_pll_reg(0x101, ispi_reg_read(0x101));
		print_pll_reg(0x102, ispi_reg_read(0x102));
		print_pll_reg(0x103, ispi_reg_read(0x103));
		print_pll_reg(0x104, ispi_reg_read(0x104));
		print_pll_reg(0x105, ispi_reg_read(0x105));
		print_pll_reg(0x106, ispi_reg_read(0x106));
		print_pll_reg(0x107, ispi_reg_read(0x107));
		print_pll_reg(0x108, ispi_reg_read(0x108));
		print_pll_reg(0x109, ispi_reg_read(0x109));
		print_pll_reg(0x10A, ispi_reg_read(0x10A));
		print_pll_reg(0x10B, ispi_reg_read(0x10B));
	}

	if (g_clock_config->DDR_CHAN_2_VALID)
	{
		print_pll_reg(0x120, ispi_reg_read(0x120));
		print_pll_reg(0x121, ispi_reg_read(0x121));
		print_pll_reg(0x122, ispi_reg_read(0x122));
		print_pll_reg(0x123, ispi_reg_read(0x123));
		print_pll_reg(0x124, ispi_reg_read(0x124));
		print_pll_reg(0x125, ispi_reg_read(0x125));
		print_pll_reg(0x126, ispi_reg_read(0x126));
		print_pll_reg(0x127, ispi_reg_read(0x127));
		print_pll_reg(0x128, ispi_reg_read(0x128));
		print_pll_reg(0x129, ispi_reg_read(0x129));
		print_pll_reg(0x12A, ispi_reg_read(0x12A));
		print_pll_reg(0x12B, ispi_reg_read(0x12B));
	}

	if (g_clock_config->DDR_CHAN_3_VALID)
	{
		print_pll_reg(0x140, ispi_reg_read(0x140));
		print_pll_reg(0x141, ispi_reg_read(0x141));
		print_pll_reg(0x142, ispi_reg_read(0x142));
		print_pll_reg(0x143, ispi_reg_read(0x143));
		print_pll_reg(0x144, ispi_reg_read(0x144));
		print_pll_reg(0x145, ispi_reg_read(0x145));
		print_pll_reg(0x146, ispi_reg_read(0x146));
		print_pll_reg(0x147, ispi_reg_read(0x147));
		print_pll_reg(0x148, ispi_reg_read(0x148));
		print_pll_reg(0x149, ispi_reg_read(0x149));
		print_pll_reg(0x14A, ispi_reg_read(0x14A));
		print_pll_reg(0x14B, ispi_reg_read(0x14B));
	}

	if (g_clock_config->DDR_CHAN_4_VALID)
	{
		print_pll_reg(0x161, ispi_reg_read(0x161));
		print_pll_reg(0x162, ispi_reg_read(0x162));
		print_pll_reg(0x163, ispi_reg_read(0x163));
		print_pll_reg(0x164, ispi_reg_read(0x164));
		print_pll_reg(0x165, ispi_reg_read(0x165));
		print_pll_reg(0x166, ispi_reg_read(0x166));
		print_pll_reg(0x167, ispi_reg_read(0x167));
		print_pll_reg(0x168, ispi_reg_read(0x168));
		print_pll_reg(0x169, ispi_reg_read(0x169));
		print_pll_reg(0x16A, ispi_reg_read(0x16A));
		print_pll_reg(0x16B, ispi_reg_read(0x16B));
	}

	print_pll_reg(0x160, ispi_reg_read(0x160));
	print_pll_reg(0x180, ispi_reg_read(0x180));
	print_pll_reg(0x181, ispi_reg_read(0x181));
	print_pll_reg(0x182, ispi_reg_read(0x182));
	print_pll_reg(0x183, ispi_reg_read(0x183));
	print_pll_reg(0x184, ispi_reg_read(0x184));
	print_pll_reg(0x185, ispi_reg_read(0x185));
	print_pll_reg(0x186, ispi_reg_read(0x186));
	print_pll_reg(0x187, ispi_reg_read(0x187));
	print_pll_reg(0x188, ispi_reg_read(0x188));
	print_pll_reg(0x189, ispi_reg_read(0x189));
	print_pll_reg(0x18A, ispi_reg_read(0x18A));
	print_pll_reg(0x18B, ispi_reg_read(0x18B));
#endif
}

static int clock_save_config(void)
{
	/* save config to nand */
	return 1;
}

#ifdef DO_DDR_PLL_DEBUG

static int ddr_get_freq(UINT8 chioce)
{
	switch(chioce)
	{
		case 1:
			return 50;
		case 2:
			return 100;
		case 3:
			return 156;
		case 4:
			return 200;
		case 5:
			return 290;
		case 6:
			return 333;
		case 7:
			return 355;
		case 8:
			return 400;
		case 9:
			return 416;
		case 10:
			return 455;
		case 11:
			return 500;
		case 12:
			return 519;
		default:
			return -1;
	}
}

static void freq_choose(void)
{
	UINT8 i = 0, buf[3] = {0}, choice = 0;
	INT32 freq_temp = 0;

	printf("\nPlese choose the ddr Freq:");
	printf("\n1.50M  2.100M  3.156M  4.200M  5.290M  6.333M  7.355M  8.400M 9.416M 10.455M 11.500M 12.519M");
	printf("\nThe number is:");

	while(1)
	{
		if (i > 2)
		{
			printf("\n Sorry, you input is wrong. Please input again:");
			i = 0;
		}
		buf[i] = serial_getc();
		serial_putc(buf[i]);

		if ( ('\r' == buf[i]) || ('\n' == buf[i]))
		{
			printf("\n");
			break;
		}

		i++;
	}

	if (1 == i)
		choice = buf[0] - 0x30;
	else if (2 == i)
		choice = 10*(buf[0] - 0x30) + (buf[1] - 0x30);
	else
		return;

	freq_temp = ddr_get_freq(choice);
	if (-1 == freq_temp )
		printf("\n Sorry, the fre you choose is wrong");
	ddrfreq = freq_temp;
}

static void data_bits_choose(void)
{
	UINT8 i = 0, buf[2] = {0};

	printf("\nPlese choose the ddr data bits:");
	printf("\n1.16  2.32");
	printf("\nThe number is:");

	while(1)
	{
		buf[i] =serial_getc();

		if (i == 1)
		{
			if (('\r' == buf[i]) || ('\n' == buf[i]))
			{
				if (1 == (buf[0] - 0x30))
					ddr32bit = 0;
				else
					ddr32bit = 1;
				return;
			}
			else
			{
				printf("\n Sorry, you input is wrong. Please input again:");
				i = 0;
				continue;
			}
		}

		serial_putc(buf[i]);

		if ((buf[i] - 0x30) > 2 || 0 == (buf[i] - 0x30))
			printf("\n Sorry, you input is wrong. Please input again:");
		else
			i++;
	}
}

#if 0
static void pmu_setup_calibration_voltage(UINT8 vcoreselect)
{
	UINT8 buf[3] = {0}, voltage = 0;
	int i = 0;
	UINT32 reg_value,temp, regid = 0;

	ispi_open(1);
	if (1 == vcoreselect)
	{
		regid = 0x2f;
		printf("\nPlese input vcore voltage(0 ~ 15):");
	}
	else
	{
		regid = 0x2a;
		printf("\nPlese input DDR voltage(0 ~ 15):");
	}


	while(1)
	{
		if (i > 2)
		{
			printf("\n Sorry, you input is wrong. Please input again:");
			i = 0;
		}
		buf[i] = serial_getc();
		serial_putc(buf[i]);

		if ( ('\r' == buf[i]) || ('\n' == buf[i]))
		{
			printf("\n");
			break;
		}

		i++;
	}

	if (1 == i)
		voltage = buf[0] - 0x30;
	else if (2 == i)
		voltage = 10*(buf[0] - 0x30) + (buf[1] - 0x30);
	else
		return;

	temp = (UINT32)voltage;
	temp = (temp & 0xf) << 12;
	reg_value = pmu_reg_read(regid);
	reg_value &= ~(0xf << 12);
	reg_value |= temp ;
	pmu_reg_write(regid, reg_value);
	mdelay(100);
}
#endif

static void pmu_setup_calibration_voltage(UINT8 vcoreselect)
{
	UINT8 buf[3] = {0}, voltage = 0;
	int i = 0;
	UINT32 reg_value,temp, regid = 0;

	ispi_open(1);
	if (1 == vcoreselect)
	{
		regid = 0x2f;
		printf("\nPlese input vcore voltage(0 ~ 15):");
	}
	else
	{
		regid = 0x2a;
		if (2 == vcoreselect)
			printf("\nPlese input DDR buck3 voltage(0 ~ 15):");
		else
			printf("\nPlese input DDR buck4 voltage(0 ~ 15):");
	}

	while(1)
	{
		if (i > 2)
		{
			printf("\n Sorry, you input is wrong. Please input again:");
			i = 0;
		}
		buf[i] = serial_getc();
		serial_putc(buf[i]);

		if ( ('\r' == buf[i]) || ('\n' == buf[i]))
		{
			printf("\n");
			break;
		}

		i++;
	}

	if (1 == i)
		voltage = buf[0] - 0x30;
	else if (2 == i)
		voltage = 10*(buf[0] - 0x30) + (buf[1] - 0x30);
	else
		return;

	if ((vcoreselect == 1) || (vcoreselect == 2))
	{
		temp = (UINT32)voltage;
		temp = (temp & 0xf) << 12;
		reg_value = pmu_reg_read(regid);
		reg_value &= ~(0xf << 12);
		reg_value |= temp ;
		pmu_reg_write(regid, reg_value);
		mdelay(100);
	}
	else
	{
		temp = (UINT32)voltage;
		temp = (temp & 0xf) << 4;
		reg_value = pmu_reg_read(regid);
		reg_value &= ~(0xf << 4);
		reg_value |= temp ;
		if (rda_metal_id_get() >= 9)
			reg_value ^= RDA_PMU_VBUCK4_BIT_ACT(8);
		pmu_reg_write(regid, reg_value);
		pmu_reg_write(0x0D, 0x92D0);
		pmu_reg_write(0x4B, 0x96A8);
		mdelay(100);
	}
}

static void pmu_buck4_buck3_choose(void)
{
	UINT8 i = 0, buf[2] = {0};
	UINT8 ddr_voltage_source = 0;

	printf("\nPlese choose the ddr voltage:");
	printf("\n1.DDR3L  2.DDR3");
	printf("\nThe number is:");

	while(1)
	{
		buf[i] =serial_getc();

		if (i == 1)
		{
			if (('\r' == buf[i]) || ('\n' == buf[i]))
			{
				if (1 == (buf[0] - 0x30))
					ddr_voltage_source =2;
				else{
					ddr_voltage_source = 3;
					pmu_setup_calibration_voltage(2);
				}
				pmu_setup_calibration_voltage(ddr_voltage_source);
				return;
			}
			else
			{
				printf("\n Sorry, you input is wrong. Please input again:");
				i = 0;
				continue;
			}
		}

		serial_putc(buf[i]);

		if ((buf[i] - 0x30) > 2 || 0 == (buf[i] - 0x30))
			printf("\n Sorry, you input is wrong. Please input again:");
		else
			i++;
	}
}


void clock_load_ddr_cal_config(void)
{
	clock_debug_config.PLL_FREQ_MEM = ddrfreq;
	if (ddrfreq < 200)
		clock_debug_config.DDR_FLAGS |= DDR_FLAGS_DLLOFF;
	else
		clock_debug_config.DDR_FLAGS &= ~DDR_FLAGS_DLLOFF;

	if (0 == ddr32bit)
	{
		clock_debug_config.DDR_CHAN_3_VALID = 0;
		clock_debug_config.DDR_CHAN_4_VALID = 0;
	}

	clock_debug_config.DDR_PARA &= ~DDR_PARA_MEM_BITS_MASK;
	if (0 == ddr32bit)
		clock_debug_config.DDR_PARA |= DDR_PARA_MEM_BITS(1);
	else
		clock_debug_config.DDR_PARA |= DDR_PARA_MEM_BITS(2);
}

static int serial_gets(UINT8 *pstr)
{
    UINT32 length;

    length = 0;
    while(1) {
        pstr[length] = serial_getc();
        if(pstr[length] == '\r') {
            pstr[length] = 0x00;
            break;
        }
        else if( pstr[length] == '\b' ) {
            if(length>0) {
                length --;
                printf("\b");
            }
        }
        else {
            serial_putc(pstr[length]);
            length ++;
        }

        if(length > 32)
            return -1;
    }
    return length;
}

UINT32 asc2hex(UINT8 *pstr, UINT8 len)
{
	UINT8 i,ch,mylen;
	UINT32 hexvalue;

	for(mylen=0,i=0; i<8; i++)
	{
		if( pstr[i] == 0 )
			break;
		mylen ++;
	}
	if( len != 0 )
	{
		if(mylen>len)
			mylen = len;
	}
	if(mylen>8)
		mylen = 8;

	hexvalue = 0;
	for (i = 0; i < mylen; i++)
	{
		hexvalue <<= 4;
		ch = *(pstr+i);
		if((ch>='0') && (ch<='9'))
			hexvalue |= ch - '0';
		else if((ch>='A') && (ch<='F'))
			hexvalue |= ch - ('A' - 10);
		else if((ch>='a') && (ch<='f'))
			hexvalue |= ch - ('a' - 10);
		else
			;
	}
	return(hexvalue);
}

#define DDR_ANALOG_PHY_NUM 65
static const UINT16 ddr_pll_num[DDR_ANALOG_PHY_NUM] = {
	0x0001,
	0x100,
	0x101,
	0x102,
	0x103,
	0x104,
	0x105,
	0x106,
	0x107,
	0x108,
	0x109,
	0x10A,
	0x10B,

	0x0002,
	0x120,
	0x121,
	0x122,
	0x123,
	0x124,
	0x125,
	0x126,
	0x127,
	0x128,
	0x129,
	0x12A,
	0x12B,

	0x0004,
	0x140,
	0x141,
	0x142,
	0x143,
	0x144,
	0x145,
	0x146,
	0x147,
	0x148,
	0x149,
	0x14A,
	0x14B,

	0x0006,
	0x160,
	0x161,
	0x162,
	0x163,
	0x164,
	0x165,
	0x166,
	0x167,
	0x168,
	0x169,
	0x16A,
	0x16B,

	0x180,
	0x181,
	0x182,
	0x183,
	0x184,
	0x185,
	0x186,
	0x187,
	0x188,
	0x189,
	0x18A,
	0x18B,
	0x18C
};

void ddr_pll_save_by_id(UINT16 reg_id, UINT16 reg_val)
{
	int i = 0;
	UINT16 * ddr_pll_reg_base =  &g_clock_config->DDR_CHAN_1_VALID;

	for (; i < DDR_ANALOG_PHY_NUM; i++)
		if (reg_id == ddr_pll_num[i])
			break;

	if (i >= DDR_ANALOG_PHY_NUM)
		return;

	writew(reg_val, ddr_pll_reg_base + i);

	return;
}

static int process_cmd(char * cmd)
{
	char cmd_element[3][16] = {{0}};
	char * cmd_temp = cmd;
	UINT8 i = 0, cmd_element_num = 0, former_space = 1;
	UINT16 reg = 0, reg_value = 0;

	if (NULL == cmd)
		return -1;

	while(('\0' != *cmd_temp) && ('\r' != *cmd_temp) && ('\n' != *cmd_temp))
	{
		if (' ' == * cmd_temp)
		{
			if (0 == former_space)
			{
				former_space = 1;
				cmd_element[cmd_element_num][i] = '\0';
				cmd_element_num++;
				if (cmd_element_num > 2)
					return -1;

				i = 0;
			}
		}
		else
		{
			former_space = 0;
			cmd_element[cmd_element_num][i] = *cmd_temp;
			i++;
			if (i > 6)
				return -1;
		}

		cmd_temp++;
	}

	cmd_element[cmd_element_num][i] = '\0';

	if (!strcmp(cmd_element[0], "read"))
	{
		if (cmd_element_num == 2)
			return -1;
		if (('0' != cmd_element[1][0]) || ('x' != cmd_element[1][1]))
			return -1;
		reg = (UINT16)asc2hex((UINT8 *)&cmd_element[1][2], 4);
		reg_value = ispi_reg_read(reg);
		printf("value = 0x%x", reg_value);
		printf("\nddrPll#");
	}
	else if (!strcmp(cmd_element[0], "write"))
	{
		if (cmd_element_num != 2)
			return -1;
		if (('0' != cmd_element[1][0]) || ('x' != cmd_element[1][1])
		   || ('0' != cmd_element[2][0]) || ('x' != cmd_element[2][1]))
			return -1;

		reg = (UINT16)asc2hex((UINT8 *)&cmd_element[1][2], 4);
		reg_value  = (UINT16)asc2hex((UINT8 *)&cmd_element[2][2], 4);
		ispi_reg_write_and_save(reg, reg_value);
		ddr_pll_save_by_id(reg, reg_value);
	}
	else if (!strcmp(cmd_element[0], "finish"))
	{
		return 1;
	}
	else if (!strcmp(cmd_element[0], "dump"))
	{
		pll_dump_reg();
		printf("ddrPll#");
	}
	else
	{
		return -1;
	}

	return 0;
}

static void cmd_input(void)
{
	char cmd[48] = {0};
	int len = 0;

	ispi_open(0);
	printf("\nddrPll#");
	while(1)
	{
		len = serial_gets((UINT8 *)cmd);
		printf("\nddrPll#");
		if (len > 0)
		{
			int result = 0;

			result =  process_cmd(cmd);
			if (-1 == result)
				printf("command error! \nddrPll#");
			else if (1 == result)
				break;
			else
				continue;
		}
	}

	return;
}

#endif /* DO_DDR_PLL_DEBUG */

#ifdef DDR_TIMING_101H_VAL_U08
static void pll_val_reset(u16 meta_id)
{
	if (0xC == meta_id){
		g_clock_config->DDR_TIMING_101H = DDR_TIMING_101H_VAL_U08;
		g_clock_config->DDR_TIMING_102H = DDR_TIMING_102H_VAL_U08;
		g_clock_config->DDR_TIMING_103H = DDR_TIMING_103H_VAL_U08;
		g_clock_config->DDR_TIMING_104H = DDR_TIMING_104H_VAL_U08;
		g_clock_config->DDR_TIMING_105H = DDR_TIMING_105H_VAL_U08;
		g_clock_config->DDR_TIMING_106H = DDR_TIMING_106H_VAL_U08;

		g_clock_config->DDR_TIMING_121H = DDR_TIMING_121H_VAL_U08;
		g_clock_config->DDR_TIMING_122H = DDR_TIMING_122H_VAL_U08;
		g_clock_config->DDR_TIMING_123H = DDR_TIMING_123H_VAL_U08;
		g_clock_config->DDR_TIMING_124H = DDR_TIMING_124H_VAL_U08;
		g_clock_config->DDR_TIMING_125H = DDR_TIMING_125H_VAL_U08;
		g_clock_config->DDR_TIMING_126H = DDR_TIMING_126H_VAL_U08;

		g_clock_config->DDR_TIMING_141H = DDR_TIMING_141H_VAL_U08;
		g_clock_config->DDR_TIMING_142H = DDR_TIMING_142H_VAL_U08;
		g_clock_config->DDR_TIMING_143H = DDR_TIMING_143H_VAL_U08;
		g_clock_config->DDR_TIMING_144H = DDR_TIMING_144H_VAL_U08;
		g_clock_config->DDR_TIMING_145H = DDR_TIMING_145H_VAL_U08;
		g_clock_config->DDR_TIMING_146H = DDR_TIMING_146H_VAL_U08;

		g_clock_config->DDR_TIMING_161H = DDR_TIMING_161H_VAL_U08;
		g_clock_config->DDR_TIMING_162H = DDR_TIMING_162H_VAL_U08;
		g_clock_config->DDR_TIMING_163H = DDR_TIMING_163H_VAL_U08;
		g_clock_config->DDR_TIMING_164H = DDR_TIMING_164H_VAL_U08;
		g_clock_config->DDR_TIMING_165H = DDR_TIMING_165H_VAL_U08;
		g_clock_config->DDR_TIMING_166H = DDR_TIMING_166H_VAL_U08;

		g_clock_config->DDR_TIMING_18BH = DDR_TIMING_18BH_VAL_U08;
	}
}
#endif

#ifdef _TGT_AP_DDR_AUTO_CALI_ENABLE
static void sys_reset_mem(void)
{
	hwp_sysCtrlAp->MEM_Rst_Set = SYS_CTRL_AP_SET_MEM_RST_DMC | SYS_CTRL_AP_SET_MEM_RST_DDRPHY_P;
	mdelay(1);
	hwp_sysCtrlAp->MEM_Rst_Clr = SYS_CTRL_AP_CLR_MEM_RST_DMC | SYS_CTRL_AP_CLR_MEM_RST_DDRPHY_P;
	mdelay(1);
}

static void pll_setup_init_cal_dynamic(u8 rda2)
{
	u16 reg_b = 0;

	reg_b=rda2*0x1000;
	reg_b |= (g_clock_config->DDR_TIMING_103H & 0xfff);

	if (g_clock_config->DDR_CHAN_1_VALID)
		ispi_reg_write_and_save(0x103, reg_b); //default 0x44b0, dqs_prsel_preset<3:0>,dq_prsel_todig<3:0>,dqs_prsel_todig<3:0>,dq_prsel_offset<35:32>

	if (g_clock_config->DDR_CHAN_2_VALID)
		ispi_reg_write_and_save(0x123, reg_b); //default 0x44b0, dqs_prsel_preset<3:0>,dq_prsel_todig<3:0>,dqs_prsel_todig<3:0>,dq_prsel_offset<35:32>

	if (g_clock_config->DDR_CHAN_3_VALID)
		ispi_reg_write_and_save(0x143, reg_b); //default 0x44b0, dqs_prsel_preset<3:0>,dq_prsel_todig<3:0>,dqs_prsel_todig<3:0>,dq_prsel_offset<35:32>

	if (g_clock_config->DDR_CHAN_4_VALID)
		ispi_reg_write_and_save(0x163, reg_b); //default 0x44b0, dqs_prsel_preset<3:0>,dq_prsel_todig<3:0>,dqs_prsel_todig<3:0>,dq_prsel_offset<35:32>
}

static void pll_save_ddr_cal_dynamic(UINT8 rda2)
{
	u16 reg_b = 0;

	reg_b = rda2*0x1000;
	reg_b |= (g_clock_config->DDR_TIMING_103H & 0xfff);

	if (g_clock_config->DDR_CHAN_1_VALID)
		g_clock_config->DDR_TIMING_103H = reg_b;

	if (g_clock_config->DDR_CHAN_2_VALID)
		g_clock_config->DDR_TIMING_123H = reg_b;

	if (g_clock_config->DDR_CHAN_3_VALID)
		g_clock_config->DDR_TIMING_143H = reg_b;

	if (g_clock_config->DDR_CHAN_4_VALID)
		g_clock_config->DDR_TIMING_163H = reg_b;
}

static void pll_setup_ddr_cal_dynamic(u16 val)
{
	if (g_clock_config->DDR_CHAN_1_VALID)
		g_clock_config->DDR_TIMING_103H = val;

	if (g_clock_config->DDR_CHAN_2_VALID)
		g_clock_config->DDR_TIMING_123H = val;

	if (g_clock_config->DDR_CHAN_3_VALID)
		g_clock_config->DDR_TIMING_143H = val;

	if (g_clock_config->DDR_CHAN_4_VALID)
		g_clock_config->DDR_TIMING_163H = val;
}
static int print_rda2_result(UINT8 * rda2, UINT8 rda2_old)
{
	int j, first_position = 0, total_num = 0, chioce_rda2 = 0;

	printf("\n\n");
	printf("rda2 stand for dqs_prsel_preset \n");
	printf("rda2=0 rda2=1 rda2=2 rda2=3 rda2=4 rda2=5 rda2=6 rda2=7\n");
	for (j=0;j<8;j=j+1){
		printf("   %c   ",rda2_result[j]);
	}
	printf("\nPlease check the result and choose the best one");

	for (j=0;j<8;j=j+1)
		if (('*' == rda2_result[j]) && ('R' == rda2_result[(j  + 1) % 8]))
			break;

	first_position = (j + 1) % 8;

	for (j=0;j<8;j=j+1)
		if ('R' == rda2_result[j])
			total_num += 1;

	chioce_rda2 = first_position + total_num/2 -1;

	if (chioce_rda2 < 0)
		printf("\nerror!The range of rda2 is 0~~7!Calculated rda2 is minus\n");

	if (0 != (total_num % 2) )
		chioce_rda2 += 1;

	if (8 == total_num)
		*rda2 = rda2_old;
	else
		*rda2 = ((UINT8)chioce_rda2 % 8);
	printf("\n final rda2= %d, rda2_old = %d \n", *rda2, rda2_old);

	return total_num;
}

static void mem_test_cal_write(UINT32 offset, UINT8 rda2)
{
	volatile unsigned int *addr, addr_val_write;

	addr = (volatile unsigned int *)(0x83000000);
	while((unsigned int)addr < offset){
		if (((unsigned int )addr % 8) == 0)
			addr_val_write = 0xfefefefe | rda2;
		else
			addr_val_write = (unsigned int)addr;
		*addr = addr_val_write;
		addr++;
	}
}

static int mem_test_cal_read(UINT32 offset, UINT8 rda2)
{
	volatile unsigned int *addr, addr_val_read, addr_val_write;

	addr = (volatile unsigned int *)(0x83000000);
	while((unsigned int)addr < offset){
		if (((unsigned int )addr % 8) == 0)
			addr_val_write = 0xfefefefe | rda2;
		else
			addr_val_write = (unsigned int)addr;
		addr_val_read = *addr;
		if (addr_val_read != addr_val_write){
			//printf("Read ddr error, write value = %x, read val = %x",(unsigned int)addr, addr_val_read);
			return -1;
		}
		addr++;
	}

	return 0;
}

static void mem_test_result(UINT32 offset, UINT8 rda2)
{
	rda2_result[rda2]='R';

	mem_test_cal_write(offset, rda2);
	if (0 != mem_test_cal_read(offset, rda2))
		rda2_result[rda2]='*';
}

static int ddr_rda2_cal(UINT32 offset, UINT8 * rda2_choose, UINT8 rda2_old)
{
	u8 rda2 = 0;

	for (rda2=0;rda2 < 8;rda2=rda2 + 1){
		sys_reset_mem();
		sys_shutdown_pll();
		ispi_open(0);
		pll_setup_init();
		pll_setup_init_cal_dynamic(rda2);
		pll_setup_freq();
		sys_setup_pll();
		sys_setup_clk();

		ddr_init(g_clock_config->DDR_FLAGS, g_clock_config->DDR_PARA);
		mem_test_result(offset, rda2);
	}

	return print_rda2_result(rda2_choose, rda2_old);
}

/**
SRAM MAP
-- 0x100000
| +CONFIG_TRAP_SIZE(0xC0)
--
| +CONFIG_UIMAGEHDR_SIZE(0x40)
-- spl_text_start_addr
| +TEXT DATA
-- ddr_cal_val_start_addr
| +CONFIG_DDR_CAL_VAL_SIZE
--
| + BSS STATCK
--
| +SPL BOARD INFO SIZE(0x200)
-- 0x110000
**/
static void ddr_auto_cal_val_save(void)
{
	spl_bd_t * spl_board_info = (spl_bd_t *)CONFIG_SPL_BOARD_INFO_ADDR;

	spl_board_info->spl_ddr_cal_info.ddr_auto_cal_flag = CONIFG_DDR_CAL_VAL_FLAG;
	spl_board_info->spl_ddr_cal_info.ddr_auto_cal_offs = (u8 *)(ddr_cal_val_start_addr)
		- (u8 *)(spl_text_start_addr) + CONFIG_UIMAGEHDR_SIZE;
	spl_board_info->spl_ddr_cal_info.ddr_auto_cal_val[0] = g_clock_config->DDR_TIMING_103H;
}

static void ddr_auto_cal_choose(void)
{
	UINT8 rda2 = 0;
	UINT8 rda2_old = 0;
	int rda2_r_total_num = 0;
	int i = 0;

	g_debug_ddr3 = 0;
	rda2_old = (g_clock_config->DDR_TIMING_103H & 0xf000) >> 12;

	for (i = 0; i < 3; i++){
		int off = i << 18;

		if (2 == i)
			off = i << 20;
		rda2_r_total_num = ddr_rda2_cal(0x83040000 + off, &rda2, rda2_old);
		if (8 != rda2_r_total_num)
			break;
	}

	sys_reset_mem();
	sys_shutdown_pll();
	ispi_open(0);
	pll_setup_init();
	pll_save_ddr_cal_dynamic(rda2);
	pll_setup_init_cal_dynamic(rda2);
	pll_setup_freq();
	sys_setup_pll();
	sys_setup_clk();

	g_debug_ddr3 = 1;
}
#endif

int clock_init(void)
{
#ifdef DO_DDR_PLL_DEBUG
	char choice = 'n';
#endif

#ifdef DDR_TIMING_101H_VAL_U08
	u16 metal_id = rda_metal_id_get();
#endif
#ifdef _TGT_AP_DDR_AUTO_CALI_ENABLE
	u16 ddr_auto_cal_val;
	spl_text_start_addr = (u16 *)&__start;
	ddr_cal_val_start_addr = (u16 *)&__ddrcal_start;

	u16 ddr_auto_cal_flag = *ddr_cal_val_start_addr;
	ddr_auto_cal_val = *(ddr_cal_val_start_addr + 1);
#endif

	/* First check current usb usage */
	check_usb_usage();

	printf("Init Clock ...\n");
	g_clock_config = get_default_clock_config();

	printf("Clock config ver: %d.%d\n",
		g_clock_config->VERSION_MAJOR, g_clock_config->VERSION_MINOR);

#ifdef _TGT_AP_DDR_AUTO_CALI_ENABLE
	if (CONIFG_DDR_CAL_VAL_FLAG == ddr_auto_cal_flag)
		pll_setup_ddr_cal_dynamic(ddr_auto_cal_val);
#endif
	pmu_setup_init();

#ifdef DO_DDR_PLL_DEBUG
	printf("If you want to config the ddr para manully ?(y = yes, n = no) \n");
	choice = serial_getc();
	if (choice == 'y')
	{
		memcpy(&clock_debug_config, g_clock_config,
				sizeof(clock_debug_config));
		g_clock_config = &clock_debug_config;
		pmu_setup_calibration_voltage(1);
		pmu_buck4_buck3_choose();
		freq_choose();
		data_bits_choose();
		clock_load_ddr_cal_config();
	}
#endif

	sys_shutdown_pll();
	ispi_open(0);

	reg_cache_start();

#ifdef DDR_TIMING_101H_VAL_U08
	printf("Metal id = %x\n", (int)metal_id);

	if (0xA == metal_id){
		printf("Default val will be used.");
	}
	else if (0xC == metal_id){
		pll_val_reset(metal_id);
	}
	else{
		printf("Sorry, metal id is not fit\n");
	}
#endif

	pll_setup_init();
#ifdef DO_DDR_PLL_DEBUG
	if (choice == 'y')
		cmd_input();
#endif
	pll_setup_freq();
	sys_setup_pll();
	sys_setup_clk();

	if (g_clock_config->DDR_CAL) {
		printf("Init DDR for ddr_cal\n");
		ddr_init(g_clock_config->DDR_FLAGS, g_clock_config->DDR_PARA);
		printf("Done\n");
		pll_setup_mem_cal();
		clock_save_config();
	} else {
		pll_setup_mem();
	}
#ifdef _TGT_AP_DDR_AUTO_CALI_ENABLE
	if (CONIFG_DDR_CAL_VAL_FLAG != ddr_auto_cal_flag){
		ddr_auto_cal_choose();
	}
#endif
	sys_dump_pll_freq();
	sys_dump_clk();
	pll_dump_reg();

	printf("Init DDR, flag = 0x%04x, para = 0x%08lx\n",
		g_clock_config->DDR_FLAGS, g_clock_config->DDR_PARA);
	ddr_init(g_clock_config->DDR_FLAGS, g_clock_config->DDR_PARA);
#ifdef _TGT_AP_DDR_AUTO_CALI_ENABLE
	ddr_auto_cal_val_save();
#endif
	reg_cache_end();

	printf("Done\n");

	return 0;
}

#else /* CONFIG_RDA_FPGA */
int clock_init(void)
{
	u16 ddr_flags = DDR_FLAGS_DLLOFF
		| DDR_FLAGS_ODT(1)
		| DDR_FLAGS_RON(0);
	//16bit
	u32 ddr_para = DDR_PARA_MEM_BITS(1)
		| DDR_PARA_BANK_BITS(3)
		| DDR_PARA_ROW_BITS(3)
		| DDR_PARA_COL_BITS(2);

	printf("Init DDR\n");
	ddr_init(ddr_flags, ddr_para);
	printf("Done\n");

#ifdef DO_SIMPLE_DDR_TEST
	mem_test_write();
	mem_test_read();
#endif
	return 0;
}

#endif /* CONFIG_RDA_FPGA */

