#pragma once

template <typename T>
struct Vec2 {
    T x;
    T y;
};

using Vec2f = Vec2<float>;
using Vec2u = Vec2<unsigned int>;

template <typename T>
struct Vec3 {
    T x;
    T y;
    T z;
};

using Vec3f = Vec3<float>;
using Vec3u = Vec3<unsigned int>;

template <typename T>
struct Col3 {
    T r;
    T g;
    T b;
};

using Col3f = Col3<float>;
using Col3u = Col3<unsigned char>;

template <typename T>
struct Col4 {
    T r;
    T g;
    T b;
    T a;
};

using Col4f = Col4<float>;
using Col4u = Col4<unsigned char>;