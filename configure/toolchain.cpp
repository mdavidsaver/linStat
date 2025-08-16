#ifdef _COMMENT_
/* Compiler / stdc++ introspection
 *
 * Will post-pre-process to filter on "^LINSTAT_" to become Makefile snippet
 */
#endif

#include <algorithm>

LINSTAT_HAVE_TOOLCHAIN = YES

#ifdef _COMMENT_
/* libstdc++ circa GCC 8.x includes preliminary c++17 support, with some quirks.
 * Specifically, std::filesystem support is placed in a seperate library libstdc++fs.a
 *
 * https://gcc.gnu.org/onlinedocs/libstdc++/manual/status.html#status.iso.2017
 */
#endif
#if defined(_GLIBCXX_RELEASE) && _GLIBCXX_RELEASE==8
LINSTAT_NEED_STDCXXFS = YES
#endif
