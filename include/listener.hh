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

//

#pragma once

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>

#include <list>
#include <thread>

#include <switch.hh>
#include <session.hh>

#include <hw/misc/externalpci.h>

namespace Switch {

  class Listener {
    Switch     &_sw;

    int         _sfd;
    sockaddr_un _local_addr;

    std::thread _thread;

    std::list<Session *> _sessions;

    void thread_fun();
    void accept_session();

  public:

    // Create a listening socket for the switch through which it can
    // be controlled by sv3-remote. If force is set, the unix file
    // socket is unlinked prior to creating a new one.
    Listener(Switch &sw, bool force = false);
    ~Listener();
  };

}

// EOF
