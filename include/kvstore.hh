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

#include <map>

#include <util.hh>

class KeyValueStore : public std::map<std::string, std::string>
{

public:

  bool has(std::string key) const { return find(key) != end(); }

  /// Create a key-value store from a container (list, ...) of
  /// strings, each formed key=value (when delimiter == '=').
  template <typename IT, typename END>
  static KeyValueStore
  create(IT args_iterator, END end, char delimiter) {
    KeyValueStore res;

    for ( ; args_iterator != end ; ++args_iterator) {
      std::stringstream ss(*args_iterator);
      std::string key, value;

      std::getline(ss, key, delimiter);
      std::getline(ss, value);

      res[key] = value;
    }

    return res;
  }

};

// EOF
