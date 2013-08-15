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
      if (args.size() != 3)
	throw ConfigurationError("ixgbe needs 2 parameters: vfio device and PCI ID.\n"
				 "Try something like --upstream-port ixgbe,/dev/vfio/8,0000:02:00.0\n");

      VfioGroup group(args[1]);
      Intel82599Port *device = group.get_device<Intel82599Port, Switch &>(args[2], sw, "upstream");

    } else {
      throw ConfigurationError("Unknown upstream port type. We only know 'ixgbe'.\n");
    }
  }

}


// EOF
