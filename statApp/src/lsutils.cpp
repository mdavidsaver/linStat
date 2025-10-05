
#include <stdexcept>

#include <string.h>

#include "linStat.h"

namespace linStat {
namespace fs = std::filesystem;

bool starts_with(const std::string& inp, const char *prefix)
{
    auto pl = strlen(prefix);
    if(inp.size() < pl)
        return false;
    return memcmp(inp.c_str(), prefix, pl)==0;
}

bool read_file(const fs::path& fname, std::string& out) {
    std::ifstream strm(fname);
    if(!strm.is_open())
        return false;

    return !!(strm>>out);
}

ReadDir::ReadDir(const char *dir)
    :dirFD(opendir(dir))
{
    if(!dirFD) {
        auto err = errno;
        throw std::runtime_error(SB()<<"opendir "<<dir<<" : "<<err);
    }
}
ReadDir::~ReadDir()
{
    (void)closedir(dirFD);
}

bool ReadDir::next()
{
    ent = readdir(dirFD);
    return ent!=NULL;
}

} // namespace linState
