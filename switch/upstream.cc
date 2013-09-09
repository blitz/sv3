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

namespace Switch {

  void create_upstream_port(Switch &sw, std::vector<std::string> const &args)
  {
    std::string type = args[0];

    if (type.compare("ixgbe") == 0) {
      if (args.size() != 4)
	throw ConfigurationError("ixgbe needs 3 parameters: vfio device, PCI ID and 1/0 for enabling/disabling TSO and LOR.\n"
				 "Try something like --upstream-port ixgbe,/dev/vfio/8,0000:02:00.0,1\n");

      VfioGroup group(args[1]);
      UNUSED Intel82599Port *device = group.get_device<Intel82599Port, Switch &>(args[2], sw, "upstream", std::stoi(args[3]));

    } else {
      throw ConfigurationError("Unknown upstream port type. We only know 'ixgbe'.\n");
    }
  }

}


// EOF
