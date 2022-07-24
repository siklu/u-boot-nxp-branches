/*
 * mx6ullevk_siklu_ethernity.c
 *
 *  Created on: Jul 12, 2022
 *      Author: yba@tkos.co.il
 */

#ifndef CONFIG_SPL_BUILD

#include <common.h>
#include <linux/ctype.h>
#include <command.h>

#include "siklu_def.h"
#include "siklu_api.h"
#include "cpld_reg.h"

/*
 *
 */
static int do_siklu_ethernity_reset(cmd_tbl_t * cmdtp, int flag, int argc,
		char * const argv[]) {

	int rc = 0;
	T_CPLD_LOGIC_WD_RW_REGS reset_reg;

	reset_reg.s.cfg_fpga_8020_rst = 1;

	rc = siklu_cpld_write(R_CPLD_LOGIC_WD_RW, reset_reg.uint8);
	if (rc != 0) {
		printf("%s() ERROR writing to CPLD, line %d\n", __func__, __LINE__);
		return rc;
	}
	return rc;
}


U_BOOT_CMD(ethernity_rst, 1, 1, do_siklu_ethernity_reset, "Reset the Ethernity board",
	"Reset the Ethernity board - no parameters");

#endif
