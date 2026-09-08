// SPDX-License-Identifier: GPL-2.0+
/*
 * Quirks for the Phytium Multimedia Card Interface
 *
 * Copyright (C) 2026, Phytium Technology Co., Ltd.
 */

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/mmc.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "phytium-mci.h"

#ifdef CONFIG_ARCH_PHYTIUM
#include <asm/phytium_cputype.h>
#endif

#define PHYTIUM_MCI_DMA_MAGIC_SIZE		8
#define PHYTIUM_MCI_DMA_MAGIC_PATTERN		0xdeadbeef
#define PHYTIUM_MCI_DMA_MAGIC_TIMEOUT_US	100000
#define PHYTIUM_MCI_RECOVERY_POLL_INTERVAL_US	1
#define PHYTIUM_MCI_RECOVERY_POLL_TIMEOUT_US	1000000
#define PHYTIUM_MCI_WRITE_STATUS_MASK		GENMASK(29, 17)

enum phytium_mci_recovery_layout {
	PHYTIUM_MCI_RECOVERY_NONE,
	PHYTIUM_MCI_RECOVERY_PER_CONTROLLER,
	PHYTIUM_MCI_RECOVERY_BIT0,
	PHYTIUM_MCI_RECOVERY_BIT1,
};

struct phytium_mci_recovery_config {
	phys_addr_t reset_reg;
	u32 reset_bit;
	bool per_controller_bit;
};

static const struct phytium_mci_recovery_config
phytium_mci_recovery_configs[] = {
	[PHYTIUM_MCI_RECOVERY_PER_CONTROLLER] = {
		.reset_reg = 0x2807e000,
		.per_controller_bit = true,
	},
	[PHYTIUM_MCI_RECOVERY_BIT0] = {
		.reset_reg = 0x28020000,
		.reset_bit = BIT(0),
	},
	[PHYTIUM_MCI_RECOVERY_BIT1] = {
		.reset_reg = 0x18016004,
		.reset_bit = BIT(1),
	},
};

struct phytium_mci_context {
	u32 cntrl;
	u32 pwren;
	u32 clkdiv;
	u32 clkena;
	u32 tmout;
	u32 ctype;
	u32 fifoth;
	u32 card_thrctl;
	u32 uhs_reg;
	u32 uhs_reg_ext;
	u32 emmc_ddr_reg;
	u32 enable_shift;
	u32 clk_divider;
	u32 card_reset;
	u32 bus_mode;
	u32 int_mask;
	u32 dmac_int_ena;
};

struct phytium_mci_dma_quirk {
	bool dma_magic_valid;
	u8 *dma_magic_virt;
	dma_addr_t dma_magic_dma;
};

struct phytium_mci_recovery_quirk {
	const struct phytium_mci_recovery_config *config;
	void __iomem *reg;
	u32 controller_id;
};

static bool phytium_mci_has_dma_read_quirk(void)
{
#ifdef CONFIG_ARCH_PHYTIUM
	switch (phyt_soc_type_t) {
	case PE220X:
	case PD2308:
	case PD2408:
		return true;
	default:
		return false;
	}
#else
	return false;
#endif
}

static enum phytium_mci_recovery_layout
phytium_mci_get_recovery_layout(void)
{
#ifdef CONFIG_ARCH_PHYTIUM
	switch (phyt_soc_type_t) {
	case PE220X:
		return PHYTIUM_MCI_RECOVERY_PER_CONTROLLER;
	case PD2308:
		return PHYTIUM_MCI_RECOVERY_BIT0;
	case PD2408:
		return PHYTIUM_MCI_RECOVERY_BIT1;
	default:
		return PHYTIUM_MCI_RECOVERY_NONE;
	}
#else
	return PHYTIUM_MCI_RECOVERY_NONE;
#endif
}

static u32 phytium_mci_recovery_controller_id(struct phytium_mci_host *host)
{
	struct device *dev = host->dev;
	struct platform_device *pdev;
	struct resource *resource;
	int id;

	if (dev_is_pci(dev)) {
		if (to_pci_dev(dev)->devfn == 2)
			return 1;

		return 0;
	}

	if (dev->of_node) {
		id = of_alias_get_id(dev->of_node, "mmc");
		if (id >= 0)
			return id;
	}

	if (dev_is_platform(dev)) {
		pdev = to_platform_device(dev);
		resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (resource)
			return (resource->start >> 12) & 0x1;
	}

	dev_warn_ratelimited(dev,
			     "controller id unavailable, using reset bit 0\n");
	return 0;
}

static bool phytium_mci_dma_quirk_is_read(struct phytium_mci_host *host,
					  struct mmc_request *mrq)
{
	if (!host->dma_quirk)
		return false;

	if (!host->is_use_dma || !mrq->data || !mrq->cmd)
		return false;

	if (!(mrq->data->flags & MMC_DATA_READ))
		return false;

	if (mrq->cmd->opcode == MMC_READ_SINGLE_BLOCK)
		return true;

	if (mrq->cmd->opcode == MMC_READ_MULTIPLE_BLOCK)
		return true;

	return false;
}

static bool phytium_mci_dma_quirk_done(u32 *magic_ptr, int *word)
{
	int i;

	for (i = 0; i < PHYTIUM_MCI_DMA_MAGIC_SIZE / sizeof(u32); i++) {
		if (magic_ptr[i] == PHYTIUM_MCI_DMA_MAGIC_PATTERN) {
			*word = i;
			return false;
		}
	}

	*word = 0;
	return true;
}

int phytium_mci_dma_quirk_init(struct phytium_mci_host *host)
{
	host->dma_quirk = NULL;

	if (!phytium_mci_has_dma_read_quirk())
		return 0;

	host->dma_quirk = devm_kzalloc(host->dev, sizeof(*host->dma_quirk),
				      GFP_KERNEL);
	if (!host->dma_quirk)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_GPL(phytium_mci_dma_quirk_init);

int phytium_mci_recovery_quirk_init(struct phytium_mci_host *host)
{
	enum phytium_mci_recovery_layout layout;
	const struct phytium_mci_recovery_config *config;

	host->recovery_quirk = NULL;

	layout = phytium_mci_get_recovery_layout();
	if (layout == PHYTIUM_MCI_RECOVERY_NONE)
		return 0;

	host->recovery_quirk = devm_kzalloc(host->dev,
					   sizeof(*host->recovery_quirk),
					   GFP_KERNEL);
	if (!host->recovery_quirk)
		return -ENOMEM;

	config = &phytium_mci_recovery_configs[layout];
	host->recovery_quirk->config = config;
	host->recovery_quirk->controller_id =
		phytium_mci_recovery_controller_id(host);
	host->recovery_quirk->reg = devm_ioremap(host->dev,
						config->reset_reg,
						sizeof(u32));
	if (!host->recovery_quirk->reg)
		dev_warn(host->dev, "failed to map recovery reset register\n");

	return 0;
}
EXPORT_SYMBOL_GPL(phytium_mci_recovery_quirk_init);

void phytium_mci_dma_quirk_start(struct phytium_mci_host *host,
				 struct mmc_request *mrq)
{
	struct phytium_mci_dma_quirk *quirk = host->dma_quirk;
	struct mmc_data *data = mrq->data;
	struct scatterlist *last_sg;
	u32 *magic_ptr;
	unsigned int dma_len;
	int i;

	if (!quirk)
		return;

	quirk->dma_magic_valid = false;
	if (!phytium_mci_dma_quirk_is_read(host, mrq))
		return;

	last_sg = sg_last(data->sg, data->sg_len);
	if (!last_sg || sg_dma_len(last_sg) < PHYTIUM_MCI_DMA_MAGIC_SIZE)
		return;

	dma_len = sg_dma_len(last_sg);
	magic_ptr = (u32 *)((u8 *)sg_virt(last_sg) + dma_len -
			    PHYTIUM_MCI_DMA_MAGIC_SIZE);
	for (i = 0; i < PHYTIUM_MCI_DMA_MAGIC_SIZE / sizeof(u32); i++)
		magic_ptr[i] = PHYTIUM_MCI_DMA_MAGIC_PATTERN;

	quirk->dma_magic_virt = (u8 *)magic_ptr;
	quirk->dma_magic_dma = sg_dma_address(last_sg) + dma_len -
			       PHYTIUM_MCI_DMA_MAGIC_SIZE;
	quirk->dma_magic_valid = true;
}
EXPORT_SYMBOL_GPL(phytium_mci_dma_quirk_start);

void phytium_mci_dma_quirk_complete(struct phytium_mci_host *host,
				    struct mmc_request *mrq)
{
	struct phytium_mci_dma_quirk *quirk = host->dma_quirk;
	u32 *magic_ptr;
	u32 timeout = PHYTIUM_MCI_DMA_MAGIC_TIMEOUT_US;
	bool dma_complete;
	int word = 0;

	if (!quirk)
		return;

	if (!quirk->dma_magic_valid || !quirk->dma_magic_virt)
		return;

	dma_sync_sg_for_cpu(host->dev, mrq->data->sg, mrq->data->sg_len,
			    DMA_FROM_DEVICE);
	magic_ptr = (u32 *)quirk->dma_magic_virt;
	dma_complete = phytium_mci_dma_quirk_done(magic_ptr, &word);

	while (!dma_complete && timeout > 0) {
		usleep_range(10, 20);
		timeout -= 10;
		dma_sync_single_for_cpu(host->dev, quirk->dma_magic_dma,
					PHYTIUM_MCI_DMA_MAGIC_SIZE,
					DMA_FROM_DEVICE);
		dma_complete = phytium_mci_dma_quirk_done(magic_ptr, &word);
	}

	if (!dma_complete)
		dev_dbg(host->dev,
			"DMA completion timeout after %u us, word%d=0x%08x\n",
			PHYTIUM_MCI_DMA_MAGIC_TIMEOUT_US, word,
			magic_ptr[word]);

	quirk->dma_magic_valid = false;
}
EXPORT_SYMBOL_GPL(phytium_mci_dma_quirk_complete);

static u32 phytium_mci_recovery_bit(struct phytium_mci_host *host)
{
	struct phytium_mci_recovery_quirk *quirk = host->recovery_quirk;
	const struct phytium_mci_recovery_config *config;

	if (!quirk)
		return 0;

	config = quirk->config;
	if (!config)
		return 0;

	if (!config->per_controller_bit)
		return config->reset_bit;

	switch (quirk->controller_id) {
	case 0:
		return BIT(0);
	case 1:
		return BIT(1);
	default:
		dev_warn_ratelimited(host->dev,
				     "invalid controller id %u, using reset bit 0\n",
				     quirk->controller_id);
		return BIT(0);
	}
}

u32 phytium_mci_recovery_fifo_reset_mask(struct phytium_mci_host *host)
{
	if (host->recovery_quirk)
		return 0;

	return MCI_CNTRL_FIFO_RESET;
}
EXPORT_SYMBOL_GPL(phytium_mci_recovery_fifo_reset_mask);

static void
phytium_mci_recovery_save_context(struct phytium_mci_host *host,
				  struct phytium_mci_context *context)
{
	context->cntrl = readl(host->base + MCI_CNTRL);
	context->pwren = readl(host->base + MCI_PWREN);
	context->clkdiv = readl(host->base + MCI_CLKDIV);
	context->clkena = readl(host->base + MCI_CLKENA);
	context->clk_divider = readl(host->base + MCI_CLK_DIVIDER);
	context->tmout = readl(host->base + MCI_TMOUT);
	context->ctype = readl(host->base + MCI_CTYPE);
	context->fifoth = readl(host->base + MCI_FIFOTH);
	context->card_thrctl = readl(host->base + MCI_CARD_THRCTL);
	context->uhs_reg = readl(host->base + MCI_UHS_REG);
	context->uhs_reg_ext = readl(host->base + MCI_UHS_REG_EXT);
	context->emmc_ddr_reg = readl(host->base + MCI_EMMC_DDR_REG);
	context->enable_shift = readl(host->base + MCI_ENABLE_SHIFT);
	context->card_reset = readl(host->base + MCI_CARD_RESET);
	context->bus_mode = readl(host->base + MCI_BUS_MODE);
	context->int_mask = readl(host->base + MCI_INT_MASK);
	context->dmac_int_ena = readl(host->base + MCI_DMAC_INT_ENA);
}

static void
phytium_mci_recovery_send_clock_command(struct phytium_mci_host *host)
{
	u32 value;
	int rc;

	writel(0, host->base + MCI_CMDARG);
	wmb(); /* Ensure the argument is visible before starting the command. */
	writel(MCI_CMD_START | MCI_CMD_UPD_CLK, host->base + MCI_CMD);
	rc = readl_relaxed_poll_timeout_atomic(host->base + MCI_CMD, value,
					       !(value & MCI_CMD_START),
					       0, 500 * 1000);
	if (rc)
		dev_warn_ratelimited(host->dev,
				     "clock update command timed out: 0x%08x\n",
				     value);
}

static void
phytium_mci_recovery_restore_clock(struct phytium_mci_host *host,
				   const struct phytium_mci_context *context)
{
	u32 value;
	int rc;

	writel(0, host->base + MCI_UHS_REG_EXT);
	writel(context->uhs_reg_ext, host->base + MCI_UHS_REG_EXT);
	rc = readl_relaxed_poll_timeout_atomic(host->base + MCI_CCLK_RDY,
					       value, value & BIT(0),
					       0, 500 * 1000);
	if (rc)
		dev_warn_ratelimited(host->dev,
				     "card clock did not become ready: 0x%08x\n",
				     value);

	writel(context->clkena & ~MCI_CLKENA_CCLK_ENABLE,
	       host->base + MCI_CLKENA);
	phytium_mci_recovery_send_clock_command(host);
	writel(context->clkdiv, host->base + MCI_CLKDIV);
	writel(context->clk_divider, host->base + MCI_CLK_DIVIDER);
	writel(context->clkena, host->base + MCI_CLKENA);
	phytium_mci_recovery_send_clock_command(host);
}

static void
phytium_mci_recovery_restore_context(struct phytium_mci_host *host,
				     const struct phytium_mci_context *context)
{
	u32 bus_mode;
	u32 cntrl;

	writel(context->pwren, host->base + MCI_PWREN);
	phytium_mci_recovery_restore_clock(host, context);
	writel(context->tmout, host->base + MCI_TMOUT);
	writel(context->ctype, host->base + MCI_CTYPE);
	writel(context->fifoth, host->base + MCI_FIFOTH);
	writel(context->card_thrctl, host->base + MCI_CARD_THRCTL);
	writel(context->uhs_reg, host->base + MCI_UHS_REG);
	writel(context->emmc_ddr_reg, host->base + MCI_EMMC_DDR_REG);
	writel(context->enable_shift, host->base + MCI_ENABLE_SHIFT);
	writel(context->card_reset, host->base + MCI_CARD_RESET);

	bus_mode = context->bus_mode;
	bus_mode &= ~(MCI_BUS_MODE_SWR | MCI_BUS_MODE_DE);
	writel(bus_mode, host->base + MCI_BUS_MODE);

	cntrl = context->cntrl;
	cntrl &= ~(MCI_CNTRL_CONTROLLER_RESET | MCI_CNTRL_FIFO_RESET |
		   MCI_CNTRL_DMA_RESET | MCI_CNTRL_DMA_ENABLE);
	writel(cntrl, host->base + MCI_CNTRL);
	writel(context->dmac_int_ena, host->base + MCI_DMAC_INT_ENA);
	writel(context->int_mask, host->base + MCI_INT_MASK);
	wmb(); /* Restore interrupts after all controller state. */
}

static int
phytium_mci_recovery_pulse(struct phytium_mci_host *host,
			   const char *reason)
{
	struct phytium_mci_recovery_quirk *quirk = host->recovery_quirk;
	u32 reset_bit;
	u32 value;
	int rc;

	if (!quirk || !quirk->config || !quirk->reg)
		return -ENODEV;

	value = readl(host->base + MCI_CNTRL);
	value &= ~MCI_CNTRL_FIFO_RESET;
	value |= MCI_CNTRL_DMA_RESET;
	writel(value, host->base + MCI_CNTRL);

	rc = readl_relaxed_poll_timeout_atomic(host->base + MCI_DMAC_STATUS,
					       value,
					       !(value &
						 MCI_DMAC_STATUS_BUSY_MASK),
					       PHYTIUM_MCI_RECOVERY_POLL_INTERVAL_US,
					       PHYTIUM_MCI_RECOVERY_POLL_TIMEOUT_US);
	if (rc) {
		dev_err_ratelimited(host->dev,
				    "recovery skipped for %s, DMA busy: 0x%08x\n",
				    reason, value);
		return rc;
	}

	reset_bit = phytium_mci_recovery_bit(host);
	value = readl(quirk->reg);
	value &= ~reset_bit;
	writel(value, quirk->reg);
	readl(quirk->reg);
	udelay(1);
	value |= reset_bit;
	writel(value, quirk->reg);
	readl(quirk->reg);
	wmb(); /* Ensure reset deassertion reaches the recovery register. */

	return 0;
}

static int phytium_mci_recovery_run(struct phytium_mci_host *host,
				    const char *reason)
{
	struct phytium_mci_context context;
	int rc;

	if (!host->recovery_quirk)
		return -EOPNOTSUPP;

	phytium_mci_recovery_save_context(host, &context);
	rc = phytium_mci_recovery_pulse(host, reason);
	if (rc)
		return rc;

	phytium_mci_recovery_restore_context(host, &context);
	return 0;
}

void phytium_mci_recovery_check_write(struct phytium_mci_host *host,
				      struct mmc_command *cmd)
{
	u32 status;
	u32 state;

	if (!host->recovery_quirk)
		return;

	if (cmd->opcode != MMC_WRITE_BLOCK &&
	    cmd->opcode != MMC_WRITE_MULTIPLE_BLOCK)
		return;

	status = readl(host->base + MCI_STATUS);
	state = FIELD_GET(PHYTIUM_MCI_WRITE_STATUS_MASK, status);
	if (!(status & MCI_STATUS_FIFO_EMPTY) || state)
		phytium_mci_recovery_run(host, "write status");
}
EXPORT_SYMBOL_GPL(phytium_mci_recovery_check_write);

void phytium_mci_recovery_handle_data_error(struct phytium_mci_host *host,
					    u32 events, bool card_removed)
{
	u32 recovery_events;

	if (card_removed)
		return;

	recovery_events = MCI_RAW_INTS_DCRC | MCI_RAW_INTS_EBE |
			  MCI_RAW_INTS_SBE_BCI | MCI_RAW_INTS_FRUN;
	if (events & recovery_events)
		phytium_mci_recovery_run(host, "data transfer error");
}
EXPORT_SYMBOL_GPL(phytium_mci_recovery_handle_data_error);

bool phytium_mci_recovery_card_removed(struct phytium_mci_host *host)
{
	int rc;

	if (!host->recovery_quirk)
		return false;

	rc = phytium_mci_recovery_pulse(host, "card removal");
	if (rc)
		return false;

	return true;
}
EXPORT_SYMBOL_GPL(phytium_mci_recovery_card_removed);

MODULE_DESCRIPTION("Phytium Multimedia Card Interface quirks");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhu Ling <zhuling2709@phytium.com.cn>");
MODULE_VERSION(PHYTIUM_MMC_DRIVER_VERSION);
