/*
 * mx6ullevk_siklu_hw.c
 *
 *  Created on: Dec 12, 2017
 *      Author: edwardk
 */
#ifndef CONFIG_SPL_BUILD

#include <common.h>
#include <linux/ctype.h>
#include <nand.h>
#include <command.h>
#include <version.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <environment.h>
#include <asm/arch/iomux.h>
#include <asm/arch/imx-regs.h>
#include <asm/arch/crm_regs.h>
#include <asm/arch/mx6-pins.h>
#include <asm/arch/sys_proto.h>
#include <asm/gpio.h>
#include <asm/mach-imx/iomux-v3.h>

#include <miiphy.h>

#include "siklu_def.h"
#include "siklu_api.h"
#include "cpld_reg.h"


#include <spi.h>

static const iomux_v3_cfg_t cpld_pads[] = { //
		MX6_PAD_CSI_DATA01__ECSPI2_SS0 | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_LCD_HSYNC__ECSPI2_SS1 | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_CSI_DATA00__ECSPI2_SCLK | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_CSI_DATA02__ECSPI2_MOSI | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_CSI_DATA03__ECSPI2_MISO | MUX_PAD_CTRL(NO_PAD_CTRL) //
//
		};

#define GPIO_ECSPI1_SS_MANUAL_CONTROL 1 // in this mode ECSPI1_SS0 andECSPI1_SS1  controlled via GPIO, not by controller!

#ifdef GPIO_ECSPI1_SS_MANUAL_CONTROL
# define GPIO_ECSPI1_SS0 IMX_GPIO_NR(4, 26)
# define GPIO_ECSPI1_SS1 IMX_GPIO_NR(3, 10)
# define GPIO_ECSPI1_SS2 IMX_GPIO_NR(3, 11)
# define GPIO_ECSPI1_SS3 IMX_GPIO_NR(3, 12)

#endif // GPIO_ECSPI1_SS_MANUAL_CONTROL

static const iomux_v3_cfg_t rfic_pads[] = { //
		/*  */
#ifndef GPIO_ECSPI1_SS_MANUAL_CONTROL
				MX6_PAD_CSI_DATA05__ECSPI1_SS0 | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_LCD_DATA05__ECSPI1_SS1 | MUX_PAD_CTRL(NO_PAD_CTRL),//
				MX6_PAD_LCD_DATA06__ECSPI1_SS2 | MUX_PAD_CTRL(NO_PAD_CTRL),//
				MX6_PAD_LCD_DATA07__ECSPI1_SS3 | MUX_PAD_CTRL(NO_PAD_CTRL),//
#else
				MX6_PAD_CSI_DATA05__GPIO4_IO26 | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_LCD_DATA05__GPIO3_IO10 | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_LCD_DATA06__GPIO3_IO11 | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_LCD_DATA07__GPIO3_IO12 | MUX_PAD_CTRL(NO_PAD_CTRL), //
#endif //
				MX6_PAD_CSI_DATA04__ECSPI1_SCLK | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_CSI_DATA06__ECSPI1_MOSI | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_CSI_DATA07__ECSPI1_MISO | MUX_PAD_CTRL(NO_PAD_CTRL) //
//
		};

static const iomux_v3_cfg_t i2c1_pads[] = { //
		MX6_PAD_CSI_MCLK__CSI_MCLK | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_CSI_PIXCLK__CSI_PIXCLK | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_CSI_MCLK__I2C1_SDA | MUX_PAD_CTRL(NO_PAD_CTRL), //
				MX6_PAD_CSI_PIXCLK__I2C1_SCL | MUX_PAD_CTRL(NO_PAD_CTRL) //
		};

static const iomux_v3_cfg_t i2c2_pads[] = { MX6_PAD_CSI_HSYNC__CSI_HSYNC
		| MUX_PAD_CTRL(NO_PAD_CTRL), MX6_PAD_CSI_VSYNC__CSI_VSYNC
		| MUX_PAD_CTRL(NO_PAD_CTRL), MX6_PAD_CSI_VSYNC__I2C2_SDA
		| MUX_PAD_CTRL(NO_PAD_CTRL), MX6_PAD_CSI_HSYNC__I2C2_SCL
		| MUX_PAD_CTRL(NO_PAD_CTRL) };


static const iomux_v3_cfg_t board_id_pads[] = {
		MX6_PAD_SD1_DATA0__GPIO2_IO18 | MUX_PAD_CTRL(NO_PAD_CTRL),
		MX6_PAD_SD1_DATA1__GPIO2_IO19 | MUX_PAD_CTRL(NO_PAD_CTRL),
		MX6_PAD_SD1_DATA2__GPIO2_IO20 | MUX_PAD_CTRL(NO_PAD_CTRL),
		MX6_PAD_SD1_DATA3__GPIO2_IO21 | MUX_PAD_CTRL(NO_PAD_CTRL) };

static void setup_iomux_siklu_cpld(void) {
	imx_iomux_v3_setup_multiple_pads(cpld_pads, ARRAY_SIZE(cpld_pads));
}


/* See also spl_get_siklu_board_id() in board/freescale/mx6ullevk/mx6ullevk.c. */
SKL_BOARD_TYPE_E siklu_get_board_type(void)
{
	SKL_BOARD_TYPE_E board_type;
	
	uint32_t reg_val;
	ulong reg_addr = 0x20A0000;
	u8 val;

	reg_val = readl((uint32_t*)reg_addr);
	//printf("reg_val - 0x%x\n",reg_val);

	val = ((reg_val & 1<<18)?(1):(0)) + ((reg_val & 1<<19)?(2):(0))
		+ ((reg_val & 1<<20)?(4):(0))
		+ ((reg_val & 1<<21)?(8):(0));

	//printf("board_id - 0x%x\n",val);

	switch (val)
	{
	case 0:
		board_type = SKL_BOARD_TYPE_PCB195;
		break;
	case 1:
		board_type = SKL_BOARD_TYPE_PCB213;
		break;
	case 2:
		board_type = SKL_BOARD_TYPE_PCB217;
		break;
	case 3:
		board_type = SKL_BOARD_TYPE_PCB277;
		break;
	case 4:
		board_type = SKL_BOARD_TYPE_PCB295;
		break;
	case 5:
		board_type = SKL_BOARD_TYPE_PCB295_AES;
		break;
	default:
		board_type = SKL_BOARD_TYPE_UNKNOWN;
		printf("Error: Unknown board type 0x%x\n", (int)val);
		break;
	}
	return board_type;
}

const char * siklu_get_board_hw_name(void)
{
    SKL_BOARD_TYPE_E board_type = siklu_get_board_type();

    switch (board_type)
    {
        case SKL_BOARD_TYPE_PCB195:
            return "PCB195"; break;
        case SKL_BOARD_TYPE_PCB213:
            return "PCB213"; break;
        case SKL_BOARD_TYPE_PCB217:
            return "PCB217"; break;
        case SKL_BOARD_TYPE_PCB277:
            return "PCB277"; break;
        case SKL_BOARD_TYPE_PCB295:
            return "PCB295"; break;
        case SKL_BOARD_TYPE_PCB295_AES:
            return "PCB295_AES"; break;
        default:
            return ""; break;
    }
}

static void setup_iomux_siklu_board_id(void) {
	imx_iomux_v3_setup_multiple_pads(board_id_pads, ARRAY_SIZE(board_id_pads));
}

static void setup_iomux_siklu_i2c(void) {
	imx_iomux_v3_setup_multiple_pads(i2c1_pads, ARRAY_SIZE(i2c1_pads));
	imx_iomux_v3_setup_multiple_pads(i2c2_pads, ARRAY_SIZE(i2c2_pads));
}

int board_spi_cs_gpio(unsigned bus, unsigned cs) {
	int rc = -1;

	switch (bus) {
	case 0:   // RF not functional in uboot
#ifdef  _GPIO_ECSPI1_SS_MANUAL_CONTROL // code below disabled. MXC_SPI doesn't control CS output, we use this manually
	{
		if (cs == 0)
		rc = IMX_GPIO_NR(4, 26);
		else if (cs == 1)
		rc = IMX_GPIO_NR(3, 10);
	}
#endif //
		break;
	case 1:  // CPLD
		break;
	case 2: // serial-NOR
	{
		if (cs == 0) {
			rc = IMX_GPIO_NR(1, 20);
		}
	}
		break;
	default:
		break;

	}

	return rc;
}

/*
 * init access only once
 */
static int siklu_rfic_module_access_init(void) {
	int rc = 0;
	static int is_init = 0;

	if (is_init)
		return rc;

	imx_iomux_v3_setup_multiple_pads(rfic_pads, ARRAY_SIZE(rfic_pads));

#ifdef 	GPIO_ECSPI1_SS_MANUAL_CONTROL
	gpio_direction_output(GPIO_ECSPI1_SS0, 1); // init all RFIC module CS (total 4) by HIGH
	gpio_direction_output(GPIO_ECSPI1_SS1, 1);
	gpio_direction_output(GPIO_ECSPI1_SS2, 1);
	gpio_direction_output(GPIO_ECSPI1_SS3, 1);
#endif //

	is_init = 1;
	return rc;
}

static int siklu_rfic_1byte_xfer(int spi_mode, u32 cs, unsigned long flags,
		u8 dout, u8* din) {
	int rc = 0, ret;
	u32 bus = CONFIG_RFIC_DEFAULT_BUS;
	u32 max_hz = CONFIG_RFIC_DEFAULT_SPEED;
	struct spi_slave *spi;
	u8 tx_buf[5];
	u8 rx_buf[5];

	spi = spi_setup_slave(bus, cs, max_hz, spi_mode);
	if (!spi) {
		printf("%s: Failed to set up slave\n", __func__);
		return rc;
	}

	ret = spi_claim_bus(spi);
	if (ret) {
		printf("%s: Failed to claim SPI bus: %d\n", __func__, ret);
		goto err_claim_bus;
	}

	memset(tx_buf, 0, sizeof(tx_buf));
	memset(rx_buf, 0, sizeof(rx_buf));
	tx_buf[0] = dout;

	ret = spi_xfer(spi, 1 * 8, tx_buf, rx_buf, flags);
	if (ret < 0) {
		printf("%s: Failed XFER SPI: ret - %d\n", __func__, ret);
		goto err_claim_bus;
	}

	*din = rx_buf[0];
	spi_free_slave(spi);

	return rc;
	err_claim_bus: //
	spi_free_slave(spi);
	return -1;

}

/*
 *
 */
int siklu_rfic_module_read(MODULE_RFIC_E module, u8 reg, u8* data) {
	int rc = 0;
	u32 cs;
	u32 spi_mode;
	u8 din;
	u32 gpio_cs;

	siklu_rfic_module_access_init();
	if (module == MODULE_RFIC_70) {
		cs = CONFIG_RFIC70_DEFAULT_CS;
		gpio_cs = GPIO_ECSPI1_SS0;
	} else {
		cs = CONFIG_RFIC80_DEFAULT_CS;
		gpio_cs = GPIO_ECSPI1_SS1;
	}

	gpio_direction_output(gpio_cs, 0);

	spi_mode = 0; // write data to RFIC in mode#0
	rc = siklu_rfic_1byte_xfer(spi_mode, cs, SPI_XFER_BEGIN, reg | 0x80, &din);
	if (rc != 0) {
		printf("%s() error on line %d\n", __func__, __LINE__);
		return rc;
	}
	spi_mode = 1; // read data from RFIC in mode#1
	rc = siklu_rfic_1byte_xfer(spi_mode, cs, SPI_XFER_END, 0xFF, data);
	if (rc != 0) {
		printf("%s() error on line %d\n", __func__, __LINE__);
		return rc;
	}
	gpio_direction_output(gpio_cs, 1);
	return 0;

}

int siklu_rfic_module_write(MODULE_RFIC_E module, u8 reg, u8 data) {
	int ret = -1;
	u32 cs;
	u32 spi_mode = 0;
	u8 __attribute__((unused)) din;
	u32 gpio_cs;
	u32 bus = CONFIG_RFIC_DEFAULT_BUS;
	u8 tx_buf[10];
	const u32 max_hz = CONFIG_CPLD_DEFAULT_SPEED;
	struct spi_slave *spi;

	siklu_rfic_module_access_init();
	if (module == MODULE_RFIC_70) {
		cs = CONFIG_RFIC70_DEFAULT_CS;
		gpio_cs = GPIO_ECSPI1_SS0;
	} else {
		cs = CONFIG_RFIC80_DEFAULT_CS;
		gpio_cs = GPIO_ECSPI1_SS1;
	}

	gpio_direction_output(gpio_cs, 0);

	spi = spi_setup_slave(bus, cs, max_hz, spi_mode);
	if (!spi) {
		printf("%s: Failed to set up slave\n", __func__);
		return ret;
	}

	ret = spi_claim_bus(spi);
	if (ret) {
		printf("%s: Failed to claim SPI bus: %d\n", __func__, ret);
		goto err_claim_bus;
	}

	memset(tx_buf, 0, sizeof(tx_buf));

	tx_buf[0] = reg & 0x7F;
	tx_buf[1] = data;

	ret = spi_xfer(spi, 2 * 8, tx_buf, NULL, SPI_XFER_BEGIN | SPI_XFER_END);
	if (ret < 0) {
		printf("%s: Failed XFER SPI: ret - %d\n", __func__, ret);
		goto err_claim_bus;
	}

	// last before exit
	gpio_direction_output(gpio_cs, 1);
	spi_free_slave(spi);

	return 0;
	err_claim_bus: //
	gpio_direction_output(gpio_cs, 1);
	spi_free_slave(spi);
	return ret;

}

/*
 * returns result in data[3]
 */
static int _siklu_cpld_read(u8 reg, u8* data) {

	int rc = CMD_RET_FAILURE;

	const u32 bus = CONFIG_CPLD_DEFAULT_BUS;
	const u32 cs = CONFIG_CPLD_DEFAULT_CS;
	const u32 max_hz = CONFIG_CPLD_DEFAULT_SPEED;
	u32 spi_mode = CONFIG_CPLD_DEFAULT_MODE;
	struct spi_slave *spi;
	int ret;
	u8 tx_buf[10];
#define CPLD_READ_SEQ_LENGTH    4
#define SPI_READ_MEMORY_COMMAND 0x0B

	spi = spi_setup_slave(bus, cs, max_hz, spi_mode);
	if (!spi) {
		printf("%s: Failed to set up slave\n", __func__);
		return rc;
	}

	ret = spi_claim_bus(spi);
	if (ret) {
		printf("%s: Failed to claim SPI bus: %d\n", __func__, ret);
		goto err_claim_bus;
	}

	memset(tx_buf, 0, sizeof(tx_buf));

	tx_buf[0] = SPI_READ_MEMORY_COMMAND;
	tx_buf[1] = reg;

	ret = spi_xfer(spi, CPLD_READ_SEQ_LENGTH * 8, tx_buf, data,
	SPI_XFER_BEGIN | SPI_XFER_END);
	if (ret < 0) {
		printf("%s: Failed XFER SPI: ret - %d\n", __func__, ret);
		goto err_claim_bus;
	}

//	printf("\n RX buf: %2x %2x %2x (%2x)\n", data[0], data[1], data[2], data[3]);

	// last before exit
	spi_free_slave(spi);

	return CMD_RET_SUCCESS;
	err_claim_bus: //
	spi_free_slave(spi);
	return CMD_RET_FAILURE;
}

u8 siklu_cpld_read(u8 reg)
{
	u8 rx_buf[10];
	int rc = _siklu_cpld_read(reg, rx_buf);
	if (rc==0)
		return rx_buf[3];
	else
		return 0xFF;
}

int siklu_cpld_write(u8 reg, u8 data) {

	int rc = CMD_RET_FAILURE;

	const u32 bus = CONFIG_CPLD_DEFAULT_BUS;
	const u32 cs = CONFIG_CPLD_DEFAULT_CS;
	const u32 max_hz = CONFIG_CPLD_DEFAULT_SPEED;
	u32 spi_mode = CONFIG_CPLD_DEFAULT_MODE;
	struct spi_slave *spi;
	int ret;
	u8 tx_buf[10];
#define CPLD_WRITE_SEQ_LENGTH    3
#define SPI_WRITE_MEMORY_COMMAND 0x02

	spi = spi_setup_slave(bus, cs, max_hz, spi_mode);
	if (!spi) {
		printf("%s: Failed to set up slave\n", __func__);
		return rc;
	}

	ret = spi_claim_bus(spi);
	if (ret) {
		printf("%s: Failed to claim SPI bus: %d\n", __func__, ret);
		goto err_claim_bus;
	}

	memset(tx_buf, 0, sizeof(tx_buf));

	tx_buf[0] = SPI_WRITE_MEMORY_COMMAND;
	tx_buf[1] = reg;
	tx_buf[2] = data;

	ret = spi_xfer(spi, CPLD_WRITE_SEQ_LENGTH * 8, tx_buf, NULL,
	SPI_XFER_BEGIN | SPI_XFER_END);
	if (ret < 0) {
		printf("%s: Failed XFER SPI: ret - %d\n", __func__, ret);
		goto err_claim_bus;
	}

	// last before exit
	spi_free_slave(spi);

	return CMD_RET_SUCCESS;
	err_claim_bus: spi_free_slave(spi);
	return CMD_RET_FAILURE;
}








/*
 *
 *	In case of error also return 0!
 */
int siklu_board_late_init_hw(void) {
	int rc = 0;
	char val[KEY_VAL_FIELD_SIZE];

	// TODO Via MDIO bus:
	// 		Enable SOHO Port#5 - output to network, configure 1G autoneg
	//		Enable SOHO Port#0 - connection to NXP SoC, configure strict 100FD
	siklu_mdio_bus_connect(SIKLU_MDIO_BUS0); // by default connect SOHO Switch

	// init SOHO switch
	rc = siklu_soho_power_up_init();
	if (rc != 0) {
		printf(" Init SOHO Fail\n");
		goto _err_hndlr;
	}

	// extract board MAC address and init environment variable
	rc = siklu_syseeprom_init();
	if (rc != 0) {
		printf(" Init SYSEEPROM Fail\n");
		goto _err_hndlr;
	}
	memset(val, 0, sizeof(val));
	rc = siklu_syseeprom_get_val("SE_mac", val);
	if (rc != 0) {
		printf(" Read MAC from SYSEEPROM Fail\n");
		goto _err_hndlr;
	}
	env_set("ethaddr", val);

	siklu_set_led_modem(SKL_LED_MODE_OFF);

	return 0;

_err_hndlr:
	env_set("ethaddr", ""); // erase environment
	return 0;

}

int siklu_board_init(void) {
	int rc = 0;
	setup_iomux_siklu_cpld();
	setup_iomux_siklu_i2c();
	siklu_rfic_module_access_init();
	setup_iomux_siklu_board_id();

	// TODO In CPLD:
	// 	put to reset all unnecessary HW devices
	// 	release SOHO reset

	return rc;
}

/*
 * do not confused with function board_late_init()!!!!
 * The function called after network init
 */
int last_stage_init(void)
{
	int rc = 0;

	return rc;
}


static int get_cpld_hw_ver(int * cpld_hw_ver)
{
	int rc = CMD_RET_FAILURE;
	u8 rx_buf[10];
	memset(rx_buf, 0, sizeof(rx_buf));

	rc = _siklu_cpld_read(R_CPLD_LOGIC_HW_ASM_VER, rx_buf);
	if (rc != CMD_RET_SUCCESS)
	{
		printf("\ncpld read failed\n");
		return rc;
	}

	*cpld_hw_ver = rx_buf[3];

	return rc;
}


int siklu_board_hw_reboot(void)
{
    int rc = CMD_RET_SUCCESS;
    while (1) {
        siklu_cpld_write(R_CPLD_LOGIC_MODEM_LEDS_CTRL, 0);
        siklu_cpld_write(R_CPLD_LOGIC_RESET_CONTROL, 0);
    }
    return rc;
}


/*
 * unconditional HW reset via CPLD
 * This is only one command for restart a board. Regular "reset" command disabled!
 */
static int do_siklu_board_hw_reboot(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    puts ("resetting ...\n");
    udelay (50000);				/* wait 50 ms */
    return siklu_board_hw_reboot();
}



static int do_siklu_board_diplay_hw_info(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    int rc = CMD_RET_SUCCESS;
    int cpld_hw_ver = 0;
    u32 val;
    int boardId0, boardId1, boardId2, boardId3;
    u8 device_grade, device_revision;
    u16 part_number;
    u32 tool_version;

    printf("Board HW name       : %s\n", siklu_get_board_hw_name());
//printf("CPU type            : 0x%02x  (%s)\n", siklu_get_cpu_type(), buffer);
//printf("Core clock          : %lld MHz\n", DIV_ROUND_UP(cvmx_clock_get_rate(CVMX_CLOCK_CORE), 1000000));
//printf("IO clock            : %lld MHz\n", divide_nint(cvmx_clock_get_rate(CVMX_CLOCK_SCLK), 1000000));
//printf("DDR clock           : %lu MHz\n", gd->mem_clk);
//printf("Board ID            : 0x%02x\n", siklu_get_board_hw_major()); // read from 4bits CPU GPIO

	rc = get_siklu_cpld_version(CONFIG_CPLD_DEFAULT_MODE, &val);
    printf("CPLD version        : 0x%02x\n", val);
//printf("CPLD board version  : 0x%02x\n", siklu_get_cpld_board_ver());
//printf("Assembly version    : 0x%02x\n", siklu_get_assembly());
//printf("Num ETH ports       : %d\n", siklu_get_product_num_eth_ports());
//printf("SF                  : %s\n", flash->name);

#define GPIO2_IO18	IMX_GPIO_NR(2, 18)
#define GPIO2_IO19	IMX_GPIO_NR(2, 19)
#define GPIO2_IO20	IMX_GPIO_NR(2, 20)
#define GPIO2_IO21	IMX_GPIO_NR(2, 21)

    gpio_direction_input(GPIO2_IO18);
    gpio_direction_input(GPIO2_IO19);
    gpio_direction_input(GPIO2_IO20);
    gpio_direction_input(GPIO2_IO21);

    boardId0 = gpio_get_value(GPIO2_IO18);
    boardId1 = gpio_get_value(GPIO2_IO19);
    boardId2 = gpio_get_value(GPIO2_IO20);
    boardId3 = gpio_get_value(GPIO2_IO21);

    printf("Board ID            : %x\n", (boardId3 << 3) | (boardId2 << 2) | (boardId1 << 1) | (boardId0 << 0));

    get_cpld_hw_ver(&cpld_hw_ver);
    printf("HW Version          : %x\n", cpld_hw_ver);

    SKL_BOARD_TYPE_E board_type = siklu_get_board_type();
    if ( (board_type == SKL_BOARD_TYPE_PCB195) || (board_type == SKL_BOARD_TYPE_PCB213) || (board_type == SKL_BOARD_TYPE_PCB217) )
    {
        get_88x3310_phy_version(&val);
        printf("88x3310 Phy Version : 0x%04x\n", val);
    }

    get_TLK10031_version(&val);
    printf("TLK10031 Version    : 0x%04x\n", val);

	get_pll_part_number(&part_number);
    get_pll_device_grade(&device_grade);
	get_pll_device_revision(&device_revision);
	get_pll_tool_version(&tool_version);

	printf("PLL Version         : %04x.%02x.%02x.%06x\n", part_number, device_grade, device_revision, tool_version);
    return rc;
}


typedef enum
{
    BIST_MODE_DISABLED = 0,
    BIST_MODE_ON = 1,
    BIST_MODE_LAST = BIST_MODE_ON,
} BIST_MODE_E;

/*
 *
 */
static int do_siklu_board_bist_mode(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    int rc = CMD_RET_FAILURE; // return false for prevent command be repeatable

    if (argc == 1) // show current mode
    {
        char *bist_state;
        bist_state = env_get(SIKLU_BIST_ENVIRONMENT_NAME);

        if (bist_state == NULL)
        {
            printf("No BIST mode\n");
        }
        else
        {
            BIST_MODE_E state = (BIST_MODE_E) simple_strtol(bist_state, NULL, 10);
            switch (state)
            {
            case BIST_MODE_DISABLED:
                printf("No BIST mode\n");
                break;
            case BIST_MODE_ON:
                printf("System in BIST mode\n");
                break;
            default:
                printf("Wrong BIST mode! Disable BIST for future runs\n");
                env_set(SIKLU_BIST_ENVIRONMENT_NAME, NULL);
                env_save();
                break;
            }
        }
    }

    else if (argc == 2)
    {
        // set new BIST mode
        BIST_MODE_E bist_mode = simple_strtoul(argv[1], NULL, 10);

        switch (bist_mode)
        {
        case BIST_MODE_DISABLED:
            printf("Disable BIST mode\n");
            env_set(SIKLU_BIST_ENVIRONMENT_NAME, NULL);
            break;
        case BIST_MODE_ON:
            printf("Set System in BIST mode\n");
            env_set(SIKLU_BIST_ENVIRONMENT_NAME, "1");
            break;
        default:
            printf("Wrong BIST mode! Disable BIST for future runs\n");
            env_set(SIKLU_BIST_ENVIRONMENT_NAME, NULL);
            break;
        }
        env_save();
    }
    else
    {
        // wrong arguments
        printf("Wrong arguments\n");
        printf("Usage:\n%s\n", cmdtp->usage);
        return CMD_RET_FAILURE;
    }

    return rc;
}

U_BOOT_CMD(shw, 5, 1, do_siklu_board_diplay_hw_info, "Display Board HW info", " Display Board HW info");
U_BOOT_CMD(sbist, 5, 1, do_siklu_board_bist_mode, "Set board to BIST Mode", "0-off,1-bist,2-bist with monitoring");

U_BOOT_CMD(reboot, 5, 1, do_siklu_board_hw_reboot, "Board HW Reboot", " Board HW Reboot");

#endif
