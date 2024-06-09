/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#include <thread>
#include <condition_variable>
#include <list>

#include <pthread.h>

#include <initHooks.h>
#include <epicsExit.h>
#include <epicsAssert.h>
#include <epicsThread.h>
#include <errlog.h>

#include "linStat.h"

// need at least one entry in section so that LD will emit __start_* and __stop_*
static
__attribute__((section("linStatTableFactory"), retain, used))
const linStat::StatTableFactory tblDummy = {
    nullptr,
    nullptr,
};

extern "C" {
extern const char __start_linStatTableFactory;
extern const char __stop_linStatTableFactory;
}

namespace linStat {

StatTable::~StatTable() {}

namespace {

struct Gbl {
    std::mutex lock;
    std::map<std::pair<std::string, std::string>, std::weak_ptr<StatTable>> tables;
    ~Gbl() {
        decltype(tables) junk;
        {
            Guard G(lock);
            junk = std::move(tables);
        }
    }
};
static Gbl gbl;

} // namespace

const StatTableFactory* StatTableIter::begin() const
{
    return reinterpret_cast<const linStat::StatTableFactory*>(&__start_linStatTableFactory);
}

const StatTableFactory *StatTableIter::end() const
{
    return reinterpret_cast<const linStat::StatTableFactory*>(&__stop_linStatTableFactory);
}

Transaction::~Transaction() {
    {
        Guard G(tbl.lock);
        tbl.current = std::move(next);
        tbl.currentTime = nextTime;
    }
    scanIoImmediate(tbl.scan, priorityHigh);
    scanIoImmediate(tbl.scan, priorityMedium);
    scanIoImmediate(tbl.scan, priorityLow);
}

void Transaction::set(const std::string& name, int64_t val, const std::string& egu)
{
    auto& ent = next[name];
    ent = std::make_pair(val, egu);
}

void Transaction::set(const std::string& name, const std::string& val)
{
    auto& ent = next[name];
    ent = val;
}

std::vector<std::shared_ptr<StatTable>> StatTable::list_all()
{
    std::vector<std::shared_ptr<StatTable>> ret;
    Guard G(gbl.lock);
    ret.reserve(gbl.tables.size());
    for(const auto& pr : gbl.tables) {
        if(auto tbl = pr.second.lock()) {
            ret.push_back(tbl);
        }
    }
    return ret;
}

std::shared_ptr<StatTable> StatTable::lookup(const std::string &name, const std::string &inst, const Reactor& react)
{
    auto key(std::make_pair(name, inst));

    Guard G(gbl.lock);
    auto it = gbl.tables.find(key);
    if(it!=gbl.tables.end()) {
        if(auto tbl = it->second.lock()) {
            return tbl;
        }
        gbl.tables.erase(it); // TODO: need more prompt cleanup?
    }

    for(auto fact : StatTableIter())
    {
        if(!fact.name || name != fact.name)
            continue;
        if(!fact.create)
            throw std::logic_error(SB()<<"Table factory "<<name<<" missing factory");

        auto tbl(fact.create(inst, react));

        // initial, synchronous, update
        {
            // establish lock order Gbl::lock -> StatTable::lock
            tbl->update();
        }

        gbl.tables.emplace(key, tbl);
        return tbl;
    }

    throw std::runtime_error(SB()<<"Not such table factory: "<<name);
}

} // namespace linStat
