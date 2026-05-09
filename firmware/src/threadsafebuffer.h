#ifndef EMBD_ENV_THREADSAFEBUFFER_H
#define EMBD_ENV_THREADSAFEBUFFER_H

// Interthread communication utility for USB keylogger
// See https://github.com/gereonsuch/keylogger for more information
// Copyright (C) 2026 Gereon Such

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include <pico/mutex.h>

template<typename T, unsigned capacity>
class ThreadsafeBuffer {
    static_assert(capacity > 0u, "capacity must be greater than 0");

    T _buf[capacity];
    unsigned _n {0}, _i {0};
    
    class PicoMutex {
        mutex_t _m;
    public:
        PicoMutex() { mutex_init(&_m); }
        void lock() { mutex_enter_blocking(&_m); }
        void unlock() { mutex_exit(&_m); }
    };
    mutable PicoMutex _pico_mtx{};

public:

    bool full() const {
        bool r;
        _pico_mtx.lock();
        r = (_n >= capacity);
        _pico_mtx.unlock();
        return r;
    }

    bool empty() const {
        bool r;
        _pico_mtx.lock();
        r = (_n <= 0u);
        _pico_mtx.unlock();
        return r;
    }

    void clear() {
        _pico_mtx.lock();
        _n = 0;
        _i = 0;
        _pico_mtx.unlock();
    }

    /// tries to push the first n items to the buffer.
    /// \returns how many items were actually pushed
    unsigned push(const T* items, unsigned n) {
        unsigned i = 0;
        _pico_mtx.lock();
        for (; i < n && _n < capacity; ++i) {
            unsigned insert_pos = (_i + _n) % capacity;
            _buf[insert_pos] = items[i];
            _n++;
        }
        _pico_mtx.unlock();
        return i;
    }

    bool push(T item) { return push(&item, 1) == 1u; }

    unsigned fetch(T* items, unsigned maxlen) {
        unsigned i = 0;
        _pico_mtx.lock();
        for (;_n > 0u && i < maxlen; ++i) {
            items[i] = _buf[_i];
            _i = (_i + 1u) % capacity;
            _n--;
        }
        _pico_mtx.unlock();
        return i;
    }

    bool fetch(T& item) { return fetch(&item, 1) == 1u; }

    /// Fetches all available items to the given target while calling push_back method on each item
    /// \returns number of items added
    template<class Vector_t>
    unsigned fetchVector(Vector_t& target) {
        unsigned i = 0;
        _pico_mtx.lock();
        for (;_n > 0u; ++i) {
            target.push_back(_buf[_i]);
            _i = (_i + 1u) % capacity;
            _n--;
        }
        _pico_mtx.unlock();
        return i;
    }
};

#endif //EMBD_ENV_THREADSAFEBUFFER_H
