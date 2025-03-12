#ifndef UTF8_H
#define UTF8_H

/* Thanks for <unicode/utf8.h> for shitting into my code but these macros are
 * very nice and efficient.
 */

#include <stdint.h>

typedef uint8_t utf8_t;

#define U8_SENTINEL (-1)

#define U8_IS_SINGLE(c) (((c)&0x80)==0)
#define U8_IS_LEAD(c) ((utf8_t)((c)-0xc2)<=0x32)
#define U8_IS_TRAIL(c) ((int8_t)(c)<-0x40)

/* Get the length of a codepoint. */
#define U8_LENGTH(c) \
    ((uint32_t)(c)<=0x7f ? 1 : \
        ((uint32_t)(c)<=0x7ff ? 2 : \
            ((uint32_t)(c)<=0xd7ff ? 3 : \
                ((uint32_t)(c))<=0xdfff || (uint32_t)(c)>0x10ffff ? 0 : \
                    ((uint32_t)(c)<=0xffff ? 3 : 4) \
            ) \
        ) \
    )

/* Append a valid codepoint to a string. */
#define U8_APPEND(s, i, c) do { \
    const uint32_t uc_ = (c); \
    if (uc_ <= 0x7f) { \
        (s)[(i)++] = (utf8_t)uc_; \
    } else { \
        if (uc_<=0x7ff) { \
            (s)[(i)++] = (utf8_t)((uc_>>6)|0xc0); \
        } else { \
            if (uc_ <= 0xffff) { \
                (s)[(i)++] = (utf8_t)((uc_>>12)|0xe0); \
            } else { \
                (s)[(i)++] = (utf8_t)((uc_>>18)|0xf0); \
                (s)[(i)++] = (utf8_t)(((uc_>>12)&0x3f)|0x80); \
            } \
            (s)[(i)++] = (utf8_t)(((uc_>>6)&0x3f)|0x80); \
        } \
        (s)[(i)++] = (utf8_t)((uc_&0x3f)|0x80); \
    }

#define U8_LEAD3_T1_BITS "\x20\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x10\x30\x30"
#define U8_IS_VALID_LEAD3_AND_T1(lead, t1) (U8_LEAD3_T1_BITS[(lead)&0xf]&(1<<((utf8_t)(t1)>>5)))

#define U8_LEAD4_T1_BITS "\x00\x00\x00\x00\x00\x00\x00\x00\x1E\x0F\x0F\x0F\x00\x00\x00\x00"
#define U8_IS_VALID_LEAD4_AND_T1(lead, t1) (U8_LEAD4_T1_BITS[(utf8_t)(t1)>>4]&(1<<((lead)&7)))

#define U8_MASK_LEAD_BYTE(lead_byte, count_trail_bytes) ((lead_byte)&=(1<<(6-(count_trail_bytes)))-1)

#define U8_COUNT_TRAIL_BYTES_UNSAFE(lead_byte) \
    (((utf8_t)(lead_byte)>=0xc2)+((utf8_t)(lead_byte)>=0xe0)+((utf8_t)(lead_byte)>=0xf0))

/* Move @i to the position after the current glyph. */
#define U8_NEXT(s, i, n, c) do { \
    const utf8_t *const s_ = (const utf8_t*) (s); \
    const size_t a_ = sizeof(*(s)); \
    (c) = s_[(i)++ * a_]; \
    if (!U8_IS_SINGLE(c)) { \
        utf8_t t_ = 0; \
        if((i)!=(n) && \
            /* fetch/validate/assemble all but last trail byte */ \
            ((c)>=0xe0 ? \
                ((c)<0xf0 ?  /* U+0800..U+FFFF except surrogates */ \
                    U8_LEAD3_T1_BITS[(c)&=0xf]&(1<<((t_=(s_)[(i) * a_])>>5)) && \
                    (t_&=0x3f, 1) : \
                   /* U+10000..U+10FFFF */ \
                    ((c)-=0xf0)<=4 && \
                    U8_LEAD4_T1_BITS[(t_=(s_)[(i) * a_])>>4]&(1<<(c)) && \
                    ((c)=((c)<<6)|(t_&0x3f), ++(i)!=(n)) && \
                    (t_=(s_)[(i) * a_]-0x80)<=0x3f) && \
                /* valid second-to-last trail byte */ \
                ((c)=((c)<<6)|t_, ++(i)!=(n)) : \
               /* U+0080..U+07FF */ \
                (c)>=0xc2 && ((c)&=0x1f, 1)) && \
            /* last trail byte */ \
            (t_=(s_)[(i) * a_]-0x80)<=0x3f && \
            ((c)=((c)<<6)|t_, ++(i), 1)) { \
        } else { \
            (c) = U8_SENTINEL; \
        } \
    } \
} while (0)

/* Move @i to the position before the current glyph. */
#define U8_PREVIOUS(s, i, c) do { \
    const utf8_t *const s_ = (const utf8_t*) (s); \
    const size_t a_ = sizeof(*(s)); \
    (c) = (utf8_t) s_[--(i) * a_]; \
    if (!U8_IS_SINGLE(c)) { \
        utf8_t b_, count_=1, shift_=6, j_ = (i); \
        /* c is a trail byte */ \
        (c)&=0x3f; \
        while (1) { \
            if (j_ == 0) { \
                (c) = U8_SENTINEL; \
                break; \
            } \
            b_=(s_)[--j_ * a_]; \
            if (U8_IS_LEAD(b_)) { \
                if (U8_COUNT_TRAIL_BYTES_UNSAFE(b_) != count_) { \
                    (c) = U8_SENTINEL; \
                    break; \
                } \
                U8_MASK_LEAD_BYTE(b_, count_); \
                (c) |= (uint32_t)b_<<shift_; \
                (i) = j_; \
                break; \
            } else if (U8_IS_TRAIL(b_)) { \
                if (count_ == 3) { \
                    (c) = U8_SENTINEL; \
                    break; \
                } \
                (c) |= (uint32_t)(b_&0x3f)<<shift_; \
                count_++; \
                shift_+=6; \
            } else { \
                (c) = U8_SENTINEL; \
                break; \
            } \
        } \
    } \
} while (0)

#endif
