// Copyright (C) 2013, Julian Stecklina <jsteckli@os.inf.tu-dresden.de>
// Economic rights: Technische Universitaet Dresden (Germany)

// This file is part of sv3.

// sv3 is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

// sv3 is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License version 2 for more details.

#include <cassert>
#include <sys/eventfd.h>
#include <fstream>
#include <cinttypes>

#include <intel82599.hh>

namespace Switch {

  // Can't set this lower than 6 according to Linux driver.
  static const unsigned itr_us = 10;

  // Constants

#define I99_REG(name, val) name = (val)/4

  enum {
    I99_REG(CTRL,     0x0000),
    I99_REG(STATUS,   0x0008),
    I99_REG(CTRL_EXT, 0x0018),
    I99_REG(LINKS,    0x42A4),

    I99_REG(EERD,    0x10014),

    I99_REG(EICR,     0x0800),
    I99_REG(EICS,     0x0808),
    I99_REG(EIMS,     0x0880),
    I99_REG(EIMC,     0x0888),
    I99_REG(EIAC,     0x0810),
    I99_REG(EIAM,     0x0890),
    I99_REG(GPIE,     0x0898),

    I99_REG(IVAR_MISC, 0xA00),
    I99_REG(IVAR0,    0x0900),
    I99_REG(EITR0,    0x0820),

    I99_REG(RDRXCTL,  0x2F00),
    I99_REG(RXCTRL,   0x3000),
    I99_REG(RXDCTL0,  0x1028),
    I99_REG(RXDCTL64, 0xD028),
    I99_REG(SRRCTL0,  0x1014),
    I99_REG(RSCCTL0,  0x102C),

    I99_REG(RAL0,     0xA200),
    I99_REG(RAH0,     0xA204),
    I99_REG(RDBAL0,   0x1000),
    I99_REG(RDBAH0,   0x1004),
    I99_REG(RDLEN0,   0x1008),
    I99_REG(RDH0,     0x1010),
    I99_REG(RDT0,     0x1018),
    I99_REG(FCTRL,    0x5080),

    I99_REG(TDBAL0,   0x6000),
    I99_REG(TDBAH0,   0x6004),
    I99_REG(TDLEN0,   0x6008),
    I99_REG(TDH0,     0x6010),
    I99_REG(TDT0,     0x6018),
    I99_REG(TDBWAL0,  0x6038),
    I99_REG(TDBWAH0,  0x603C),
    I99_REG(TXDCTL0,  0x6028),
    I99_REG(DMATXCTL, 0x4A80),

    I99_REG(HLREG0,   0x4240),

    I99_REG(TPT,      0x40D4),
    I99_REG(TPR,      0x40D0),
    I99_REG(QBTCL0,   0x8700),
    I99_REG(QBTCH0,   0x8704),

    I99_REG(RTTDCS,   0x4900),

    I99_REG(DCA_CTRL,    0x11074),
    I99_REG(DCA_RXCTRL0, 0x100C),
    I99_REG(DCA_TXCTRL0, 0x600C),

    I99_REG(SECRXCTRL, 0x8D00),
    I99_REG(SECRXSTAT, 0x8D04),

    I99_REG(PSRTYPE0,  0xEA00),

  };
#undef I99_REG

  enum {
    MSIX_RXTX_VECTOR = 0,
    MSIX_MISC_VECTOR = 1,
  };

  enum {
    CTRL_MASTER_DISABLE   = 1U << 2,
    CTRL_LRST             = 1U << 3,  /* Link Reset */
    CTRL_RST              = 1U << 26, /* Device Reset */

    STATUS_MASTER_ENABLE  = 1U << 19,

    CTRL_EXT_NS_DIS       = 1U << 16,
    CTRL_EXT_RO_DIS       = 1U << 17,
    CTRL_EXT_DRV_LOAD     = 1U << 28,

    GPIE_MULTIPLE_MSIX    = 1U << 4,
    GPIE_EIAME            = 1U << 30, /* Automask Enable */
    GPIE_PBA              = 1U << 31, /* Support PBA bits */
    GPIE_RSC_DELAY_SHIFT  = 11,
    GPIE_RSC_DELAY_MASK   = 3U << GPIE_RSC_DELAY_SHIFT,

    EITR_INTERVAL_SHIFT   = 3,
    EITR_INTERVAL_MASK    = 0x1FFU << EITR_INTERVAL_SHIFT,

    RDRXCTL_CRC_STRIP     = 1U << 0,
    RDRXCTL_RSCACKC       = 1U << 25,

    RXCTRL_RXEN           = 1U << 0,

    RXDCTL_EN             = 1U << 25,

    FCTRL_MPE             = 1U << 8,
    FCTRL_UPE             = 1U << 9,
    FCTRL_BAM             = 1U << 10,

    HLREG0_TXCRCEN        = 1U << 0,
    HLREG0_RXCRCSTRIP     = 1U << 1,
    HLREG0_TXPADEN        = 1U << 10,

    TDBWAL_HEAD_WB_EN     = 1U << 0,

    TXDCTL_EN             = 1U << 25,

    DMATXCTL_TE           = 1U,

    SRRCTL_DESCTYPE_MASK  = (7 << 25),
    SRRCTL_DESCTYPE_ADV1B = (1 << 25),
    SRRCTL_DROP_EN        = (1 << 28),
    SRRCTL_BSIZEPACKET_MASK  = 0x1F,
    SRRCTL_BSIZEPACKET_SHIFT = 0,
    SRRCTL_BSIZEHEADER_SHIFT = 8,

    RSCCTL_MAXDESC_MASK   = (3 << 2),
    RSCCTL_MAXDESC_16     = (3 << 2),
    RSCCTL_MAXDESC_8      = (2 << 2),
    RSCCTL_RSCEN          = (1 << 0),

    LINKS_LINK_UP          = (1 << 30),
    LINKS_LINK_SPEED_SHIFT = 28,
    LINKS_LINK_SPEED_MASK  = 3,

    RTTDCS_ARBDIS         = (1U << 6),

    DCA_CTRL_DCA_DIS       = (1U << 0),
    DCA_CTRL_MODE_10       = (1U << 1),
    DCA_RXCTRL_DESC_DCA_EN = (1U << 5),
    DCA_RXCTRL_HEAD_DCA_EN = (1U << 6),
    DCA_RXCTRL_DATA_DCA_EN = (1U << 7),
    DCA_RXCTRL_CPU_SHIFT   = 24,

    DCA_RXCTRL_DESC_RRO_EN = (1U << 9),
    DCA_RXCTRL_DATA_RRO_EN = (1U << 13),
    DCA_RXCTRL_HEAD_RRO_EN = (1U << 15),

    DCA_TXCTRL_DESC_RRO_EN = (1U << 9),
    DCA_TXCTRL_WB_RRO_EN   = (1U << 11),
    DCA_TXCTRL_DATA_RRO_EN = (1U << 13),
    DCA_TXCTRL_DESC_DCA_EN = (1U << 5),
    DCA_TXCTRL_CPU_SHIFT   = 24,

    SECRXCTRL_SECRX_DIS   = (1U << 0),
    SECRXSTAT_SECRX_RDY   = (1U << 0),

    PSRTYPE_PSR_TYPE4     = (1U << 4),

    RXDESC_LO_DD          = (1ULL << 0),
    RXDESC_LO_EOP         = (1ULL << 1),
    RXDESC_LO_NEXTP_SHIFT = 4,
    RXDESC_LO_NEXTP_MASK  = 0xFFFFULL << RXDESC_LO_NEXTP_SHIFT,
    RXDESC_LO_PKT_LEN_SHIFT = 32,
    RXDESC_LO_PKT_LEN_MASK  = 0xFFFFULL << RXDESC_LO_PKT_LEN_SHIFT,
    RXDESC_HI_RSCCNT_SHIFT  = 17,
    RXDESC_HI_RSCCNT_MASK   = 0xFULL << RXDESC_HI_RSCCNT_SHIFT,
    RXDESC_LO_STATUS_IPCS   = (1ULL << 6),
    RXDESC_LO_STATUS_L4I    = (1ULL << 5),
    RXDESC_LO_STATUS_UDPV   = (1ULL << 10),
    RXDESC_LO_ERROR_IPE     = (1ULL << (11 + 20)),
    RXDESC_LO_ERROR_L4E     = (1ULL << (10 + 20)),
    RXDESC_LO_ERROR_RXE     = (1ULL << (9 + 20)),

    RXDESC_HI_PACKET_TYPE_IPV4  = (1ULL << (0 + 4)),
    RXDESC_HI_PACKET_TYPE_IPV4E = (1ULL << (1 + 4)),
    RXDESC_HI_PACKET_TYPE_IPV6  = (1ULL << (2 + 4)),
    RXDESC_HI_PACKET_TYPE_IPV6E = (1ULL << (3 + 4)),
    RXDESC_HI_PACKET_TYPE_TCP   = (1ULL << (4 + 4)),
    RXDESC_HI_PACKET_TYPE_UDP   = (1ULL << (5 + 4)),

    TXDESC_LO_DTYP_ADV_DTA = (3ULL << 20),
    TXDESC_LO_DTYP_ADV_CTX = (2ULL << 20),
    TXDESC_LO_DCMD_TSE    = (1ULL << (7 + 24)),
    TXDESC_LO_DCMD_DEXT   = (1ULL << (5 + 24)),
    TXDESC_LO_DCMD_RS     = (1ULL << (3 + 24)),
    TXDESC_LO_DCMD_IFCS   = (1ULL << (1 + 24)),
    TXDESC_LO_DCMD_EOP    = (1ULL << (0 + 24)),
    TXDESC_HI_MACLEN_SHIFT = 9,
    TXDESC_HI_IPLEN_SHIFT = 0,
    TXDESC_LO_MSS_SHIFT   = 48,
    TXDESC_LO_L4LEN_SHIFT = 40,
    TXDESC_LO_IDX         = (1ULL << 36),
    TXDESC_LO_TUCMD_IPV4    = (1ULL << (1 + 9)),
    TXDESC_LO_TUCMD_L4_UDP  = (0ULL << (2 + 9)),
    TXDESC_LO_TUCMD_L4_TCP  = (1ULL << (2 + 9)),

    TXDESC_LO_POPTS_TXSM    = (1ULL << (1 + 40)),
    TXDESC_LO_POPTS_IXSM    = (1ULL << (0 + 40)),
    TXDESC_LO_CC            = (1ULL << 39),

  };

  void pointer_store(void *p,
		     uint32_t volatile &lo,
		     uint32_t volatile &hi)
  {
    uint64_t u = (uintptr_t)p;
    hi = u >> 32;
    lo = u;
  }

  uint64_t Intel82599::receive_address(unsigned idx)
  {
    assert(idx < 128);
    return uint64_t(_reg[RAH0 + 2*idx]) << 32 | _reg[RAL0 + 2*idx];
  }

  void Intel82599::master_disable()
  {
    _reg[CTRL] |= CTRL_MASTER_DISABLE;
    /* If this fails, we need to double reset. */
    poll_for(1000, [&] { return (_reg[STATUS] & STATUS_MASTER_ENABLE) == 0; });
  }

  std::string Intel82599::status()
  {
    std::stringstream ss;
    uint32_t links  = _reg[LINKS];
    //uint32_t status = _reg[STATUS];

    static const char *speed[] = { "???", "100 MBit/s", "1 GBit/s", "10 GBit/s" };
    unsigned speed_idx = (links >> LINKS_LINK_SPEED_SHIFT) & LINKS_LINK_SPEED_MASK;

    ss << "Link is "
       << ((links & LINKS_LINK_UP) ? "UP" : "DOWN")
       << " at "
       << speed[speed_idx]
       << ".";

    return ss.str();
  }

  void Intel82599::reset()
  {
    // There is lots of magic missing in this reset code to cope with
    // various hardware bugs.

    _reg[EIMC] = ~0U;

    master_disable();
    _reg[CTRL] |= CTRL_RST | CTRL_LRST;
    usleep(1000);
    poll_for(1000, [&] { return (_reg[CTRL] & (CTRL_RST | CTRL_LRST)) == 0; });

    _reg[EIMC] = ~0U;

    // Enable bus master support
    write_config(4, read_config(4, 2) | 7, 2);

    uint64_t mac = receive_address(0);
    if (mac >> 63 == 0) printf("No valid MAC!\n");

    //printf("MAC %012llx\n", mac & ((1ULL << 48) - 1));

    // Recommended RSC delay numbers are: 8 us at 10 Gb/s link and 28 us at 1 Gb/s link.
    // RSC is not recommended when operating at 100 Mb/s link.
    // EITR must be larger than RSC_DELAY!

    // Don't touch RSC/EITR settings after we're done configuring!
    // This seems to cause queue hangs.
    //_reg[GPIE]  = GPIE_MULTIPLE_MSIX | GPIE_EIAME | GPIE_PBA | (4 << GPIE_RSC_DELAY_SHIFT);

    int rsc_delay = 0;
    if (_enable_lro) {
      rsc_delay = std::min<int>(7, std::max<int>(0,((int)itr_us / 4) - 1));
      printf("Interrupt rate is 1 per %uus. RSC delay set to %d.\n", itr_us, rsc_delay);

    }

    _reg[GPIE]  = GPIE_MULTIPLE_MSIX | GPIE_EIAME | GPIE_PBA | (0 << GPIE_RSC_DELAY_SHIFT);
    _reg[EITR0] = (itr_us / 2) << EITR_INTERVAL_SHIFT;

    // We configure two MSI-X vectors. Interrupts are automasked.
    _reg[EIAM] = 0x7FFFFFFF;
    _reg[EIAC] = 0xFFFF;// | (1 << 30);

    /* TCP Timer and Misc map to IRQ MSIX_MISC_VECTOR. */
    _reg[IVAR_MISC] = (0x80 | MSIX_MISC_VECTOR) |
      ((0x80 | MSIX_MISC_VECTOR) << 8);

    /* RX and TX IRQs of queue 0 map to IRQ MSIX_RXTX_VECTOR. */
    _reg[IVAR0] = (0x80 | MSIX_RXTX_VECTOR) |
      ((0x80 | MSIX_RXTX_VECTOR) << 8);

    set_irq_eventfd(VFIO_PCI_MSIX_IRQ_INDEX, MSIX_MISC_VECTOR, _misc_eventfd);
    set_irq_eventfd(VFIO_PCI_MSIX_IRQ_INDEX, MSIX_RXTX_VECTOR, _rxtx_eventfd);

    /* RX/TX */
    _reg[HLREG0]  |= HLREG0_TXCRCEN | HLREG0_TXPADEN | HLREG0_RXCRCSTRIP;

    // Initialize RX general settings


    _reg[FCTRL]   |= FCTRL_UPE | FCTRL_MPE | FCTRL_BAM;

    // Initialize RX queue 0
    _rx_desc       = alloc_dma_mem<desc>(sizeof(desc[QUEUE_LEN]));
    pointer_store(_rx_desc, _reg[RDBAL0], _reg[RDBAH0]);
    _reg[RDLEN0]   = QUEUE_LEN * sizeof(desc);
    _reg[RDT0] = _reg[RDH0] = 0;
    _reg[RDRXCTL] |= RDRXCTL_CRC_STRIP | RDRXCTL_RSCACKC;

    static_assert(RX_BUFFER_SIZE % 1024 == 0, "RX buffer size must be multiple of 1024");
    /* MAXDESC * SRRCTL.BSIZEPACKET must be smaller than (2^16 - 1)! */
    static_assert(8 * RX_BUFFER_SIZE < 65535, "Invalid RX buffer configuration");

    uint32_t srrctl = (_reg[SRRCTL0] & ~(SRRCTL_DESCTYPE_MASK | SRRCTL_BSIZEPACKET_MASK));
    _reg[SRRCTL0] = srrctl | SRRCTL_DESCTYPE_ADV1B | SRRCTL_DROP_EN
      | ((RX_BUFFER_SIZE / 1024) << SRRCTL_BSIZEPACKET_SHIFT) | (4 << SRRCTL_BSIZEHEADER_SHIFT);

    if (_enable_lro) {
      _reg[RSCCTL0]  = (_reg[RSCCTL0] & ~RSCCTL_MAXDESC_MASK)  | RSCCTL_MAXDESC_8 | RSCCTL_RSCEN;
      _reg[PSRTYPE0] = PSRTYPE_PSR_TYPE4;
    }

    _reg[RXDCTL0] = RXDCTL_EN;	// XXX What else?
    poll_for(1000, [&] { return (_reg[RXDCTL0] & RXDCTL_EN) != 0; });

    // Enable receive path
    _reg[SECRXCTRL]   |= SECRXCTRL_SECRX_DIS;
    poll_for(1000, [&] { return (_reg[SECRXSTAT] & SECRXSTAT_SECRX_RDY) != 0; });
    _reg[RXCTRL]      |= RXCTRL_RXEN;
    _reg[SECRXCTRL]   &= ~SECRXCTRL_SECRX_DIS;

    _reg[CTRL_EXT]    |=  CTRL_EXT_NS_DIS | CTRL_EXT_RO_DIS | CTRL_EXT_DRV_LOAD;
    _reg[DCA_RXCTRL0] &= ~( (1 << 12 /* magic */) | DCA_RXCTRL_HEAD_RRO_EN | DCA_RXCTRL_DATA_RRO_EN | DCA_RXCTRL_DESC_RRO_EN);

    /* Transmit enable. See Spec chapter 4.6.8 */

    // Be sure that everything is disabled.
    _reg[DMATXCTL] &= ~DMATXCTL_TE;
    _reg[TXDCTL0]  &= ~TXDCTL_EN;

    // Program TX segmentation via DMTXCTL, DTXTCPFLGL, DTXTCPFLGH and DCA via DCA_TXCTRL.

    _reg[RTTDCS]  |= RTTDCS_ARBDIS;

    // Program DTXMXSZRQ, TXPBSIZE, TXPBTHRESH, MTQX, MNGTXMAP.

    _reg[RTTDCS]  &= ~RTTDCS_ARBDIS;

    _tx_desc       = alloc_dma_mem<desc>(sizeof(desc[QUEUE_LEN]));
    pointer_store(_tx_desc,  _reg[TDBAL0], _reg[TDBAH0]);
    _reg[TDLEN0]   = QUEUE_LEN * sizeof(desc);
    _reg[TDT0]     = _reg[TDH0] = 0;

    _tx_writeback  = alloc_dma_mem<uint32_t>();
     pointer_store((void *)_tx_writeback, _reg[TDBWAL0], _reg[TDBWAH0]);
     _reg[TDBWAL0] |= TDBWAL_HEAD_WB_EN;

     // XXX Check
     _reg[TXDCTL0] =  (1 << 8) | 32;

    _reg[DMATXCTL] |= DMATXCTL_TE;
    _reg[TXDCTL0]  |= TXDCTL_EN;

    poll_for(1000, [&] { return (_reg[TXDCTL0] & TXDCTL_EN) != 0; });

    _reg[DCA_TXCTRL0] &= ~(DCA_TXCTRL_DATA_RRO_EN | DCA_TXCTRL_WB_RRO_EN | DCA_TXCTRL_DESC_RRO_EN);

    // Enable IRQs.
    _reg[EICR] = ~0U;
    _reg[EIMS] = ~0U;

    // XXX TODO Enable DCA for descriptor writeback and packet headers.
  }

  void Intel82599::unmask_misc_irq()
  {
    // Clear interrupt cause register and IRQ mask bits.
    _reg[EICR] = ~0xFFFF;
    _reg[EIMS] = ~0xFFFF;
  }

  void Intel82599::unmask_rxtx_irq()
  {
    // Clear interrupt cause register and IRQ mask bits.
    _reg[EICR] = 1;
    _reg[EIMS] = 1;
  }

  Intel82599::Intel82599(VfioGroup group, std::string device_id, int fd, int rxtx_eventfd, bool enable_lro)
    : VfioDevice(group, device_id, fd), _rxtx_eventfd(rxtx_eventfd), _enable_lro(enable_lro)
  {
    size_t mmio_size;
    _reg = (uint32_t volatile *)map_bar(VFIO_PCI_BAR0_REGION_INDEX, &mmio_size);
    _misc_eventfd = eventfd(0, 0);

    uint32_t id = read_config(0, 4);
    if (id != 0x151c8086 /* 82599/X520 "Niantic" 10G NIC */)
      throw ConfigurationError("Wrong type of device: %04x:%04x",
			       id & 0xFFFF, id >> 16);
  }

  // Intel82599 Switch Port

  bool Intel82599Port::tx_has_room()
  {
    return advance_qp(_shadow_tdt0) != _shadow_tdh0;
  }

  void Intel82599Port::receive(Packet &p)
  {
    // logf("TX %u fragment(s). XXX Ignoring virtio header!", p.fragments);
    
    virtio_net_hdr_mrg_rxbuf const *hdr = reinterpret_cast<virtio_net_hdr_mrg_rxbuf *>(p.fragment[0]);
    assert(p.fragment_length[0] == sizeof(*hdr));
    assert(p.fragments > 1);

    // We assume that flags is a good indicator for anything that needs offloads.
    assert(not (not hdr->flags and hdr->gso_type));

    unsigned payload_size  = p.packet_length - p.fragment_length[0];
    uint64_t offload_flags = 0;

    if (hdr->flags) {
      if (not tx_has_room()) goto fail;

      Ethernet::Header *ehdr = (Ethernet::Header *)p.fragment[1];
      unsigned maclen = sizeof(Ethernet::Header);
      bool     ipv4   = ehdr->type == Ethernet::Ethertype::IPV4;
      bool     udp    = (uint16_t)IPv4::Proto::UDP == (ipv4 ?
						       (uint16_t)ehdr->ipv4->proto :
						       (uint16_t)ehdr->ipv6->next_header);
      assert((uint16_t)IPv4::Proto::UDP == (uint16_t)IPv6::Proto::UDP);

      // This is only valid when csum_start is set.
      unsigned iplen        = hdr->csum_start - maclen;

      // XXX Assert minimum packet length!

      desc ctx;;
      ctx.hi = (uint64_t)maclen << TXDESC_HI_MACLEN_SHIFT;
      ctx.lo = TXDESC_LO_DCMD_DEXT | TXDESC_LO_DTYP_ADV_CTX;

      if (hdr->gso_type != VIRTIO_NET_HDR_GSO_NONE) {
	// Needs segmentation.
	assert(not udp);

	// hdr_len in virtio header is crap. m( Fuck it, we'll do it live!
	unsigned l4len = ((TCP::Header *)((char *)ehdr + hdr->csum_start))->off * 4;

	// Payload size doesn't include header in TSO mode. If this is
	// wrong, the NIC will complete descriptors past TDT!
	payload_size -= l4len + maclen + iplen;

	// logf("TX segment type %u csum_start %u csum_offset %u hdr_len %u l4len %u gso_size %u",
	//      hdr->gso_type, hdr->csum_start, hdr->csum_offset, hdr->hdr_len, l4len, hdr->gso_size);


	ctx.lo |= (uint64_t)hdr->gso_size << TXDESC_LO_MSS_SHIFT;
	ctx.lo |= (uint64_t)l4len         << TXDESC_LO_L4LEN_SHIFT;

	if (ipv4) {
	  // Linux prefills this, but we need it to be zero.
	  ehdr->ipv4->checksum = 0;

	  // The TCP checksum needs to be filled with a partial-pseudo
	  // header.
	  ehdr->ipv4->payload()->tcp.checksum = OnesComplement::fold(ehdr->ipv4->pseudo_checksum(false));

	} else {
	  // IPv6
	  assert(false);
	}

	offload_flags |= (ipv4 ? TXDESC_LO_POPTS_IXSM : 0ULL);
	offload_flags |= TXDESC_LO_DCMD_TSE;
      }

      if (hdr->flags & VIRTIO_NET_HDR_F_NEEDS_CSUM) {
	assert(not ipv4 or (iplen == (unsigned)ehdr->ipv4->ihl*4));

	ctx.hi |= (uint64_t)iplen << TXDESC_HI_IPLEN_SHIFT;
	ctx.lo |= 0
	  | (udp ? TXDESC_LO_TUCMD_L4_UDP : TXDESC_LO_TUCMD_L4_TCP)
	  | (ipv4 ? TXDESC_LO_TUCMD_IPV4 : 0);

	offload_flags |= TXDESC_LO_POPTS_TXSM;

	// logf("TX csum ip %04x l4 %04x",
	//      ehdr->ipv4->checksum, ehdr->ipv4->payload()->tcp.checksum);

	// Can't enable IP checksumming, because it will assume 0 in
	// the checksum field and Linux has already computed this.
	// offload_flags |= (ipv4 ? TXDESC_LO_POPTS_IXSM : 0ULL);
      }

      // logf("CTX %016llx %016llx", ctx.hi, ctx.lo);

      _tx_buffers[_shadow_tdt0].need_completion = false;
      _tx_desc[_shadow_tdt0] = ctx;

      _tx0_inflight++;

      _shadow_tdt0 = advance_qp(_shadow_tdt0);
      __atomic_store_n(&_reg[TDT0], _shadow_tdt0, __ATOMIC_RELEASE);
    }


    {
      unsigned shadow_tdt = _shadow_tdt0;
      unsigned last_tdt   = shadow_tdt;
      unsigned s = 0;

      for (unsigned i = 1; i < p.fragments; i++) {
	last_tdt = shadow_tdt;

	_tx_buffers[shadow_tdt].need_completion = false;

	// logf("TX %s len %x", (i+1 == p.fragments) ? "EOP" : "   ", p.fragment_length[i]);
	_tx_desc[shadow_tdt] = populate_tx_desc(p.fragment[i], p.fragment_length[i],
						payload_size,
						i == 1, offload_flags,
						i+1 == p.fragments);

	// logf(" TX %016llx %016llx", _tx_desc[shadow_tdt].hi, _tx_desc[shadow_tdt].lo);

	s++;
	shadow_tdt = advance_qp(shadow_tdt);
	if (UNLIKELY(_shadow_tdh0 == shadow_tdt))
	  goto fail;
      }

      _tx_buffers[last_tdt].need_completion = true;
      _tx_buffers[last_tdt].info = p.copy_completion_info();

      _tx0_inflight += s;
      _shadow_tdt0 = shadow_tdt;
      __atomic_store_n(&_reg[TDT0], shadow_tdt, __ATOMIC_RELEASE);
    }

    assert(_shadow_tdt0 == _reg[TDT0]);
    return;

  fail:
    logf("TX queue full!");

    assert(_shadow_tdt0 == _reg[TDT0]);
  }

  bool Intel82599Port::poll(Packet &p, bool enable_notifications)
  {
    assert(_shadow_tdt0 == _reg[TDT0]);

    // Prefetch next RX descriptor
    __builtin_prefetch(&_rx_desc[_shadow_rdh0], 0);

    if (UNLIKELY(enable_notifications)) {
      // logf("Unmasking RX/TX IRQ. EICR %08x EIMS %08x",
      // 	   _reg[EICR], _reg[EIMS]);
      unmask_rxtx_irq();
    }

    // XXX As long as we don't use DCA, we can prefetch _tx_writeback
    // here and only access it after we look for received packets.
    unsigned tx_wb = __atomic_load_n(_tx_writeback, __ATOMIC_RELAXED);
    while (tx_wb != _shadow_tdh0) {
      auto &info = _tx_buffers[_shadow_tdh0];
      // logf("%u %u:%u Completed TX index %u. Needed callback: %u.",
      // 	   tx_wb, _reg[TDH0], _reg[TDT0],
      // 	   _shadow_tdh0, info.need_completion);

      // Complete TX packets. This is rather easy because we don't
      // need to look at the descriptors.
      if (info.need_completion)
	info.info.src_port->mark_done(info.info);

      _tx0_inflight--;
      assert(_tx0_inflight >= 0);

      _shadow_tdh0 = advance_qp(_shadow_tdh0);
    }

    // Consume buffers and remember our knowledge about buffer chains
    // in _rx_buffers until we either run out of descriptors with DD
    // set or we found a complete packet (EOP set).

    desc rx;
    while (LIKELY(_shadow_rdh0 != _shadow_rdt0)) {

      // We don't read the 128-bit descriptor atomically. Since lo
      // contains the descriptor done bit (DD), we have to make sure
      // to read it in first, otherwise we race with the NIC and see
      // garbage rx.hi values occasionally.

      auto &_rx = _rx_desc[_shadow_rdh0];

      rx.lo = __atomic_load_n(&_rx.lo, __ATOMIC_ACQUIRE);
      rx.hi = __atomic_load_n(&_rx.hi, __ATOMIC_ACQUIRE);

      if (not (rx.lo & RXDESC_LO_DD))
	break;

      // Prefetch header of packet
      if (not (_rx_buffers[_shadow_rdh0].flags & rx_info::FLAGS_NOT_FIRST))
	__builtin_prefetch(_rx_buffers[_shadow_rdh0].buffer->data, 0, 1);

      unsigned rsccnt = (rx.hi & RXDESC_HI_RSCCNT_MASK) >> RXDESC_HI_RSCCNT_SHIFT;

      // If this is the first packet, we check RSCCNT and remember the
      // result for all packets in the chain.
      if (not (_rx_buffers[_shadow_rdh0].flags & rx_info::FLAGS_NOT_FIRST) and rsccnt)
	_rx_buffers[_shadow_rdh0].flags |= rx_info::FLAGS_RSC;
      else
	assert(not (_rx_buffers[_shadow_rdh0].flags & rx_info::FLAGS_RSC) or rsccnt);

      unsigned nextp  = (_rx_buffers[_shadow_rdh0].flags & rx_info::FLAGS_RSC) ?
	((rx.lo & RXDESC_LO_NEXTP_MASK) >> RXDESC_LO_NEXTP_SHIFT) : advance_qp(_shadow_rdh0);

      _rx_buffers[_shadow_rdh0].packet_length += (rx.lo & RXDESC_LO_PKT_LEN_MASK) >> RXDESC_LO_PKT_LEN_SHIFT;

      if (rx.lo & RXDESC_LO_EOP)
	goto packet_eop;

      _rx_buffers[nextp].flags         = rx_info::FLAGS_NOT_FIRST | _rx_buffers[_shadow_rdh0].flags;
      _rx_buffers[nextp].rsc_last      = _shadow_rdh0;
      _rx_buffers[nextp].rsc_number    = _rx_buffers[_shadow_rdh0].rsc_number + 1;
      _rx_buffers[nextp].packet_length = _rx_buffers[_shadow_rdh0].packet_length;

      _shadow_rdh0 = advance_qp(_shadow_rdh0);
    }

    return false;

  packet_eop:
    // Received a complete packet. Backtrace our steps and build a
    // fragment list. The last descriptor is in rx.

    auto &last_info = _rx_buffers[_shadow_rdh0];

    // Construct a virtio header first.
    p.fragments = 1;
    p.fragment_length[0] = sizeof(last_info.hdr);
    p.packet_length      = sizeof(last_info.hdr) + last_info.packet_length;
    p.fragment[0]        = (uint8_t *)&last_info.hdr;

    memset(&last_info.hdr, 0, sizeof(last_info.hdr));

    // We can correctly set GSO stuff here. But I have no idea what
    // for or if this is used in Linux.

    // bool ipv4 = rx.hi & (RXDESC_HI_PACKET_TYPE_IPV4 | RXDESC_HI_PACKET_TYPE_IPV4E);
    // bool ipv6 = rx.hi & (RXDESC_HI_PACKET_TYPE_IPV6 | RXDESC_HI_PACKET_TYPE_IPV6E);
    // bool tcp  = rx.hi &  RXDESC_HI_PACKET_TYPE_TCP;
    // bool udp  = rx.hi &  RXDESC_HI_PACKET_TYPE_UDP;
    // last_info.hdr.gso_type = VIRTIO_NET_HDR_GSO_NONE;
    // if (last_info.rsc_count > 0) {
    //   if (tcp) {
    // 	if (ipv4) {
    //       #warning This is guessed! Need to look in the header.
    // 	  last_info.hdr.gso_type = VIRTIO_NET_HDR_GSO_TCPV4;
    // 	  last_info.hdr.hdr_len = sizeof(Ethernet::Header) + sizeof(IPv4::Header) + sizeof(TCP::Header);
    // 	} else if (ipv6) {
    // 	  last_info.hdr.gso_type = VIRTIO_NET_HDR_GSO_TCPV6;
    // 	  last_info.hdr.hdr_len = sizeof(Ethernet::Header) + sizeof(IPv6::Header) + sizeof(TCP::Header);
    // 	}
    //   }
    //   if (last_info.hdr.gso_type != VIRTIO_NET_HDR_GSO_NONE)
    // 	last_info.hdr.gso_size = (last_info.packet_length + last_info.hdr.hdr_len + last_info.rsc_count - 1) / last_info.rsc_count;
    // }

    // Inform the guest that checksums are valid, if the NIC has seen
    // a correct L4 checksum. Chicken out, if the hardware has seen a
    // broken IPv4 header.
    if ((rx.lo & RXDESC_LO_STATUS_L4I) and not (rx.lo & RXDESC_LO_ERROR_L4E)
        and (not (rx.lo & RXDESC_LO_STATUS_IPCS) or not (rx.lo & RXDESC_LO_ERROR_IPE)))
      last_info.hdr.flags = VIRTIO_NET_HDR_F_DATA_VALID;

    // XXX There seems to be an errata about udp packets with zero
    // checksum. See Linux code: ixgbe_main.c
    
    unsigned fragments = 1 + _rx_buffers[_shadow_rdh0].rsc_number;
    p.fragments        = 1 + fragments;

    assert(p.fragments < Packet::MAX_FRAGMENTS);

    // Fill fragment list backwards.
    unsigned cur_idx    = _shadow_rdh0;
    p.completion_info.intel82599.rx_idx = cur_idx;

    for (unsigned cur_frag = fragments; cur_frag > 0; cur_frag--) {
      auto &info = _rx_buffers[cur_idx];
      auto &desc = _rx_desc[cur_idx];

      unsigned flen = (desc.lo & RXDESC_LO_PKT_LEN_MASK) >> RXDESC_LO_PKT_LEN_SHIFT;

      p.fragment_length[cur_frag] = flen;
      p.fragment[cur_frag]        = info.buffer->data;

      // logf("Fragment %02u: %p+%x idx %u->%u num %u cnt %d", cur_frag, info.buffer->data, flen,
      // 	   cur_idx, info.rsc_last, info.rsc_number, info.rsc_count);

      // First fragment has not_first == false.
      assert(cur_frag != 0 or not (info.flags & rx_info::FLAGS_NOT_FIRST));
      cur_idx = info.rsc_last;
    }

    // Advance our head pointer for last descriptor in packet.
    _shadow_rdh0 = advance_qp(_shadow_rdh0);

    return true;
  }

  void Intel82599Port::mark_done(Packet::CompletionInfo &c)
  {
    unsigned not_first;
    unsigned idx = c.intel82599.rx_idx;
    do {
      unsigned   next_idx = _rx_buffers[idx].rsc_last;
      rx_buffer *buf      = _rx_buffers[idx].buffer;

      not_first = _rx_buffers[idx].flags & rx_info::FLAGS_NOT_FIRST;

      memset(&_rx_buffers[idx],          0, sizeof(_rx_buffers[0]));

      // Not necessary, but let's be on the careful side of things.
      memset(&_rx_buffers[_shadow_rdt0], 0, sizeof(_rx_buffers[0]));

      _rx_buffers[_shadow_rdt0].buffer = buf;
      _rx_desc[_shadow_rdt0] = populate_rx_desc(buf->data);

      _shadow_rdt0 = advance_qp(_shadow_rdt0);

      idx = next_idx;
    } while (not_first);

    __atomic_store_n(&_reg[RDT0], _shadow_rdt0, __ATOMIC_RELEASE);
  }

  void Intel82599Port::misc_thread_fn()
  {
    ssize_t res;
    uint64_t v;

    logf("IRQ thread is up.");

    while ((res = read(misc_event_fd(), &v, sizeof(v))) == sizeof(v)) {
      logf("%s", status().c_str());

      unmask_misc_irq();
    }

    logf("IRQ thread exits.");
  }

  Intel82599::desc Intel82599Port::populate_rx_desc(uint8_t *data)
  {
    desc r;
    r.hi = (uintptr_t)data;
    r.lo = 0;
    return r;
  }

  Intel82599::desc Intel82599Port::populate_tx_desc(uint8_t *data, uint16_t len, uint16_t total_len,
						    bool first, uint64_t first_flags, bool eop)
  {
    desc r;

    r.hi = (uintptr_t)data;
    r.lo = (uint64_t)len | TXDESC_LO_DTYP_ADV_DTA | TXDESC_LO_DCMD_DEXT;

    if (first)
      r.lo |= ((uint64_t)total_len << 46) | TXDESC_LO_DCMD_IFCS | first_flags;

    if (eop)
      r.lo |= TXDESC_LO_DCMD_EOP | TXDESC_LO_DCMD_RS;

    return r;
  }

  Intel82599Port::Intel82599Port(VfioGroup group, std::string device_id, int fd,
                                 Switch &sw, std::string name, bool enable_lro)
    : Intel82599(group, device_id, fd, sw.event_fd(), enable_lro),
      Port(sw, name),
      _misc_thread(&Intel82599Port::misc_thread_fn, this),
      _shadow_rdt0(0), _shadow_rdh0(0),
      _shadow_tdt0(0), _shadow_tdh0(0), _tx0_inflight(0)
  {
    _switch.register_dma_memory_callback([&] (void *p, size_t s) {
	logf("Registering DMA memory: %p+%zx", p, s);
	map_memory_to_device(p, s, true, true);
      });

    logf("Resetting device.");
    reset();

    // Bind IRQs
    auto cpus = thread_cpus();
    if (cpus.size() == 1) {
      logf("Pinned at %u!", thread_apic_id());
      logf("DCA is %s.", system_supports_dca() ? "available" : "unavailable");

      uint64_t set = 0;
      for (auto cpu : cpus) set |= (1ULL << cpu);

      for (unsigned irq : irqs()) {
	std::stringstream ss; ss << boost::format("/proc/irq/%d/smp_affinity") % irq;
	std::fstream pirq(ss.str(), std::ios_base::out);
	pirq << boost::format("%x") % set;
        logf("Bind IRQ%u to %" PRIx64 ".", irq, set);
      }

    } else {
      logf("NOT pinned. DCA not possible.");
    }

    memset(_rx_buffers, 0, sizeof(_rx_buffers));
    memset(_tx_buffers, 0, sizeof(_tx_buffers));

    logf("Queueing RX buffers.");
    // Create initial set of buffers and enqueue them.
    for (unsigned i = 0; i < RX_BUFFERS; i++) {
      rx_buffer *r = alloc_dma_mem<rx_buffer>();
      static_assert(sizeof(r->data) == 4096, "We only support page sized buffers here");

      // Enqueue buffer
      _rx_buffers[_shadow_rdt0].buffer = r;
      _rx_desc[_shadow_rdt0] = populate_rx_desc(r->data);

      __atomic_store_n(&_reg[RDT0], _shadow_rdt0 = advance_qp(_shadow_rdt0), __ATOMIC_RELEASE);

    }

    enable();
  }

}


// EOF
