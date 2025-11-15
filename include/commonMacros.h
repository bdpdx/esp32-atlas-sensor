#pragma once

#ifndef __cplusplus
    #ifndef nullptr
        #define nullptr                     NULL
    #endif
#endif

#define _delete(pointer)                	do { delete pointer; pointer = nullptr; } while (0)
#define _deleteArray(pointer)             	do { delete[] pointer; pointer = nullptr; } while (0)
#define _free(pointer)	                	do { if (pointer) { free((void *) pointer); pointer = nullptr; } } while (0)
#define freeThenAssignFrom(_old, _new)  	do { _free(_old); _old = _new; _new = nullptr; } while (0)
#define _release(pointer)                	do { if (pointer) { pointer->release(); pointer = nullptr; } } while (0)
#define sizeofMember(type, member)      	sizeof(((type *)0)->member)
#define TIME_MAX                            ((time_t)(~(1ull << (sizeof(time_t) * 8 - 1))))
#define zalloc(type)                    	((type *) calloc(1, sizeof(type)))

#define _swap16(n)                      	(((n) << 8) | ((n) >> 8))
#define _swap32(n)                      	(((n) << 24) | ((n) << 8 & 0xff0000) | ((n) >> 8 & 0xff00) | ((n) >> 24))
#define _swap64(n)                      	(((n) << 56) | ((n) << 40 & 0xff000000000000ull) | ((n) << 24 & 0xff0000000000ull) | ((n) << 8 & 0xff00000000ull) | ((n) >> 8 & 0xff000000ull) | ((n) >> 24 & 0xff0000ull) | ((n) >> 40 & 0xff00ull) | ((n) >> 56))

#ifdef __cplusplus
	inline uint16_t                     	swap16(uint16_t n) { return _swap16(n); }
	inline uint32_t                     	swap32(uint32_t n) { return _swap32(n); }
	inline uint64_t                     	swap64(uint64_t n) { return _swap64(n); }
#else
	#define swap16(n)                   	_swap16(n)
	#define swap32(n)	                	_swap32(n)
	#define swap64(n)	                	_swap64(n)
#endif  // __cplusplus

#if BYTE_ORDER == BIG_ENDIAN
	#define big16(n)						(n)
	#define big32(n)						(n)
	#define big64(n)						(n)

	#define little16(n)						swap16((uint16_t)(n))
	#define little32(n)						swap32((uint32_t)(n))
	#define little64(n)						swap64((uint64_t)(n))
#else
	#define big16(n)						swap16((uint16_t)(n))
	#define big32(n)						swap32((uint32_t)(n))
	#define big64(n)						swap64((uint64_t)(n))

	#define little16(n)						(n)
	#define little32(n)						(n)
	#define little64(n)						(n)
#endif // BYTE_ORDER

#ifdef __cplusplus

template<typename t> inline const t &min(const t &lhs, const t &rhs) {
    return lhs < rhs ? lhs : rhs;
}
template<typename t> inline const t &max(const t &lhs, const t &rhs) {
    return lhs > rhs ? lhs : rhs;
}

template<typename t> inline constexpr const t &clamp(const t &value, const t &low, const t &high) {
    return min(max(value, low), high);
}

#endif  // __cplusplus
