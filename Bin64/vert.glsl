#version 430 core
#define PI 3.1415926535897932384626433832795

uniform float Lx;
uniform float Ly;

const float triSize = 0.004;

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
	uint iPart = gl_VertexID / 3;
	uint iV = gl_VertexID % 3;
	
	vec2 center = vec2((comps[iPart].p.x / Lx) * 2.0 - 1.0, (comps[iPart].p.y / Lx) * 2.0 - 1.0);
	float velSize = (length(comps[iPart].v));
	vec2 dir;
	if (velSize < 1e-7)
		dir = vec2(1.0, 0.0);
	else
		dir = vec2(comps[iPart].v.xy) / velSize;

	float s = sin(2.0 * PI / 3.0 * float(iV));
	float c = cos(2.0 * PI / 3.0 * float(iV));

	mat2 rotMat = mat2(c, -s, s, c);

	gl_Position = vec4((rotMat * dir * triSize) + center, 0.0, 1.0);
};