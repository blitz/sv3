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

#include <intel82599.hh>

namespace Switch {

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

    I99_REG(HLREG0,   0x4240),

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

    GPIE_MULTIPLE_MSIX    = 1U << 4,
    GPIE_EIAME            = 1U << 30, /* Automask Enable */
    GPIE_PBA              = 1U << 31, /* Support PBA bits */

    RDRXCTL_CRC_STRIP     = 1U << 0,

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

    SRRCTL_DESCTYPE_MASK  = (7 << 25),
    SRRCTL_DESCTYPE_ADV1B = (1 << 25),
    SRRCTL_DROP_EN        = (1 << 28),
    SRRCTL_BSIZEPACKET_MASK  = 0x1F,
    SRRCTL_BSIZEPACKET_SHIFT = 0,

    RSCCTL_MAXDESC_MASK   = (3 << 2),
    RSCCTL_MAXDESC_16     = (3 << 2),
    RSCCTL_MAXDESC_8      = (2 << 2),
    RSCCTL_RSCEN          = (1 << 0),

    LINKS_LINK_UP          = (1 << 30),
    LINKS_LINK_SPEED_SHIFT = 28,
    LINKS_LINK_SPEED_MASK  = 3,

    RXDESC_LO_DD          = (1ULL << 0),
    RXDESC_LO_EOP         = (1ULL << 1),
    RXDESC_LO_NEXTP_SHIFT = 4,
    RXDESC_LO_NEXTP_MASK  = 0xFFFFULL << RXDESC_LO_NEXTP_SHIFT,
    RXDESC_LO_PKT_LEN_SHIFT = 32,
    RXDESC_LO_PKT_LEN_MASK  = 0xFFFFULL << RXDESC_LO_PKT_LEN_SHIFT,
    RXDESC_HI_RSCCNT_SHIFT  = 17,
    RXDESC_HI_RSCCNT_MASK   = 0xFULL << RXDESC_HI_RSCCNT_SHIFT,
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

    ss << "Link is "
       << ((links & LINKS_LINK_UP) ? "UP" : "DOWN")
       << " at "
       << speed[(links >> LINKS_LINK_SPEED_SHIFT) & LINKS_LINK_SPEED_MASK]
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

    /* We configure two MSI-X vectors. Interrupts are automasked. */
    _reg[GPIE] |= GPIE_MULTIPLE_MSIX | GPIE_EIAME | GPIE_PBA;
    _reg[EIAM] = 0x7FFFFFFF;
    _reg[EIAC] = 0xFFFF | (1 << 30);

    /* TCP Timer and Misc map to IRQ MSIX_MISC_VECTOR. */
    _reg[IVAR_MISC] = (0x80 | MSIX_MISC_VECTOR) |
      ((0x80 | MSIX_MISC_VECTOR) << 8);

    /* RX and TX IRQs of queue 0 map to IRQ MSIX_RXTX_VECTOR. */
    _reg[IVAR0] = (0x80 | MSIX_RXTX_VECTOR) |
      ((0x80 | MSIX_RXTX_VECTOR) << 8);
    

    /* RX/TX */
    _reg[HLREG0]  |= HLREG0_TXCRCEN | HLREG0_TXPADEN | HLREG0_RXCRCSTRIP;

    /* Receive */
    _rx_desc       = alloc_dma_mem<desc>(sizeof(desc[QUEUE_LEN]));
    pointer_store(_rx_desc, _reg[RDBAL0], _reg[RDBAH0]);
    _reg[RDLEN0]   = QUEUE_LEN * sizeof(desc);
    _reg[RDT0] = _reg[RDH0] = 0;
    _reg[RDRXCTL] |= RDRXCTL_CRC_STRIP;

    static_assert(RX_BUFFER_SIZE % 1024 == 0, "RX buffer size must be multiple of 1024");
    /* MAXDESC * SRRCTL.BSIZEPACKET must be smaller than (2^16 - 1)! */
    static_assert(8 * RX_BUFFER_SIZE < 65535, "Invalid RX buffer configuration");

    uint32_t srrctl = (_reg[SRRCTL0] & ~(SRRCTL_DESCTYPE_MASK | SRRCTL_BSIZEPACKET_MASK));
    _reg[SRRCTL0] = srrctl | SRRCTL_DESCTYPE_ADV1B | SRRCTL_DROP_EN
      | ((RX_BUFFER_SIZE / 1024) << SRRCTL_BSIZEPACKET_SHIFT);
    _reg[RSCCTL0] = (_reg[RSCCTL0] & ~RSCCTL_MAXDESC_MASK)  | RSCCTL_MAXDESC_8 | RSCCTL_RSCEN;

    /* XXX Verify that RSC is working. Manual claims that hardware
       only merges packets with no TCP options. Linux always adds
       timestamp option. Later it says timestamp options are okay. */

    _reg[RXDCTL0] |= RXDCTL_EN;
    _reg[RXCTRL]  |= RXCTRL_RXEN;
    _reg[FCTRL]   |= FCTRL_UPE | FCTRL_MPE | FCTRL_BAM;

    /* Transmit */
    _tx_desc       = alloc_dma_mem<desc>(sizeof(desc[QUEUE_LEN]));
    pointer_store(_rx_desc,      _reg[TDBAL0], _reg[TDBAH0]);
    _reg[TDLEN0]   = QUEUE_LEN * sizeof(desc);
    _reg[TDT0] = _reg[TDH0] = 0;

    _tx_writeback  = alloc_dma_mem<uint32_t>();
    pointer_store((void *)_tx_writeback, _reg[TDBWAL0], _reg[TDBWAH0]);
    _reg[TDBWAL0] |= TDBWAL_HEAD_WB_EN;

    _reg[TXDCTL0] |= TXDCTL_EN;

    set_irq_eventfd(VFIO_PCI_MSIX_IRQ_INDEX, MSIX_MISC_VECTOR, _misc_eventfd);
    set_irq_eventfd(VFIO_PCI_MSIX_IRQ_INDEX, MSIX_RXTX_VECTOR, _rxtx_eventfd);


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

  Intel82599::Intel82599(VfioGroup group, int fd, int rxtx_eventfd)
    : VfioDevice(group, fd), _rxtx_eventfd(rxtx_eventfd)
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

  void Intel82599Port::receive(Packet &p)
  {
    logf("Send a packet. DROP.");
    // XXX
  }

  void deadfill(uint8_t *p)
  {
    unsigned l = 4096/4;
    asm volatile ( "rep stosl" : "+D" (p), "+c" (l) : "a" (0) : "memory");
  }

  bool Intel82599Port::poll(Packet &p, bool enable_notifications)
  {
    if (UNLIKELY(enable_notifications)) {
      unmask_rxtx_irq();
    }

    desc &rx  = _rx_desc[_shadow_rdh0];

    // Consume buffers and remember our knowledge about buffer chains
    // in _rx_buffers until we either run out of descriptors with DD
    // set or we found a complete packet (EOP set).

    while ((_shadow_rdh0 != _shadow_rdt0) and
	   __atomic_load_n(&rx.lo, __ATOMIC_ACQUIRE) & RXDESC_LO_DD) {

      if (rx.lo & RXDESC_LO_EOP)
	goto packet_eop;

      unsigned rsccnt = (rx.hi & RXDESC_HI_RSCCNT_MASK) >> RXDESC_HI_RSCCNT_SHIFT;
      unsigned nextp  = (rsccnt == 0) ?
	advance_qp(_shadow_rdt0) : ((rx.lo & RXDESC_LO_NEXTP_MASK) >> RXDESC_LO_NEXTP_SHIFT);

      _rx_buffers[nextp].not_first  = true;
      _rx_buffers[nextp].rsc_last   = _shadow_rdt0;
      _rx_buffers[nextp].rsc_number = _rx_buffers[_shadow_rdh0].rsc_number + 1;

      _shadow_rdh0 = advance_qp(_shadow_rdh0);
    }

    return false;

  packet_eop:
    // Received a complete packet. Backtrace our steps and build a
    // fragment list.

    // Construct a virtio header first.
    p.fragments = 1;
    p.fragment_length[0] = sizeof(p.intel82599.hdr);
    p.packet_length      = sizeof(p.intel82599.hdr);
    p.fragment[0]        = (uint8_t *)&p.intel82599.hdr;

    memset(&p.intel82599.hdr, 0, sizeof(p.intel82599.hdr));
    // XXX Fill out header with checksum info
    // When IPCS or L4CS is set, check IPE or TCPE in Error field

    unsigned fragments = 1 + _rx_buffers[_shadow_rdh0].rsc_number;
    p.fragments        = 1 + fragments;
    logf("RX %2u fragments", p.fragments);

    assert(p.fragments < Packet::MAX_FRAGMENTS);

    // Fill fragment list backwards.
    unsigned cur_idx    = _shadow_rdh0;
    p.intel82599.rx_idx = cur_idx;

    for (unsigned cur_frag = fragments; cur_frag > 0; cur_frag--) {
      auto &info = _rx_buffers[cur_idx];
      auto &desc = _rx_desc[cur_idx];

      unsigned flen = (desc.lo & RXDESC_LO_PKT_LEN_MASK) >> RXDESC_LO_PKT_LEN_SHIFT;

      p.packet_length            += flen;
      p.fragment_length[cur_frag] = flen;
      p.fragment[cur_frag]        = info.buffer->data;

      logf("Fragment %02u: %p+%x idx %u", cur_frag, info.buffer->data, flen, cur_idx);
      logf("           : not_first %u", info.not_first);

      // First fragment has not_first == false.
      assert(cur_frag != 0 or not info.not_first);
    }

    logf("Packet %u bytes.", unsigned(p.packet_length));

    // XXX Debugging foo
    Ethernet::Header *hdr = (Ethernet::Header *)p.fragment[1];
    if (hdr->src == hdr->dst) {
      logf(std::string("\n") + hexdump(p.fragment[1], p.fragment_length[0]));
      assert(false);
    }

    _shadow_rdh0 = advance_qp(_shadow_rdh0);
    return true;
  }

  void Intel82599Port::mark_done(Packet &p)
  {
    unsigned not_first;
    unsigned idx = p.intel82599.rx_idx;
    do {
      unsigned   next_idx = _rx_buffers[idx].rsc_last;
      rx_buffer *buf      = _rx_buffers[idx].buffer;

      not_first = _rx_buffers[idx].not_first;

      logf("Marking buffer idx %u %p done. not_first %u", idx, buf->data, not_first);
      deadfill(buf->data);

      memset(&_rx_buffers[idx],          0, sizeof(_rx_buffers[0]));

      logf("Enqueue idx %u.", _shadow_rdt0);
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
      logf(status());
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

  Intel82599Port::Intel82599Port(VfioGroup group, int fd,
                                 Switch &sw, std::string name)
    : Intel82599(group, fd, sw.event_fd()),
      Port(sw, name),
      _misc_thread(&Intel82599Port::misc_thread_fn, this),
      _shadow_rdt0(0), _shadow_rdh0(0),
      _shadow_tdt0(0), _shadow_tdh0(0)
  {
    logf("Resetting device.");
    reset();

    memset(_rx_buffers, 0, sizeof(_rx_buffers));

    logf("Queueing RX buffers.");
    // Create initial set of buffers and enqueue them.
    for (unsigned i = 0; i < RX_BUFFERS; i++) {
      rx_buffer *r = alloc_dma_mem<rx_buffer>();
      static_assert(sizeof(r->data) == 4096, "We only support page sized buffers here");

      deadfill(r->data);

      // Enqueue buffer
      _rx_buffers[_shadow_rdt0].buffer = r;
      _rx_desc[_shadow_rdt0] = populate_rx_desc(r->data);

      __atomic_store_n(&_reg[RDT0], _shadow_rdt0 = advance_qp(_shadow_rdt0), __ATOMIC_RELEASE);

    }

    enable();
  }

}


// EOF
