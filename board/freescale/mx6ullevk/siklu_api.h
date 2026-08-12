/*
 * siklu_api.h
 *
 *  Created on: Aug 29, 2017
 *      Author: edwardk
 *      Same code used in host/sysapi/sysapi-nxp/
 *      Any changes here should be reflected
 */

#ifndef SIKLU_API_H_
#define SIKLU_API_H_


extern int siklu_is_restore2fact_default(void);


extern uint32_t get_nand_part_offset_by_name(const char* name);
extern int siklu_board_init(void);
extern int siklu_board_late_init_hw(void);
extern int siklu_board_late_init_env(void);

extern u8 current_pll_addr;

extern u8 siklu_cpld_read(u8 reg);
extern int siklu_cpld_write(u8 reg, u8 data);

extern int siklu_88e639x_reg_read(u8 port, u8 reg, u16* val);
extern int siklu_88e639x_reg_write(u8 port, u8 reg, u16 val);

extern void siklu_si5344d_get_pll_device_addr(void);
extern int siklu_si5344d_pll_reg_burn(void);

extern void get_88x3310_phy_version(u32 *val_version);
extern void get_TLK10031_version(u32 *val_version);
extern int get_siklu_cpld_version(u32 spi_mode, u32 *val);
extern int get_pll_part_number(u16 *part_number);
extern int get_pll_device_grade(u8 *device_grade);
extern int get_pll_device_revision(u8 *device_revision);
extern int get_pll_tool_version(u32 *tool_version);

typedef enum {
	MODULE_RFIC_70,
	MODULE_RFIC_80,
} MODULE_RFIC_E;

extern int siklu_rfic_module_read (MODULE_RFIC_E module, u8 reg, u8* data);
extern int siklu_rfic_module_write(MODULE_RFIC_E module, u8 reg, u8  data);

typedef enum {
	SIKLU_MDIO_BUS0, // SOHO
	SIKLU_MDIO_BUS1, // 10G PHY and TI Transceiver
} SIKLU_MDIO_BUS_E;


typedef enum {
    SKL_LED_MODEM,
    SKL_LED_ETH1,
    SKL_LED_ETH2_0,
    SKL_LED_ETH2_1,
    SKL_LED_ETH2,
    SKL_LED_ETH3,
    SKL_LED_POWER,
    SKL_LED_XPIC,
    SKL_LED_ALL,
} SKL_BOARD_LED_TYPE_E;

typedef enum {
    SKL_LED_MODE_OFF,
    SKL_LED_MODE_GREEN,
    SKL_LED_MODE_YELLOW,
    SKL_LED_MODE_GREEN_BLINK,
    SKL_LED_MODE_YELLOW_BLINK,
}  SKL_BOARD_LED_MODE_E;


typedef enum {
	SKL_BOARD_TYPE_UNKNOWN = 0,
	SKL_BOARD_TYPE_PCB195 = 1,
	SKL_BOARD_TYPE_PCB213 = 2,
	SKL_BOARD_TYPE_PCB217 = 3,
	SKL_BOARD_TYPE_PCB277 = 4,
	SKL_BOARD_TYPE_PCB295 = 5,
	SKL_BOARD_TYPE_PCB295_AES = 6,
}  SKL_BOARD_TYPE_E;


extern int siklu_mdio_bus_connect(SIKLU_MDIO_BUS_E bus);

extern int siklu_sf_sys_eeprom_read(const char* buf, int size);
extern int siklu_sf_sys_eeprom_write(const char* buf, int size);
extern int siklu_sf_sys_eeprom_erase(void);

// SYSEEPROM related API
extern int siklu_syseeprom_init(void);
extern int siklu_syseeprom_display(void);
extern int siklu_syseeprom_restore_default(void);
extern int siklu_syseeprom_get_val(const char* name, char* val);
extern int siklu_syseeprom_set_val(const char* name, const char* val);
extern int siklu_syseeprom_udate(void);

extern int siklu_soho_power_up_init(void);

// enable/disable network connection between 1G rj45 management port and cpu
extern int siklu_cpu_netw_cntrl(int is_ena);
extern SKL_BOARD_TYPE_E siklu_get_board_type(void);
extern int siklu_set_led_modem(SKL_BOARD_LED_MODE_E mode);

#endif /* SIKLU_API_H_ */
