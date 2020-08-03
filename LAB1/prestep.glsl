#version 430 core

uniform int N;
uniform int Nrad32;
uniform float dt;
uniform float Lx;
uniform float Ly;

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

vec2 Periodic(vec2 posin)
{
	vec2 pos = posin;
	if (pos.x < 0)
		pos.x += Lx;
	if (pos.x > Lx)
		pos.x -= Lx;
	if (pos.y < 0)
		pos.y += Ly;
	if (pos.y > Ly)
		pos.y -= Ly;
	return pos;
}

void main()
{
	uint i = gl_GlobalInvocationID.x;

	if (i >= N) return;

	comps[i].p = Periodic(comps[i].p + comps[i].v * dt + 0.5 * comps[i].a * dt * dt);
	//comps[i].p = comps[i].p + vec2(0.0, 0.01);
	comps[i].v += 0.5 * comps[i].a * dt;
}