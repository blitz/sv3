// -*- Mode: C++ -*-

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cassert>
#include <pcap.h>

#include <header/ethernet.hh>
#include <header/ipv4.hh>
#include <header/tcp.hh>

static void parse_ipv4(IPv4::Header const *ipv4)
{
  assert(ipv4->version == 4);
  printf("ipv4 checksum %s ", ipv4->checksum_ok() ? "ok" : "wrong");
  switch (ipv4->proto) {
  case IPv4::Proto::TCP:
    printf("tcp %s ", ipv4->tcp()->checksum_ok(ipv4) ? "ok" : "wrong");
    break;
  default:
    break;
  }
}

int main(int argc, char **argv)
{
  char err[PCAP_ERRBUF_SIZE];
  pcap_t *handle = pcap_open_offline(argv[1], err);

  if (!handle) { puts(err); return EXIT_FAILURE; }

  struct pcap_pkthdr header;
  uint8_t const     *packet;

  while (nullptr != (packet = pcap_next(handle, &header))) {
    printf("len %4u ", header.len);
    assert(header.len == header.caplen);

    Ethernet::Header const *header = reinterpret_cast<Ethernet::Header const *>(packet);
    // XXX We do not validate buffer lengths!!!
    switch (header->type) {
    case Ethernet::Ethertype::IPV4: 
      parse_ipv4(header->ipv4);
      break;
    case Ethernet::Ethertype::IPV6:
      printf("ipv6 ");
      break;
    }

    puts("");
  }

  return 0;
}

// EOF
