/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <asm/arch/clock.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>
#include <asm/mach-imx/boot_mode.h>
#include <asm/io.h>
#include <netdev.h>
#include <fsl_esdhc.h>
#include <linux/sizes.h>
#include <phy.h>
#include <linux/mdio.h>

#include <asm/arch/imx-regs.h>
#include <asm/arch/mx6ull_pins.h>

#include "siklu_api.h"

DECLARE_GLOBAL_DATA_PTR;

#ifndef CONFIG_SPL_BUILD

/* See comment on MTDPARTS_DEFAULT in include/configs/mx6ullevk_siklu.h.
 * This function is called in cmd/mtdparts.c and cmd/siklu/siklu_nfs_boot.c. */
void board_mtdparts_default(const char **mtdids, const char **mtdparts)
{
	static canary = 0;
	static char *siklu_mtdparts;
	static char *ids;
	
	if (0 == canary) {
		ids = MTDIDS_DEFAULT;
		/*
		 * WARNING: Change similar code in mx6ullevk_siklu_pcb19x_linux.c
		 * run_linux_code().
		 */
		SKL_BOARD_TYPE_E board_type = siklu_get_board_type();
		switch (board_type) {
			case SKL_BOARD_TYPE_PCB195:
			case SKL_BOARD_TYPE_PCB213:
			case SKL_BOARD_TYPE_PCB217:
			case SKL_BOARD_TYPE_PCB295:
			case SKL_BOARD_TYPE_PCB295_AES:
				siklu_mtdparts = MTDPARTS_DEFAULT_PCB217;
				break;
			case SKL_BOARD_TYPE_PCB277: /* EH8020F */
				siklu_mtdparts = MTDPARTS_DEFAULT_PCB277;
				break;
			default:
				printf("Error: Unknown board type 0x%x. Using PCB_217\n",
					board_type);
				siklu_mtdparts = MTDPARTS_DEFAULT_PCB217;
				break;
		}
		canary = 1;
	}
	*mtdids = ids;
	*mtdparts = siklu_mtdparts;
}


/* Called from ./common/board_f.c */
int dram_init(void)
{
	gd->ram_size = imx_ddr_size();
	return 0;
}

int board_mmc_get_env_dev(int devno)
{
	return devno;
}

int mmc_map_to_kernel_blk(int devno)
{
	return devno;
}

#ifdef CONFIG_FEC_MXC

#define MDIO_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
	PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST | PAD_CTL_ODE)

#ifdef CONFIG_SIKLU_BOARD
# define MDC_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
	PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST ) // Siklu board pcb19x requires Open-Drain Disabled for MDC
#else
# define MDC_PAD_CTRL  (MDIO_PAD_CTRL)
#endif



#define ENET_PAD_CTRL  (PAD_CTL_PUS_100K_UP | PAD_CTL_PUE |     \
	PAD_CTL_SPEED_HIGH   |                                  \
	PAD_CTL_DSE_48ohm   | PAD_CTL_SRE_FAST)

#define ENET_CLK_PAD_CTRL  (PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST)

/*
 * pin conflicts for fec1 and fec2, GPIO1_IO06 and GPIO1_IO07 can only
 * be used for ENET1 or ENET2, cannot be used for both.
 */
static const iomux_v3_cfg_t  fec1_pads[] = {
#ifdef SIKLU_PCB19x_SWITCH_MDIO_BUS   // edikk  fec1_pads. used this setup
	/* Special commentaries for Siklu board:
		28.12.2017
		PCB19x allows use only ENET1 controller.
		I tried init ENET2 MDIO module only but it isn't simple
		Therefore I decided connect ENET1 MDC to both buses (the pin is push-pull out
		and able to push both buses, but MDIO switch before generate transaction to
		specified bus)
	 */

	MX6_PAD_ENET2_RX_DATA1__ENET1_MDC | MUX_PAD_CTRL(MDC_PAD_CTRL), // ENET1 MDC  connect 10GPHY and Transceiver
	MX6_PAD_GPIO1_IO07__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),    //  ENET1 MDC  also connected to SOHO
	MX6_PAD_ENET2_RX_DATA0__ENET1_MDIO  | MUX_PAD_CTRL(MDIO_PAD_CTRL),// ENET1 MDIO connect 10GPHY and Transceiver
	// MX6_PAD_GPIO1_IO06__ENET1_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL),   // by default connected to GPIO1_IO06 pin

#else

	MX6_PAD_GPIO1_IO06__ENET1_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL),
	MX6_PAD_GPIO1_IO07__ENET1_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),
#endif //
	MX6_PAD_ENET1_TX_DATA0__ENET1_TDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_DATA1__ENET1_TDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_EN__ENET1_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_TX_CLK__ENET1_REF_CLK1 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL),
	MX6_PAD_ENET1_RX_DATA0__ENET1_RDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_DATA1__ENET1_RDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_ER__ENET1_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET1_RX_EN__ENET1_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
};

static const iomux_v3_cfg_t  fec2_pads[] = {
	MX6_PAD_GPIO1_IO06__ENET2_MDIO | MUX_PAD_CTRL(MDIO_PAD_CTRL),
	MX6_PAD_GPIO1_IO07__ENET2_MDC | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX6_PAD_ENET2_TX_DATA0__ENET2_TDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_TX_DATA1__ENET2_TDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_TX_CLK__ENET2_REF_CLK2 | MUX_PAD_CTRL(ENET_CLK_PAD_CTRL),
	MX6_PAD_ENET2_TX_EN__ENET2_TX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),

	MX6_PAD_ENET2_RX_DATA0__ENET2_RDATA00 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_DATA1__ENET2_RDATA01 | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_EN__ENET2_RX_EN | MUX_PAD_CTRL(ENET_PAD_CTRL),
	MX6_PAD_ENET2_RX_ER__ENET2_RX_ER | MUX_PAD_CTRL(ENET_PAD_CTRL),
};

static void setup_iomux_fec(int fec_id)
{
	if (fec_id == 0)
		imx_iomux_v3_setup_multiple_pads(fec1_pads,
						 ARRAY_SIZE(fec1_pads));
	else
		imx_iomux_v3_setup_multiple_pads(fec2_pads,
						 ARRAY_SIZE(fec2_pads));
}

int board_eth_init(bd_t *bis) /* NOT SPL */
{
	int rc;

	setup_iomux_fec(CONFIG_FEC_ENET_DEV);
	siklu_mdio_bus_connect(SIKLU_MDIO_BUS0);
	rc =  fecmxc_initialize_multi(bis, CONFIG_FEC_ENET_DEV,
				       CONFIG_FEC_MXC_PHYADDR, IMX_FEC_BASE);
	return rc;
}

static int setup_fec(int fec_id)
{
	struct iomuxc *const iomuxc_regs = (struct iomuxc *)IOMUXC_BASE_ADDR;
	int ret;

	if (fec_id == 0) {
		/*
		 * Use 50M anatop loopback REF_CLK1 for ENET1,
		 * clear gpr1[13], set gpr1[17].
		 */
		clrsetbits_le32(&iomuxc_regs->gpr[1], IOMUX_GPR1_FEC1_MASK,
				IOMUX_GPR1_FEC1_CLOCK_MUX1_SEL_MASK);
	} else {
		/*
		 * Use 50M anatop loopback REF_CLK2 for ENET2,
		 * clear gpr1[14], set gpr1[18].
		 */
		clrsetbits_le32(&iomuxc_regs->gpr[1], IOMUX_GPR1_FEC2_MASK,
				IOMUX_GPR1_FEC2_CLOCK_MUX1_SEL_MASK);
	}

	ret = enable_fec_anatop_clock(fec_id, ENET_50MHZ);
	if (ret)
		return ret;

	enable_enet_clk(1);

	return 0;
}

int board_phy_config(struct phy_device *phydev)
{
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1f, 0x8190);

	if (phydev->drv->config)
		phydev->drv->config(phydev);

	return 0;
}
#endif


void siklu_primary_si5344d_pll_init(void)
{
	siklu_si5344d_get_pll_device_addr();
	siklu_si5344d_pll_reg_burn();
}


/* Called from ./common/board_r.c */
int board_init(void)	/* NOT SPL */
{
	int rc = 0;
	/* Address of boot parameters */
	gd->bd->bi_boot_params = PHYS_SDRAM + 0x100;

#ifdef	CONFIG_FEC_MXC
	setup_fec(CONFIG_FEC_ENET_DEV);
#endif

#ifdef CONFIG_SIKLU_BOARD
	rc = siklu_board_init();
#endif // 	CONFIG_SIKLU_BOARD

	return rc;
}

#ifdef CONFIG_CMD_BMODE
static const struct boot_mode board_boot_modes[] = {
	/* 4 bit bus width */
#ifdef CONFIG_SIKLU_BOARD

		/*
 	{"ecspi1:0",	MAKE_CFGVAL(0x30, 0x00, 0x00, 0x08)},
	{"ecspi1:1",	MAKE_CFGVAL(0x30, 0x00, 0x00, 0x18)},
	{"ecspi1:2",	MAKE_CFGVAL(0x30, 0x00, 0x00, 0x28)},
	{"ecspi1:3",	MAKE_CFGVAL(0x30, 0x00, 0x00, 0x38)},

From CLI:
	bmode - sd1|sd2|qspi1|normal|usb|sata|ecspi1:0|ecspi1:1|ecspi1:2|ecspi1:3|esdhc1|esdhc2|esdhc3|esdhc4 [noreset]
		 */

#else
	{"sd1", MAKE_CFGVAL(0x42, 0x20, 0x00, 0x00)},
	{"sd2", MAKE_CFGVAL(0x40, 0x28, 0x00, 0x00)},
	{"qspi1", MAKE_CFGVAL(0x10, 0x00, 0x00, 0x00)},
#endif //
	{NULL,	 0},
};
#endif

/* Called from ./common/board_r.c */
int board_late_init(void)	/* NOT SPL */
{
	int rc = 0;
#ifdef CONFIG_CMD_BMODE
	add_board_boot_modes(board_boot_modes);
#endif

#ifdef CONFIG_ENV_VARS_UBOOT_RUNTIME_CONFIG
	env_set("board_name", "EVK");
	env_set("board_rev", "14X14");
	env_set("ubifs_default_version", UBIFS_DEFAULT_VERSION);
#endif

	siklu_primary_si5344d_pll_init();

#ifdef CONFIG_SIKLU_BOARD
	rc = siklu_board_late_init_hw();
	rc = siklu_board_late_init_env();
#endif // 	CONFIG_SIKLU_BOARD

	return rc;
}


/*
 *
 */
int checkboard(void) {
#ifdef CONFIG_SIKLU_BOARD
	SKL_BOARD_TYPE_E board_type = siklu_get_board_type();

	switch (board_type) {
		case SKL_BOARD_TYPE_PCB195:
			puts("Board: Siklu PCB195\n");
			break;
		case SKL_BOARD_TYPE_PCB213:
			puts("Board: Siklu PCB213\n");
			break;
		case SKL_BOARD_TYPE_PCB217:
			puts("Board: Siklu PCB217\n");
			break;
		case SKL_BOARD_TYPE_PCB277:
			puts("Board: Siklu PCB277\n");
			break;
		case SKL_BOARD_TYPE_PCB295:
			puts("Board: Siklu PCB295\n");
			break;
		case SKL_BOARD_TYPE_PCB295_AES:
			puts("Board: Siklu PCB295_AES\n");
			break;
		default:
			printf("Board: Siklu Unknown (0x%08X)\n", (int)board_type);
			break;
	}
#else
	puts("Board: MX6ULL 14x14 EVK\n");
#endif
	return 0;
}

/* Called from ./common/board_f.c */
int board_early_init_f(void)	/* NOT SPL */
{
	return 0;
}

#endif /* #ifndef CONFIG_SPL_BUILD */

#ifdef CONFIG_SPL_BUILD
#include <asm/arch/mx6-ddr.h>

#define GPIO2_IO18	IMX_GPIO_NR(2, 18)
#define GPIO2_IO19	IMX_GPIO_NR(2, 19)
#define GPIO2_IO20	IMX_GPIO_NR(2, 20)
#define GPIO2_IO21	IMX_GPIO_NR(2, 21)

#define UART_PAD_CTRL  (PAD_CTL_PKE | PAD_CTL_PUE |		\
	PAD_CTL_PUS_100K_UP | PAD_CTL_SPEED_MED |		\
	PAD_CTL_DSE_40ohm   | PAD_CTL_SRE_FAST  | PAD_CTL_HYS)

static iomux_v3_cfg_t const uart1_pads[] = {
	MX6_PAD_UART1_TX_DATA__UART1_DCE_TX | MUX_PAD_CTRL(UART_PAD_CTRL),
	MX6_PAD_UART1_RX_DATA__UART1_DCE_RX | MUX_PAD_CTRL(UART_PAD_CTRL),
};

/* The DDR3 parameters in the following structs have not been checked carefully. */

/* K4B4G1646E-BMMA 500 MB
 * https://semiconductor.samsung.com/resources/data-sheet/DS_K4B4G1646E_BY_M_Rev1_11-0.pdf */
static struct mx6ul_iomux_grp_regs eh8010_grp_ioregs = {
	.grp_addds = 		0x00000030,
	.grp_ddrmode_ctl =	0x00020000,
	.grp_b0ds =		0x00000030,
	.grp_ctlds =		0x00000030,
	.grp_b1ds =		0x00000030,
	.grp_ddrpke =		0x00000000,
	.grp_ddrmode =		0x00020000,
	.grp_ddr_type =		0x000c0000,
};

/* IS43TR16512B-107MBLI 1GB
 * https://www.issi.com/WW/pdf/43-46TR16512B-81024BL.pdf */
static struct mx6ul_iomux_grp_regs eh8020_grp_ioregs = {
	.grp_addds =		0x00000030,
	.grp_ddrmode_ctl =	0x00020000,
	.grp_b0ds =		0x00000030,
	.grp_ctlds =		0x00000030,
	.grp_b1ds =		0x00000030,
	.grp_ddrpke =		0x00000000,
	.grp_ddrmode =		0x00020000,
	.grp_ddr_type =		0x000c0000,
};

/* K4B4G1646E-BMMA 500 MB
 * https://semiconductor.samsung.com/resources/data-sheet/DS_K4B4G1646E_BY_M_Rev1_11-0.pdf */
static struct mx6ul_iomux_ddr_regs eh8010_ddr_ioregs = {
	.dram_dqm0 =		0x00000030,
	.dram_dqm1 =		0x00000030,
	.dram_ras =		0x00000030,
	.dram_cas =		0x00000030,
	.dram_odt0 =		0x00000030,
	.dram_odt1 =		0x00000030,
	.dram_sdba2 =		0x00000000,
	.dram_sdclk_0 =		0x00000030,
	.dram_sdqs0 =		0x00000030,
	.dram_sdqs1 =		0x00000030,
	.dram_reset =		0x00000030,
};

/* IS43TR16512B-107MBLI 1GB
 * https://www.issi.com/WW/pdf/43-46TR16512B-81024BL.pdf */
static struct mx6ul_iomux_ddr_regs eh8020_ddr_ioregs = {
	.dram_dqm0 =		0x00000030,
	.dram_dqm1 =		0x00000030,
	.dram_ras =		0x00000030,
	.dram_cas =		0x00000030,
	.dram_odt0 =		0x00000030,
	.dram_odt1 =		0x00000030,
	.dram_sdba2 =		0x00000000,
	.dram_sdclk_0 =		0x00000030,
	.dram_sdqs0 =		0x00000030,
	.dram_sdqs1 =		0x00000030,
	.dram_reset =		0x00000030,
};

/* K4B4G1646E-BMMA 500 MB
 * https://semiconductor.samsung.com/resources/data-sheet/DS_K4B4G1646E_BY_M_Rev1_11-0.pdf */
static struct mx6_mmdc_calibration eh8010_mmcd_calib = {
	.p0_mpwldectrl0 =	0x00000000,
	.p0_mpdgctrl0 =		0x41570155,
	.p0_mprddlctl =		0x4040474A,
	.p0_mpwrdlctl =		0x40405550,
};

/* IS43TR16512B-107MBLI 1GB
 * https://www.issi.com/WW/pdf/43-46TR16512B-81024BL.pdf */
static struct mx6_mmdc_calibration eh8020_mmcd_calib = {
	.p0_mpwldectrl0 =	0x00000000,
	.p0_mpdgctrl0 =		0x41570155,
	.p0_mprddlctl =		0x4040474A,
	.p0_mpwrdlctl =		0x40405550,
};

/* K4B4G1646E-BMMA 500 MB
 * https://semiconductor.samsung.com/resources/data-sheet/DS_K4B4G1646E_BY_M_Rev1_11-0.pdf */
struct mx6_ddr_sysinfo eh8010_ddr_sysinfo = {
	.dsize = 0,
	.cs_density = 20,
	.ncs = 1,
	.cs1_mirror = 0,
	.rtt_wr = 2,
	.rtt_nom = 1,		/* RTT_Nom = RZQ/2 */
	.walat = 0,		/* Write additional latency */
	.ralat = 5,		/* Read additional latency */
	.mif3_mode = 3,		/* Command prediction working mode */
	.bi_on = 1,		/* Bank interleaving enabled */
	.sde_to_rst = 0x10,	/* 14 cycles, 200us (JEDEC default) */
	.rst_to_cke = 0x23,	/* 33 cycles, 500us (JEDEC default) */
	.ddr_type = DDR_TYPE_DDR3,
	.refsel = 0,	/* Refresh cycles at 64KHz */
	.refr = 1,	/* 2 refresh commands per refresh cycle */
};

/* IS43TR16512B-107MBLI 1GB
 * https://www.issi.com/WW/pdf/43-46TR16512B-81024BL.pdf */
struct mx6_ddr_sysinfo eh8020_ddr_sysinfo = {
	.dsize = 0,
	.cs_density = 24,
	.ncs = 1,
	.cs1_mirror = 0,
	.rtt_wr = 2,
	.rtt_nom = 1,		/* RTT_Nom = RZQ/2 */
	.walat = 0,		/* Write additional latency */
	.ralat = 5,		/* Read additional latency */
	.mif3_mode = 3,		/* Command prediction working mode */
	.bi_on = 1,		/* ?: Bank interleaving enabled */
	.sde_to_rst = 0x10,	/* ?: 14 cycles, 200us (JEDEC default) */
	.rst_to_cke = 0x23,	/* RST_to_CKE: 33 cycles, 500us (JEDEC default) */
	.ddr_type = DDR_TYPE_DDR3,
	.refsel = 0,		/* REF_SEL: Refresh cycles at 64KHz */
	.refr = 1,		/* REFR: 2 refresh commands per refresh cycle */
};

/* K4B4G1646E-BMMA 500 MB
 * https://semiconductor.samsung.com/resources/data-sheet/DS_K4B4G1646E_BY_M_Rev1_11-0.pdf */
static struct mx6_ddr3_cfg eh8010_mem_ddr = {
	.mem_speed = 800,
	.density = 4,
	.width = 16,
	.banks = 8,
	.rowaddr = 15,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1375,
	.trcmin = 4875,
	.trasmin = 3500,
};

/* IS43TR16512B-107MBLI 1GB
 * https://www.issi.com/WW/pdf/43-46TR16512B-81024BL.pdf */
static struct mx6_ddr3_cfg eh8020_mem_ddr = {
	.mem_speed = 1600,
	.density = 8,
	.width = 16,
	.banks = 8,
	.rowaddr = 16,
	.coladdr = 10,
	.pagesz = 2,
	.trcd = 1375,
	.trcmin = 4875,
	.trasmin = 3500,
};


/* See also siklu_get_board_type() in 
 * board/freescale/mx6ullevk/mx6ullevk_siklu_hw.c. */
static unsigned int spl_get_siklu_board_id()
{
	unsigned int id = 0;

	gpio_direction_input(GPIO2_IO18);
	gpio_direction_input(GPIO2_IO19);
	gpio_direction_input(GPIO2_IO20);
	gpio_direction_input(GPIO2_IO21);

	id = id | gpio_get_value(GPIO2_IO21);
	id = id << 1;
	id = id | gpio_get_value(GPIO2_IO20);
	id = id << 1;
	id = id | gpio_get_value(GPIO2_IO19);
	id = id << 1;
	id = id | gpio_get_value(GPIO2_IO18);

	/* That's the way the ID's are defined in siklu_api.h.
	 * 0 is SKL_BOARD_TYPE_UNKNOWN: */
	id++;
	return id;
}


static void setup_iomux_uart(void)
{
	imx_iomux_v3_setup_multiple_pads(uart1_pads, ARRAY_SIZE(uart1_pads));
}


static void spl_dram_init(void)
{
	struct mx6_ddr3_cfg		*mem_ddr;
	struct mx6ul_iomux_ddr_regs	*mx6_ddr_ioregs;
	struct mx6ul_iomux_grp_regs	*mx6_grp_ioregs;
	struct mx6_ddr_sysinfo		*ddr_sysinfo;
	struct mx6_mmdc_calibration	*mx6_mmcd_calib;

	unsigned int siklu_board_id = spl_get_siklu_board_id();

	/* See siklu_api.h for enum defs */
	switch (siklu_board_id) {
		case SKL_BOARD_TYPE_PCB195:
		case SKL_BOARD_TYPE_PCB213:
		case SKL_BOARD_TYPE_PCB217:
		case SKL_BOARD_TYPE_PCB295:
		case SKL_BOARD_TYPE_PCB295_AES:
			/* K4B4G1646E-BMMA 500 MB
			 * https://semiconductor.samsung.com/resources/data-sheet/DS_K4B4G1646E_BY_M_Rev1_11-0.pdf */
			mem_ddr		= &eh8010_mem_ddr;
			mx6_ddr_ioregs	= &eh8010_ddr_ioregs;
			mx6_grp_ioregs	= &eh8010_grp_ioregs;
			ddr_sysinfo	= &eh8010_ddr_sysinfo;
			mx6_mmcd_calib	= &eh8010_mmcd_calib;
			break;
		case SKL_BOARD_TYPE_PCB277: /* EH8020F */
			/* IS43TR16512B-107MBLI 1GB
			 * https://www.issi.com/WW/pdf/43-46TR16512B-81024BL.pdf */
			mem_ddr		= &eh8020_mem_ddr;
			mx6_ddr_ioregs	= &eh8020_ddr_ioregs;
			mx6_grp_ioregs	= &eh8020_grp_ioregs;
			ddr_sysinfo	= &eh8020_ddr_sysinfo;
			mx6_mmcd_calib	= &eh8020_mmcd_calib;
			break;
		default:
			printf("Siklu unrecognized board id %u\n", siklu_board_id);
			break;
	}
	mx6ul_dram_iocfg(mem_ddr->width, mx6_ddr_ioregs, mx6_grp_ioregs);
	mx6_dram_cfg(ddr_sysinfo, mx6_mmcd_calib, mem_ddr);
}


/* This voodoo is copy-pasted from
 * board/freescale/mx6ul_14x14_evk/mx6ul_14x14_evk.c. It is needed particularly
 * for the NAND DMA initialization. */
static void ccgr_init(void)
{
	struct mxc_ccm_reg *ccm = (struct mxc_ccm_reg *)CCM_BASE_ADDR;

	writel(0xFFFFFFFF, &ccm->CCGR0);
	writel(0xFFFFFFFF, &ccm->CCGR1);
	writel(0xFFFFFFFF, &ccm->CCGR2);
	writel(0xFFFFFFFF, &ccm->CCGR3);
	writel(0xFFFFFFFF, &ccm->CCGR4);
	writel(0xFFFFFFFF, &ccm->CCGR5);
	writel(0xFFFFFFFF, &ccm->CCGR6);
	writel(0xFFFFFFFF, &ccm->CCGR7);
}


int board_spi_cs_gpio(unsigned bus, unsigned cs)
{
        return IMX_GPIO_NR(1, 20);
}


void board_init_f(ulong dummy) { /* SPL */
	/* Critical - clock tree init */
	ccgr_init();

	/* Setup AIPS and disable watchdog */
	arch_cpu_init();

	/* Setup GP timer (Siklu needed) */
	timer_init();

	/* Setup console output and test it to see that the SPL is alive. */
	setup_iomux_uart();
	enable_uart_clk(1);
	preloader_console_init();

	spl_dram_init(); /* See above */
}

#endif /* #ifdef CONFIG_SPL_BUILD */
