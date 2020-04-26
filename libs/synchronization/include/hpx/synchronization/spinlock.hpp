////////////////////////////////////////////////////////////////////////////////
//  Copyright (c) 2011 Bryce Lelbach
//  Copyright (c) 2011-2020 Hartmut Kaiser
//  Copyright (c) 2014 Thomas Heller
//  Copyright (c) 2008 Peter Dimov
//  Copyright (c) 2018 Patrick Diehl
//
//  SPDX-License-Identifier: BSL-1.0
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
////////////////////////////////////////////////////////////////////////////////

#if !defined(HPX_B3A83B49_92E0_4150_A551_488F9F5E1113)
#define HPX_B3A83B49_92E0_4150_A551_488F9F5E1113

#include <hpx/config.hpp>

#include <hpx/basic_execution/register_locks.hpp>
#include <hpx/basic_execution/this_thread.hpp>
#include <hpx/concurrency/itt_notify.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>

///////////////////////////////////////////////////////////////////////////////
namespace hpx { namespace lcos { namespace local {
    // std::mutex-compatible spinlock class
    struct spinlock
    {
    public:
        HPX_NON_COPYABLE(spinlock);

    private:
        std::atomic<bool> v_;

    public:
        spinlock(char const* const desc = "hpx::lcos::local::spinlock")
          : v_(false)
        {
            HPX_ITT_SYNC_CREATE(this, desc, "");
        }

        ~spinlock()
        {
            HPX_ITT_SYNC_DESTROY(this);
        }

        void lock()
        {
            HPX_ITT_SYNC_PREPARE(this);

            for (;;)
            {
                if (acquire_lock())
                {
                    break;
                }

                for (std::size_t k = 0; is_locked(); ++k)
                {
                    util::detail::yield_k(
                        k, "hpx::lcos::local::spinlock::lock");
                }
            }

            HPX_ITT_SYNC_ACQUIRED(this);
            util::register_lock(this);
        }

        bool try_lock()
        {
            HPX_ITT_SYNC_PREPARE(this);

            bool r = acquire_lock();    //-V707

            if (r)
            {
                HPX_ITT_SYNC_ACQUIRED(this);
                util::register_lock(this);
                return true;
            }

            HPX_ITT_SYNC_CANCEL(this);
            return false;
        }

        void unlock()
        {
            HPX_ITT_SYNC_RELEASING(this);

            relinquish_lock();

            HPX_ITT_SYNC_RELEASED(this);
            util::unregister_lock(this);
        }

    private:
        // returns whether the mutex has been acquired
        bool acquire_lock()
        {
            return !v_.exchange(true, std::memory_order_acquire);
        }

        // relinquish lock
        void relinquish_lock()
        {
            v_.store(false, std::memory_order_release);
        }

        bool is_locked() const
        {
            return v_.load(std::memory_order_relaxed);
        }
    };
}}}    // namespace hpx::lcos::local

#endif    // HPX_B3A83B49_92E0_4150_A551_488F9F5E1113
