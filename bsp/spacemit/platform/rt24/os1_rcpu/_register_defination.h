#ifndef __SPACEMIT_REGISTER_DEF_H__
#define __SPACEMIT_REGISTER_DEF_H__

#include <rtdef.h>

typedef volatile struct {
	union {
		rt_uint32_t RBR;	/* Offset: 0x000 Receive buffer register */
		rt_uint32_t THR;	/* Offset: 0x000 Transmission hold register */
		rt_uint32_t DLL;	/* Offset: 0x000 Clock frequency division low section register */
	};
	union {
		rt_uint32_t DLH;	/* Offset: 0x004 Interrupt enable register */
		rt_uint32_t IER;	/* Offset: 0x004 Clock frequency division high section register */
	};
	union {
		rt_uint32_t IIR;	/* Offset: 0x008 Interrupt indicia register */
		rt_uint32_t FCR;	/* Offset: 0x008 FIFO control register */
	};
	rt_uint32_t LCR;	/* Offset: 0x00C Transmission control register */
	rt_uint32_t MCR;
	rt_uint32_t LSR;	/* Offset: 0x014 Transmission state register */
	rt_uint32_t MSR;	/* Offset: 0x018 Modem state register */
	rt_uint32_t SCR;
	rt_uint32_t ISR;
	rt_uint32_t FOR;
	rt_uint32_t ABR;
	rt_uint32_t ACR;
} pxa_uart_reg_t;

/* AUDIO PMU */
#define AUDIO_PMU_VOTE_REG			0xc088c018
#define VOTE_FOR_AUDIO_ENTER_PWROFF_MODE	3
#define VOTE_FOR_AUDIO_ENTER_PLLOFF_MODE	2
#define VOTE_FOR_AUDIO_ENTER_LOWPWR_MODE	1

typedef volatile union audio_pmu_vote_reg {
	rt_uint32_t val;
	struct {
		rt_uint32_t vote_for_clk_off:1;
		rt_uint32_t reserved:1;
		rt_uint32_t vote_for_plloff:1;
		rt_uint32_t reserved2:29;
	} bits;
} audio_pmu_vote_t;

#define AUDIO_VOTE_FOR_MAIN_PMU			0xc088c020
#define VOTE_FOR_AP_AXI_CLK_OFF			6
#define VOTE_FOR_DDR_SHUTDOWN			5
#define VOTE_FOR_VCTCXO_OFF			3
#define VOTE_FOR_MAIN_PMU_ENTER_SLEEP_STATE	2
#define VOTE_FOR_AP_GOTO_STANDBY_STATE		1

typedef volatile union audio_vote_for_main_mpu_reg {
	rt_uint32_t val;
	struct {
		rt_uint32_t reserved0:1;
		rt_uint32_t audio_pmu_vote_stben:1;
		rt_uint32_t audio_pmu_vote_slpen:1;
		rt_uint32_t audio_pmu_vote_vctcxosd:1;
		rt_uint32_t reserved1:1;
		rt_uint32_t audio_pmu_vote_ddrsd:1;
		rt_uint32_t audio_pmu_vote_axisd:1;
		rt_uint32_t reserved2:25;
	} bits;
} audio_vote_for_main_mpu_t;


#define AUDIO_WAKEUP_EN_REG			0xc088c028

typedef volatile union audio_wakeup_en_reg {
	rt_uint32_t val;
	struct {
		rt_uint32_t shub_int_wkup_en:1;
		rt_uint32_t ipc_ap_wkup_en:1;
		rt_uint32_t ipc_cp_wkup_en:1;
		rt_uint32_t ap_wkup_en:1;
		rt_uint32_t timer_wkup_en:1;
		rt_uint32_t ipc_msa_wkup_en:1;
		rt_uint32_t reserved0:1;
		rt_uint32_t icu_wkup_en:1;
		rt_uint32_t reserved1:2;
		rt_uint32_t ap_c0_m2_wkup_en:1;
		rt_uint32_t ap_c0_m2_enter_wkup_en:1;
		rt_uint32_t ap_c1_m2_wkup_en:1;
		rt_uint32_t ap_c1_m2_enter_wkup_en:1;
		rt_uint32_t ap_c2_m2_wkup_en:1;
		rt_uint32_t ap_c2_m2_enter_wkup_en:1;
		rt_uint32_t ap_c3_m2_wkup_en:1;
		rt_uint32_t ap_c3_m2_enter_wkup_en:1;
		rt_uint32_t d2_enter_en:1;
		rt_uint32_t d2_exit_en:1;
		rt_uint32_t reserved2:12;
	} bits;
} audio_wakeup_en_t;

#define SOC_TOP_D2_LP_CTRL			0xc088c0fc

typedef volatile union soc_top_d2_lp_ctrl_reg {
	rt_uint32_t val;
	struct {
		rt_uint32_t rcpu_ctrl_soc_top_d2_lp_en:1;
		rt_uint32_t clr_soc_top_d2_wakeup_hw_mask:1;
		rt_uint32_t soc_top_d2_enter_int_clr:1;
		rt_uint32_t reserved0:1;
		rt_uint32_t soc_top_d2_enter_int_status:1;
		rt_uint32_t soc_top_d2_wakeup_int_status:1;
		rt_uint32_t reserved1:26;
	} bits;
} soc_top_d2_lp_ctrl;

typedef volatile union clusterx_m2_lp_ctrl_reg {
	rt_uint32_t val;
	struct {
		rt_uint32_t rcpu_ctrl_clx_m2_lp_en:1;
		rt_uint32_t clr_clx_m2_wkup_hw_msk:1;
		rt_uint32_t clx_m2_enter_int_clk:1;
		rt_uint32_t reserved0:1;
		rt_uint32_t clx_m2_enter_int:1;
		rt_uint32_t clx_m2_wkup_int:1;
		rt_uint32_t clx_mp_state:6;
		rt_uint32_t reserved1:20;
	} bits;
} clusterx_m2_lp_ctrl;

#define RT24_CORE0_IDLE_CFG_REG		(0xc088c000 + 0xdc)

typedef volatile union rt24_core0_idle_cfg_reg {
	rt_uint32_t val;
	struct {
		rt_uint32_t core_idle:1;
		rt_uint32_t core_pwrdwn:1;
		rt_uint32_t msk_core_clk_statble_check:1;
		rt_uint32_t msk_core_wfi_state_check:1;
		rt_uint32_t reserved:28;
	} bits;
} rt24_core0_idle_cfg;

#define RT24_CORE1_IDLE_CFG_REG		(0xc088c000 + 0xe0)

#define RT24_PMU_STATUS			(0xc0880000 + 0x64)

#define PWRCTL_LP_WAKEUP_MASK		(0xc088c060)

#define RCPU_CORE0_BOOT_ENTRY_LO	0xc088007c
#define RCPU_CORE0_BOOT_ENTRY_HI	0xc0880080

#define RCPU_CORE1_BOOT_ENTRY_LO	0xc088008c
#define RCPU_CORE1_BOOT_ENTRY_HI	0xc0880090

#define APCR_PER_VETE_REG              (0xd4050000 + 0x1098)

#define RT_HEAP_START		0x100a00000
#define RT_HEAP_END		0x100c00000
#define SHARED_MEM_PA		0x30200000

#define RT24_CORE0_SW_WAKEUP_REG	(0xc088c0d4)
#define RT24_CORE1_SW_WAKEUP_REG	(0xc088c0d8)
#define RT24_CORE0_SW_RESET_REG		(0xc088c0cc)
#define RT24_CORE1_SW_RESET_REG		(0xc088c0d0)
#define RCPU_CORE1_HART_ID_SET		0xc0880094

#define PMU_AUDIO_CLK_CTRL		(0xd428294c)
#define AUIO_FORCE_PWR_ON_OFFSET	(13)
#define AUDIO_CTRL_BY_AP_OFFSET		(28)

#define AP_C0_M2_ENTER_INT_NUM		(83)
#define AP_C0_M2_EXIT_INT_NUM		(84)
#define AWUCRM_REG			(0xd4050000 + 0x104c)

#define DTB_TABLE_BASE_ADDR		(0x100d01000)
#define DTB_TABLE_STEP			(1024 * 50)

/* ap & rt24 data interaction space */
#define AR_DATA_INTERACTION_BASE	(0x100d00000)

#endif
