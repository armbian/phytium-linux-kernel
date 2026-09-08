// SPDX-License-Identifier: GPL-2.0+
/*
 * Quirks for the Phytium Multimedia Card Interface
 *
 * Copyright (C) 2026, Phytium Technology Co., Ltd.
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/mmc/mmc.h>
#include <linux/module.h>
#include <linux/scatterlist.h>
#include <linux/slab.h>

#include "phytium-mci.h"

#ifdef CONFIG_ARCH_PHYTIUM
#include <asm/phytium_cputype.h>
#endif

#define PHYTIUM_MCI_DMA_MAGIC_SIZE		8
#define PHYTIUM_MCI_DMA_MAGIC_PATTERN		0xdeadbeef
#define PHYTIUM_MCI_DMA_MAGIC_TIMEOUT_US	100000

struct phytium_mci_dma_quirk {
	bool dma_magic_valid;
	u8 *dma_magic_virt;
	dma_addr_t dma_magic_dma;
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

MODULE_DESCRIPTION("Phytium Multimedia Card Interface quirks");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Zhu Ling <zhuling2709@phytium.com.cn>");
MODULE_VERSION(PHYTIUM_MMC_DRIVER_VERSION);
