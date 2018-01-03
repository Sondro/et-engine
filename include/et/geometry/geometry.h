/*
 * This file is part of `et engine`
 * Copyright 2009-2016 by Sergey Reznik
 * Please, modify content only if you know what are you doing.
 *
 */

#pragma once

#include <et/geometry/ray.h>
#include <et/geometry/triangleex.h>
#include <et/geometry/segment2d.h>
#include <et/geometry/segment3d.h>
#include <et/geometry/plane.h>
#include <et/geometry/sphere.h>
#include <et/geometry/equations.h>
#include <et/geometry/boundingbox.h>

namespace et {
typedef Line2d<float> line2d;

typedef Ray2d<float> ray2d;
typedef Ray3d<float> ray3d;

typedef Segment2d<float> segment2d;
typedef Segment3d<float> segment3d;

typedef Triangle<float> triangle;
typedef TriangleEx<float> triangleEx;

typedef Plane<float> plane;

struct RayIntersection
{
	float time = std::numeric_limits<float>::max();
	bool occurred = false;
};

template <typename T>
Triangle<T> operator * (const matrix3<T>& m, const Triangle<T>& t) {
	return Triangle<T>(m * t.v1, m * t.v2, m * t.v3);
}

template <typename T>
Triangle<T> operator * (const matrix4<T>& m, const Triangle<T>& t) {
	return Triangle<T>(m * t.v1(), m * t.v2(), m * t.v3());
}

template <typename T>
inline matrix4<T> orientationForNormal(const vector3<T>& n) {
	vector3<T> up = normalize(n);
	T theta = asin(up.y) - static_cast<T>(HALF_PI);
	T phi = atan2(up.z, up.x) + static_cast<T>(HALF_PI);
	T csTheta = cos(theta);
	vector3<T> side2(csTheta * cos(phi), sin(theta), csTheta * sin(phi));
	vector3<T> side1 = up.cross(side2);

	matrix4<T> result(T(1));
	result[0].xyz() = vector3<T>(side1.x, up.x, side2.x);
	result[1].xyz() = vector3<T>(side1.y, up.y, side2.y);
	result[2].xyz() = vector3<T>(side1.z, up.z, side2.z);
	return result;
}

template <typename T>
inline vector2<T> multiplyWithoutTranslation(const vector2<T>& v, const matrix4<T>& m) {
	return vector2<T>(m[0][0] * v.x + m[1][0] * v.y, m[0][1] * v.x + m[1][1] * v.y);
}

template <typename T>
inline vector2<T> operator * (const matrix4<T>& m, const vector2<T>& v) {
	return vector2<T>(m[0][0] * v.x + m[1][0] * v.y + m[3][0],
		m[0][1] * v.x + m[1][1] * v.y + m[3][1]);
}

quaternion matrixToQuaternion(const mat3& m);
quaternion matrixToQuaternion(const mat4& m);

vec3 removeMatrixScale(mat3& m);
void decomposeMatrix(const mat4& mat, vec3& translation, quaternion& rotation, vec3& scale);

vec3 randVector(float sx = 1.0f, float sy = 1.0f, float sz = 1.0f);

uint32_t randomInteger(uint32_t limit = RAND_MAX);

float randomFloat(float low, float up);
float randomFloat();

float signOrZero(float s);
float signNoZero(float s);

mat4 rotation2DMatrix(float angle);
mat4 transform2DMatrix(float a, const vec2& scale, const vec2& translate);
mat3 rotation2DMatrix3(float angle);

vec3 circleFromPoints(const vec2& p1, const vec2& p2, const vec2& p3);
vec3 perpendicularVector(const vec3&);
vec3 randomVectorOnHemisphere(const vec3& normal, float distributionAngle);
vec3 randomVectorOnDisk(const vec3& normal);
void buildOrthonormalBasis(const vec3& n, vec3& t, vec3& b);

vec3 rotateAroundVector(const vec3& axis, const vec3& v, float);

quaternion quaternionFromAngles(float x, float y, float z);
}
