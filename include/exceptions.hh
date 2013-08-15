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

#pragma once

#include <errno.h>
#include <cstring>

#include <string>
#include <sstream>

#include <boost/format.hpp>

namespace Switch {

  class Exception {

    std::string _reason;

    void format_reason(boost::format &msg)
    {
      _reason = msg.str();
    }

    template <typename T, typename... ARGS>
    void format_reason(boost::format &msg, T first, ARGS... rest)
    {
      msg % first;
      format_reason(msg, rest...);
    }

  public:
    virtual std::string reason() const { return _reason; }

    template <typename... ARGS>
    explicit Exception(std::string reason, ARGS... param)
    {
      boost::format msg(reason);
      format_reason(msg, param...);
    }

  };

  class ConfigurationError : public Exception {
  public:
    using Exception::Exception;
  };

  class SystemError : public Exception {
    int _errno;
  public:

    std::string reason() const override
    {
      std::stringstream ss;
      char buf[128];
      char const *msg = buf;
#ifdef _GNU_SOURCE
      // Fuck you, Ulrich Drepper. See the Linux man page on
      // strerror_r for why this is necessary. _GNU_SOURCE is always
      // defined by libstdc++ on Linux!
      msg =
#endif
      strerror_r(_errno, buf, sizeof(buf));

      ss << Exception::reason();
      ss << "\nError " << errno << ": "
	 << msg << "\n";
      return ss.str();
    }

    template <typename... ARGS>
    SystemError(std::string reason, ARGS... param) : Exception(reason, param...), _errno(errno)
    { }
  };

}

// EOF
