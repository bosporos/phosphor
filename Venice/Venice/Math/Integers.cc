//
// project venice
// author Maximilien M. Cura
//

#include <Venice/Math/Integers>

#define vnz_defintop_lit(type, bang)                           \
    vnz::type operator"" bang (unsigned long long i)           \
    {                                                          \
        return { static_cast<vnz::type::UnderlyingType> (i) }; \
    }

// A quick note:
// Pointer literals are uptr/iptr instead of up/ip (which would seem to better
// follow convention) in order to preserve the 'ip' literal for future use for
// networking purposes (i.e. ip addresses)

vnz_defintop_lit (u8, u8);
vnz_defintop_lit (u16, u16);
vnz_defintop_lit (u32, u32);
vnz_defintop_lit (u64, u64);
vnz_defintop_lit (uptr, uptr);
vnz_defintop_lit (usize, uz);

vnz_defintop_lit (i8, i8);
vnz_defintop_lit (i16, i16);
vnz_defintop_lit (i32, i32);
vnz_defintop_lit (i64, i64);
vnz_defintop_lit (iptr, iptr);
vnz_defintop_lit (isize, iz);
