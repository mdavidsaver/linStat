
#include <fstream>
#include <stdexcept>

#include <string.h>

#include <sys/stat.h>

#include "linStat.h"

namespace linStat {

bool starts_with(const std::string& inp, const char *prefix)
{
    auto pl = strlen(prefix);
    if(inp.size() < pl)
        return false;
    return memcmp(inp.c_str(), prefix, pl)==0;
}

bool read_file(const std::string &fname, std::string& out) {
    std::ifstream strm(fname);
    if(!strm.is_open())
        return false;

    return !!(strm>>out);
}

ReadDir::ReadDir(const std::string& dir)
    :dirname([&]() -> std::string { // ensure trailing '/'
        auto dn(dir);
        if(dn.empty())
            dn = "./";
        else if(dn.back()!='/')
            dn.push_back('/');
        return dn;
    }())
    ,dirFD(opendir(dirname.c_str()))
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

bool ReadDir::next() noexcept
{
    ent = readdir(dirFD);
    if(ent && ent->d_type==DT_LNK) {
        // readdir() does not follow symlinks, fstatat does
        struct stat st{};
        if(fstatat(dirfd(dirFD), ent->d_name, &st, 0)==0) {
            switch(st.st_mode & S_IFMT) {
            case S_IFDIR: ent->d_type = DT_DIR; break;
            case S_IFREG: ent->d_type = DT_REG; break;
            default: break; // unnecessary so far
            }
        }
    }
    return ent!=NULL;
}

std::string ReadDir::path() const
{
    return dirname + filename();
}

} // namespace linState
