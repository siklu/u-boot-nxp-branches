#
# (C) Copyright 2000-2007
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# SPDX-License-Identifier:	GPL-2.0+
#

# There are many options which enable SPI, so make this library available
ifdef CONFIG_DM_SPI
obj-y += spi-uclass.o
obj-$(CONFIG_SANDBOX) += spi-emul-uclass.o
obj-$(CONFIG_SOFT_SPI) += soft_spi.o
else
obj-y += spi.o
obj-$(CONFIG_SOFT_SPI) += soft_spi_legacy.o
endif

obj-$(CONFIG_ALTERA_SPI) += altera_spi.o
obj-$(CONFIG_ATH79_SPI) += ath79_spi.o
obj-$(CONFIG_ATMEL_SPI) += atmel_spi.o
obj-$(CONFIG_CADENCE_QSPI) += cadence_qspi.o cadence_qspi_apb.o
obj-$(CONFIG_CF_SPI) += cf_spi.o
obj-$(CONFIG_DAVINCI_SPI) += davinci_spi.o
obj-$(CONFIG_DESIGNWARE_SPI) += designware_spi.o
obj-$(CONFIG_EXYNOS_SPI) += exynos_spi.o

obj-$(CONFIG_FSL_DSPI) += fsl_dspi.o
obj-$(CONFIG_FSL_ESPI) += fsl_espi.o
obj-$(CONFIG_FSL_QSPI) += fsl_qspi.o
# follow file added to support imx6ull SPI bus
obj-$(CONFIG_FSL_IMX_ESPI) += fsl_imx_espi.o

obj-$(CONFIG_ICH_SPI) +=  ich.o
obj-$(CONFIG_KIRKWOOD_SPI) += kirkwood_spi.o
obj-$(CONFIG_LPC32XX_SSP) += lpc32xx_ssp.o
obj-$(CONFIG_MPC8XX_SPI) += mpc8xx_spi.o
obj-$(CONFIG_MPC8XXX_SPI) += mpc8xxx_spi.o
obj-$(CONFIG_MVEBU_A3700_SPI) += mvebu_a3700_spi.o
obj-$(CONFIG_MXC_SPI) += mxc_spi.o
obj-$(CONFIG_MXS_SPI) += mxs_spi.o
obj-$(CONFIG_NDS_AE3XX_SPI) += nds_ae3xx_spi.o
obj-$(CONFIG_OMAP3_SPI) += omap3_spi.o
obj-$(CONFIG_PIC32_SPI) += pic32_spi.o
obj-$(CONFIG_ROCKCHIP_SPI) += rk_spi.o
obj-$(CONFIG_SANDBOX_SPI) += sandbox_spi.o
obj-$(CONFIG_SH_SPI) += sh_spi.o
obj-$(CONFIG_SH_QSPI) += sh_qspi.o
obj-$(CONFIG_STM32_QSPI) += stm32_qspi.o
obj-$(CONFIG_TEGRA114_SPI) += tegra114_spi.o
obj-$(CONFIG_TEGRA20_SFLASH) += tegra20_sflash.o
obj-$(CONFIG_TEGRA20_SLINK) += tegra20_slink.o
obj-$(CONFIG_TEGRA210_QSPI) += tegra210_qspi.o
obj-$(CONFIG_TI_QSPI) += ti_qspi.o
obj-$(CONFIG_XILINX_SPI) += xilinx_spi.o
obj-$(CONFIG_ZYNQ_SPI) += zynq_spi.o
obj-$(CONFIG_ZYNQ_QSPI) += zynq_qspi.o
