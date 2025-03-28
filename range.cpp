#include "cos.h"
#include <functional>
#include <memory>
#include <range.h>
#include <unordered_map>

using std::unordered_map;

static int hash(symbolic& s) { return s.version; }
static int equality(symbolic& s1, symbolic& s2) { return s1.version == s2.version; }

namespace std {
    template<> struct hash<symbolic>
    {
        size_t operator()(const symbolic& s) const
        {
            return s.version;
        }
    };

    template<> struct equal_to<symbolic>
    {
        bool operator()(const symbolic& s1, const symbolic& s2) const
        {
            return s1.version == s2.version;
        }
    };
}



unordered_map<symbolic, range>& get_ranges(inner& inner)
{

    unordered_map<symbolic, range>* ranges = new
        unordered_map<symbolic, range>(10, std::hash<symbolic>(),
        std::equal_to<symbolic>(), std::allocator<unordered_map<symbolic, range>>());

    for(term& t: inner.ands) {
        range& r = range::make_range(t);
        (*ranges)[r.sym] = r;
    }

    return *ranges;
}