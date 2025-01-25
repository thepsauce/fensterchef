#ifndef UTF8_H
#define UTF8_h

#include <stdint.h>

#define U8_SENTINEL (-1)

#define U8_IS_SINGLE(c) (((c)&0x80)==0)
#define U8_IS_LEAD(c) ((uint8_t)((c)-0xc2)<=0x32)
#define U8_IS_TRAIL(c) ((int8_t)(c)<-0x40)

/* Get the length of a codepoint */
#define U8_LENGTH(c) \
    ((uint32_t)(c)<=0x7f ? 1 : \
        ((uint32_t)(c)<=0x7ff ? 2 : \
            ((uint32_t)(c)<=0xd7ff ? 3 : \
                ((uint32_t)(c))<=0xdfff || (uint32_t)(c)>0x10ffff ? 0 : \
                    ((uint32_t)(c)<=0xffff ? 3 : 4) \
            ) \
        ) \
    )

/* Append a valid codepoint to a string */
#define U8_APPEND(s, i, c) do { \
    uint32_t __uc = (c); \
    if (__uc <= 0x7f) { \
        (s)[(i)++] = (uint8_t)__uc; \
    } else { \
        if (__uc<=0x7ff) { \
            (s)[(i)++] = (uint8_t)((__uc>>6)|0xc0); \
        } else { \
            if (__uc <= 0xffff) { \
                (s)[(i)++] = (uint8_t)((__uc>>12)|0xe0); \
            } else { \
                (s)[(i)++] = (uint8_t)((__uc>>18)|0xf0); \
                (s)[(i)++] = (uint8_t)(((__uc>>12)&0x3f)|0x80); \
            } \
            (s)[(i)++] = (uint8_t)(((__uc>>6)&0x3f)|0x80); \
        } \
        (s)[(i)++] = (uint8_t)((__uc&0x3f)|0x80); \
    }

#define U8_LEAD3_T1_BITS "\x20\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x30\x10\x30\x30"
#define U8_IS_VALID_LEAD3_AND_T1(lead, t1) (U8_LEAD3_T1_BITS[(lead)&0xf]&(1<<((uint8_t)(t1)>>5)))

#define U8_LEAD4_T1_BITS "\x00\x00\x00\x00\x00\x00\x00\x00\x1E\x0F\x0F\x0F\x00\x00\x00\x00"
#define U8_IS_VALID_LEAD4_AND_T1(lead, t1) (U8_LEAD4_T1_BITS[(uint8_t)(t1)>>4]&(1<<((lead)&7)))

#define U8_MASK_LEAD_BYTE(lead_byte, count_trail_bytes) ((lead_byte)&=(1<<(6-(count_trail_bytes)))-1)

#define U8_COUNT_TRAIL_BYTES_UNSAFE(lead_byte) \
    (((uint8_t)(lead_byte)>=0xc2)+((uint8_t)(lead_byte)>=0xe0)+((uint8_t)(lead_byte)>=0xf0))

#define U8_NEXT(s, i, n, c) do { \
    const uint8_t *const _s = (const uint8_t*) (s); \
    const size_t _a = sizeof(*(s)); \
    (c) = _s[(i)++ * _a]; \
    if (!U8_IS_SINGLE(c)) { \
        uint8_t __t = 0; \
        if((i)!=(n) && \
            /* fetch/validate/assemble all but last trail byte */ \
            ((c)>=0xe0 ? \
                ((c)<0xf0 ?  /* U+0800..U+FFFF except surrogates */ \
                    U8_LEAD3_T1_BITS[(c)&=0xf]&(1<<((__t=(_s)[(i) * _a])>>5)) && \
                    (__t&=0x3f, 1) \
                :  /* U+10000..U+10FFFF */ \
                    ((c)-=0xf0)<=4 && \
                    U8_LEAD4_T1_BITS[(__t=(_s)[(i) * _a])>>4]&(1<<(c)) && \
                    ((c)=((c)<<6)|(__t&0x3f), ++(i)!=(n)) && \
                    (__t=(_s)[(i) * _a]-0x80)<=0x3f) && \
                /* valid second-to-last trail byte */ \
                ((c)=((c)<<6)|__t, ++(i)!=(n)) \
            :  /* U+0080..U+07FF */ \
                (c)>=0xc2 && ((c)&=0x1f, 1)) && \
            /* last trail byte */ \
            (__t=(_s)[(i) * _a]-0x80)<=0x3f && \
            ((c)=((c)<<6)|__t, ++(i), 1)) { \
        } else { \
            (c) = U8_SENTINEL; \
        } \
    } \
} while (0)

#define U8_PREV(s, i, c) do { \
    const uint8_t *const _s = (const uint8_t*) (s); \
    const size_t _a = sizeof(*(s)); \
    (c) = (uint8_t) _s[--(i) * _a]; \
    if (!U8_IS_SINGLE(c)) { \
        uint8_t __b, __count=1, __shift=6, __j = (i); \
        /* c is a trail byte */ \
        (c)&=0x3f; \
        while (1) { \
            if (__j == 0) { \
                (c) = U8_SENTINEL; \
                break; \
            } \
            __b=(_s)[--__j * _a]; \
            if (U8_IS_LEAD(__b)) { \
                if (U8_COUNT_TRAIL_BYTES_UNSAFE(__b) != __count) { \
                    (c) = U8_SENTINEL; \
                    break; \
                } \
                U8_MASK_LEAD_BYTE(__b, __count); \
                (c) |= (uint32_t)__b<<__shift; \
                (i) = __j; \
                break; \
            } else if (U8_IS_TRAIL(__b)) { \
                if (__count == 3) { \
                    (c) = U8_SENTINEL; \
                    break; \
                } \
                (c) |= (uint32_t)(__b&0x3f)<<__shift; \
                __count++; \
                __shift+=6; \
            } else { \
                (c) = U8_SENTINEL; \
                break; \
            } \
        } \
    } \
} while (0)

#endif
