// 

#include <qpport.hh>
#include <listener.hh>
#include <sv3-client.h>

namespace Switch {

  void QpPort::receive(UNUSED Port &src_port, UNUSED Packet &p)
  {
    Sv3Desc d;
    if (not sv3_queue_dequeue(&_qp->rx, &d))
      // No receive buffers
      return;

    uint8_t  *buf = _session.translate_ptr<uint8_t>(d.buf_ptr);
    unsigned  len = d.len;

    for (unsigned i = 0; i < p.fragments; i++) {
      unsigned chunk = std::min<unsigned>(len, p.fragment_length[i]);
      memcpy(buf, p.fragment[i], chunk);

      buf += chunk;
      len -= chunk;

      if (chunk < p.fragment_length[i])
        // Cropped packet
        break;
    }

    d.type = SV3_DESC_RX_DONE;
    sv3_queue_enqueue(&_qp->done, &d);
  }

  bool QpPort::poll(Packet &p)
  {
    Sv3Queue *tx      = &_qp->tx;
    unsigned oldhead  = tx->head;
    Sv3Desc  d;

    p.packet_length = 0;
    p.fragments     = 0;

    while (sv3_queue_dequeue(tx, &d)) {

      p.fragment[p.fragments]        = _session.translate_ptr<uint8_t>(d.buf_ptr);
      p.fragment_length[p.fragments] = d.len;
      p.packet_length               += d.len;
      p.fragments ++;      
      if (d.type == SV3_DESC_TX_FIN)
        break;

      // assert(d.type == SV3_DESC_TX_CON);
    }

    if (d.type == SV3_DESC_TX_CON)
      // Incomplete DMA program.
      goto backtrack;
      
    return true;
  backtrack:
    tx->head = oldhead;
    return false;
  }

  QpPort::QpPort(Switch &sw, char const *name, Session &session, uint64_t qp)
    : Port(sw, name), _session(session)
  {
    this->_qp = session.translate_ptr<Sv3QueuePair>(qp);
    session._ports.push_front(this);
  }

}

// EOF
