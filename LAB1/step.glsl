#version 430 core

uniform int N;
uniform float dt;

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

struct Component
{
	vec2 p;
	vec2 v;
	vec2 a;
};

layout(std430, binding = 2) buffer ComponentsLayout
{
	Component comps[];
};
layout(std430, binding = 3) buffer SumLayout
{
	vec2 forces[];
};

void main()
{
	uint i = gl_GlobalInvocationID.x;

	if (i >= N) return;

	comps[i].v += 0.5 * comps[i].a * dt;
}