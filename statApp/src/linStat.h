/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
#ifndef LINSTAT_H
#define LINSTAT_H

#include <map>
#include <atomic>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <sstream>
#include <fstream>
#include <variant>
#include <filesystem>

#include <stdint.h>

#include <epicsAssert.h>
#include <epicsVersion.h>
#include <epicsTime.h>
#include <callback.h>
#include <dbScan.h>
#include <errlog.h>

#include "nlreact.h"

#define GLIBC_VERSION VERSION_INT(__GLIBC__, __GLIBC_MINOR__, 0, 0)

namespace linStat {
using Guard = std::unique_lock<std::mutex>;

extern int linStatDebug;
LINSTAT_API
extern Reactor linStatReactor;

struct SB {
    std::ostringstream strm;
    SB() {}
    operator std::string() const { return strm.str(); }
    std::string str() const { return strm.str(); }
    template<typename T>
    SB& operator<<(const T& i) { strm<<i; return *this; }
};

// (value, egu)
using IntVal = std::pair<int64_t, std::string>;
using Value =  std::variant<std::monostate, std::string, IntVal >;

struct StatTable {
    const std::string fact, inst;
    const Reactor react;
    IOSCANPVT scan;

    std::atomic<bool> queued{false}; // guarded by work-queue lock
    bool last_ok = true; // local to the worker thread

    std::mutex lock; // guards remaining

    std::map<std::string, Value> current;
    epicsTime currentTime;

    explicit StatTable(const std::string& fact, const std::string& inst, const Reactor& react)
        :fact(fact)
        ,inst(inst)
        ,react(react)
    {
        scanIoInit(&scan);
    }
    virtual ~StatTable();

    // caller must hold lock
    virtual void update() {};
    Value& lookup(const std::string& param) {
        auto it(current.find(param));
        if(it==current.end())
            throw std::runtime_error(SB()<<"Not param "<<param);
        return it->second;
    }

    static
    std::vector<std::shared_ptr<StatTable>>
    list_all();
    static
    std::shared_ptr<StatTable>
    lookup(const std::string& name, const std::string& inst, const Reactor &react);
};

struct Transaction {
    StatTable& tbl;
    decltype(tbl.current) next;
    epicsTime nextTime;

    explicit Transaction(StatTable& tbl) :tbl(tbl) {
        nextTime = epicsTime::getCurrent();
    }
    ~Transaction();

    void set(const std::string& name, int64_t val, const std::string &egu = std::string());
    void set(const std::string& name, const std::string &val);
    void set(const std::string& name);
};

struct StatTableFactory {
    const char *name;
    std::shared_ptr<StatTable> (*create)(const std::string&, const Reactor& react);
};

template<typename Tbl>
std::shared_ptr<StatTable> tblFactory(const std::string& inst, const Reactor& react) {
    return std::make_shared<Tbl>(inst, react);
}
#define DEFINE_TABLE(name, TblKlass) \
    static \
    __attribute__((section("linStatTableFactory"), retain, used)) \
    const linStat::StatTableFactory tbl ## TblKlass = { \
        name, \
        &tblFactory<TblKlass>, \
    };


struct StatTableIter {
    const StatTableFactory *begin() const;
    const StatTableFactory* end() const;
};

// utility
bool starts_with(const std::string& inp, const char *prefix);
bool read_file(const std::filesystem::path& fname, std::string& out);

} // namespace linStat

#endif // LINSTAT_H
