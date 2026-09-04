// SPDX-License-Identifier: GPL-2.0
/* CAN bus driver for Phytium CAN controller
 *
 * Copyright (c) 2021-2024 Phytium Technology Co., Ltd.
 */

#include <linux/bitfield.h>

#include "phytium_can.h"

/* register definition */
enum phytium_can_reg {
	CAN_CTRL		= 0x00,		/* Global control register */
	CAN_INTR		= 0x04,		/* Interrupt register */
	CAN_ARB_RATE_CTRL	= 0x08,		/* Arbitration rate control register */
	CAN_DAT_RATE_CTRL	= 0x0c,		/* Data rate control register */
	CAN_ACC_ID0		= 0x10,		/* Acceptance identifier0 register */
	CAN_ACC_ID1		= 0x14,		/* Acceptance identifier1 register */
	CAN_ACC_ID2		= 0x18,		/* Acceptance identifier2 register */
	CAN_ACC_ID3		= 0x1c,		/* Acceptance identifier3 register */
	CAN_ACC_ID0_MASK	= 0x20,		/* Acceptance identifier0 mask register */
	CAN_ACC_ID1_MASK	= 0x24,		/* Acceptance identifier1 mask register */
	CAN_ACC_ID2_MASK	= 0x28,		/* Acceptance identifier2 mask register */
	CAN_ACC_ID3_MASK	= 0x2c,		/* Acceptance identifier3 mask register */
	CAN_XFER_STS		= 0x30,		/* Transfer status register */
	CAN_ERROR_CNT		= 0x34,		/* Error counter register */
	CAN_FIFO_CNT		= 0x38,		/* FIFO counter register */
	CAN_DMA_CTRL		= 0x3c,		/* DMA request control register */
	CAN_XFER_EN		= 0x40,		/* Transfer enable register */
	CAN_INTR1		= 0x44,		/* Interrupt register 1 */
	CAN_FRM_INFO		= 0x48,		/* Frame valid number register */
	CAN_TIME_OUT		= 0x4c,		/* Timeout register */
	CAN_TIME_OUT_CNT	= 0x50,		/* Timeout counter register */
	CAN_INTR2		= 0x54,		/* Interrupt register 2 */
	CAN_TX_FIFO		= 0x100,	/* TX FIFO shadow register */
	CAN_RX_FIFO		= 0x200,	/* RX FIFO shadow register */
	CAN_RX_INFO_FIFO	= 0x300,	/* RX information FIFO shadow register */
	CAN_PIDR4		= 0xfd0,	/* Peripheral Identification Register 4 */
	CAN_PIDR0		= 0xfe0,	/* Peripheral Identification Register 0 */
	CAN_PIDR1		= 0xfe4,	/* Peripheral Identification Register 1 */
	CAN_PIDR2		= 0xfe8,	/* Peripheral Identification Register 2 */
	CAN_PIDR3		= 0xfec,	/* Peripheral Identification Register 3 */
	CAN_CIDR0		= 0xff0,	/* Component Identification Register 0 */
	CAN_CIDR1		= 0xff4,	/* Component Identification Register 1 */
	CAN_CIDR2		= 0xff8,	/* Component Identification Register 2 */
	CAN_CIDR3		= 0xffc,	/* Component Identification Register 3 */
};

/* Global control register (CTRL) */
#define CTRL_XFER		BIT(0)	/* Transfer enable */
#define CTRL_TXREQ		BIT(1)	/* Transmit request */
#define CTRL_AIME		BIT(2)	/* Acceptance identifier mask enable */
#define CTRL_TTS		BIT(3)	/* Transmit trigger strategy */
#define CTRL_RST		BIT(7)	/* Write 1 to soft reset and self clear */
#define CTRL_RFEIDF		BIT(8)	/* Allow RX frame end interrupt during ID filtered frame */
#define CTRL_RFEDT		BIT(9)	/* Allow RX frame end interrupt during TX frame */
#define CTRL_IOF		BIT(10)	/* Ignore overload flag internally */
#define CTRL_FDCRC		BIT(11)	/* CANFD CRC mode */

/* Interrupt register (INTR) */
#define INTR_BOIS	BIT(0)  /* Bus off interrupt status */
#define INTR_PWIS	BIT(1)  /* Passive warning interrupt status */
#define INTR_PEIS	BIT(2)  /* Passive error interrupt status */
#define INTR_RFIS	BIT(3)  /* RX FIFO full interrupt status */
#define INTR_TFIS	BIT(4)  /* TX FIFO empty interrupt status */
#define INTR_REIS	BIT(5)  /* RX frame end interrupt status */
#define INTR_TEIS	BIT(6)  /* TX frame end interrupt status */
#define INTR_EIS	BIT(7)  /* Error interrupt status */
#define INTR_BOIE	BIT(8)  /* Bus off interrupt enable */
#define INTR_PWIE	BIT(9)  /* Passive warning interrupt enable */
#define INTR_PEIE	BIT(10) /* Passive error interrupt enable */
#define INTR_RFIE	BIT(11) /* RX FIFO full interrupt enable */
#define INTR_TFIE	BIT(12) /* TX FIFO empty interrupt enable */
#define INTR_REIE	BIT(13) /* RX frame end interrupt enable */
#define INTR_TEIE	BIT(14) /* TX frame end interrupt enable */
#define INTR_EIE	BIT(15) /* Error interrupt enable */
#define INTR_BOIC	BIT(16) /* Bus off interrupt clear */
#define INTR_PWIC	BIT(17) /* Passive warning interrupt clear */
#define INTR_PEIC	BIT(18) /* Passive error interrupt clear */
#define INTR_RFIC	BIT(19) /* RX FIFO full interrupt clear */
#define INTR_TFIC	BIT(20) /* TX FIFO empty interrupt clear */
#define INTR_REIC	BIT(21) /* RX frame end interrupt clear */
#define INTR_TEIC	BIT(22) /* TX frame end interrupt clear */
#define INTR_EIC	BIT(23) /* Error interrupt clear */

#define INTR_STATUS_MASK (INTR_BOIS | INTR_PWIS | INTR_PEIS | INTR_RFIS | \
			  INTR_TFIS | INTR_REIS | INTR_TEIS | INTR_EIS)

#define INTR_EN_MASK    (INTR_BOIE | INTR_PWIE | INTR_PEIE | INTR_RFIE | \
			 INTR_REIE | INTR_TEIE | INTR_EIE)

#define INTR_CLEAR_MASK	 (INTR_BOIC | INTR_PWIC | INTR_PEIC | INTR_RFIC | \
			  INTR_TFIC | INTR_REIC | INTR_TEIC | INTR_EIC)

/* Arbitration rate control register (ARB_RATE_CTRL) */
#define ARB_RATE_CTRL_ARJW	GENMASK(1, 0)	/* Arbitration field resync jump width */
#define ARB_RATE_CTRL_APRS	GENMASK(4, 2)	/* Arbitration field propagation segment */
#define ARB_RATE_CTRL_APH1S	GENMASK(7, 5)	/* Arbitration field phase1 segment */
#define ARB_RATE_CTRL_APH2S	GENMASK(10, 8)	/* Arbitration field phase2 segment */
#define ARB_RATE_CTRL_APD	GENMASK(28, 16)	/* Arbitration field prescaler divider */

/* Data rate control register (DAT_RATE_CTRL) */
#define DAT_RATE_CTRL_DRJW	GENMASK(1, 0)	/* Data field resync jump width */
#define DAT_RATE_CTRL_DPRS	GENMASK(4, 2)	/* Data field propagation segment */
#define DAT_RATE_CTRL_DPH1S	GENMASK(7, 5)	/* Data field phase1 segment */
#define DAT_RATE_CTRL_DPH2S	GENMASK(10, 8)	/* Data field phase2 segment */
#define DAT_RATE_CTRL_DPD	GENMASK(28, 16)	/* Data field prescaler divider */

/* Acceptance identifierX register (ACC_IDX) */
#define ACC_IDX_AID_MASK	GENMASK(28, 0)	/* Acceptance identifier */

/* Acceptance identifier0 mask register (ACC_ID0_MASK) */
#define ACC_IDX_MASK_AID_MASK	GENMASK(28, 0)	/* Acceptance identifier mask */

/* Transfer status register (XFER_STS) */
#define XFER_STS_FRAS		GENMASK(2, 0)	/* Frame status */
#define XFER_STS_FIES		GENMASK(7, 3)	/* Field status */
#define  XFER_STS_FIES_IDLE		(0x0)	/* idle */
#define  XFER_STS_FIES_ARBITRATION	(0x1)	/* arbitration */
#define  XFER_STS_FIES_TX_CTRL		(0x2)	/* transmit control */
#define  XFER_STS_FIES_TX_DATA		(0x3)	/* transmit data */
#define  XFER_STS_FIES_TX_CRC		(0x4)	/* transmit crc */
#define  XFER_STS_FIES_TX_FRM		(0x5)	/* transmit frame */
#define  XFER_STS_FIES_RX_CTRL		(0x6)	/* receive control */
#define  XFER_STS_FIES_RX_DATA		(0x7)	/* receive data */
#define  XFER_STS_FIES_RX_CRC		(0x8)	/* receive crc */
#define  XFER_STS_FIES_RX_FRM		(0x9)	/* receive frame */
#define  XFER_STS_FIES_INTERMISSION	(0xa)	/* intermission */
#define  XFER_STS_FIES_TX_SUSPD		(0xb)	/* transmit suspend */
#define  XFER_STS_FIES_BUS_IDLE		(0xc)	/* bus idle */
#define  XFER_STS_FIES_OVL_FLAG		(0xd)	/* overload flag */
#define  XFER_STS_FIES_OVL_DLM		(0xe)	/* overload delimiter */
#define  XFER_STS_FIES_ERR_FLAG		(0xf)	/* error flag */
#define  XFER_STS_FIES_ERR_DLM		(0x10)	/* error delimiter */
#define  XFER_STS_FIES_BUS_OFF		(0x11)	/* bus off */
#define XFER_STS_TS			BIT(8)	/* Transmit status */
#define XFER_STS_RS			BIT(9)	/* Receive status */
#define XFER_STS_XFERS			BIT(10)	/* Transfer status */

/* Error counter register (ERR_CNT) */
#define ERR_CNT_REC		GENMASK(8, 0)	/* Receive error counter */
#define ERR_CNT_TEC		GENMASK(24, 16)	/* Transmit error counter */

/* FIFO counter register (FIFO_CNT) */
#define FIFO_CNT_RFN		GENMASK(6, 0)	/* Receive FIFO valid data number */

/* DMA request control register (DMA_CTRL) */
#define DMA_CTRL_RFTH		GENMASK(5, 0)	/* Receive FIFO DMA request threshold */
#define DMA_CTRL_RFRE		BIT(6)		/* Receive FIFO DMA request enable */
#define DMA_CTRL_TFTH		GENMASK(21, 16)	/* Transmit FIFO DMA request threshold */
#define DMA_CTRL_TFRE		BIT(22)		/* Transmit FIFO DMA request enable */

/* Transfer enable register (XFER_EN) */
#define XFER_EN_XFER		BIT(0)		/* Transfer enable */

/* Interrupt register 1 (INTR1) */
#define INTR1_RF1IS	BIT(0)	/* RX FIFO 1/4 interrupt status */
#define INTR1_RF2IS	BIT(1)	/* RX FIFO 1/2 interrupt status */
#define INTR1_RF3IS	BIT(2)	/* RX FIFO 3/4 interrupt status */
#define INTR1_RF4IS	BIT(3)	/* RX FIFO full interrupt status */
#define INTR1_TF1IS	BIT(4)	/* TX FIFO 1/4 interrupt status */
#define INTR1_TF2IS	BIT(5)	/* TX FIFO 1/2 interrupt status */
#define INTR1_TF3IS	BIT(6)	/* TX FIFO 3/4 interrupt status */
#define INTR1_TF4IS	BIT(7)	/* TX FIFO empty interrupt status */
#define INTR1_RF1IE	BIT(8)	/* RX FIFO 1/4 interrupt enable */
#define INTR1_RF2IE	BIT(9)	/* RX FIFO 1/2 interrupt enable */
#define INTR1_RF3IE	BIT(10)	/* RX FIFO 3/4 interrupt enable */
#define INTR1_RF4IE	BIT(11)	/* RX FIFO full interrupt enable */
#define INTR1_TF1IE	BIT(12)	/* TX FIFO 1/4 interrupt enable */
#define INTR1_TF2IE	BIT(13)	/* TX FIFO 1/2 interrupt enable */
#define INTR1_TF3IE	BIT(14)	/* TX FIFO 3/4 interrupt enable */
#define INTR1_TF4IE	BIT(15)	/* TX FIFO empty interrupt enable */
#define INTR1_RF1IC	BIT(16)	/* RX FIFO 1/4 interrupt clear */
#define INTR1_RF2IC	BIT(17)	/* RX FIFO 1/2 interrupt clear */
#define INTR1_RF3IC	BIT(18)	/* RX FIFO 3/4 interrupt clear */
#define INTR1_RF4IC	BIT(19)	/* RX FIFO full interrupt clear */
#define INTR1_TF1IC	BIT(20)	/* TX FIFO 1/4 interrupt clear */
#define INTR1_TF2IC	BIT(21)	/* TX FIFO 1/2 interrupt clear */
#define INTR1_TF3IC	BIT(22)	/* TX FIFO 3/4 interrupt clear */
#define INTR1_TF4IC	BIT(23)	/* TX FIFO empty interrupt clear */
#define INTR1_RF1RIS	BIT(24)	/* RX FIFO 1/4 raw interrupt status */
#define INTR1_RF2RIS	BIT(25)	/* RX FIFO 1/2 raw interrupt status */
#define INTR1_RF3RIS	BIT(26)	/* RX FIFO 3/4 raw interrupt status */
#define INTR1_RF4RIS	BIT(27)	/* RX FIFO full raw interrupt status */
#define INTR1_TF1RIS	BIT(28)	/* TX FIFO 1/4 raw interrupt status */
#define INTR1_TF2RIS	BIT(29)	/* TX FIFO 1/2 raw interrupt status */
#define INTR1_TF3RIS	BIT(30)	/* TX FIFO 3/4 raw interrupt status */
#define INTR1_TF4RIS	BIT(31)	/* TX FIFO empty raw interrupt status */

/* Frame valid number register (FRM_INFO) */
#define FRM_INFO_RXFC	GENMASK(5, 0)	/* Valid frame number in RX FIFO */
#define FRM_INFO_SSPD	GENMASK(31, 16)	/* Secondary sample point delay */

/* Interrupt register 2 (INTR2) */
#define INTR2_TOIS	BIT(0)	/* RX FIFO time out interrupt status */
#define INTR2_TOIM	BIT(8)	/* RX FIFO time out interrupt mask */
#define INTR2_TOIC	BIT(16)	/* RX FIFO time out interrupt clear */
#define INTR2_TORIS	BIT(24)	/* RX FIFO time out raw interrupt status */

/* RX information FIFO shadow register (RX_INFO_FIFO) */
#define RX_INFO_FIFO_WNORF	GENMASK(4, 0)	/* Word (4-byte) number of current receive frame */
#define RX_INFO_FIFO_RORF	BIT(5)		/* RTR value of current receive frame */
#define RX_INFO_FIFO_FORF	BIT(6)		/* FDF value of current receive frame */
#define RX_INFO_FIFO_IORF	BIT(7)		/* IDE value of current receive frame */

/* Arbitration Bits */
#define CAN_ID1_MASK		GENMASK(31, 21)	/* Base identifer */
/* Standard Remote Transmission Request */
#define CAN_ID1_RTR_MASK	BIT(20)
/* Extended Substitute remote TXreq */
#define	CAN_ID2_SRR_MASK	BIT(20)
#define CAN_IDE_MASK		BIT(19)		/* IDentifier extension flag */
#define CAN_ID2_MASK		GENMASK(18, 1)	/* Identifier extension */
/* Extended frames remote TX request */
#define CAN_ID2_RTR_MASK	BIT(0)
#define CAN_ID1_FDF_MASK	BIT(18)
#define CAN_ID1_DLC_MASK	GENMASK(17, 14)
#define CANFD_ID1_BRS_MASK	BIT(16)
#define CANFD_ID1_ESI_MASK	BIT(15)
#define CANFD_ID1_DLC_MASK	GENMASK(14, 11)

#define CAN_ID2_FDF_MASK	BIT(31)
#define CAN_ID2_DLC_MASK	GENMASK(29, 26)
#define CANFD_ID2_BRS_MASK	BIT(29)
#define CANFD_ID2_ESI_MASK	BIT(28)
#define CANFD_ID2_DLC_MASK	GENMASK(27, 24)

#define CAN_ID1_DLC_OFF		14
#define CANFD_ID1_DLC_OFF	11
#define CAN_ID2_DLC_OFF		26
#define CANFD_ID2_DLC_OFF	24

#define CAN_IDR_ID1_SHIFT       21  /* Standard Messg Identifier */
#define CAN_IDR_ID2_SHIFT       1   /* Extended Message Identifier */
#define CAN_IDR_SDLC_SHIFT      14
#define CAN_IDR_EDLC_SHIFT      26

/* CANFD Standard msg padding 1 */
#define CANFD_IDR_PAD_MASK	0x000007FF
#define CAN_IDR_PAD_MASK	0x00003FFF	/* Standard msg padding 1 */

/**
 * phytium_can_set_reg_bits - set a bit value to the device register
 * @cdev:	Driver private data structure
 * @reg:	Register offset
 * @bs:     The bit mask
 *
 * Read data from the particular CAN register
 * Return: value read from the CAN register
 */
static void
phytium_can_set_reg_bits(const struct phytium_can_dev *cdev,
			 enum phytium_can_reg reg, u32 bs)
{
	u32 val = readl(cdev->base + reg);

	val |= bs;
	writel(val, cdev->base + reg);
}

/**
 * phytium_can_clr_reg_bits - clear a bit value to the device register
 * @cdev:	Driver private data structure
 * @reg:	Register offset
 * @bs:     The bit mask
 *
 * Read data from the particular CAN register
 * Return: value read from the CAN register
 */
static void
phytium_can_clr_reg_bits(const struct phytium_can_dev *cdev,
			 enum phytium_can_reg reg, u32 bs)
{
	u32 val = readl(cdev->base + reg);

	val &= ~bs;
	writel(val, cdev->base + reg);
}

static inline u32 phytium_can_read(const struct phytium_can_dev *cdev, enum phytium_can_reg reg)
{
	return readl(cdev->base + reg);
}

static inline void phytium_can_write(const struct phytium_can_dev *cdev, enum phytium_can_reg reg,
				     u32 val)
{
	writel(val, cdev->base + reg);
}

static inline void phytium_can_enable_all_interrupts(struct phytium_can_dev *cdev)
{
	phytium_can_write(cdev, CAN_INTR, INTR_EN_MASK);
}

static inline void phytium_can_disable_all_interrupt(struct phytium_can_dev *cdev)
{
	phytium_can_write(cdev, CAN_INTR, 0x0);
}

static int phytium_can_get_berr_counter(const struct net_device *dev,
					struct can_berr_counter *bec)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);

	bec->rxerr = phytium_can_read(cdev, CAN_ERROR_CNT) & ERR_CNT_REC;
	bec->txerr = (phytium_can_read(cdev, CAN_ERROR_CNT) & ERR_CNT_TEC) >> 16;

	return 0;
}

static int phytium_can_read_fifo(struct net_device *dev)
{
	struct net_device_stats *stats = &dev->stats;
	struct phytium_can_dev *cdev = netdev_priv(dev);
	struct canfd_frame *cf;
	struct sk_buff *skb;
	u32 id, dlc, i;

	/* Read the frame header from FIFO */
	id = phytium_can_read(cdev, CAN_RX_FIFO);
	id = be32_to_cpup(&id);
	if (id & CAN_IDE_MASK) {
		/* Received an extended frame */
		dlc = phytium_can_read(cdev, CAN_RX_FIFO);
		dlc = be32_to_cpup(&dlc);
		if (dlc & CAN_ID2_FDF_MASK)
			skb = alloc_canfd_skb(dev, &cf);
		else
			skb = alloc_can_skb(dev, (struct can_frame **)&cf);

		if (unlikely(!skb)) {
			stats->rx_dropped++;
			return 0;
		}

		if (dlc & CAN_ID2_FDF_MASK) {
			/* CAN FD extended frame */
			if (dlc & CANFD_ID2_BRS_MASK)
				cf->flags |= CANFD_BRS;
			if (dlc & CANFD_ID2_ESI_MASK)
				cf->flags |= CANFD_ESI;
			cf->len = can_fd_dlc2len((dlc & CANFD_ID2_DLC_MASK) >> CANFD_ID2_DLC_OFF);
		} else {
			/* CAN extended frame */
			cf->len = can_cc_dlc2len((dlc & CAN_ID2_DLC_MASK) >> CAN_ID2_DLC_OFF);
		}

		cf->can_id = (id & CAN_ID1_MASK) >> 3;
		cf->can_id |= (id & CAN_ID2_MASK) >> 1;
		cf->can_id |= CAN_EFF_FLAG;

		if (id & CAN_ID2_RTR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	} else {
		/* Received a standard frame */
		if (id & CAN_ID1_FDF_MASK)
			skb = alloc_canfd_skb(dev, &cf);
		else
			skb = alloc_can_skb(dev, (struct can_frame **)&cf);

		if (unlikely(!skb)) {
			stats->rx_dropped++;
			return 0;
		}

		if (id & CAN_ID1_FDF_MASK) {
			/* CAN FD extended frame */
			if (id & CANFD_ID1_BRS_MASK)
				cf->flags |= CANFD_BRS;
			if (id & CANFD_ID1_ESI_MASK)
				cf->flags |= CANFD_ESI;
			cf->len = can_fd_dlc2len((id & CANFD_ID1_DLC_MASK) >> CANFD_ID1_DLC_OFF);
		} else {
			/* CAN extended frame */
			cf->len = can_cc_dlc2len((id & CAN_ID1_DLC_MASK) >> CAN_ID1_DLC_OFF);
		}

		cf->can_id = (id & CAN_ID1_MASK) >> 21;

		if (id & CAN_ID1_RTR_MASK)
			cf->can_id |= CAN_RTR_FLAG;
	}

	if (!(cf->can_id & CAN_RTR_FLAG))
		/* Receive data frames */
		for (i = 0; i < cf->len; i += 4)
			*(__be32 *)(cf->data + i) = phytium_can_read(cdev, CAN_RX_FIFO);

	stats->rx_packets++;
	stats->rx_bytes += cf->len;
	netif_receive_skb(skb);

	return 1;
}

static int phytium_can_do_rx_poll(struct net_device *dev, int quota)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);
	u32 rxfs, pkts = 0;

	rxfs = phytium_can_read(cdev, CAN_FIFO_CNT) & FIFO_CNT_RFN;
	if (!rxfs) {
		netdev_dbg(dev, "no messages in RX FIFO\n");
		return 0;
	}

	while ((rxfs != 0) && (quota > 0)) {
		pkts += phytium_can_read_fifo(dev);
		quota--;
		rxfs = phytium_can_read(cdev, CAN_FIFO_CNT) & FIFO_CNT_RFN;
		netdev_dbg(dev, "Next received %d frame again.\n", rxfs);
	}

	return pkts;
}

static int phytium_can_poll(struct napi_struct *napi, int quota)
{
	struct net_device *dev = napi->dev;
	struct phytium_can_dev *cdev = netdev_priv(dev);
	int work_done;
	unsigned long flags;

	netdev_dbg(dev, "The receive processing is going on !\n");

	work_done = phytium_can_do_rx_poll(dev, quota);

	/* Don't re-enable interrupts if the driver had a fatal error
	 * (e.g., FIFO read failure)
	 */
	if (work_done >= 0 && work_done < quota) {
		napi_complete_done(napi, work_done);

		spin_lock_irqsave(&cdev->lock, flags);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_REIE);
		spin_unlock_irqrestore(&cdev->lock, flags);
	}

	return work_done;
}

static void phytium_can_write_frame(struct phytium_can_dev *cdev,
				    const struct canfd_frame *cf,
				    bool is_canfd)
{
	struct net_device *dev = cdev->net;
	u32 i, id, dlc = 0, frame_head[2] = {0, 0};
	u32 data_len;

	data_len = can_fd_len2dlc(cf->len);

	/* This frame becomes the only one in flight.  Drop any completion that
	 * is still latched for an earlier frame, so that once is_tx_done is
	 * cleared below a late TEIS cannot be mistaken for this frame's
	 * completion.  Called with cdev->lock held.
	 */
	phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_TEIC | INTR_TFIC);

	/* Watch carefully on the bit sequence */
	if (cf->can_id & CAN_EFF_FLAG) {
		/* Extended CAN ID format */
		id = ((cf->can_id & CAN_EFF_MASK) << 1) & CAN_ID2_MASK;
		id |= (((cf->can_id & CAN_EFF_MASK) >>
			(CAN_EFF_ID_BITS - CAN_SFF_ID_BITS)) <<
			CAN_IDR_ID1_SHIFT) & CAN_ID1_MASK;

		/* The substibute remote TX request bit should be "1"
		 * for extended frames as in the Phytium CAN datasheet
		 */
		id |= CAN_IDE_MASK | CAN_ID2_SRR_MASK;

		if (cf->can_id & CAN_RTR_FLAG)
			/* Extended frames remote TX request */
			id |= CAN_ID2_RTR_MASK;
		if ((cdev->can.ctrlmode & CAN_CTRLMODE_FD) && is_canfd)
			dlc = data_len << CANFD_ID2_DLC_OFF;
		else
			dlc = data_len << CAN_ID2_DLC_OFF;

		if (cdev->can.ctrlmode & CAN_CTRLMODE_FD) {
			dlc |= CAN_ID2_FDF_MASK;
			if (cf->flags & CANFD_BRS)
				dlc |= CANFD_ID2_BRS_MASK;
			if (cf->flags & CANFD_ESI)
				dlc |= CANFD_ID2_ESI_MASK;
		}

		frame_head[0] = cpu_to_be32p(&id);
		frame_head[1] = cpu_to_be32p(&dlc);

		/* Write the Frame to Phytium CAN TX FIFO */
		phytium_can_write(cdev, CAN_TX_FIFO, frame_head[0]);
		phytium_can_write(cdev, CAN_TX_FIFO, frame_head[1]);
		netdev_dbg(dev, "Write atbitration field [0]:0x%x [1]:0x%x\n",
			   frame_head[0], frame_head[1]);
	} else {
		/* Standard CAN ID format */
		id = ((cf->can_id & CAN_SFF_MASK) << CAN_IDR_ID1_SHIFT)
			& CAN_ID1_MASK;

		if (cf->can_id & CAN_RTR_FLAG)
			/* Standard frames remote TX request */
			id |= CAN_ID1_RTR_MASK;

		if (cdev->can.ctrlmode & CAN_CTRLMODE_FD)
			dlc = (data_len << CANFD_ID1_DLC_OFF)
				| CANFD_IDR_PAD_MASK;
		else
			dlc = (data_len << CAN_ID1_DLC_OFF) | CAN_IDR_PAD_MASK;

		id |= dlc;

		if (cdev->can.ctrlmode & CAN_CTRLMODE_FD) {
			id |= CAN_ID1_FDF_MASK;
			if (cf->flags & CANFD_BRS)
				id |= CANFD_ID1_BRS_MASK;
			if (cf->flags & CANFD_ESI)
				id |= CANFD_ID1_ESI_MASK;
		}

		frame_head[0] =  cpu_to_be32p(&id);
		/* Write the Frame to Phytium CAN TX FIFO */
		phytium_can_write(cdev, CAN_TX_FIFO, frame_head[0]);
		netdev_dbg(dev, "Write atbitration field [0] 0x%x\n",
			   frame_head[0]);
	}

	if (!(cf->can_id & CAN_RTR_FLAG)) {
		netdev_dbg(dev, "Write CAN data frame\n");
		for (i = 0; i < cf->len; i += 4) {
			phytium_can_write(cdev, CAN_TX_FIFO,
					  *(__be32 *)(cf->data + i));
			netdev_dbg(dev, "[%d]:%x\n", i,
				   *(__be32 *)(cf->data + i));
		}
	}

	cdev->is_tx_done = false;
	mod_timer(&cdev->timer, jiffies + HZ / 10);

	netdev_dbg(dev, "Trigger send message!\n");

	return;
}

static netdev_tx_t phytium_can_tx_handler(struct phytium_can_dev *cdev,
					  struct sk_buff *skb)
{
	struct net_device *dev = cdev->net;
	struct canfd_frame cf = { };
	bool is_canfd = can_is_canfd_skb(skb);
	unsigned long flags;
	int err;

	/* This controller only reports a frame-end status bit, not a completed
	 * FIFO slot/index.  Keep a single skb in flight so TEIS maps exactly to
	 * echo slot 0, like other single-completion CAN controllers.
	 */
	spin_lock_irqsave(&cdev->lock, flags);

	netif_stop_queue(dev);
	memcpy(&cf, skb->data, skb->len);

	/*
	 * can_put_echo_skb() consumes @skb, including on its -EBUSY and
	 * -ENOMEM paths.  Keep a private frame copy so echo ownership can be
	 * transferred before the FIFO is programmed without dereferencing a
	 * consumed skb afterwards.
	 */
	err = can_put_echo_skb(skb, dev, 0, 0);
	if (err) {
		/* Do not push a frame to the controller without an echo entry. */
		netdev_err(dev, "failed to reserve TX echo slot: %d\n", err);
		dev->stats.tx_dropped++;
		/* -EINVAL is the sole can_put_echo_skb() error which leaves skb
		 * owned by the caller.  It is not expected for fixed echo slot 0.
		 */
		if (err == -EINVAL)
			dev_kfree_skb_any(skb);
		netif_wake_queue(dev);

		spin_unlock_irqrestore(&cdev->lock, flags);
		return NETDEV_TX_OK;
	}

	phytium_can_write_frame(cdev, &cf, is_canfd);

	spin_unlock_irqrestore(&cdev->lock, flags);

	return NETDEV_TX_OK;
}

/**
 * phytium_can_tx_interrupt - Tx Done Isr
 * @ndev:	net_device pointer
 * @isr:	Interrupt status register value
 */
static void phytium_can_tx_interrupt(struct net_device *ndev, u32 isr)
{
	struct phytium_can_dev *cdev = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	unsigned int len;

	if (isr & INTR_TEIS)
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_TEIC);

	/* A timeout resets the controller and discards its TX FIFO before the
	 * queue is reopened.  A completion observed afterwards is therefore
	 * stale and must never consume a newly installed echo skb.
	 */
	if (cdev->is_tx_done) {
		if (netif_queue_stopped(ndev))
			netif_wake_queue(ndev);
		return;
	}

	len = can_get_echo_skb(ndev, 0, NULL);
	stats->tx_bytes += len;
	stats->tx_packets++;

	cdev->is_tx_done = true;
	del_timer(&cdev->timer);

	netdev_dbg(ndev, "Finish transform packets %lu\n", stats->tx_packets);

	phytium_can_set_reg_bits(cdev, CAN_INTR,
				 INTR_BOIE | INTR_PWIE | INTR_PEIE);

	netif_wake_queue(ndev);
}

static void phytium_can_start(struct net_device *dev);

static void phytium_can_tx_done_timeout(struct timer_list *t)
{
	struct phytium_can_dev *priv = from_timer(priv, t, timer);
	struct net_device *ndev = priv->net;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);
	if (!priv->is_tx_done) {
		netdev_dbg(ndev, "tx done timeout\n");

		priv->is_tx_done = true;
		ndev->stats.tx_errors++;

		/* A frame-end interrupt does not carry a FIFO index.  Re-requesting
		 * the timed-out FIFO entry can yield both its original completion and
		 * its retry completion, which a boolean cannot distinguish from the
		 * next frame.  Resetting the controller drops the old FIFO entry and
		 * its pending completion before accepting another skb.
		 */
		phytium_can_clr_reg_bits(priv, CAN_CTRL, CTRL_XFER);
		phytium_can_set_reg_bits(priv, CAN_INTR, INTR_TEIC | INTR_TFIC);
		can_free_echo_skb(ndev, 0, NULL);

		/* Only a controller that is still on the bus may be restarted
		 * here.  After bus-off the controller is held in configuration
		 * mode and bringing it back is up to can_restart(), which only
		 * runs if restart-ms was set; while the interface is being
		 * stopped or suspended it has to stay off.  The echo slot is
		 * released above in every case, so a timed-out frame can never
		 * pin it down.
		 */
		if (priv->can.state == CAN_STATE_ERROR_ACTIVE ||
		    priv->can.state == CAN_STATE_ERROR_WARNING ||
		    priv->can.state == CAN_STATE_ERROR_PASSIVE) {
			phytium_can_start(ndev);
			netif_wake_queue(ndev);
		}
	}
	spin_unlock_irqrestore(&priv->lock, flags);
}

static void phytium_can_err_interrupt(struct net_device *ndev, u32 isr)
{
	struct phytium_can_dev *cdev = netdev_priv(ndev);
	struct net_device_stats *stats = &ndev->stats;
	struct can_frame *cf;
	struct sk_buff *skb;
	u32  txerr = 0, rxerr = 0;

	skb = alloc_can_err_skb(ndev, &cf);

	rxerr = phytium_can_read(cdev, CAN_ERROR_CNT) & ERR_CNT_REC;
	txerr = ((phytium_can_read(cdev, CAN_ERROR_CNT) & ERR_CNT_TEC) >> 16);

	if (isr & INTR_BOIS) {
		netdev_dbg(ndev, "bus_off %s: txerr :%u rxerr :%u\n",
			   __func__, txerr, rxerr);
		cdev->can.state = CAN_STATE_BUS_OFF;
		cdev->can.can_stats.bus_off++;
		/* Leave device in Config Mode in bus-off state */
		phytium_can_write(cdev, CAN_CTRL, CTRL_RST);
		can_bus_off(ndev);
		if (skb)
			cf->can_id |= CAN_ERR_BUSOFF;
	} else if ((isr & INTR_PEIS) == INTR_PEIS) {
		netdev_dbg(ndev, "error_passive %s: txerr :%u rxerr :%u\n",
			   __func__, txerr, rxerr);
		cdev->can.state = CAN_STATE_ERROR_PASSIVE;
		cdev->can.can_stats.error_passive++;
		/* Clear interrupt condition */
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_PEIC);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_PWIC);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_TEIC);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_EIC);
		if (skb) {
			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] = (rxerr > 127) ?
					CAN_ERR_CRTL_RX_PASSIVE :
					CAN_ERR_CRTL_TX_PASSIVE;
			cf->data[6] = txerr;
			cf->data[7] = rxerr;
		}
	} else if (isr & INTR_PWIS) {
		netdev_dbg(ndev, "error_warning %s: txerr :%u rxerr :%u\n",
			   __func__, txerr, rxerr);
		cdev->can.state = CAN_STATE_ERROR_WARNING;
		cdev->can.can_stats.error_warning++;
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_PWIC);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_TEIC);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_EIC);
		if (skb) {
			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] |= (txerr > rxerr) ?
					CAN_ERR_CRTL_TX_WARNING :
					CAN_ERR_CRTL_RX_WARNING;
			cf->data[6] = txerr;
			cf->data[7] = rxerr;
		}
	}

	/* Check for RX FIFO Overflow interrupt */
	if (isr & INTR_RFIS) {
		stats->rx_over_errors++;
		stats->rx_errors++;

		if (skb) {
			cf->can_id |= CAN_ERR_CRTL;
			cf->data[1] |= CAN_ERR_CRTL_RX_OVERFLOW;
		}
	}

	if (skb) {
		stats->rx_packets++;
		stats->rx_bytes += cf->can_dlc;
		netif_rx(skb);
	}
}

/**
 * phytium_can_isr - CAN Isr
 * @irq:	irq number
 * @dev_id:	device id poniter
 *
 * This is the phytium CAN Isr. It checks for the type of interrupt
 * and invokes the corresponding ISR.
 *
 * Return:
 * * IRQ_NONE - If CAN device is in sleep mode, IRQ_HANDLED otherwise
 */
static irqreturn_t phytium_can_isr(int irq, void *dev_id)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct phytium_can_dev *cdev = netdev_priv(dev);
	u32 isr;

	/* Get the interrupt status */
	isr = phytium_can_read(cdev, CAN_INTR) & INTR_STATUS_MASK;
	if (!isr)
		return IRQ_NONE;

	spin_lock(&cdev->lock);

	/* Re-sample the status now that the lock is held.  The TX completion
	 * handling below judges these bits against is_tx_done, which
	 * phytium_can_tx_handler() clears while holding the same lock.  A
	 * snapshot taken before the lock could be arbitrarily older than the
	 * state it is compared with, and a completion latched for the previous
	 * frame would then be attributed to the one just queued.
	 */
	isr = phytium_can_read(cdev, CAN_INTR) & INTR_STATUS_MASK;
	if (!isr) {
		spin_unlock(&cdev->lock);
		return IRQ_HANDLED;
	}

	/* Check for FIFO full interrupt and alarm */
	if ((isr & INTR_RFIS)) {
		netdev_dbg(dev, "rx_fifo is full!.\n");
		phytium_can_clr_reg_bits(cdev, CAN_INTR, INTR_RFIE);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_RFIC);
		napi_schedule(&cdev->napi);
	}

	/* Check for FIFO empty interrupt and alarm */
	if ((isr & INTR_TFIS)) {
		netdev_dbg(dev, "tx_fifo is empty!.\n");
		isr &= (~INTR_TFIS);
		phytium_can_clr_reg_bits(cdev, CAN_INTR, INTR_TFIE);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_TFIC);
	}

	/* Check for Tx interrupt and Processing it.
	 *
	 * The completion is handled before the error branch below.  A frame
	 * that reached the end of the frame on the wire is a real completion
	 * even when an error status is latched in the same snapshot; dropping
	 * it would hand that frame to the TX timeout instead, which counts a TX
	 * error for a frame that was sent, keeps echo slot 0 occupied and
	 * resets the controller a hundred milliseconds later.
	 *
	 * Bus-off is the one case where the frame did not complete: the
	 * controller is reset into configuration mode there and the echo slot
	 * is released by the timeout or by can_restart().  Leave the latched
	 * completion to the error branch, which drops it.
	 */
	if ((isr & INTR_TEIS) && !(isr & INTR_BOIS) &&
	    cdev->can.state != CAN_STATE_BUS_OFF)
		phytium_can_tx_interrupt(dev, isr);

	/* Check for the type of error interrupt and Processing it */
	if (isr & (INTR_EIS | INTR_RFIS | INTR_BOIS | INTR_PWIS | INTR_PEIS)) {
		phytium_can_clr_reg_bits(cdev, CAN_INTR, (INTR_EIE | INTR_RFIE |
					 INTR_BOIE | INTR_PWIE | INTR_PEIE));
		phytium_can_err_interrupt(dev, isr);
		phytium_can_set_reg_bits(cdev, CAN_INTR, (INTR_EIC | INTR_RFIC |
					 INTR_BOIC | INTR_PWIC | INTR_PEIC));

		/* A completion which was not handled above belongs to a frame
		 * that never completed, or is stale from before the last
		 * controller reset.  Drop it: leaving it latched would attribute
		 * it to whichever frame is in flight when it is serviced.
		 */
		if (isr & INTR_TEIS)
			phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_TEIC);

		spin_unlock(&cdev->lock);
		return IRQ_HANDLED;
	}

	/* Check for the type of receive interrupt and Processing it */
	if (isr & (INTR_REIS)) {
		phytium_can_clr_reg_bits(cdev, CAN_INTR, INTR_REIE);
		phytium_can_set_reg_bits(cdev, CAN_INTR, INTR_REIC);
		napi_schedule(&cdev->napi);
	}
	spin_unlock(&cdev->lock);
	return IRQ_HANDLED;
}

/**
 * phytium_can_set_bittiming - CAN set bit timing routine
 * @dev:	Pointer to net_device structure
 *
 * This is the driver set bittiming  routine.
 * Return: 0 on success and failure value on error
 */
static int phytium_can_set_bittiming(struct net_device *dev)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);
	const struct can_bittiming *bt = &cdev->can.bittiming;
	const struct can_bittiming *dbt = &cdev->can.data_bittiming;
	u32 btr, dbtr;
	u32 is_config_mode;

	/**
	 * Check whether Phytium CAN is in configuration mode.
	 * It cannot set bit timing if Phytium CAN is not in configuration mode.
	 */
	is_config_mode = phytium_can_read(cdev, CAN_CTRL) & CTRL_XFER;
	if (is_config_mode) {
		netdev_alert(dev, "BUG! Cannot set bittiming - CAN is not in config mode\n");
		return -EPERM;
	}

	/* Setting Baud Rate prescalar value in BRPR Register */
	btr = (bt->brp - 1) << 16;

	/* Setting Time Segment 1 in BTR Register */
	btr |= (bt->prop_seg - 1) << 2;

	btr |= (bt->phase_seg1 - 1) << 8;

	/* Setting Time Segment 2 in BTR Register */
	btr |= (bt->phase_seg2 - 1) << 5;

	/* Setting Synchronous jump width in BTR Register */
	btr |= (bt->sjw - 1);

	dbtr = (dbt->brp - 1) << 16;
	dbtr |= (dbt->prop_seg - 1) << 2;
	dbtr |= (dbt->phase_seg1 - 1) << 8;
	dbtr |= (dbt->phase_seg2 - 1) << 5;
	dbtr |= (dbt->sjw - 1);

	if (cdev->can.ctrlmode & CAN_CTRLMODE_FD) {
		phytium_can_write(cdev, CAN_ARB_RATE_CTRL, btr);
		phytium_can_write(cdev, CAN_DAT_RATE_CTRL, dbtr);
	} else {
		phytium_can_write(cdev, CAN_ARB_RATE_CTRL, btr);
		phytium_can_write(cdev, CAN_DAT_RATE_CTRL, btr);
	}

	netdev_dbg(dev, "DAT=0x%08x, ARB=0x%08x\n",
		   phytium_can_read(cdev, CAN_DAT_RATE_CTRL),
		   phytium_can_read(cdev, CAN_ARB_RATE_CTRL));

	return 0;
}

/**
 * phytium_can_start - This the drivers start routine
 * @dev:	Pointer to net_device structure
 *
 * This is the drivers start routine.
 * Based on the State of the CAN device it puts
 * the CAN device into a proper mode.
 *
 * Return: 0 on success and failure value on error
 */
static void phytium_can_start(struct net_device *dev)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);
	u32 ctrl;

	/* Disable transfer */
	ctrl = phytium_can_read(cdev, CAN_CTRL);
	ctrl &= ~CTRL_XFER;
	phytium_can_write(cdev, CAN_CTRL, ctrl);

	/* XXX: If CANFD, reset the controller */
	phytium_can_write(cdev, CAN_CTRL, (ctrl | CTRL_RST));

	/* Bittiming setup */
	phytium_can_set_bittiming(dev);

	/* Acceptance identifier mask setup */
	phytium_can_write(cdev, CAN_ACC_ID0_MASK, ACC_IDX_MASK_AID_MASK);
	phytium_can_write(cdev, CAN_ACC_ID1_MASK, ACC_IDX_MASK_AID_MASK);
	phytium_can_write(cdev, CAN_ACC_ID2_MASK, ACC_IDX_MASK_AID_MASK);
	phytium_can_write(cdev, CAN_ACC_ID3_MASK, ACC_IDX_MASK_AID_MASK);
	ctrl |= CTRL_AIME;

	if (cdev->can.ctrlmode & CAN_CTRLMODE_FD)
		ctrl |= CTRL_IOF | CTRL_FDCRC;

	phytium_can_write(cdev, CAN_CTRL, ctrl);

	cdev->can.state = CAN_STATE_ERROR_ACTIVE;

	phytium_can_enable_all_interrupts(cdev);

	if (cdev->can.ctrlmode & CAN_CTRLMODE_LISTENONLY)
		ctrl |= CTRL_XFER;
	else
		ctrl |= CTRL_XFER | CTRL_TXREQ;

	phytium_can_write(cdev, CAN_CTRL, ctrl);
}

/**
 * phytium_can_stop - Driver stop routine
 * @dev:	Pointer to net_device structure
 *
 * This is the drivers stop routine. It will disable the
 * interrupts and put the device into configuration mode.
 */
static void phytium_can_stop(struct net_device *dev)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);
	unsigned long flags;
	u32 ctrl;

	/* Publish the stopped state first.  The TX timeout restarts the
	 * controller while it is still on the bus, so it has to observe this
	 * state before it is waited for below, or a callback which is already
	 * past its state check would undo the shutdown with a concurrent
	 * phytium_can_start().
	 */
	spin_lock_irqsave(&cdev->lock, flags);
	cdev->can.state = CAN_STATE_STOPPED;
	spin_unlock_irqrestore(&cdev->lock, flags);

	/* del_timer_sync() and not del_timer(): the close path flushes the echo
	 * skbs in close_candev() without taking the driver lock, so a callback
	 * still running in can_free_echo_skb() would free slot 0 twice.  The
	 * lock must not be held across this call, the callback takes it.
	 */
	del_timer_sync(&cdev->timer);

	/* Disable all interrupts */
	phytium_can_disable_all_interrupt(cdev);

	/* Disable transfer and switch to receive-only mode */
	ctrl = phytium_can_read(cdev, CAN_CTRL);
	ctrl &= ~(CTRL_XFER | CTRL_TXREQ);
	phytium_can_write(cdev, CAN_CTRL, ctrl);
}

static void phytium_can_clean(struct net_device *dev)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);
	unsigned long flags;

	/* The ISR and the TX timeout may both be about to consume echo slot 0.
	 * Taking the lock here keeps the is_tx_done test, the skb release and
	 * the flag update in one critical section, so the slot can only be
	 * freed once.
	 */
	spin_lock_irqsave(&cdev->lock, flags);
	if (!cdev->is_tx_done) {
		dev->stats.tx_errors++;
		can_free_echo_skb(cdev->net, 0, NULL);
		cdev->is_tx_done = true;
	}
	spin_unlock_irqrestore(&cdev->lock, flags);
}

static int phytium_can_set_mode(struct net_device *dev, enum can_mode mode)
{
	switch (mode) {
	case CAN_MODE_START:
		phytium_can_clean(dev);
		phytium_can_start(dev);
		netif_wake_queue(dev);
		break;
	default:
		return -EOPNOTSUPP;
	}
	return 0;
}

/**
 * phytium_can_open - Driver open routine
 * @dev:	Pointer to net_device structure
 *
 * This is the driver open routine.
 * Return: 0 on success and failure value on error
 */
static int phytium_can_open(struct net_device *dev)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);
	int ret;

	ret = pm_runtime_get_sync(cdev->dev);
	if (ret < 0) {
		netdev_err(dev, "%s: pm_runtime_get failed(%d)\n",
			   __func__, ret);
		return ret;
	}

	/* Open the CAN device */
	ret = open_candev(dev);
	if (ret) {
		netdev_err(dev, "failed to open can device\n");
		goto disable_clk;
	}

	/* Register interrupt handler */
	ret = request_irq(dev->irq, phytium_can_isr,
			  IRQF_SHARED, dev->name, dev);
	if (ret < 0) {
		netdev_err(dev, "failed to request interrupt\n");
		goto fail;
	}

	/* Start the controller */
	phytium_can_start(dev);

	netdev_dbg(dev, "%s is going on\n", __func__);

	napi_enable(&cdev->napi);
	netif_start_queue(dev);

	return 0;

fail:
	pm_runtime_put(cdev->dev);
	close_candev(dev);
disable_clk:
	pm_runtime_put_sync(cdev->dev);
	return ret;
}

/**
 * phytium_can_close - Driver close routine
 * @dev:	Pointer to net_device structure
 *
 * Return: 0 always
 */
static int phytium_can_close(struct net_device *dev)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);

	netif_stop_queue(dev);
	napi_disable(&cdev->napi);

	phytium_can_stop(dev);
	free_irq(dev->irq, dev);
	pm_runtime_put_sync(cdev->dev);

	close_candev(dev);

	return 0;
}

/**
 * phytium_can_start_xmit - Starts the transmission
 *
 * Return: 0 on success.
 */
static netdev_tx_t phytium_can_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct phytium_can_dev *cdev = netdev_priv(dev);

	if (can_dev_dropped_skb(dev, skb))
		return NETDEV_TX_OK;

	/* This controller implements classic CAN and CAN FD only.  A CAN XL
	 * frame is accepted by can_dropped_invalid_skb() with up to CANXL_MTU
	 * bytes and does not fit into the struct canfd_frame which
	 * phytium_can_tx_handler() copies it into.  Dropping it here also
	 * prevents canxl_frame::flags (CANXL_XLF, 0x80) from being read as
	 * canfd_frame::len by phytium_can_write_frame().
	 */
	if (unlikely(can_is_canxl_skb(skb))) {
		kfree_skb(skb);
		dev->stats.tx_dropped++;
		return NETDEV_TX_OK;
	}

	return phytium_can_tx_handler(cdev, skb);
}

static const struct net_device_ops phytium_can_netdev_ops = {
	.ndo_open = phytium_can_open,
	.ndo_stop = phytium_can_close,
	.ndo_start_xmit = phytium_can_start_xmit,
	.ndo_change_mtu = can_change_mtu,
};

static int register_phytium_can_dev(struct net_device *dev)
{
	dev->flags |= IFF_ECHO;
	dev->netdev_ops = &phytium_can_netdev_ops;

	return register_candev(dev);
}

static int phytium_can_dev_setup(struct phytium_can_dev *cdev)
{
	struct net_device *dev = cdev->net;

	netif_napi_add(dev, &cdev->napi, phytium_can_poll);

	cdev->can.do_set_mode = phytium_can_set_mode;
	cdev->can.do_get_berr_counter = phytium_can_get_berr_counter;

	cdev->can.ctrlmode_supported = CAN_CTRLMODE_LISTENONLY |
				       CAN_CTRLMODE_BERR_REPORTING;
	cdev->can.bittiming_const = cdev->bit_timing;

	if (cdev->fdmode) {
		cdev->can.ctrlmode_supported |= CAN_CTRLMODE_FD;
		dev->mtu = CANFD_MTU;
		cdev->can.ctrlmode = CAN_CTRLMODE_FD;
		cdev->can.data_bittiming_const = cdev->bit_timing;
	}
	spin_lock_init(&cdev->lock);
	return 0;
}

struct phytium_can_dev *phytium_can_allocate_dev(struct device *dev, int sizeof_priv,
						 int tx_fifo_depth)
{
	struct phytium_can_dev *cdev = NULL;
	struct net_device *net_dev;

	/* Allocate the can device struct */
	net_dev = alloc_candev(sizeof_priv, tx_fifo_depth);
	if (!net_dev) {
		dev_err(dev, "Failed to allocate CAN device.\n");
		goto out;
	}

	cdev = netdev_priv(net_dev);
	cdev->net = net_dev;
	cdev->dev = dev;
	SET_NETDEV_DEV(net_dev, dev);

out:
	return cdev;
}
EXPORT_SYMBOL(phytium_can_allocate_dev);

void phytium_can_free_dev(struct net_device *net)
{
	free_candev(net);
}
EXPORT_SYMBOL(phytium_can_free_dev);

int phytium_can_register(struct phytium_can_dev *cdev)
{
	int ret;

	ret = phytium_can_dev_setup(cdev);
	if (ret)
		goto fail;

	ret = register_phytium_can_dev(cdev->net);
	if (ret) {
		dev_err(cdev->dev, "registering %s failed (err=%d)\n",
			cdev->net->name, ret);
		goto fail;
	}

	cdev->is_tx_done = true;
	timer_setup(&cdev->timer, phytium_can_tx_done_timeout, 0);

	dev_info(cdev->dev, "%s device registered (irq=%d)\n",
		 KBUILD_MODNAME, cdev->net->irq);

	/* Probe finished
	 * Stop clocks. They will be reactivated once the device is opened.
	 */
	pm_runtime_put_sync(cdev->dev);

	return 0;

fail:
	pm_runtime_put_sync(cdev->dev);
	return ret;
}
EXPORT_SYMBOL(phytium_can_register);

void phytium_can_unregister(struct phytium_can_dev *cdev)
{
	unregister_candev(cdev->net);
}
EXPORT_SYMBOL(phytium_can_unregister);

int phytium_can_suspend(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct phytium_can_dev *cdev = netdev_priv(ndev);

	if (netif_running(ndev)) {
		netif_stop_queue(ndev);
		netif_device_detach(ndev);
		phytium_can_stop(ndev);

		/* A frame may have been in flight when the interface was
		 * detached.  phytium_can_stop() only disables interrupts and the
		 * TX timeout, so without this the echo slot would stay occupied
		 * and every later can_put_echo_skb() would fail with -EBUSY,
		 * leaving the device unable to transmit for good.  The close path
		 * does not need it because close_candev() flushes the echo skbs.
		 * Done after the stop so the IRQ handler and the timer are
		 * already out of the way.
		 */
		phytium_can_clean(ndev);

		pm_runtime_put_sync(cdev->dev);
	}

	cdev->can.state = CAN_STATE_SLEEPING;

	return 0;
}
EXPORT_SYMBOL(phytium_can_suspend);

int phytium_can_resume(struct device *dev)
{
	struct net_device *ndev = dev_get_drvdata(dev);
	struct phytium_can_dev *cdev = netdev_priv(ndev);
	int ret;

	if (netif_running(ndev)) {
		ret = pm_runtime_resume(cdev->dev);
		if (ret)
			return ret;

		/* phytium_can_start() puts the controller back into
		 * CAN_STATE_ERROR_ACTIVE.
		 */
		phytium_can_start(ndev);
		netif_device_attach(ndev);
		netif_start_queue(ndev);
	} else {
		/* Nothing was started, so the controller is still in the
		 * configuration mode phytium_can_stop() left it in.
		 */
		cdev->can.state = CAN_STATE_STOPPED;
	}

	return 0;
}
EXPORT_SYMBOL(phytium_can_resume);

MODULE_AUTHOR("Cheng Quan <chengquan@phytium.com.cn>");
MODULE_AUTHOR("Chen Baozi <chenbaozi@phytium.com.cn>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("CAN bus driver for Phytium CAN controller");
