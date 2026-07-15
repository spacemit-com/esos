#ifndef __K3_HSM_H__
#define __k3_HSM_H__

/***************************cpu******************************/
#define C0_RVBADDR_LO_ADDR          (0xD4282DB0)
#define C0_RVBADDR_HI_ADDR          (0xD4282DB4)

#define C1_RVBADDR_LO_ADDR          (0xD4282C00 + 0x2B0)
#define C1_RVBADDR_HI_ADDR          (0xD4282C00 + 0X2B4)

#define C2_RVBADDR_LO_ADDR          (0xD4282C00 + 0x3E8)
#define C2_RVBADDR_HI_ADDR          (0xD4282C00 + 0X3EC)

#define C3_RVBADDR_LO_ADDR          (0xD4282C00 + 0x260)
#define C3_RVBADDR_HI_ADDR          (0xD4282C00 + 0X264)

#define PMU_CAP_CORE0_WAKEUP            (0xD4282800 + 0x12C)
#define PMU_CAP_CORE1_WAKEUP            (0xD4282800 + 0x130)
#define PMU_CAP_CORE2_WAKEUP            (0xD4282800 + 0x134)
#define PMU_CAP_CORE3_WAKEUP            (0xD4282800 + 0x138)
#define PMU_CAP_CORE4_WAKEUP            (0xD4282800 + 0x324)
#define PMU_CAP_CORE5_WAKEUP            (0xD4282800 + 0x328)
#define PMU_CAP_CORE6_WAKEUP            (0xD4282800 + 0x32C)
#define PMU_CAP_CORE7_WAKEUP            (0xD4282800 + 0x330)
#define PMU_CAP_CORE8_WAKEUP            (0xD4282800 + 0x360)
#define PMU_CAP_CORE9_WAKEUP            (0xD4282800 + 0x364)
#define PMU_CAP_CORE10_WAKEUP           (0xD4282800 + 0x368)
#define PMU_CAP_CORE11_WAKEUP           (0xD4282800 + 0x36C)
#define PMU_CAP_CORE12_WAKEUP           (0xD4282800 + 0x22C)
#define PMU_CAP_CORE13_WAKEUP           (0xD4282800 + 0x230)
#define PMU_CAP_CORE14_WAKEUP           (0xD4282800 + 0x234)
#define PMU_CAP_CORE15_WAKEUP           (0xD4282800 + 0x238)

#define PMU_CAP_CORE0_IDLE_CFG          (0xD4282800 + 0x124)
#define PMU_CAP_CORE1_IDLE_CFG          (0xD4282800 + 0x128)
#define PMU_CAP_CORE2_IDLE_CFG          (0xD4282800 + 0x160)
#define PMU_CAP_CORE3_IDLE_CFG          (0xD4282800 + 0x164)
#define PMU_CAP_CORE4_IDLE_CFG          (0xD4282800 + 0x304)
#define PMU_CAP_CORE5_IDLE_CFG          (0xD4282800 + 0x308)
#define PMU_CAP_CORE6_IDLE_CFG          (0xD4282800 + 0x30c)
#define PMU_CAP_CORE7_IDLE_CFG          (0xD4282800 + 0x310)
#define PMU_CAP_CORE8_IDLE_CFG          (0xD4282800 + 0x340)
#define PMU_CAP_CORE9_IDLE_CFG          (0xD4282800 + 0x344)
#define PMU_CAP_CORE10_IDLE_CFG         (0xD4282800 + 0x348)
#define PMU_CAP_CORE11_IDLE_CFG         (0xD4282800 + 0x34c)
#define PMU_CAP_CORE12_IDLE_CFG         (0xD4282800 + 0x20c)
#define PMU_CAP_CORE13_IDLE_CFG         (0xD4282800 + 0x210)
#define PMU_CAP_CORE14_IDLE_CFG         (0xD4282800 + 0x214)
#define PMU_CAP_CORE15_IDLE_CFG         (0xD4282800 + 0x218)

#define PMU_CX_CAPMP_IDLE_CFG0          (0xd4282800 + 0x120)
#define PMU_CX_CAPMP_IDLE_CFG1          (0xd4282800 + 0xe4)
#define PMU_CX_CAPMP_IDLE_CFG2          (0xd4282800 + 0x150)
#define PMU_CX_CAPMP_IDLE_CFG3          (0xd4282800 + 0x154)
#define PMU_CX_CAPMP_IDLE_CFG4          (0xd4282800 + 0x314)
#define PMU_CX_CAPMP_IDLE_CFG5          (0xd4282800 + 0x318)
#define PMU_CX_CAPMP_IDLE_CFG6          (0xd4282800 + 0x31c)
#define PMU_CX_CAPMP_IDLE_CFG7          (0xd4282800 + 0x320)
#define PMU_CX_CAPMP_IDLE_CFG8          (0xd4282800 + 0x350)
#define PMU_CX_CAPMP_IDLE_CFG9          (0xd4282800 + 0x354)
#define PMU_CX_CAPMP_IDLE_CFG10         (0xd4282800 + 0x358)
#define PMU_CX_CAPMP_IDLE_CFG11         (0xd4282800 + 0x35c)
#define PMU_CX_CAPMP_IDLE_CFG12         (0xd4282800 + 0x21c)
#define PMU_CX_CAPMP_IDLE_CFG13         (0xd4282800 + 0x220)
#define PMU_CX_CAPMP_IDLE_CFG14         (0xd4282800 + 0x224)
#define PMU_CX_CAPMP_IDLE_CFG15         (0xd4282800 + 0x228)

#define APCR_CORE0_VETE_REG		(0xd4050000 + 0x10c0)
#define APCR_CORE1_VETE_REG		(0xd4050000 + 0x10c4)
#define APCR_CORE2_VETE_REG		(0xd4050000 + 0x10c8)
#define APCR_CORE3_VETE_REG		(0xd4050000 + 0x10cc)
#define APCR_CORE4_VETE_REG		(0xd4050000 + 0x10d0)
#define APCR_CORE5_VETE_REG		(0xd4050000 + 0x10d4)
#define APCR_CORE6_VETE_REG		(0xd4050000 + 0x10d8)
#define APCR_CORE7_VETE_REG		(0xd4050000 + 0x10dc)
#define APCR_CORE8_VETE_REG		(0xd4050000 + 0x10e0)
#define APCR_CORE9_VETE_REG		(0xd4050000 + 0x10e4)
#define APCR_CORE10_VETE_REG		(0xd4050000 + 0x10e8)
#define APCR_CORE11_VETE_REG		(0xd4050000 + 0x10ec)
#define APCR_CORE12_VETE_REG		(0xd4050000 + 0x10f0)
#define APCR_CORE13_VETE_REG		(0xd4050000 + 0x10f4)
#define APCR_CORE14_VETE_REG		(0xd4050000 + 0x10f8)
#define APCR_CORE15_VETE_REG		(0xd4050000 + 0x10fc)
#define APCR_PER_VETE_REG		(0xd4050000 + 0x1098)

#define APCR_COREX_DEFAULT_VATE_VALUE	((1 << 3) | (1 << 13) | (1 << 14) | (1 << 19) | (1 << 25) | (1 << 26) | (1 << 27) | (1 << 29) | (1 << 31))

#define PMU_CORE_STATUS0		(0xd4282800 + 0x90)
#define PMU_CORE_STATUS1		(0xd4282800 + 0x80)

#define PMU_CC2_AP			(0xd4282900)
#define PMU_CC3_AP			(0xd4282b38)

#define CORE0_POP_RST_BIT		(0)
#define CORE1_POP_RST_BIT		(3)
#define CORE2_POP_RST_BIT		(6)
#define CORE3_POP_RST_BIT		(9)
#define CORE4_POP_RST_BIT		(16)
#define CORE5_POP_RST_BIT		(19)
#define CORE6_POP_RST_BIT		(22)
#define CORE7_POP_RST_BIT		(25)
#define CORE8_POP_RST_BIT		(6)
#define CORE9_POP_RST_BIT		(9)
#define CORE10_POP_RST_BIT		(12)
#define CORE11_POP_RST_BIT		(15)
#define CORE12_POP_RST_BIT		(16)
#define CORE13_POP_RST_BIT		(19)
#define CORE14_POP_RST_BIT		(22)
#define CORE15_POP_RST_BIT		(25)

#define AP_C0_M2_ENTER_INT_NUM		(83)
#define AP_C0_M2_EXIT_INT_NUM		(84)
#define AP_C1_M2_ENTER_INT_NUM		(85)
#define AP_C1_M2_EXIT_INT_NUM		(86)
#define AP_C2_M2_ENTER_INT_NUM		(87)
#define AP_C2_M2_EXIT_INT_NUM		(88)
#define AP_C3_M2_ENTER_INT_NUM		(89)
#define AP_C3_M2_EXIT_INT_NUM		(90)

#define AP_C0_M2_INT_EN_REG		(0xc088c000 + 0x108)
#define AP_C1_M2_INT_EN_REG		(0xc088c000 + 0x10c)
#define AP_C2_M2_INT_EN_REG		(0xc088c000 + 0x110)
#define AP_C3_M2_INT_EN_REG		(0xc088c000 + 0x114)

#define PLATFORM_MAX_CPUS_PER_CLUSTER	4

#define PMU_C0_L2_FLUSH_CTRL            (0xd8440000 + 0x1b0)
#define PMU_C1_L2_FLUSH_CTRL            (0xd8440000 + 0x1b4)
#define PMU_C2_L2_FLUSH_CTRL            (0xd8440000 + 0x1c4)
#define PMU_C3_L2_FLUSH_CTRL            (0xd8440000 + 0x1ec)

#define PMU_L2_FLUSH_HW_TYPE            (1 << 0)
#define PMU_L2_FLUSH_HW_EN              (1 << 2)

#define CPU_MASK_FI_INTTERUPT		((1 << 3) | (1 << 4))
#define CPU_PWR_DOWN_VALUE              (0x1f)
#define CLUSTER_PWR_DOWN_VALUE          (0x8f)

#define CPU_TO_CLUSTER(cpu)    ((cpu) / PLATFORM_MAX_CPUS_PER_CLUSTER)

void spacemit_cx_m2_int_enable(rt_uint32_t hartid);
void spacemit_cx_m2_enter_wait(rt_uint32_t hartid);
void spacemit_cx_m2_int_disabled(rt_uint32_t hartid);
void spacemit_deassert_corex(unsigned int hartid);
void spacemit_assert_corex(unsigned int hartid);
void spacemit_vote_powrdown_cluster(unsigned int hartid);
void boot_entry_dummy(unsigned int hartid);
void c0boot_entry_dummy(unsigned int hartid);
void spacemit_devote_pwrdown_c2(void);
void spacemit_vote_powrdown_c2_core(void);
int spacemit_wakeup_c2(void);
unsigned long long spacemit_get_c2_bootenty(void);
void spacemit_set_c2_bootenty(unsigned long long _entry);
unsigned long long spacemit_get_c0_bootenty(void);
void spacemit_set_c0_bootenty(unsigned long long _entry);
void spacemit_wait_c2_pwrup(void);

void spacemit_devote_pwrdown_c3(void);
void spacemit_vote_powrdown_c3_core(void);
int spacemit_wakeup_c3(void);
unsigned long long spacemit_get_c3_bootenty(void);
void spacemit_set_c3_bootenty(unsigned long long _entry);
void spacemit_wait_c3_pwrup(void);
void spacemit_wakeup_rcpu1(void);
int spacemit_wakeup_c0(void);

#endif /* __k3_HSM_H__ */
