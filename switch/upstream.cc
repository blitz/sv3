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

#include <cstdio>

#include <exceptions.hh>
#include <upstream.hh>
#include <intel82599.hh>

#include <kvstore.hh>

namespace Switch {

  void create_upstream_port(Switch &sw, std::vector<std::string> const &args)
  {
    std::string type = args[0];

    if (type.compare("ixgbe") == 0) {
      if (args.size() < 3)
	throw ConfigurationError("ixgbe needs parameters.\n"
				 "Try --upstream-port ixgbe,ARG,ARG... where ARG is:\n"
				 "\tdevice=/dev/vfio/N (required)\n"
				 "\tpciid=PCIID (required)\n"
				 "\ttso=0/1\n"
				 );

      KeyValueStore kv = KeyValueStore::create(args.cbegin() + 1, args.cend(), '=');
      KeyValueStore::iterator k;

      if (not kv.has("device") or
	  not kv.has("pciid")) throw ConfigurationError("ixgbe needs at least device= and pciid= parameters.");


      bool tso = 1;		// TCP Segmentation
      if ((k = kv.find("tso"))      != kv.end()) tso      = std::stoi(k->second);

      unsigned irq_rate = 8000;
      if ((k = kv.find("irq_rate")) != kv.end()) irq_rate = std::stoi(k->second);

      VfioGroup group(kv["device"]);
      UNUSED Intel82599Port *device = group.get_device<Intel82599Port, Switch &>(kv["pciid"], sw, "upstream",
										 tso, irq_rate);

    } else {
      throw ConfigurationError("Unknown upstream port type. We only know 'ixgbe'.\n");
    }
  }

}


// EOF
