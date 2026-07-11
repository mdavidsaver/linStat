/**
 * Copyright - See the COPYRIGHT that is included with this distribution.
 * linStat is distributed subject to a Software License Agreement found
 * in file LICENSE that is included with this distribution.
 */
// Read RTC status.  "/proc/driver/rtc"

#include <algorithm>
#include <fstream>
#include <regex>

#include <time.h>

#include "linStat.h"

namespace {
using namespace linStat;

const char * const tblName = "rtc";

struct RTCTable : public StatTable {
    explicit RTCTable(const std::string& inst, const Reactor& react)
        :StatTable(tblName, inst, react)
    {}
    virtual ~RTCTable() {}

    virtual void update() override final {
        Transaction tr(*this);

        std::ifstream strm(inst);
        if(!strm.is_open()) {
            throw std::runtime_error(SB()<<"Unable to open: "<<inst);
        }

        tr.set("battery", 0);

        struct tm rtctm{};
        bool haveDate = false, haveTime = false;
        std::string line;
        while(std::getline(strm, line)) {
            // lines like:
            //   Key : Value
            static const std::regex expr(R"(^(\S+)\s*:\s*(\S+)\s*$)");

            std::smatch M;
            if(!std::regex_match(line, M, expr)) {
                if(linStatDebug>=3)
                    errlogPrintf("RTC Unable to parse:%s\n", line.c_str());
                continue;
            }
            assert(M.size()==3);
            auto key(M[1].str());
            auto val(M[2].str());
            tr.set("rtc:"+key, val);

            if(key=="batt_status") {
                // Linux circa 6.12
                // two drivers report batt_status.  rtc-cmos.c and rtc-isl1208.c
                if(val=="okay") {
                    tr.set("battery", 2);

                } else if(val=="bad" || val=="dead") {
                    tr.set("battery", 1);

                } else {
                    if(linStatDebug>=3)
                        errlogPrintf("RTC Unable to Parse:%s\n", line.c_str());
                    tr.set("battery", 3);
                }

            } else if(key=="rtc_time") {
                // rtc_time        : 16:05:52
                if(*strptime(val.c_str(), "%H:%M:%S", &rtctm)) {
                    if(linStatDebug>=3)
                        errlogPrintf("RTC Unable to Parse:%s\n", line.c_str());
                } else {
                    haveTime = true;
                }

            } else if(key=="rtc_date") {
                // rtc_date        : 2026-07-11
                if(*strptime(val.c_str(), "%Y-%m-%d", &rtctm)) {
                    if(linStatDebug>=3)
                        errlogPrintf("RTC Unable to Parse:%s\n", line.c_str());
                } else {
                    haveDate = true;
                }
            }
        }

        if(haveDate && haveTime) {
            time_t_wrapper sysnow = tr.nextTime;
            // Linux RTC is usually UTC, but can't say for certain.  Might be local TZ
            auto rtcgm(rtctm); // surprise!  timegm() and mktime() will modify the struct tm
            time_t deltagm = std::abs(sysnow.ts - timegm(&rtcgm));
            auto rtcloc(rtctm);
            time_t deltaloc= std::abs(sysnow.ts - mktime(&rtcloc));

            tr.set("ref", deltaloc < deltagm ? 1 : 0);
            tr.set("delta", std::min(deltagm, deltaloc));
        }
    }
};

} // namespace

DEFINE_TABLE(tblName, RTCTable)
