#include <rthw.h>
#include <rtthread.h>
#include <riscv-ops.h>
#include <riscv_encoding.h>
#include "k3_hsm.h"
#include <register_defination.h>
#include "../spacemit-rpmi.h"

void spacemit_cx_m2_int_enable(rt_uint32_t hartid)
{
	rt_uint32_t val;
	clusterx_m2_lp_ctrl *clx_m2_lp_ctl;
	rpmi_uint32_t cluster_id = CPU_TO_CLUSTER(hartid);

	switch (cluster_id) {
	case 0:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C0_M2_INT_EN_REG;
		val = readl((unsigned int *)AUDIO_WAKEUP_EN_REG);
		val |= (3 << 10);
		writel(val, (unsigned int *)AUDIO_WAKEUP_EN_REG);
		break;
	case 1:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C1_M2_INT_EN_REG;
		val = readl((unsigned int *)AUDIO_WAKEUP_EN_REG);
		val |= (3 << 12);
		writel(val, (unsigned int *)AUDIO_WAKEUP_EN_REG);
		break;
	case 2:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C2_M2_INT_EN_REG;
		val = readl((unsigned int *)AUDIO_WAKEUP_EN_REG);
		val |= (3 << 14);
		writel(val, (unsigned int *)AUDIO_WAKEUP_EN_REG);
		break;
	case 3:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C3_M2_INT_EN_REG;
		val = readl((unsigned int *)AUDIO_WAKEUP_EN_REG);
		val |= (3 << 16);
		writel(val, (unsigned int *)AUDIO_WAKEUP_EN_REG);
		break;
	default:
		break;
	}

	val = readl((unsigned int *)clx_m2_lp_ctl);
	val |= (1 << 0);
	writel(val, (unsigned int *)clx_m2_lp_ctl);
}

void spacemit_cx_m2_enter_wait(rt_uint32_t hartid)
{
	rt_uint32_t val;
	clusterx_m2_lp_ctrl *clx_m2_lp_ctl;
	rpmi_uint32_t cluster_id = CPU_TO_CLUSTER(hartid);

	switch (cluster_id) {
	case 0:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C0_M2_INT_EN_REG;
		break;
	case 1:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C1_M2_INT_EN_REG;
		break;
	case 2:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C2_M2_INT_EN_REG;
		break;
	case 3:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C3_M2_INT_EN_REG;
		break;
	default:
		break;
	}

	val = readl((unsigned int *)clx_m2_lp_ctl);
	val |= (1 << 2);
	writel(val, (unsigned int *)clx_m2_lp_ctl);

	val = readl((unsigned int *)clx_m2_lp_ctl);
	while ((val >> 4) & 0x1) {
		val = readl((unsigned int *)clx_m2_lp_ctl);	
	}

	val = readl((unsigned int *)clx_m2_lp_ctl);
	val &= ~(1 << 2);
	writel(val, (unsigned int *)clx_m2_lp_ctl);
}

void spacemit_cx_m2_int_disabled(rt_uint32_t hartid)
{
	rt_uint32_t val;
	clusterx_m2_lp_ctrl *clx_m2_lp_ctl;
	rpmi_uint32_t cluster_id = CPU_TO_CLUSTER(hartid);

	switch (cluster_id) {
	case 0:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C0_M2_INT_EN_REG;
		val = readl((unsigned int *)AUDIO_WAKEUP_EN_REG);
		val &= ~(3 << 10);
		writel(val, (unsigned int *)AUDIO_WAKEUP_EN_REG);
		break;
	case 1:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C1_M2_INT_EN_REG;
		val = readl((unsigned int *)AUDIO_WAKEUP_EN_REG);
		val &= ~(3 << 12);
		writel(val, (unsigned int *)AUDIO_WAKEUP_EN_REG);
		break;
	case 2:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C2_M2_INT_EN_REG;
		val = readl((unsigned int *)AUDIO_WAKEUP_EN_REG);
		val &= ~(3 << 14);
		writel(val, (unsigned int *)AUDIO_WAKEUP_EN_REG);
		break;
	case 3:
		clx_m2_lp_ctl = (clusterx_m2_lp_ctrl *)AP_C3_M2_INT_EN_REG;
		val = readl((unsigned int *)AUDIO_WAKEUP_EN_REG);
		val &= ~(3 << 16);
		writel(val, (unsigned int *)AUDIO_WAKEUP_EN_REG);
		break;
	default:
		break;
	}

	val = readl((unsigned int *)clx_m2_lp_ctl);
	val |= (0x1 << 1);
	writel(val, (unsigned int *)clx_m2_lp_ctl);

 	val = readl((unsigned int *)clx_m2_lp_ctl);
	while (((val >> 6) & 0x3f) != 1) {
		val = readl((unsigned int *)clx_m2_lp_ctl);
	}

	val = readl((unsigned int *)clx_m2_lp_ctl);
	val &= ~(0x1 << 1);
	writel(val, (unsigned int *)clx_m2_lp_ctl);

	val = readl((unsigned int *)clx_m2_lp_ctl);
	val &= ~(0x1 << 0);
	writel(val, (unsigned int *)clx_m2_lp_ctl);
}

void spacemit_assert_corex(unsigned int hartid)
{
	unsigned int val;

	switch (hartid) {
	case 0:
		val = readl((unsigned int *)PMU_CC2_AP);
		val |= (1 << 0);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;	
	case 1:
		val = readl((unsigned int *)PMU_CC2_AP);
		val |= (1 << 3);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 2:
		val = readl((unsigned int *)PMU_CC2_AP);
		val |= (1 << 6);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 3:
		val = readl((unsigned int *)PMU_CC2_AP);
		val |= (1 << 9);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 4:
		val = readl((unsigned int *)PMU_CC2_AP);
		val |= (1 << 16);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 5:
		val = readl((unsigned int *)PMU_CC2_AP);
		val |= (1 << 19);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 6:
		val = readl((unsigned int *)PMU_CC2_AP);
		val |= (1 << 22);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 7:
		val = readl((unsigned int *)PMU_CC2_AP);
		val |= (1 << 25);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 8:
		val = readl((unsigned int *)PMU_CC3_AP);
		val |= (1 << 6);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 9:
		val = readl((unsigned int *)PMU_CC3_AP);
		val |= (1 << 9);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 10:
		val = readl((unsigned int *)PMU_CC3_AP);
		val |= (1 << 12);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 11:
		val = readl((unsigned int *)PMU_CC3_AP);
		val |= (1 << 15);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 12:
		val = readl((unsigned int *)PMU_CC3_AP);
		val |= (1 << 16);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 13:
		val = readl((unsigned int *)PMU_CC3_AP);
		val |= (1 << 19);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 14:
		val = readl((unsigned int *)PMU_CC3_AP);
		val |= (1 << 22);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 15:
		val = readl((unsigned int *)PMU_CC3_AP);
		val |= (1 << 25);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	}
}

void spacemit_deassert_corex(unsigned int hartid)
{
	unsigned int val;

	switch (hartid) {
	case 0:
		val = readl((unsigned int *)PMU_CC2_AP);
		val &= ~(1 << 0);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;	
	case 1:
		val = readl((unsigned int *)PMU_CC2_AP);
		val &= ~(1 << 3);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 2:
		val = readl((unsigned int *)PMU_CC2_AP);
		val &= ~(1 << 6);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 3:
		val = readl((unsigned int *)PMU_CC2_AP);
		val &= ~(1 << 9);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 4:
		val = readl((unsigned int *)PMU_CC2_AP);
		val &= ~(1 << 16);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 5:
		val = readl((unsigned int *)PMU_CC2_AP);
		val &= ~(1 << 19);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 6:
		val = readl((unsigned int *)PMU_CC2_AP);
		val &= ~(1 << 22);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 7:
		val = readl((unsigned int *)PMU_CC2_AP);
		val &= ~(1 << 25);
		writel(val, (unsigned int *)PMU_CC2_AP);
		break;
	case 8:
		val = readl((unsigned int *)PMU_CC3_AP);
		val &= ~(1 << 6);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 9:
		val = readl((unsigned int *)PMU_CC3_AP);
		val &= ~(1 << 9);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 10:
		val = readl((unsigned int *)PMU_CC3_AP);
		val &= ~(1 << 12);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 11:
		val = readl((unsigned int *)PMU_CC3_AP);
		val &= ~(1 << 15);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 12:
		val = readl((unsigned int *)PMU_CC3_AP);
		val &= ~(1 << 16);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 13:
		val = readl((unsigned int *)PMU_CC3_AP);
		val &= ~(1 << 19);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 14:
		val = readl((unsigned int *)PMU_CC3_AP);
		val &= ~(1 << 22);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	case 15:
		val = readl((unsigned int *)PMU_CC3_AP);
		val &= ~(1 << 25);
		writel(val, (unsigned int *)PMU_CC3_AP);
		break;
	}
}

void spacemit_vote_powrdown_cluster(unsigned int hartid)
{
	unsigned int value;

	/* vote core power-down & cluster power-down */
	switch (hartid) {
	case 0:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG0);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG0);
		break;
	case 1:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG1);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG1);
		break;
	case 2:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG2);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG2);
		break;
	case 3:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG3);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG3);
		break;
	case 4:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG4);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG4);
		break;
	case 5:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG5);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG5);
		break;
	case 6:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG6);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG6);
		break;
	case 7:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG7);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG7);
		break;
	case 8:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG8);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG8);
		break;
	case 9:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG9);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG9);
		break;
	case 10:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG10);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG10);
		break;
	case 11:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG11);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG11);
		break;
	case 12:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG12);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG12);
		break;
	case 13:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG13);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG13);
		break;
	case 14:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG14);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG14);
		break;
	case 15:
		value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG15);
		value |= CLUSTER_PWR_DOWN_VALUE;
		writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG15);
		break;
	default:
		break;
	}
}

int spacemit_wakeup_c0(void)
{
	writel((1 << 0), (unsigned int *)PMU_CAP_CORE0_WAKEUP);
}

void spacemit_devote_pwrdown_c2(void)
{
	unsigned int value;

	value = readl((unsigned int *)PMU_CAP_CORE8_IDLE_CFG);
	value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
	writel(value, (unsigned int *)PMU_CAP_CORE8_IDLE_CFG);
	value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG8);
	value &= ~CLUSTER_PWR_DOWN_VALUE;
	writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG8);
}

void spacemit_vote_powrdown_c2_core(void)
{
	unsigned int value;

	value = readl((unsigned int *)PMU_CAP_CORE8_IDLE_CFG);
	value |= CPU_PWR_DOWN_VALUE;
	writel(value, (unsigned int *)PMU_CAP_CORE8_IDLE_CFG);
}

int spacemit_wakeup_c2(void)
{
	writel((1 << 8), (unsigned int *)PMU_CAP_CORE8_WAKEUP);
}

void spacemit_devote_pwrdown_c3(void)
{
	unsigned int value;

	value = readl((unsigned int *)PMU_CAP_CORE12_IDLE_CFG);
	value &= ~(CPU_PWR_DOWN_VALUE | CPU_MASK_FI_INTTERUPT);
	writel(value, (unsigned int *)PMU_CAP_CORE12_IDLE_CFG);
	value = readl((unsigned int *)PMU_CX_CAPMP_IDLE_CFG12);
	value &= ~CLUSTER_PWR_DOWN_VALUE;
	writel(value, (unsigned int *)PMU_CX_CAPMP_IDLE_CFG12);
}

void spacemit_vote_powrdown_c3_core(void)
{
	unsigned int value;

	value = readl((unsigned int *)PMU_CAP_CORE12_IDLE_CFG);
	value |= CPU_PWR_DOWN_VALUE;
	writel(value, (unsigned int *)PMU_CAP_CORE12_IDLE_CFG);
}

int spacemit_wakeup_c3(void)
{
	writel((1 << 12), (unsigned int *)PMU_CAP_CORE12_WAKEUP);
}

void boot_entry_dummy(unsigned int hartid)
{
	rpmi_uint32_t cluster_id = CPU_TO_CLUSTER(hartid);

	if (cluster_id == 2) {
		spacemit_devote_pwrdown_c2();
		spacemit_vote_powrdown_c2_core();
	} else if (cluster_id == 3) {
		spacemit_devote_pwrdown_c3();
		spacemit_vote_powrdown_c3_core();
	}

	/* disable local timer */
	write_csr(0x14D, 0xffffffffffffffff);
	/* disable all irq */
        clear_csr(0x304, MIP_SSIP | MIP_MSIP | MIP_STIP | MIP_MTIP | MIP_SEIP | MIP_MEIP);
        /* disable prefetch */
        asm volatile ("fence iorw, iorw");

        while (1)
		asm volatile ("wfi");
}

unsigned long long spacemit_get_c2_bootenty(void)
{
	unsigned long long low, high;

	low = readl((unsigned int *)C2_RVBADDR_LO_ADDR);
	high = readl((unsigned int *)C2_RVBADDR_HI_ADDR);

	return ((high << 32) | low);
}

void spacemit_set_c2_bootenty(unsigned long long _entry)
{
	/* re-set the bootentry of cluster2 */
	writel((unsigned long)_entry & 0xffffffff, (unsigned int *)(C2_RVBADDR_LO_ADDR));

	writel((((unsigned long)_entry) >> 32) & 0xffffffff, (unsigned int*)(C2_RVBADDR_HI_ADDR));
}

unsigned long long spacemit_get_c0_bootenty(void)
{
	unsigned long long low, high;

	low = readl((unsigned int *)C0_RVBADDR_LO_ADDR);
	high = readl((unsigned int *)C0_RVBADDR_HI_ADDR);

	return ((high << 32) | low);
}

void spacemit_set_c0_bootenty(unsigned long long _entry)
{
	/* re-set the bootentry of cluster2 */
	writel((unsigned long)_entry & 0xffffffff, (unsigned int *)(C0_RVBADDR_LO_ADDR));

	writel((((unsigned long)_entry) >> 32) & 0xffffffff, (unsigned int*)(C0_RVBADDR_HI_ADDR));
}

void spacemit_wait_c2_pwrup(void)
{
	unsigned int value;

	while (1) {
		value = readl((unsigned int *)PMU_CORE_STATUS1);
		if (((value & (1 << 3)) == 0) && (value & (1 << 6)))
			break;
	}
}

unsigned long long spacemit_get_c3_bootenty(void)
{
	unsigned long long low, high;

	low = readl((unsigned int *)C3_RVBADDR_LO_ADDR);
	high = readl((unsigned int *)C3_RVBADDR_HI_ADDR);

	return ((high << 32) | low);
}

void spacemit_set_c3_bootenty(unsigned long long _entry)
{
	/* re-set the bootentry of cluster2 */
	writel((unsigned long)_entry & 0xffffffff, (unsigned int *)(C3_RVBADDR_LO_ADDR));

	writel((((unsigned long)_entry) >> 32) & 0xffffffff, (unsigned int*)(C3_RVBADDR_HI_ADDR));
}

void spacemit_wait_c3_pwrup(void)
{
	unsigned int value;

	while (1) {
		value = readl((unsigned int *)PMU_CORE_STATUS1);
		if (((value & (1 << 19)) == 0) && (value & (1 << 22)))
			break;
	}
}

void spacemit_wakeup_rcpu1(void)
{
	unsigned int val;

	/* exit from low power mode */
	val = readl((unsigned int *)PMU_AUDIO_CLK_CTRL);
	val |= ((1 << AUIO_FORCE_PWR_ON_OFFSET) | (1 << AUDIO_CTRL_BY_AP_OFFSET));
	writel(val, (unsigned int *)PMU_AUDIO_CLK_CTRL);
	/* assert rcpu1 */
	writel(0, (unsigned int *)RT24_CORE1_SW_RESET_REG);
	/* keep rcpu1 sleep */
	writel(0, (unsigned int *)RT24_CORE1_SW_WAKEUP_REG);
	/* set hartid */
	writel(1, (unsigned int *)RCPU_CORE1_HART_ID_SET);
	/* de-assert rcpu1 */
	writel(1, (unsigned int *)RT24_CORE1_SW_RESET_REG);
	/* wakeup rcpu1 */
	writel(1, (unsigned int *)RT24_CORE1_SW_WAKEUP_REG);
}
