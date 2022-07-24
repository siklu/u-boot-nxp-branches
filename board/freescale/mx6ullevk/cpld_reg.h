/*
 * cpld_reg.h
 *
 *  Created on: Jan 3, 2018
 *      Author: noama
 */

#ifndef CPLD_REG_H_
#define CPLD_REG_H_


//****************************************
//   CPLD (Prototype: CPLD_LOGIC)
//****************************************
#define R_CPLD_LOGIC_MAJOR                  0x0
#define R_CPLD_LOGIC_MINOR_BOARDTYPE        0x1
#define R_CPLD_LOGIC_RESET_CONTROL          0x2
#define R_CPLD_LOGIC_RESET_CAUSE            0x3
#define R_CPLD_LOGIC_MISC_STATUS            0x4
#define R_CPLD_LOGIC_WD_RW                  0x5
#define R_CPLD_LOGIC_ETHERNITY_BOARD        0x6
#define R_CPLD_LOGIC_SFP_MODE               0x7
#define R_CPLD_LOGIC_DIP_MODE               0x8
#define R_CPLD_LOGIC_MODEM_LEDS_CTRL        0x9
#define R_CPLD_LOGIC_ETHERNET_LEDS_CTRL     0xA
#define R_CPLD_LOGIC_POWER_LEDS_CTRL        0xB
#define R_CPLD_LOGIC_POWER_STATUS           0xC
#define R_CPLD_LOGIC_INT_HNDLR_0            0xE
#define R_CPLD_LOGIC_INT_HNDLR_1            0xF
#define R_CPLD_LOGIC_INT_HNDLR_0_MASK       0x10
#define R_CPLD_LOGIC_INT_HNDLR_1_MASK       0x11
#define R_CPLD_LOGIC_CPLD_INT_CAUSE_RO      0x12
#define R_CPLD_LOGIC_MISC_0                 0x13
#define R_CPLD_LOGIC_MISC_1                 0x14
#define R_CPLD_LOGIC_GPIO                   0x1C
#define R_CPLD_LOGIC_CFG_SEL_MISC           0x1D
#define R_CPLD_LOGIC_HW_ASM_VER             0x23
#define R_CPLD_LOGIC_MISC_2                 0x24
#define R_CPLD_LOGIC_SER_EEPROM_IF          0x27

#define LAST_CPLD_REG (R_CPLD_LOGIC_SER_EEPROM_IF)

//****************************************
//   Registers Fields of CPLD
//   General width  8
//****************************************
typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_cpld_major_version:8;
	} s;
} T_CPLD_LOGIC_MAJOR_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_cpld_board_type:4;
		u8 cfg_cpld_minor_version:4;
	} s;
} T_CPLD_LOGIC_MINOR_BOARDTYPE_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_sw_rst:1;	// Active low SW reset ,
		u8 cfg_cause_reg_clear_on_read_en:1;	// setting this bit to '1' enables the clear on read of the cause register  (address 0x3) , default state is '0' because it used as asynchronic reset to the cause register. (power-up on time reset).  , SW should change this bit to '1' after boot and only then, clear the cause register by reading address 0x3.  , If SW will not enable this bit before clearing the cause register the RST-LED might malfunction when user presses it.  ,
		u8 cfg_pll_rst_n:1;		// Active low reset to main PLL
		u8 cfg_ten_g_rst_n:1;	// Active low reset to 10G PHY
		u8 cfg_phy1_rst_c:1;	// Not in use
		u8 cfg_sprom_flash_wp_n:1;
		u8 cfg_mdm_rst_n:1;		// Active low reset to modem
		u8 cfg_wdi_cnt_en:1;
	} s;
} T_CPLD_LOGIC_RESET_CONTROL_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_startup_rst:1;	// Indicates that the system woke up for the first time
		u8 cfg_no_sw_wdi_rst:1;	// The CPLD had issued reset (through the push button), because the SW didn't activate the WDI after startup
		u8 cfg_power_fail_rst:1;	// note to clear this indication with reading address 0x9. (power indications) ,
		u8 cfg_wd_rst:1;	// indicates WD reset
		u8 cfg_ext_pb_short_rst:1;	// Indicates that the reset push button (active low) was pushed for less than 5 sec. , If the push button is pressed, the CPLD issues reset to the board.
		u8 cfg_ext_pb_long_rst:1;	// Indicates that the reset push button (active low) was pushed for more than 5 sec. , If the push button is pressed, the CPLD issues reset to the board.
		u8 cfg_cpu_rst:1;	// Indicates that the SW instructed the CPLD to reset the board, through register TBD
		u8 cfg_ext_pb_medium_rst:1;
	} s;
} T_CPLD_LOGIC_RESET_CAUSE_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_rssi_indication:1;	// RSSI exist
		u8 padding0:1;
		u8 cfg_poe_exist:1;		// Standard Poe detected
		u8 cfg_usb_fault:1;		// USB fault detected
		u8 padding1:1;
		u8 cfg_nand_ry_by:1;	// NAND Ready/Busy
		u8 padding2:2;
	} s;
} T_CPLD_LOGIC_MISC_STATUS_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		/* When asserted to '1' it selects a dummy clock to the WD
		 * device on board. As a result, It cancels the WD operation.
		 * (Software WD triggers no longer necessary, reset will not be
		 * done to the board upon missing SW WD), -- This is not an
		 * operational mode -- ONLY for debug */
		u8 cfg_wd_src:1;

		/* Strobe the WD reset device. When writing 1 to this register,
		 * the CPLD issues a single 20us pulse to the WD device */
		u8 cfg_wd_ignore:1;

		u8 padding0:2;
		u8 cfg_wd_wdi:1;
		u8 cfg_fpga_8020_rst:1;
		u8 from_fpga_8020_rst:1;	/* Not yet implemented */
		u8 padding1:1;
	} s;
} T_CPLD_LOGIC_WD_RW_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_rst_n:1;		// When asserted to '0' resets the Ethernity switch.
	} s;
} T_CPLD_LOGIC_ETHERNITY_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_sfp_mod_def:1;		// 10G SFP + exist
		u8 padding0:3;
		u8 cfg_sfp_los_signal:1;	// 10G SFP + Los of Signal
		u8 padding1:3;
	} s;
} T_CPLD_LOGIC_SFP_MODE_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_dip:4;
		u8 cfg_poe_pair1_exist:1;
		u8 cfg_poe_pair2_exist:1;
		u8 cfg_pll_lol_n:1;	// PLL Los of Lock
		u8 padding0:1;
	} s;
} T_CPLD_LOGIC_DIP_MODE_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_modem_led:2;	// Modem led control , 0 - Green , 1 - Blinking green , 2 - Yellow , 3 - Off
		u8 padding0:1;
		u8 cfg_aes_phy_rst_n:1;	// Active low reset to AES 10G PHY (MACSec)
		u8 cfg_10g_board:1;
		u8 padding1:1;
		u8 cfg_tlk_rst_n:1;		// Active low reset to 10G SERDES
		u8 cfg_switch_rst_n:1;	// Active low reset to L2 Switch   MSB
	} s;
} T_CPLD_LOGIC_MODEM_LEDS_CTRL_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_eth1_led_color:1;	// 0 - green , 1 - yellow
		u8 cfg_eth2_0_led_color:1;	// 0 - green , 1 - yellow
		u8 cfg_eth2_1_led_color:1;	// 0 - green , 1 - yellow
		u8 padding0:5;
	} s;
} T_CPLD_LOGIC_ETHERNET_LEDS_CTRL_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_power_led:3;		// 0 - Red , 1 - Blink Red, 2 - Green, 3 - Blink Green, 4 - Off, 5 - Red/Green, 6 - Red pulse, 7 - Green pulse
		u8 cfg_max_pll_mux_sel1:5;
	} s;
} T_CPLD_LOGIC_POWER_LEDS_CTRL_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_pg_l2_1v5:1;
		u8 cfg_pg_2v5:1;
		u8 cfg_pg_l2_1v1:1;
		u8 cfg_pg_phy_1v8:1;
		u8 cfg_pg_1v35_ddr:1;
		u8 cfg_pg_sd_1v0:1;
		u8 cfg_pg_modem_core:1;
		u8 cfg_pg_phy_0v8:1;
	} s;
} T_CPLD_LOGIC_POWER_STATUS_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_sfp_los_s:1;
		u8 padding0:3;
		u8 cfg_sfp_mod_def0:1;
		u8 padding1:3;
	} s;
} T_CPLD_LOGIC_INT_HNDLR_0_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_phy10g_int:1;
		u8 cfg_sfp_txfault:1;
		u8 padding0:2;
		u8 cfg_switch_int:1;
		u8 cfg_nsync1588:1;
		u8 cfg_rtc_int:1;
		u8 cfg_modem_int:1;
	} s;
} T_CPLD_LOGIC_INT_HNDLR_1_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_sfp_los_s_mask:1;
		u8 padding0:3;
		u8 cfg_sfp_mod_def_mask:1;
		u8 padding1:3;
	} s;
} T_CPLD_LOGIC_INT_HNDLR_0_MASK_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_phy10g_int_mask:1;
		u8 cfg_sfp_txfault_mask:1;
		u8 padding0:2;
		u8 cfg_switch_mask:1;
		u8 cfg_nsync1588_mask:1;
		u8 cfg_rtc_mask:1;
		u8 cfg_modem_mask:1;
	} s;
} T_CPLD_LOGIC_INT_HNDLR_1_MASK_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cpld_int_cause_ro:2;
		u8 padding0:6;
	} s;
} T_CPLD_LOGIC_CPLD_INT_CAUSE_RO_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_int_hndlr_0_global_enable:1;
		u8 cfg_int_hndlr_1_global_enable:1;
		u8 padding0:6;
	} s;
} T_CPLD_LOGIC_MISC_0_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_sfp_tx_disable:1;
		u8 padding0:5;
		u8 cfg_usb_start:1;	// 1= enable USB chip
		u8 padding1:1;
	} s;
} T_CPLD_LOGIC_MISC_1_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 gpio70:4;
		u8 gpio80:4;
	} s;
} T_CPLD_LOGIC_GPIO_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_rf_tx_sel:2;	// Select TX 70/80:  0 - 70, 1 - 80, 2 - Loopback, 3 - Reserved
		u8 cfg_rf_rx_sel:2;	// Select RX 70/80:  0 - 70, 1 - 80, 2 - Loopback, 3 - Reserved
		u8 cfg_aux_a2d2_sel2:1;
		u8 cfg_clk_out_test_sel:2;
		u8 padding0:1;
	} s;
} T_CPLD_LOGIC_CFG_SEL_MISC_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_hw_asm_ver:8;	// Read HW assembly resistors
	} s;
} T_CPLD_LOGIC_HW_ASM_VER_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_serial_nor_flash_wp_n:1;
		u8 padding0:7;
	} s;
} T_CPLD_LOGIC_MISC_2_REGS;


typedef union
{
	u8 uint8;
	struct
	{
		u8 cfg_eeprom_70_dir:1;
		u8 cfg_eeprom_70_data_out:1;
		u8 cfg_eeprom_70_data_in:1;
		u8 cfg_eeprom_80_dir:1;
		u8 cfg_eeprom_80_data_out:1;
		u8 cfg_eeprom_80_data_in:1;
		u8 padding0:2;
	} s;
} T_CPLD_LOGIC_SER_EEPROM_IF_REGS;




#endif /* CPLD_REG_H_ */
