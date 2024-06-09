/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <deque>
#include <thread>
#include <mutex>
#include <stdexcept>
#include <map>

#include <pthread.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include <dbDefs.h>
#include <epicsGuard.h>
#include <errlog.h>

#include "nlreact.h"

namespace linStat {
using Guard = epicsGuard<std::mutex>;
using UnGuard = epicsGuardRelease<std::mutex>;

namespace {
struct FD {
    int fd = -1;
    FD() = default;
    explicit FD(int fd) :fd(fd) {
        if(fd<0)
            throw std::runtime_error("Failed to open/create socket");
    }
    FD(const FD&) = delete;
    FD& operator=(const FD&) = delete;
    FD(FD&& o) noexcept
        :fd(o.fd)
    {
        o.fd = -1;
    }
    FD& operator=(FD&& o) noexcept {
        FD().swap(*this);
        swap(o);
        return *this;
    }
    ~FD() {
        if(fd>=0)
            (void)close(fd);
    }
    void swap(FD& o) noexcept {
        std::swap(fd, o.fd);
    }
    void reset(int fd) noexcept {
        FD().swap(*this);
        this->fd = fd;
    }
};
} // namespace

struct Operation {
    uint32_t seq;
    std::function<void(NLMsg&&)> cb;
};

struct Job {
    std::function<void ()> cb;
};

struct Reactor::Pvt {
    std::weak_ptr<Pvt> weak_inner;
    std::mutex lock;

    FD notify; // RX end of pipe
    FD wake; // TX end of pipe
    FD nlsock;
    uint32_t nlpid;
    uint32_t nextseq = 0;
    bool running = false;

    std::deque<std::weak_ptr<Job>> todo;
    std::deque<NLMsg> ready;
    std::map<uint32_t, std::weak_ptr<Operation>> handlers;

    std::thread worker;

    Pvt();
    ~Pvt();
    void poke();
    void start();
    void stop();
    void run();
};

Reactor Reactor::create()
{
    auto inner(std::make_shared<Pvt>());

    Reactor ret;
    ret.pvt.reset(inner.get(), [inner](Pvt*) mutable {
        auto pvt(std::move(inner));
        pvt->stop();
    });
    return ret;
}

Reactor::~Reactor() {}

Reactor::Pvt::Pvt()
{
    {
        int fds[2];
        if(auto err = pipe(fds)) {
            (void)err;
            throw std::logic_error("Unable to allocate reator pipe");
        }
        notify.reset(fds[0]);
        wake.reset(fds[1]);
    }

    // create non-blocking rtnetlink socket
    nlsock.reset(socket(AF_NETLINK, SOCK_DGRAM|SOCK_CLOEXEC|SOCK_NONBLOCK, NETLINK_ROUTE));

    {
        sockaddr_nl addr{};
        addr.nl_family = AF_NETLINK;
        if(bind(nlsock.fd, (const sockaddr*)&addr, sizeof(addr))) {
            throw std::logic_error("Unable to bind NL socket");
        }
        socklen_t alen = sizeof(addr);
        if(getsockname(nlsock.fd, (sockaddr*)&addr, &alen)) {
            throw std::logic_error("Unable to getsockname NL socket");
        }
        nlpid = addr.nl_pid;
    }
}

Reactor::Pvt::~Pvt()
{
    assert(!running);
}

void Reactor::Pvt::start() {
    Guard G(lock);
    if(running)
        return;
    worker = std::thread([this](){ run(); });
    running = true;
}

void Reactor::Pvt::stop() {
    std::thread junk;
    {
        Guard G(lock);
        if(!running)
            return;
        running = false;
    }
    poke();
    worker.join();
}

void Reactor::Pvt::poke()
{
    char msg = '!';
    auto ret = write(wake.fd, &msg, 1);
    if(ret!=1)
        throw std::logic_error("Missed reactor wakeup");
}

void Reactor::Pvt::run()
{
#ifdef SCHED_IDLE
    {
        // Set idle scheduler for this thread.  Very lowest priority.
        struct sched_param param{};
        param.sched_priority = 0;
        if(int ret = pthread_setschedparam(pthread_self(), SCHED_IDLE, &param)) {
            errlogPrintf("%s " ERL_WARNING " fails to set SCHED_IDLE: %d\n", __func__, ret);
        }
    }
#else
#  warning No SCHED_IDLE
#endif

    pollfd PFD[2] = {
        {notify.fd, POLLIN, 0},
        {nlsock.fd, POLLIN, 0},
    };

    Guard G(lock);
    while(running || !todo.empty()) {
        PFD[0].revents = PFD[1].revents = 0; // paranoia...

        if(ready.empty())
            PFD[1].events &= ~POLLOUT;
        else
            PFD[1].events |= POLLOUT;

        {
            UnGuard U(G);
            auto ret = poll(PFD, NELEMENTS(PFD), todo.empty() ? -1 : 0);
            if(ret<0) {
                auto err = errno;
                if(err!=EINTR) {
                    errlogPrintf(ERL_ERROR " Reactor poll() error: %d\n", err);
                    break;
                }
            }
        }

        {
            auto jobs(std::move(todo));
            for(auto cjob : jobs) {
                if(auto job = cjob.lock()) {
                    try {
                        UnGuard U(G);
                        job->cb();
                    } catch(std::exception& e){
                        errlogPrintf(ERL_ERROR " Unhandled error in job : %s\n", e.what());
                    }
                }
            }
        }

        if(PFD[0].revents&POLLIN) {
            char junk[16];
            auto ret = read(notify.fd, junk, sizeof(junk));
            if(ret<0) {
                errlogPrintf(ERL_ERROR " waiting for reactor wakeup: %d\n", errno);
                PFD[0].revents &= ~POLLIN;
            }
        }

        if(PFD[1].revents&POLLIN) {
            sockaddr_nl src;
            socklen_t srclen = sizeof(src);
            auto msg(std::make_shared<std::vector<char>>(0x7fff));
            // no need to unlock for non-blocking
            auto ret = recvfrom(nlsock.fd, msg->data(), msg->size(), 0, (sockaddr*)&src, &srclen);
            if(ret<0) {
                auto err = errno;
                if(err!=EWOULDBLOCK && err!=EAGAIN) {
                    errlogPrintf(ERL_ERROR " reactor recvfrom() fails %d\n", err);
                    PFD[1].revents &= ~POLLIN;
                }

            } else {
                auto hdr = reinterpret_cast<nlmsghdr*>(msg->data());
                if(!NLMSG_OK(hdr, msg->size())) {
                    errlogPrintf(ERL_ERROR " reactor ignores invalid msg\n");

                } else {
                    if(hdr->nlmsg_pid==nlpid) { // related to a request
                        auto it(handlers.find(hdr->nlmsg_seq));
                        if(it==handlers.end()) {
                            errlogPrintf(ERL_ERROR " Ignoring message for unknown sequence %u\n",
                                         hdr->nlmsg_seq);

                        } else {
                            if(auto H = it->second.lock()) {
                                try {
                                    NLMsg m(msg, hdr); // alias...
                                    UnGuard U(G);
                                    H->cb(std::move(m));
                                } catch(std::exception& e){
                                    errlogPrintf(ERL_ERROR " Unhandled error in handler : %s\n", e.what());
                                }
                            }
                        }

                    } else if(hdr->nlmsg_pid==0) { // from an mcast subscription
                        errlogPrintf(ERL_WARNING " Ignoring mcast message\n");

                    } else {
                        errlogPrintf(ERL_WARNING " Ignoring message with pid %d\n", hdr->nlmsg_pid);
                    }
                }
            }
        }

        if(PFD[1].revents&POLLOUT) {
            while(!ready.empty()) {
                const auto msg = std::move(ready.front());
                ready.pop_front();

                sockaddr_nl dst{};
                dst.nl_family = AF_NETLINK;
                dst.nl_pid = 0; // kernel
                // nlmsg_len includes nlmsghdr length
                // no need to unlock for non-blocking
                auto ret = sendto(nlsock.fd, msg.get(), msg->nlmsg_len, 0, (const sockaddr*)&dst, sizeof(dst));
                if(ret<0) {
                    auto err = errno;
                    if(err!=EWOULDBLOCK && err!=EAGAIN)
                        errlogPrintf(ERL_ERROR " reactor sendto() fails %d\n", err);

                } else if(ret!=msg->nlmsg_len) {
                    errlogPrintf(ERL_ERROR " reactor sendto() incomplete\n");
                }
            }

        }
    }
}

void Reactor::start()
{
    if(!pvt)
        throw std::logic_error("NULL Reactor");

    pvt->start();
}

void Reactor::stop()
{
    if(!pvt)
        throw std::logic_error("NULL Reactor");

    pvt->stop();
}

JobHandle Reactor::submit(std::function<void ()> &&fn) const
{
    if(!pvt)
        throw std::logic_error("NULL Reactor");

    auto inner(std::make_shared<Job>());
    inner->cb = std::move(fn);
    bool wake;
    {
        Guard G(pvt->lock);
        if(!pvt->running)
            throw std::runtime_error("Reactor stopped");

        wake = pvt->todo.empty() && pvt->ready.empty();

        pvt->todo.push_back(inner);
    }
    if(wake)
        pvt->poke();

    std::weak_ptr<Pvt> wpvt;
    JobHandle ret(inner.get(), [wpvt, inner](Job*){
        if(auto pvt = wpvt.lock()) {
            Guard G(pvt->lock);
            inner->cb = nullptr;
        } else {
            // worker already joined, no concurrency
        }
    });

    return ret;
}

Handle Reactor::request(NLMsg &&req, std::function<void (NLMsg &&)> &&fn) const
{
    if(!pvt)
        throw std::logic_error("NULL Reactor");
    auto msg(std::move(req));

    if(msg->nlmsg_len < sizeof(*msg))
        throw std::logic_error("nlmsghdr too small");

    auto inner(std::make_shared<Operation>());
    inner->cb = std::move(fn);
    bool wake;
    {
        Guard G(pvt->lock);

        while(pvt->handlers.find(pvt->nextseq)!=pvt->handlers.end())
            pvt->nextseq++;

        inner->seq = msg->nlmsg_seq = pvt->nextseq++;
        msg->nlmsg_pid = pvt->nlpid;

        pvt->handlers[inner->seq] = inner;
        pvt->ready.push_back(msg);

        wake = pvt->todo.empty() && pvt->ready.empty();

    }
    if(wake)
        pvt->poke();

    std::weak_ptr<Pvt> wpvt;
    Handle ret(inner.get(), [wpvt, inner](Operation*){
        if(auto pvt = wpvt.lock()) {
            Guard G(pvt->lock);
            inner->cb = nullptr;
            pvt->handlers.erase(inner->seq);
        } else {
            // worker already joined, no concurrency
        }
    });

    return ret;
}

} // namespace linStat
