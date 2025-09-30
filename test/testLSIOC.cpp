/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */

#define USE_TYPED_RSET
#define USE_TYPED_DSET

#include <linux/capability.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <epicsUnitTest.h>
#include <envDefs.h>
#include <alarm.h>
#include <dbUnitTest.h>
#include <testMain.h>
#include <dbAccess.h>
#include <dbChannel.h>
#include <iocsh.h>
#include <epicsEvent.h>

#include "linStat.h"

namespace {
using namespace linStat;

struct ChannelDeleter {
    void operator()(dbChannel *chan) const {
        if(chan)
            dbChannelDelete(chan);
    }
};

struct Chan {
    std::unique_ptr<dbChannel, ChannelDeleter> chan;
    Chan() = default;
    explicit Chan(const char *name)
        :chan(dbChannelCreate(name))
    {
        if(!chan || dbChannelOpen(chan.get())) {
            throw std::runtime_error(SB()<<"Unable to open: "<<name);
        }
    }

    template<typename T>
    T _read(short dbr) {
        T ret;
        if(dbChannelGet(chan.get(), dbr, &ret, nullptr, nullptr, nullptr))
            throw std::runtime_error(SB()<<"Unable to read: "<<dbChannelName(chan));
        return ret;
    }
    double readF64() {
        return this->_read<double>(DBR_DOUBLE);
    }
    int64_t readI64() {
        return this->_read<epicsInt64>(DBR_INT64);
    }
    std::string readStr(size_t max) {
        std::vector<char> buf(max);
        long nReq = buf.size();
        if(dbChannelGet(chan.get(), DBR_CHAR, buf.data(), nullptr, &nReq, nullptr) || nReq<0)
            throw std::runtime_error(SB()<<"Unable to read: "<<dbChannelName(chan));
        return std::string(buf.data(), buf.data()+nReq);
    }
};

void testHost() {
    testDiag("%s", __func__);

    auto mem_max(Chan("LOCALHOST:MEM_MAX").readI64());
    auto mem_free(Chan("LOCALHOST:MEM_FREE").readI64());
    testOk(mem_free >0 && mem_free<=mem_max, "MEM_FREE %ld MAX %ld", mem_free, mem_max);

    auto ncpu(Chan("LOCALHOST:CPU_CNT").readI64());
    testOk(ncpu>0, "ncpu=%ld", ncpu);
}

void testProc() {
    testDiag("%s", __func__);

    auto kernel_vers(Chan("LOCALHOST:KERNEL_VERS.$").readStr(80u));
    testOk(!kernel_vers.empty(), "KERNEL_VERS: %s", kernel_vers.c_str());

    auto pid(Chan("LOCALHOST:PROCESS_ID").readI64());
    testOk(pid>0, "pid=%ld", pid);

    auto fd_max(Chan("LOCALHOST:FD_MAX").readI64());
    testOk(fd_max>0, "fd_max=%ld", fd_max);

    auto fd_cnt(Chan("LOCALHOST:FD_CNT").readI64());
    testOk(fd_cnt>0, "fd_cnt=%ld", fd_cnt);
}

void testEnv() {
    testDiag("%s", __func__);

    testdbGetFieldEqual("LOCALHOST:LOCATION", DBR_STRING, "No where");
    testdbGetFieldEqual("LOCALHOST:ENGINEER", DBR_STRING, "Someone");
    testdbGetFieldEqual("LOCALHOST:CA_RPTR_PORT", DBR_STRING, "5065");
    testdbGetFieldEqual("LOCALHOST:CA_RPTR_PORT.SEVR", DBR_LONG, NO_ALARM);
    iocshCmd("epicsEnvSet EPICS_CA_REPEATER_PORT 9999");
    iocshCmd("dbpf LOCALHOST:CA_RPTR_PORT.PROC 1");
    testdbGetFieldEqual("LOCALHOST:CA_RPTR_PORT", DBR_STRING, "9999");
    testdbGetFieldEqual("LOCALHOST:CA_RPTR_PORT.SEVR", DBR_LONG, NO_ALARM);
    iocshCmd("epicsEnvSet EPICS_CA_REPEATER_PORT \"Really long string!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!x\"");
    iocshCmd("dbpf LOCALHOST:CA_RPTR_PORT.PROC 1");
    testdbGetFieldEqual("LOCALHOST:CA_RPTR_PORT", DBR_STRING, "Really long string!!!!!!!!!!!!!!!!!!...");
    testdbGetFieldEqual("LOCALHOST:CA_RPTR_PORT.SEVR", DBR_LONG, MINOR_ALARM);
    iocshCmd("epicsEnvSet EPICS_CA_REPEATER_PORT \"Really long string!!!!!!!!!!!!!!!!!!!!x\"");
    iocshCmd("dbpf LOCALHOST:CA_RPTR_PORT.PROC 1");
    testdbGetFieldEqual("LOCALHOST:CA_RPTR_PORT", DBR_STRING, "Really long string!!!!!!!!!!!!!!!!!!!!x");
    testdbGetFieldEqual("LOCALHOST:CA_RPTR_PORT.SEVR", DBR_LONG, NO_ALARM);
}

void testFS() {
    testDiag("%s", __func__);

    auto root_size(Chan("LOCALHOST:ROOT:SIZE").readI64());
    auto root_avail(Chan("LOCALHOST:ROOT:AVAIL").readI64());
    testOk(root_avail >0 && root_avail<=root_size, "ROOT AVAIL %ld MAX %ld", root_avail, root_size);
}

void testCapArr() {
    testDiag("%s", __func__);

    using estr = char[MAX_STRING_SIZE];

    testdbPutFieldOk("TST:CAP_EFF", DBR_LONG, 0);
    testdbGetArrFieldEqual("TST:CAP_EFF_N", DBF_STRING, 16, 0, nullptr);

    testdbPutFieldOk("TST:CAP_EFF", DBR_LONG, CAP_TO_MASK(CAP_SYS_ADMIN));
    {
        const estr expect[] = {"SYS_ADMIN"};
        testdbGetArrFieldEqual("TST:CAP_EFF_N", DBF_STRING, 16, NELEMENTS(expect), expect);
    }

    testdbPutFieldOk("TST:CAP_EFF", DBR_LONG, CAP_TO_MASK(CAP_SYS_ADMIN)|CAP_TO_MASK(CAP_IPC_LOCK));
    {
        const estr expect[] = {"IPC_LOCK", "SYS_ADMIN"};
        testdbGetArrFieldEqual("TST:CAP_EFF_N", DBF_STRING, 16, NELEMENTS(expect), expect);
    }
}

} // namespace

extern "C" void testioc_registerRecordDeviceDriver(struct dbBase *);

MAIN(testLSIOC)
{
    testPlan(23);
    testdbPrepare();
    epicsEnvSet("LOCATION", "No where");
    epicsEnvSet("ENGINEER", "Someone");
    testdbReadDatabase("testioc.dbd", nullptr, nullptr);
    testioc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("linStatHost.db", "../../db", "IOC=LOCALHOST");
    testdbReadDatabase("linStatProc.db", "../../db", "IOC=LOCALHOST");
    testdbReadDatabase("linStatNIC.db", "../../db", "IOC=LOCALHOST,NIC=lo");
    testdbReadDatabase("linStatFS.db", "../../db", "P=LOCALHOST:ROOT,DIR=/");
    testdbReadDatabase("caparr.db", "..:.", "IOC=TST");
    iocshCmd("var linStatDebug 5");
    testIocInitOk();
    iocshCmd("dbior drvLinStat 3 > drvLinStat.dbior");
    iocshCmd("dbl > testLSIOC.dbl");
    {
        // tables queued for first scan by PINI=RUNNING
        // put ourselves at the end of the queue to wait for in-progress to complete
        epicsEvent happened;
        auto task(linStat::linStatReactor.submit([&](){
            happened.trigger();
        }));
        happened.wait();
    }
    testHost();
    testProc();
    testEnv();
    testFS();
    testCapArr();
    testIocShutdownOk();
    testdbCleanup();
    return testDone();
}
