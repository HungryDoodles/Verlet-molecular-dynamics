#pragma once
#include <memory>
#include <vector>
#include "GL/freeglut.h"

template<typename T>
struct Limits 
{
	T min;
	T max;
};

template<typename T>
struct Vector2 
{
	T x;
	T y;

	__forceinline  T SizeSqr() const { return x*x + y*y; }
	__forceinline  T Size() const { return sqrt(SizeSqr()); }

	__forceinline Vector2<T> Normalized() const
	{
		T size = Size();
		if (size < 1e-6) return Vector2<T>{ 1, 0 };
		return (Vector2<T>{ x / size, y / size } );
	}
	__forceinline Vector2<T>& operator -= (const Vector2<T>& other)
	{
		x -= other.x;
		y -= other.y;
		return *this;
	}
	__forceinline Vector2<T>& operator += (const Vector2<T>& other)
	{
		x += other.x;
		y += other.y;
		return *this;
	}
	__forceinline Vector2<T>& operator *= (T mul)
	{
		x *= mul;
		y *= mul;
		return *this;
	}

	__forceinline static Vector2<T> Cross(const Vector2<T>& a, const Vector2<T>& b)
	{
		return Vector2<T>{a.x * b.y - a.y * b.x};
	}
};

template<typename T>
__forceinline Vector2<T> operator *(T m, const Vector2<T>& v)
{
	return {v.x * m, v.y * m};
}
template<typename T>
__forceinline Vector2<T> operator *(const Vector2<T>& v, T m)
{
	return { v.x * m, v.y * m };
}
template<typename T>
__forceinline Vector2<T> operator /(const Vector2<T>& v, T m)
{
	return { v.x / m, v.y / m };
}
template<typename T>
__forceinline T operator* (const Vector2<T>& a, const Vector2<T>& b)
{
	return a.x * b.x + a.y * b.y;
}

template<typename T>
__forceinline Vector2<T> operator + (const Vector2<T>& a, const Vector2<T>& b)
{
	return{ a.x + b.x, a.y + b.y };
}
template<typename T>
__forceinline Vector2<T> operator - (const Vector2<T>& a, const Vector2<T>& b)
{
	return{ a.x - b.x, a.y - b.y };
}

typedef Vector2<double> Vector2d;
typedef Vector2<float> Vector2f;

template<typename real>
struct Component
{
	Vector2<real> p;
	Vector2<real> v;
	Vector2<real> a;
};

struct GLContext 
{
	HGLRC hgrlc;
	HDC hdc;
};