#pragma once

#include "Platform.h"

#if defined(BUILD_DEBUG) || defined(BUILD_DEVELOPER) || defined(BUILD_TEST)
#define Assert(x)                                                             \
    do                                                                        \
    {                                                                         \
        if (!(x))                                                             \
            TriggerBreakpoint();                                              \
    } while (0)
#else
#define DebugTriggerbreakpoint()
#define Assert(x) \
    do            \
    {             \
        0;        \
    } while (0)
#endif

//
//
//

template <typename T>
struct Span {
	imem count;
	T *  data;

	constexpr Span():count(0), data(nullptr) {}
	Span(const T *p, imem n) : count(n), data((T *)p) {}
	template <imem _Count> constexpr Span(const T(&a)[_Count]) : count(_Count), data((T *)a) {}
	inline T &operator[](int64_t index) const { Assert(index < count); return data[index]; }
	inline T *begin() { return data; }
	inline T *end() { return data + count; }
	inline const T *begin() const { return data; }
	inline const T *end() const { return data + count; }
};

//
//
//

#define ArrFmt              "{ %zd, %p }"
#define ArrArg(x)           ((x).count), ((x).data)
#define ArrSizeInBytes(arr) ((arr).count * sizeof(*((arr).data)))
#define StrFmt              "%.*s"
#define StrArg(x)           (int)((x).count), ((x).data)

struct String {
	imem count;
	u8 * data;

	constexpr String(): data(0), count(0) {}
	String(Span<u8> av): count(av.count), data(av.data) {}
	String(Span<char> av): count(av.count), data((u8 *)av.data) {}
	template <imem _Length> constexpr String(const char(&a)[_Length]) : data((u8 *)a), count(_Length - 1) {}
	String(const u8 *_Data, imem _Length): data((u8 *)_Data), count(_Length) {}
	String(const char *_Data, imem _Length): data((u8 *)_Data), count(_Length) {}
	const u8 &operator[](const imem index) const { Assert(index < count); return data[index]; }
	u8 &operator[](const imem index) { Assert(index < count); return data[index]; }
	inline u8 *begin() { return data; }
	inline u8 *end() { return data + count; }
	inline const u8 *begin() const { return data; }
	inline const u8 *end() const { return data + count; }
	operator Span<u8>() { return Span<u8>(data, count); }
};

//
//
//

#define ContactHelper(x, y) x##y
#define ConcatRaw(x, y)     ContactHelper(x, y)

template <typename T>
struct Exit_Scope {
	T lambda;
	Exit_Scope(T lambda): lambda(lambda) {
	}
	~Exit_Scope() {
		lambda();
	}
};
struct Exit_Scope_Help {
	template <typename T>
	Exit_Scope<T> operator+(T t) {
		return t;
	}
};
#define Defer const auto &ConcatRaw(defer__, __LINE__) = Exit_Scope_Help() + [&]()
