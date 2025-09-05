/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#ifndef NLREACT_H
#define NLREACT_H

#ifndef __linux__
#  error Linux specific...
#endif

#include <memory>
#include <functional>

#include <errlog.h>

#ifndef ERL_ERROR
#  define ERL_ERROR "ERROR"
#endif
#ifndef ERL_WARNING
#  define ERL_WARNING "WARNING"
#endif

#define LINSTAT_API __attribute__ ((visibility("default")))

extern "C" {
struct nlmsghdr;
} // extern "C"

namespace linStat {

struct Reactor;
struct Job;
struct Operation;
using NLMsg = std::shared_ptr<nlmsghdr>;
using JobHandle = std::shared_ptr<Job>;
using Handle = std::shared_ptr<Operation>;

struct LINSTAT_API Reactor {
    struct Pvt;

    Reactor() = default;
    static
    Reactor create();
    ~Reactor();

    void start();
    void stop();
    JobHandle submit(std::function<void()>&& fn) const;
    Handle request(NLMsg&& req, std::function<void(NLMsg&&)>&& fn) const;

    void report();

private:
    std::shared_ptr<Pvt> pvt;
};

} // namespace linStat

#endif // NLREACT_H
