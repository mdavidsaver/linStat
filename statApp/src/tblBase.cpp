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
#include <epicsStdio.h>

#include "linStat.h"

namespace linStat {

StatTable::~StatTable() {}

namespace {

struct Gbl {
    std::mutex lock;
    std::map<std::string, factoryfn> factories;
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

void addStatTableFactory(const std::string& name,
                         const factoryfn& factory) noexcept
{
    try {
        Guard G(gbl.lock);
        gbl.factories[name] = factory;
    }catch(std::exception& e){
        fprintf(stderr, ERL_ERROR " : Unable to register linStat%s : %s\n", name.c_str(), e.what());
    }
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

void Transaction::setf(const std::string &name, double val, const std::string &egu)
{
    auto& ent = next[name];
    ent = std::make_pair(val, egu);
}

void Transaction::set(const std::string& name, const std::string& val)
{
    auto& ent = next[name];
    ent = val;
}

void Transaction::set(const std::string &name)
{
    next[name] = std::monostate();
}

std::shared_ptr<StatTable> StatTable::lookup(const std::string &name, const std::string &inst, const Reactor& react)
{
    auto key(std::make_pair(name, inst));

    Guard G(gbl.lock);
    {
        auto it = gbl.tables.find(key);
        if(it!=gbl.tables.end()) {
            if(auto tbl = it->second.lock()) {
                return tbl;
            }
            gbl.tables.erase(it); // TODO: need more prompt cleanup?
        }
    }

    std::shared_ptr<StatTable> tbl;
    {
        auto it(gbl.factories.find(name));
        if(it==gbl.factories.end())
            throw std::runtime_error(SB()<<"Not such table factory: "<<name);
        tbl = it->second(inst, react);
    }

    // initial, synchronous, update
    try {
        // establish lock order Gbl::lock -> StatTable::lock
        tbl->update();
        tbl->last_ok = true;
    } catch(std::exception& e){
        errlogPrintf("Tbl %s:%s " ERL_ERROR " first : %s\n",
                     tbl->fact.c_str(),
                     tbl->inst.c_str(),
                     e.what());
        tbl->last_ok = false;
    }

    gbl.tables.emplace(key, tbl);
    return tbl;
}

static
std::vector<std::string> factories_all()
{
    std::vector<std::string> ret;
    Guard G(gbl.lock);
    ret.reserve(gbl.factories.size());
    for(const auto& pr : gbl.factories) {
        ret.push_back(pr.first);
    }
    return ret;
}

static
std::vector<std::shared_ptr<StatTable>> list_all()
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

long linStatReport(int level) noexcept {
    try {
        if(level<=0){
            linStatReactor.report();
            return 0;
        }

        for(const auto& name : factories_all()) {
            epicsStdoutPrintf("  Factory %s\n", name.c_str());
        }

        if(level<=2)
            return 0;

        for(const auto& tbl : list_all()) {
            epicsStdoutPrintf("  Table: %s.%s\n", tbl->fact.c_str(), tbl->inst.c_str());
            Guard G(tbl->lock);
            for(const auto& pr : tbl->current) {
                if(auto val = std::get_if<IntVal>(&pr.second)) {
                    epicsStdoutPrintf("    int64_t %s = %lld %s\n",
                                      pr.first.c_str(),
                                      (long long)val->first,
                                      val->second.c_str());
                } else if(auto val = std::get_if<FltVal>(&pr.second)) {
                    epicsStdoutPrintf("    double %s = %g %s\n",
                                      pr.first.c_str(),
                                      val->first,
                                      val->second.c_str());
                } else if(auto val = std::get_if<std::string>(&pr.second)) {
                    epicsStdoutPrintf("    string %s = %s\n",
                                      pr.first.c_str(),
                                      val->c_str());
                } else {
                    epicsStdoutPrintf("    ?????? %s\n",
                                      pr.first.c_str());
                }
            }
        }

        return 0;
    }catch(std::exception& e){
        epicsStdoutPrintf(ERL_ERROR " : %s\n", e.what());
        return -1;
    }
}


} // namespace linStat
