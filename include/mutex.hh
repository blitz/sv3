#pragma once

#include <cstdlib>
#include <pthread.h>

namespace Switch {

  class Mutex {
    pthread_mutex_t _mtx;
  public:

    class Guard {
      pthread_mutex_t &_mtx;
    public:
      Guard(Mutex &mtx) : _mtx(mtx._mtx) {
        if (0 != pthread_mutex_lock(&_mtx))
          abort();
      }

      ~Guard() {
        if (0 != pthread_mutex_unlock(&_mtx))
          abort();
      }
    };


    Mutex() : _mtx(PTHREAD_MUTEX_INITIALIZER) { }
  };

}

// EOF
