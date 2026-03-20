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
#include <functional>
#include <variant>
#include <filesystem>

#include <stdint.h>

#include <sys/types.h>
#include <dirent.h>

#include <epicsAssert.h>
#include <epicsVersion.h>
#include <epicsTime.h>
#include <callback.h>
#include <dbScan.h>
#include <errlog.h>

#include "nlreact.h"

#include <epicsExport.h>

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
using FltVal = std::pair<double, std::string>;
using Value =  std::variant<std::monostate, std::string, IntVal, FltVal >;

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
    void setf(const std::string& name, double val, const std::string &egu = std::string());
    void set(const std::string& name, const std::string &val);
    void set(const std::string& name);
};

using factoryfn = std::function<std::shared_ptr<StatTable>(const std::string& inst, const Reactor& react)>;

long linStatReport(int level) noexcept;

void addStatTableFactory(const std::string& name,
                         const factoryfn& factory) noexcept;

#define DEFINE_TABLE(name, TblKlass) \
    static \
    void linStat ## TblKlass () noexcept { \
        addStatTableFactory(name, [](const std::string& inst, const Reactor& react) -> std::shared_ptr<StatTable> { \
            return std::make_shared<TblKlass>(inst, react); \
        }); \
    }; \
    extern "C" { epicsExportRegistrar(linStat ## TblKlass); }


// utility
bool starts_with(const std::string& inp, const char *prefix);
bool read_file(const std::filesystem::path& fname, std::string& out);

struct ReadDir final {
    DIR* const dirFD;
    struct dirent *ent;
    ReadDir(const char *dir);
    ReadDir(const std::string& dir) :ReadDir(dir.c_str()) {}
    ~ReadDir();
    bool next();

    const struct dirent* operator->() const { return ent; }
};

} // namespace linStat

#endif // LINSTAT_H
