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
#include <linux/bug.h>
#include <environment.h>
#include <asm/mach-imx/mxc_i2c.h>

#include "siklu_def.h"
#include "siklu_api.h"

#include "Si5344D-Dxxx-GM-V1-Registers.h"
#include "Si5344D-Dxxx-GM-V2-Registers.h"


#define PLL_PAGE_REG_ADDR	0x0001

/* Page 0 register holding the device's own i2c address. */
#define PLL_I2C_ADDR_REG	0x0B

/*
 * The generated tables open with a three-entry preamble ending in a write to
 * 0x0540, and the 300 ms pause the vendor script calls for belongs right after
 * it. The pause below and the check in siklu_si5344d_pll_reg_burn() are keyed
 * on these two constants together, so a table regenerated with a preamble of a
 * different length is reported instead of quietly moving the pause somewhere
 * it does nothing.
 */
#define PLL_PREAMBLE_LAST_INDEX		2
#define PLL_PREAMBLE_LAST_REG_ADDR	0x0540

/*
 * Page 0 register 0x1E, bit 1: HARD_RST. The Si5345/44/42 Rev D family
 * reference manual describes it as performing the same function as power
 * cycling the device, restoring every register to its default value.
 *
 * Bit 0 of the same register is PDN, which powers the device down. The value
 * written here is therefore exactly 0x02 and never 0x03: setting PDN on a
 * device that is already failing to answer would leave nothing to recover.
 */
#define PLL_HARD_RST_REG		0x1E
#define PLL_HARD_RST_VALUE		0x02

/*
 * DEVICE_READY, readable from any page, reads 0x0F once the device has
 * finished loading its registers from NVM. The reference manual is explicit
 * that no other register may be read or written until then - the page register
 * 0x01 included - because an access during the load can corrupt the NVM.
 */
#define PLL_DEVICE_READY_REG		0xFE
#define PLL_DEVICE_READY_VALUE		0x0F
#define PLL_DEVICE_READY_TIMEOUT_MS	1000
#define PLL_DEVICE_READY_POLL_US	10000

/*
 * Time for the device to act on a blind HARD_RST before it is asked anything.
 * The reset starts a load from NVM, so the wait matters here; how long it
 * really takes is settled by the DEVICE_READY poll and its timeout, not by
 * this figure.
 */
#define PLL_HARD_RST_SETTLE_US		50000

/*
 * Clocking the bus idle resets nothing, so the device needs only enough time
 * to see an idle bus before it is probed. No NVM load follows it and no
 * DEVICE_READY poll belongs after it.
 */
#define PLL_BUS_IDLE_SETTLE_US		1000

/*
 * Board resets attempted for one unconfigured PLL, counted across boots in the
 * environment. The counter is cleared by a successful burn, so a unit that
 * heals never carries its history forward, and the ladder stops rather than
 * looping when the resets do not help.
 */
#define PLL_RECOVERY_ATTEMPTS_ENV	"pll_recovery_attempts"
#define PLL_RECOVERY_MAX_ATTEMPTS	3


u8 current_pll_addr = -1;
static int current_page = -1;

/*
 * The device has exactly two addresses, the factory one and the one the burn
 * moves it to. current_pll_addr starts as 0xFF and stays there when neither
 * answered, so this is the question "do we know where the device is".
 */
static int pll_addr_is_known(void)
{
	return current_pll_addr == CONFIG_SYS_I2C_UNBURNED_PLL_ADDR ||
		   current_pll_addr == CONFIG_SYS_I2C_BURNED_PLL_ADDR;
}


/*
 * The register table rewrites the device's own I2C address at entry 6 of 462
 * ({ 0x000B, 0x58 }), which moves it from the unburned address to the burned one.
 * i2c-0 is shared with the RTC, the temperature sensor and an SFP cage and it
 * rejects transfers transiently, so a single lost probe here used to send the
 * remaining 455 writes to an address nothing answers on.
 */
#define PLL_ADDR_PROBE_RETRIES		5
#define PLL_ADDR_PROBE_DELAY_US		20000

static int pll_probe_addr(u8 addr)
{
	int attempt, rc = -1;

	for (attempt = 0 ; attempt < PLL_ADDR_PROBE_RETRIES && rc != 0 ; attempt++)
	{
		if (attempt)
			udelay(PLL_ADDR_PROBE_DELAY_US);

		rc = i2c_probe(addr);
	}

	return rc;
}

/*
 * i2c_reg_write() returns void and i2c_reg_read() returns the data byte, so
 * neither carries the transfer's status and every write on the burn path used
 * to be treated as if it had succeeded. i2c_write()/i2c_read() do carry it,
 * which is the whole reason these two wrappers exist. The retry answers the
 * same transient refusal on the same shared bus that pll_probe_addr() above
 * answers.
 *
 * 15 x 20 ms covers the 300 ms the register headers call the worst case for the
 * device to finish a calibration, which is the longest window in which it can
 * be expected to refuse a transfer: the postamble writes 0x0540 and 0x0B24
 * right after the soft reset at 0x001C. The budget is only ever spent on a
 * refused transfer, so an accepted write costs nothing.
 */
#define PLL_XFER_RETRIES		15
#define PLL_XFER_RETRY_DELAY_US	20000


static int pll_write_reg(u8 addr, u8 reg, u8 val)
{
	int attempt, rc = -1;

	for (attempt = 0 ; attempt < PLL_XFER_RETRIES && rc != 0 ; attempt++)
	{
		if (attempt)
			udelay(PLL_XFER_RETRY_DELAY_US);

		rc = i2c_write(addr, reg, 1, &val, 1);
	}

	return rc;
}

static int pll_read_reg(u8 addr, u8 reg, u8 *val)
{
	int attempt, rc = -1;

	for (attempt = 0 ; attempt < PLL_XFER_RETRIES && rc != 0 ; attempt++)
	{
		if (attempt)
			udelay(PLL_XFER_RETRY_DELAY_US);

		rc = i2c_read(addr, reg, 1, val, 1);
	}

	return rc;
}

static int set_pll_page_reg(u8 new_page)
{
	int rc;
	int old_bus = i2c_get_bus_num();

	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	rc = pll_write_reg(current_pll_addr, PLL_PAGE_REG_ADDR, new_page);
	i2c_set_bus_num(old_bus);

	if (rc == 0)
	{
		current_page = new_page;
	}
	else
	{
		/*
		 * The page the device holds is now unknown, and carrying the old
		 * cached value forward would send every later register of the table
		 * to whatever page it kept. Force the next access to select again.
		 */
		current_page = -1;
		printf("Error: PLL page 0x%02x select failed at addr 0x%02x, rc %d\n",
				new_page, current_pll_addr, rc);
	}

	return rc;
}

static int si5344d_pll_reg_read(u8 page, u8 reg, u8 *val)
{
	int rc;
	int old_bus;

	if (current_page != page && set_pll_page_reg(page) != 0)
		return CMD_RET_FAILURE;

	old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	rc = pll_read_reg(current_pll_addr, reg, val);
	i2c_set_bus_num(old_bus);

	if (rc != 0)
	{
		printf("Error: PLL read failed, page 0x%02x, reg 0x%02x, addr 0x%02x, rc %d\n",
				page, reg, current_pll_addr, rc);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

static int si5344d_pll_reg_write(u8 page, u8 reg, u8 val)
{
	int rc;
	int old_bus;

	if (current_page != page && set_pll_page_reg(page) != 0)
		return CMD_RET_FAILURE;

	old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	rc = pll_write_reg(current_pll_addr, reg, val);
	i2c_set_bus_num(old_bus);

	if (rc != 0)
	{
		printf("Error: PLL write failed, page 0x%02x, reg 0x%02x, val 0x%02x, addr 0x%02x, rc %d\n",
				page, reg, val, current_pll_addr, rc);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

/*
 * A write to page 0 register PLL_I2C_ADDR_REG moves the device to another i2c
 * address, which makes current_pll_addr stale the moment the write lands - and
 * moving it by hand is a working way to drive the chip from the console. Ask
 * both addresses and adopt the one that answers, so the commands that follow
 * reach the device instead of talking to nobody.
 *
 * The write's own return code is not consulted: a device that has just changed
 * address can refuse the transfer that changed it, so the probe is the better
 * witness. The burn path deliberately keeps its own handling, because there the
 * goal is to force the device onto the burned address rather than to follow it
 * wherever it went.
 */
static void pll_recheck_device_addr(u8 written_val)
{
	int old_bus = i2c_get_bus_num();
	u8 old_addr = current_pll_addr;

	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);

	if (pll_probe_addr(CONFIG_SYS_I2C_BURNED_PLL_ADDR) == 0)
		current_pll_addr = CONFIG_SYS_I2C_BURNED_PLL_ADDR;
	else if (pll_probe_addr(CONFIG_SYS_I2C_UNBURNED_PLL_ADDR) == 0)
		current_pll_addr = CONFIG_SYS_I2C_UNBURNED_PLL_ADDR;
	else
	{
		printf("Error: after writing I2C_ADDR=0x%02x the PLL answers on neither 0x%02x nor 0x%02x on i2c-%d.\n",
				written_val, CONFIG_SYS_I2C_BURNED_PLL_ADDR, CONFIG_SYS_I2C_UNBURNED_PLL_ADDR,
				CONFIG_SYS_PLL_BUS_NUM);
		printf("       It sits at an address this code does not track; a power cycle reloads I2C_ADDR from NVM.\n");
	}

	if (current_pll_addr != old_addr)
		printf("PLL: device address is now 0x%02x, was 0x%02x\n", current_pll_addr, old_addr);
	else
		printf("PLL: device address is still 0x%02x\n", current_pll_addr);

	current_page = -1;

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

	rc = si5344d_pll_reg_read(page, reg, &val);

	if (rc == CMD_RET_SUCCESS)
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
		rc = CMD_RET_FAILURE;
		return rc;
	}

	page = simple_strtoul(argv[1], NULL, 16);
	reg  = simple_strtoul(argv[2], NULL, 16);
	val  = simple_strtoul(argv[3], NULL, 16);

	rc = si5344d_pll_reg_write(page, reg, val);

	if (page == 0 && reg == PLL_I2C_ADDR_REG)
		pll_recheck_device_addr(val);

	return rc;
}

int get_pll_part_number(u16 *part_number)
{
	int rc;
	u8 val0, val1;
#define PLL_PART_NUMBER_REG_ADDR_0 0x2
#define PLL_PART_NUMBER_REG_ADDR_1 0x3
	rc = si5344d_pll_reg_read(0, PLL_PART_NUMBER_REG_ADDR_0, &val0);
	if (rc != CMD_RET_SUCCESS)
		return rc;

	rc = si5344d_pll_reg_read(0, PLL_PART_NUMBER_REG_ADDR_1, &val1);
	if (rc != CMD_RET_SUCCESS)
		return rc;

	*part_number = val1 | (val0 << 8);
	return CMD_RET_SUCCESS;
}

int get_pll_device_grade(u8 *device_grade)
{
#define PLL_DEVICE_GRADE_REG_ADDR 0x4
	return si5344d_pll_reg_read(0, PLL_DEVICE_GRADE_REG_ADDR, device_grade);
}

int get_pll_device_revision(u8 *device_revision)
{
#define PLL_DEVICE_REVISION_REG_ADDR 0x5
	return si5344d_pll_reg_read(0, PLL_DEVICE_REVISION_REG_ADDR, device_revision);
}

int get_pll_tool_version(u32 *tool_version)
{
	int rc;
	u8 val_special_and_revision, val_minor, val_minor_and_major;
#define PLL_TOOL_VERSION_SPECIAL_AND_REVISION_REG_ADDR 	0x6
#define PLL_TOOL_VERSION_MINOR_REG_ADDR 				0x7
#define PLL_TOOL_VERSION_MINOR_AND_MAJOR_REG_ADDR 		0x8
	rc = si5344d_pll_reg_read(0, PLL_TOOL_VERSION_SPECIAL_AND_REVISION_REG_ADDR, &val_special_and_revision);
	if (rc != CMD_RET_SUCCESS)
		return rc;

	rc = si5344d_pll_reg_read(0, PLL_TOOL_VERSION_MINOR_REG_ADDR, &val_minor);
	if (rc != CMD_RET_SUCCESS)
		return rc;

	rc = si5344d_pll_reg_read(0, PLL_TOOL_VERSION_MINOR_AND_MAJOR_REG_ADDR, &val_minor_and_major);
	if (rc != CMD_RET_SUCCESS)
		return rc;

	*tool_version = (val_special_and_revision << 16) | (val_minor << 8) | val_minor_and_major;
	return CMD_RET_SUCCESS;
}

/*
 * Registers 0x0C..0x13 of page 0 are the device's status and flag block. This
 * tree carries no bit-level description of them - the design reports in the
 * Si5344D-Dxxx-GM-V*-Registers.h headers list only the registers the
 * configuration writes - so the values are printed raw and named by address.
 * Decoding them needs the Si5344 Rev D datasheet.
 */
#define PLL_STATUS_REG_FIRST	0x0C
#define PLL_STATUS_REG_LAST		0x13

/*
 * Reports what the device says about itself, so that a burn is confirmed by
 * the chip rather than by the count of writes this code issued.
 */
static void print_pll_device_state(void)
{
	u16 part_number = 0;
	u8 device_grade = 0;
	u8 device_revision = 0;
	u32 tool_version = 0;
	u8 reg, val;

	if (get_pll_part_number(&part_number) == CMD_RET_SUCCESS &&
		get_pll_device_grade(&device_grade) == CMD_RET_SUCCESS &&
		get_pll_device_revision(&device_revision) == CMD_RET_SUCCESS &&
		get_pll_tool_version(&tool_version) == CMD_RET_SUCCESS)
	{
		printf("PLL: part %04x, grade %02x, rev %02x, tool version %06x\n",
				part_number, device_grade, device_revision, tool_version);
	}
	else
	{
		printf("Error: PLL identification registers unreadable at addr 0x%02x\n", current_pll_addr);
	}

	printf("PLL: page 0 raw status (needs the Si5344 datasheet to decode):");

	for (reg = PLL_STATUS_REG_FIRST ; reg <= PLL_STATUS_REG_LAST ; reg++)
	{
		if (si5344d_pll_reg_read(0, reg, &val) == CMD_RET_SUCCESS)
			printf(" 0x%02x=0x%02x", reg, val);
		else
			printf(" 0x%02x=??", reg);
	}

	printf("\n");
}

int siklu_si5344d_pll_reg_burn(void)
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
			/*
			 * Which of the two register sets a board wants is a property of
			 * its hardware, so an unknown board type leaves nothing safe to
			 * burn and the PLL keeps whatever its NVM loaded.
			 */
			printf("Error: Unknown board type 0x%x, skipping PLL burn\n", board_type);
			return CMD_RET_FAILURE;
	}

	/*
	 * The tables are data in generated headers, and C cannot assert on their
	 * contents at build time - an element of a const array is not a constant
	 * expression, which gcc rejects in _Static_assert. What is a constant is
	 * their size, and a regenerated export is very likely to change it, so
	 * that much is caught before the image is ever written to a unit.
	 *
	 * The two checks below are written as runtime tests for that reason, but
	 * at -O2 gcc folds them against the const tables and drops both branches -
	 * their messages are absent from the linked image. So they cost nothing on
	 * a device today, and they come back as a real refusal to burn the moment
	 * a table stops matching. Do not go looking for them in a disassembly.
	 */
	BUILD_BUG_ON_MSG(SI5344_V1_REVD_REG_CONFIG_NUM_REGS != 462,
			"Si5344 V1 table regenerated: re-check where its preamble ends");
	BUILD_BUG_ON_MSG(SI5344_V2_REVD_REG_CONFIG_NUM_REGS != 462,
			"Si5344 V2 table regenerated: re-check where its preamble ends");

	if (si5344_revd_register_config_num <= PLL_PREAMBLE_LAST_INDEX)
	{
		printf("Error: PLL table holds %d entries, too few to carry the preamble, skipping burn\n",
				si5344_revd_register_config_num);
		return CMD_RET_FAILURE;
	}

	if (si5344_revd_registers[PLL_PREAMBLE_LAST_INDEX].address != PLL_PREAMBLE_LAST_REG_ADDR)
	{
		printf("Error: PLL table entry %d writes 0x%04x, the preamble is expected to end at 0x%04x there\n",
				PLL_PREAMBLE_LAST_INDEX,
				si5344_revd_registers[PLL_PREAMBLE_LAST_INDEX].address,
				PLL_PREAMBLE_LAST_REG_ADDR);
		printf("       The 300 ms calibration pause would land in the wrong place, skipping burn\n");
		return CMD_RET_FAILURE;
	}

	int old_bus = i2c_get_bus_num();
	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);

	/*
	 * siklu_si5344d_get_pll_device_addr() leaves current_pll_addr untouched when
	 * neither address answers, and it starts out as 0xFF, so without this the
	 * whole table can be written to an address that was never probed.
	 */
	if (!pll_addr_is_known())
	{
		printf("Error: PLL addr 0x%02x is neither 0x%02x nor 0x%02x, skipping burn\n",
				current_pll_addr, CONFIG_SYS_I2C_UNBURNED_PLL_ADDR, CONFIG_SYS_I2C_BURNED_PLL_ADDR);
		i2c_set_bus_num(old_bus);
		return CMD_RET_FAILURE;
	}

	for (i=0 ; i<si5344_revd_register_config_num ; i++)
	{
		int wr_rc;

		val  = si5344_revd_registers[i].value;
		page = si5344_revd_registers[i].address >> 8;
		reg  = si5344_revd_registers[i].address & 0xFF;

		if (current_page != page && set_pll_page_reg(page) != 0)
		{
			printf("Error: PLL burn stopped at entry %d of %d, page 0x%02x unreachable at addr 0x%02x\n",
					i, si5344_revd_register_config_num, page, current_pll_addr);
			i2c_set_bus_num(old_bus);
			return CMD_RET_FAILURE;
		}

		wr_rc = pll_write_reg(current_pll_addr, reg, val);

		if (i == PLL_PREAMBLE_LAST_INDEX)
		{
			udelay(300000); //Wait 300 ms
		}

		if (reg == PLL_I2C_ADDR_REG && page == 0) // I2C Address
		{
			/*
			 * This entry moves the device to another address, so the probe
			 * below says more about what happened than the write's own return
			 * code does. A refused write here is a symptom, not the verdict.
			 */
			if (wr_rc != 0)
			{
				printf("Warning: PLL I2C_ADDR write at addr 0x%02x returned %d, probing both addresses\n",
						current_pll_addr, wr_rc);
			}

			rc = pll_probe_addr(CONFIG_SYS_I2C_BURNED_PLL_ADDR);

			if (rc == 0)
			{
				current_pll_addr = CONFIG_SYS_I2C_BURNED_PLL_ADDR;
			}
			else if (i2c_probe(current_pll_addr) == 0)
			{
				/*
				 * The device kept the address it had, so the write above was
				 * lost. Linux binds the PLL at the burned address only, so
				 * repeat the write instead of carrying on at this one.
				 */
				printf("Warning: PLL kept addr 0x%02x, repeating the I2C_ADDR write\n", current_pll_addr);

				pll_write_reg(current_pll_addr, reg, val);
				rc = pll_probe_addr(CONFIG_SYS_I2C_BURNED_PLL_ADDR);

				if (rc == 0)
					current_pll_addr = CONFIG_SYS_I2C_BURNED_PLL_ADDR;
			}

			if (rc != 0)
			{
				/*
				 * The device answers on neither address, so the remaining 455
				 * writes have nowhere to go. A power cycle reloads I2C_ADDR
				 * from NVM and the next boot burns the table again.
				 */
				printf("Error: Expected PLL device addr 0x%02x was not found, aborting burn at register %d\n",
						CONFIG_SYS_I2C_BURNED_PLL_ADDR, i);
				i2c_set_bus_num(old_bus);
				return CMD_RET_FAILURE;
			}
		}
		else if (wr_rc != 0)
		{
			/*
			 * Carrying on would leave a device holding part of one
			 * configuration and part of whatever its NVM loaded, and the
			 * success line below would still claim 462 registers. A power
			 * cycle reloads the NVM and the next boot burns the table again.
			 */
			printf("Error: PLL burn stopped at entry %d of %d: page 0x%02x, reg 0x%02x, val 0x%02x, addr 0x%02x, rc %d\n",
					i, si5344_revd_register_config_num, page, reg, val, current_pll_addr, wr_rc);
			i2c_set_bus_num(old_bus);
			return CMD_RET_FAILURE;
		}
	}

	printf("PLL: %d registers burned, device addr 0x%02x\n", si5344_revd_register_config_num, current_pll_addr);

	print_pll_device_state();

	i2c_set_bus_num(old_bus);

	return CMD_RET_SUCCESS;
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


void siklu_si5344d_get_pll_device_addr(void)
{
	int old_bus = i2c_get_bus_num();

	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);

	/*
	 * A transient refusal here decides the whole boot: the burn that follows
	 * has no address to write to and the PLL keeps whatever its NVM loaded.
	 * Retry both probes the way pll_probe_addr() does on the burn path.
	 */
	if (pll_probe_addr(CONFIG_SYS_I2C_UNBURNED_PLL_ADDR) == 0)
		current_pll_addr = CONFIG_SYS_I2C_UNBURNED_PLL_ADDR;
	else if (pll_probe_addr(CONFIG_SYS_I2C_BURNED_PLL_ADDR) == 0)
		current_pll_addr = CONFIG_SYS_I2C_BURNED_PLL_ADDR;
	else
	{
		printf("Error: PLL answers on neither 0x%02x nor 0x%02x on i2c-%d\n",
				CONFIG_SYS_I2C_UNBURNED_PLL_ADDR, CONFIG_SYS_I2C_BURNED_PLL_ADDR,
				CONFIG_SYS_PLL_BUS_NUM);
	}

	/*
	 * The device's page register is a property of the device, and which device
	 * this code is talking to has just been decided, so the cached page has to
	 * go with it.
	 */
	current_page = -1;

	i2c_set_bus_num(old_bus);
}


/*
 * Recovery ladder, walked at boot when the device does not answer.
 *
 * Stage 1 is the retried probe of both addresses in
 * siklu_si5344d_get_pll_device_addr() above, which covers a transfer the bus
 * refused once. Stages 2 and 3 below cover a device that is not answering at
 * all. Each stage prints what failed, what it is about to do and how it ended,
 * so that a field log shows which stage healed the unit.
 */

/*
 * Both writes of the blind pair go to one address. The device is page-based, so
 * page 0 has to be selected before HARD_RST can be reached, and neither write
 * can be confirmed - that is the whole point of this stage.
 *
 * i2c_write_blind() ends each transfer with a STOP, so the bus is released
 * between the two writes and again afterwards; no separate stop is needed.
 */
static int pll_blind_hard_reset_at(u8 addr)
{
	u8 page = 0x00;
	u8 reset = PLL_HARD_RST_VALUE;
	int rc;

	rc = i2c_write_blind(CONFIG_SYS_PLL_BUS_NUM, addr, PLL_PAGE_REG_ADDR, 1, &page, 1);
	if (rc != 0)
		return rc;

	return i2c_write_blind(CONFIG_SYS_PLL_BUS_NUM, addr, PLL_HARD_RST_REG, 1, &reset, 1);
}

/*
 * Waits for the register load that a hard reset starts. Reads DEVICE_READY and
 * nothing else, on both addresses because a reset device returns to the one its
 * NVM holds. Bounded by a timeout: a device that never reports ready must leave
 * the boot free to carry on, which is exactly what the version of this wait
 * that used to sit in this file - an unbounded loop - could not do.
 */
static int pll_wait_device_ready(void)
{
	int old_bus = i2c_get_bus_num();
	int rc = -1;
	ulong start;

	i2c_set_bus_num(CONFIG_SYS_PLL_BUS_NUM);
	start = get_timer(0);

	do {
		u8 val = 0;

		if (i2c_read(CONFIG_SYS_I2C_UNBURNED_PLL_ADDR, PLL_DEVICE_READY_REG, 1, &val, 1) == 0 &&
			val == PLL_DEVICE_READY_VALUE)
		{
			rc = 0;
			break;
		}

		if (i2c_read(CONFIG_SYS_I2C_BURNED_PLL_ADDR, PLL_DEVICE_READY_REG, 1, &val, 1) == 0 &&
			val == PLL_DEVICE_READY_VALUE)
		{
			rc = 0;
			break;
		}

		udelay(PLL_DEVICE_READY_POLL_US);
	} while (get_timer(start) < PLL_DEVICE_READY_TIMEOUT_MS);

	i2c_set_bus_num(old_bus);

	return rc;
}

/*
 * One blind HARD_RST attempt: the pair of writes to both addresses, the wait
 * for the NVM load a reset starts, and the probe that says whether it worked.
 * Reporting the outcome per pass is what lets a field log name the action that
 * revived the device rather than only the fact that it came back.
 */
static int pll_blind_hard_reset_pass(int pass)
{
	pll_blind_hard_reset_at(CONFIG_SYS_I2C_UNBURNED_PLL_ADDR);
	pll_blind_hard_reset_at(CONFIG_SYS_I2C_BURNED_PLL_ADDR);

	udelay(PLL_HARD_RST_SETTLE_US);

	if (pll_wait_device_ready() == 0)
	{
		printf("PLL: stage 2, blind HARD_RST pass %d written, the device reports DEVICE_READY\n", pass);
	}
	else
	{
		printf("PLL: stage 2, blind HARD_RST pass %d written, no DEVICE_READY within %d ms\n",
				pass, PLL_DEVICE_READY_TIMEOUT_MS);
	}

	siklu_si5344d_get_pll_device_addr();

	if (!pll_addr_is_known())
		return -1;

	printf("PLL: stage 2 succeeded on blind HARD_RST pass %d, the device answers on 0x%02x\n",
			pass, current_pll_addr);

	return 0;
}

/*
 * Stage 2: reach a device that listens to the bus without acknowledging.
 *
 * The device is probed after every action rather than once at the end, so the
 * log names the action that revived it: freeing the bus, the first blind
 * HARD_RST, or the second. Stopping at the first success also spares a healthy
 * bus the blind writes it does not need.
 */
static void pll_recover_blind_hard_reset(void)
{
	int idle_rc;

	printf("PLL: stage 2, the device answered on neither 0x%02x nor 0x%02x; freeing the bus first\n",
			CONFIG_SYS_I2C_UNBURNED_PLL_ADDR, CONFIG_SYS_I2C_BURNED_PLL_ADDR);

	idle_rc = siklu_i2c0_force_idle();
	if (idle_rc == 0)
	{
		printf("PLL: stage 2, i2c-%d clocked idle\n", CONFIG_SYS_PLL_BUS_NUM);
	}
	else
	{
		printf("PLL: stage 2, i2c-%d stayed busy through the clocking, rc %d; carrying on\n",
				CONFIG_SYS_PLL_BUS_NUM, idle_rc);
	}

	/*
	 * Freeing the bus resets nothing, so there is no NVM load to wait out and
	 * no DEVICE_READY to poll. The device only needs to see an idle bus before
	 * it is asked anything.
	 */
	udelay(PLL_BUS_IDLE_SETTLE_US);

	siklu_si5344d_get_pll_device_addr();

	if (pll_addr_is_known())
	{
		printf("PLL: stage 2 succeeded on the bus release alone, the device answers on 0x%02x\n",
				current_pll_addr);
		return;
	}

	printf("PLL: stage 2, the device stayed silent after the bus release; writing HARD_RST blind\n");

	if (pll_blind_hard_reset_pass(1) == 0)
		return;

	/*
	 * A second pass, because one byte lost on the way costs the whole attempt
	 * and nothing on this path can tell whether that happened.
	 */
	if (pll_blind_hard_reset_pass(2) == 0)
		return;

	printf("PLL: stage 2 failed, the device answers on neither address after two blind HARD_RST passes\n");
}

/*
 * Clears the board-reset counter. Touches the environment only when a counter
 * is actually stored, so a healthy boot never writes flash.
 */
static void pll_recovery_attempts_clear(void)
{
	if (env_get(PLL_RECOVERY_ATTEMPTS_ENV) == NULL)
		return;

	if (env_set(PLL_RECOVERY_ATTEMPTS_ENV, NULL) == 0 && env_save() == 0)
		printf("PLL: cleared the recovery attempt counter\n");
	else
		printf("Warning: PLL recovery attempt counter survives in the environment; clear %s by hand\n",
				PLL_RECOVERY_ATTEMPTS_ENV);
}

/*
 * Stage 3: reset the whole board through the CPLD, which drops cfg_pll_rst_n
 * along with the rest of register 0x02 and gives the PLL a genuine power-on.
 * This is what S99reboot does in Linux, and a PLL observably comes back from
 * it; the driver's own cpld_unconditional_reset_board() clears cfg_sw_rst only
 * and leaves the PLL running, which is why the whole register goes to zero
 * here.
 *
 * Every path that cannot count the attempt returns instead of resetting. A unit
 * that cannot record how many times it has tried would reset for ever, and an
 * endless reboot is worse than a board running with an unconfigured PLL.
 */
static void pll_recover_board_reset(void)
{
	const char *stored = env_get(PLL_RECOVERY_ATTEMPTS_ENV);
	ulong attempts = stored ? simple_strtoul(stored, NULL, 10) : 0;

	if (attempts >= PLL_RECOVERY_MAX_ATTEMPTS)
	{
		printf("PLL: stage 3 spent, %lu board resets left the device silent; continuing the boot with an unconfigured PLL\n",
				attempts);
		printf("PLL: clear %s in the environment to let the ladder try again\n",
				PLL_RECOVERY_ATTEMPTS_ENV);
		return;
	}

	if (env_set_ulong(PLL_RECOVERY_ATTEMPTS_ENV, attempts + 1) != 0)
	{
		printf("PLL: stage 3 skipped, the attempt counter could not be set; continuing the boot\n");
		return;
	}

	if (env_save() != 0)
	{
		printf("PLL: stage 3 skipped, the attempt counter could not be stored; continuing the boot\n");
		return;
	}

	printf("PLL: stage 3, rebooting the board through the CPLD, attempt %lu of %d\n",
			attempts + 1, PLL_RECOVERY_MAX_ATTEMPTS);

	/*
	 * siklu_board_hw_reboot() is this tree's production reboot, the one the
	 * reboot command uses and the one S99reboot mirrors in Linux: it writes the
	 * LED register and then the reset control register, and keeps writing the
	 * pair until the board goes down. It does not return.
	 *
	 * Writing the reset register once and then carrying on would be worse than
	 * staying in that loop. Zero in R_CPLD_LOGIC_RESET_CONTROL holds
	 * cfg_ten_g_rst_n and cfg_mdm_rst_n low along with cfg_pll_rst_n, so a boot
	 * that continued past the write would reach Linux with the 10G PHY and the
	 * modem held in reset - a worse unit than the unconfigured PLL this ladder
	 * exists to repair.
	 *
	 * The attempt counter is already stored at this point, so even a board
	 * whose CPLD never carries the reset out advances its count on every power
	 * cycle and boots normally once the three attempts are spent.
	 */
	siklu_board_hw_reboot();
}

/*
 * Brings the PLL up at boot, walking the ladder only as far as it has to.
 */
void siklu_si5344d_pll_bring_up(void)
{
	siklu_si5344d_get_pll_device_addr();

	if (!pll_addr_is_known())
		pll_recover_blind_hard_reset();

	if (!pll_addr_is_known())
	{
		/* Resets the board when it can, so it usually does not return. */
		pll_recover_board_reset();
		return;
	}

	if (siklu_si5344d_pll_reg_burn() == CMD_RET_SUCCESS)
		pll_recovery_attempts_clear();
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
