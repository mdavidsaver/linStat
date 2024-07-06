/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
/* Read sequence-of-tables format use for:
 * - /proc/net/netstat
 * - /proc/net/snmp
 * - /proc/self/net/netstat
 * - /proc/self/net/snmp
 */

#include <sstream>

#include <errlog.h>
#include <epicsThread.h>
#include <epicsString.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "netstat";

struct NetstatTable : public StatTable {
    std::string fname;
    std::vector<std::string> labels; // persist to avoid re-alloc

    explicit NetstatTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
        ,fname(inst)
    {}
    virtual ~NetstatTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        std::ifstream strm(fname);
        if(!strm.is_open()) {
            throw std::runtime_error(SB()<<"Unable to open: "<<fname);
        }

        while(true) {
            // consecutive pairs of lines like:
            //   Tbl: Label1 Label2 ...
            //   Tbl: Value1 Value2 ...
            std::string head, vals;

            if(!std::getline(strm, head) || !std::getline(strm, vals)) {
                if(strm.eof())
                    break;
                throw std::runtime_error("I/O error");
            }

            std::istringstream hstrm(head), vstrm(vals);

            std::string table;
            {
                std::string othert;
                if(!(hstrm>>table) || !(vstrm>>othert) || table!=othert || table.empty() || table.back()!=':')
                    throw std::runtime_error(SB()<<"Ill-formatted table "<<table);
            }

            std::string col;
            uint64_t val;
            while((hstrm>>col) && (vstrm>>val)) {
                tr.set(SB()<<table<<col, val);
            }
            if(hstrm.eof() && vstrm.eof())
                continue;

            throw std::runtime_error(SB()<<"Invalid table values "<<table);
        }
    }
};


} // namespace

DEFINE_TABLE(tblName, NetstatTable)
