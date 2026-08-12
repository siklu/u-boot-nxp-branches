/*
 * mx6ullevk_siklu_si5344d_pll.c
 *
 *  Created on: Feb 27, 2018
 *      Author: noama
 */

#ifndef CONFIG_SPL_BUILD

#include <common.h>
#include <command.h>
#include <version.h>
#include <libfdt.h>
#include <fdt_support.h>
#include <asm/io.h>
#include <linux/errno.h>
#include <linux/compiler.h>
#include <console.h>
#include <spi.h>
#include <spi_flash.h>
#include <i2c.h>
#include <linux/delay.h>

#include "siklu_def.h"
#include "siklu_api.h"

#include "Si5344D-Dxxx-GM-V1-Registers.h"
#include "Si5344D-Dxxx-GM-V2-Registers.h"


#define ACTIVE_NVM_BANK		0x00E2
#define NVM_WRITE			0x00E3
#define NVM_READ_BANK		0x00E4
#define DEVICE_READY		0x00FE

#define PLL_PAGE_REG_ADDR	0x0001


u8 current_pll_addr = -1;
static int current_page = -1;


#if 0
/*
Any attempt to read or write any register other than DEVICE_READY before DEVICE_READY reads as 0x0F may corrupt
the NVM programming and may corrupt the register contents, as they are read from NVM
*/
static int wait_for_device_ready(void)
{
	int rc = CMD_RET_SUCCESS;
	int old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	u8 val;

	do {
		val = (i2c_reg_read(DEVICE_READY, DEVICE_READY));
	} while (val != 0x0F);

	printf(" reg 0x%04x, val 0x%02x\n", DEVICE_READY, val);

	i2c_set_bus_num(old_bus);

	return rc;
}
#endif

static void set_pll_page_reg(u8 new_page)
{
	int old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	i2c_reg_write(current_pll_addr, PLL_PAGE_REG_ADDR, new_page);
	current_page = new_page;
	i2c_set_bus_num(old_bus);
}

static int si5344d_pll_reg_read(u8 page, u8 reg, u8 *val)
{
	int rc = CMD_RET_SUCCESS;

	if (current_page != page)
		set_pll_page_reg(page);

	int old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	*val = (i2c_reg_read(current_pll_addr, reg));
	i2c_set_bus_num(old_bus);

//	printf("reg 0x%04x, val 0x%02x\n", reg, *val);

	return rc;
}

static void si5344d_pll_reg_write(u8 page, u8 reg, u8 val)
{
	if (current_page != page)
		set_pll_page_reg(page);

	int old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	i2c_reg_write(current_pll_addr, reg, val);
	i2c_set_bus_num(old_bus);
}

static int do_siklu_si5344d_pll_reg_read(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]) //
{
    int rc = CMD_RET_SUCCESS;
    u8 reg = 0;
    u8 page = 0;
	u8 val;

	if (argc < 3)
	{
		printf("Error: Not enough arguments\n");
		rc = CMD_RET_FAILURE;
		return rc;
	}

	page= simple_strtoul(argv[1], NULL, 16);
	reg = simple_strtoul(argv[2], NULL, 16);

	si5344d_pll_reg_read(page, reg, &val);

	printf("page:0x%02x, reg:0x%04x, val:0x%02x\n", page, reg, val);

	return rc;
}

static int do_siklu_si5344d_pll_reg_write(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    int rc = CMD_RET_SUCCESS;
    u8 reg = 0;
    u8 page = 0;
	u8 val;

	if (argc < 4)
	{
		printf("Error: Not enough arguments\n");
	}

	page = simple_strtoul(argv[1], NULL, 8);
	reg  = simple_strtoul(argv[2], NULL, 8);
	val  = simple_strtoul(argv[3], NULL, 8);

	si5344d_pll_reg_write(page, reg, val);

	return rc;
}

int get_pll_part_number(u16 *part_number)
{
	int rc = CMD_RET_FAILURE;
	u8 val0, val1;
#define PLL_PART_NUMBER_REG_ADDR_0 0x2
#define PLL_PART_NUMBER_REG_ADDR_1 0x3
	si5344d_pll_reg_read(0, PLL_PART_NUMBER_REG_ADDR_0, &val0);
	si5344d_pll_reg_read(0, PLL_PART_NUMBER_REG_ADDR_1, &val1);
	*part_number = val1 | (val0 << 8);
	return rc;
}

int get_pll_device_grade(u8 *device_grade)
{
	int rc = CMD_RET_FAILURE;
#define PLL_DEVICE_GRADE_REG_ADDR 0x4
	si5344d_pll_reg_read(0, PLL_DEVICE_GRADE_REG_ADDR, device_grade);
	return rc;

}

int get_pll_device_revision(u8 *device_revision)
{
	int rc = CMD_RET_FAILURE;
#define PLL_DEVICE_REVISION_REG_ADDR 0x5
	si5344d_pll_reg_read(0, PLL_DEVICE_REVISION_REG_ADDR, device_revision);
	return rc;

}

int get_pll_tool_version(u32 *tool_version)
{
	int rc = CMD_RET_FAILURE;
	u8 val_special_and_revision, val_minor, val_minor_and_major;
#define PLL_TOOL_VERSION_SPECIAL_AND_REVISION_REG_ADDR 	0x6
#define PLL_TOOL_VERSION_MINOR_REG_ADDR 				0x7
#define PLL_TOOL_VERSION_MINOR_AND_MAJOR_REG_ADDR 		0x8
	si5344d_pll_reg_read(0, PLL_TOOL_VERSION_SPECIAL_AND_REVISION_REG_ADDR, &val_special_and_revision);
	si5344d_pll_reg_read(0, PLL_TOOL_VERSION_MINOR_REG_ADDR, &val_minor);
	si5344d_pll_reg_read(0, PLL_TOOL_VERSION_MINOR_AND_MAJOR_REG_ADDR, &val_minor_and_major);
	*tool_version = (val_special_and_revision << 16) || (val_minor << 8) || val_minor_and_major;
	return rc;

}

int siklu_si5344d_pll_reg_burn()
{
    int i, rc = CMD_RET_SUCCESS;
    u8 reg = 0;
    u8 page = 0;
	u8 val;

//	printf("Configure PLL. Device addr: 0x%02x\n", current_pll_addr);

	const si5344_revd_register_t * si5344_revd_registers;
	SKL_BOARD_TYPE_E board_type = siklu_get_board_type();
	int si5344_revd_register_config_num;

	switch (board_type) {
		case SKL_BOARD_TYPE_PCB195:
			si5344_revd_registers = si5344_v1_revd_registers;
			si5344_revd_register_config_num = SI5344_V1_REVD_REG_CONFIG_NUM_REGS;
			break;
		case SKL_BOARD_TYPE_PCB213:
		case SKL_BOARD_TYPE_PCB217:
		case SKL_BOARD_TYPE_PCB277:
		case SKL_BOARD_TYPE_PCB295:
		case SKL_BOARD_TYPE_PCB295_AES:
			si5344_revd_registers = si5344_v2_revd_registers;
			si5344_revd_register_config_num = SI5344_V2_REVD_REG_CONFIG_NUM_REGS;
			break;
		default:
			printf("Error: Unknown board type 0x%x. Burn latest registers set\n", board_type);
			si5344_revd_registers = si5344_v2_revd_registers;
			si5344_revd_register_config_num = SI5344_V2_REVD_REG_CONFIG_NUM_REGS;
			return CMD_RET_FAILURE;
	}

	int old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);

	for (i=0 ; i<si5344_revd_register_config_num ; i++)
	{
		val  = si5344_revd_registers[i].value;
		page = si5344_revd_registers[i].address >> 8;
		reg  = si5344_revd_registers[i].address & 0xFF;

		if (current_page != page)
			set_pll_page_reg(page);

		i2c_reg_write(current_pll_addr, reg, val);

		if (i==2)
		{
			udelay(300000); //Wait 300 ms
		}

		if (reg == 0xB && page == 0) // I2C Address
		{
			rc = i2c_probe(CONFIG_SYS_I2C_BURNED_PLL_ADDR);

			if (rc == 0)
			{
				current_pll_addr = CONFIG_SYS_I2C_BURNED_PLL_ADDR;
			}
			else
			{
				printf("Error: Expected PLL device addr 0x%02x was not found\n", CONFIG_SYS_I2C_BURNED_PLL_ADDR);
			}
		}
	}

//	printf("PLL is ready. Device addr: 0x%02x\n", current_pll_addr);

	i2c_set_bus_num(old_bus);

	return rc;
}

static int do_siklu_si5344d_pll_reg_burn(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    int rc = CMD_RET_SUCCESS;

	printf("Burn new PLL version? [y/N]");

	if ( ! confirm_yesno() )
		return rc;

	rc = siklu_si5344d_pll_reg_burn();

	return rc;
}


void siklu_si5344d_get_pll_device_addr()
{
	int ret;
	int old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	ret = i2c_probe(CONFIG_SYS_I2C_UNBURNED_PLL_ADDR);

	if (ret == 0) {
		current_pll_addr = CONFIG_SYS_I2C_UNBURNED_PLL_ADDR;
		i2c_set_bus_num(old_bus);
		return;
	}

	ret = i2c_probe(CONFIG_SYS_I2C_BURNED_PLL_ADDR);
	if (ret == 0) {
		current_pll_addr = CONFIG_SYS_I2C_BURNED_PLL_ADDR;
		i2c_set_bus_num(old_bus);
		return;
	}

	printf("Error: I2c PLL Addr is not recognized !!\n");
}


static int do_siklu_si5344d_pll_get_version(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
    int rc = CMD_RET_SUCCESS;
    u8 device_grade, device_revision;
    u16 part_number;
    u32 tool_version;

	get_pll_part_number(&part_number);
    get_pll_device_grade(&device_grade);
	get_pll_device_revision(&device_revision);
	get_pll_tool_version(&tool_version);

	printf("PLL version: %04x.%02x.%02x.%06x\n", part_number, device_grade, device_revision, tool_version);

	return rc;
}


U_BOOT_CMD(pll_rr, 4, 1, do_siklu_si5344d_pll_reg_read, "Si5344D PLL Read Register",
        "[page] [reg]" " - Si5344D PLL Read Register\n");

U_BOOT_CMD(pll_wr, 4, 1, do_siklu_si5344d_pll_reg_write, "Si5344D PLL Write Register",
        "[page] [reg] [val]" " - Si5344D PLL Write Register\n");

U_BOOT_CMD(pll_burn, 1, 1, do_siklu_si5344d_pll_reg_burn, "Si5344D PLL Burn All Registers",
        " \n");

U_BOOT_CMD(pll_ver, 1, 1, do_siklu_si5344d_pll_get_version, "Si5344D PLL Get Version",
        "[part-number].[device-grade].[device-revision].[tool-version]\n");

#endif
