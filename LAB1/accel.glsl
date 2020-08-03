#version 430 core

uniform float Lx;
uniform float Ly;
uniform float dt;
uniform int N;
uniform int Nrad32; 
uniform float sigma;
uniform float epsilon;

layout(local_size_x = 1, local_size_y = 32, local_size_z = 1) in;

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
// Y --> this comp
// X --> other comp
shared Component compsFetch[32];
shared vec2 forcesSh[32];


void main() 
{
	uint ix = gl_GlobalInvocationID.x;
	uint iy = gl_GlobalInvocationID.y;
	uint iLoc = gl_LocalInvocationID.y;
	uint iGlob = gl_WorkGroupID.y;

	if (ix >= N) return;
	if (iy >= N) return;

	Component thisComp = comps[ix];

	compsFetch[iLoc] = comps[iy];

	if (ix != iy)
	{
		vec2 d = thisComp.p - compsFetch[iLoc].p;

		d = vec2(
			Lx * (fract(d.x / Lx - 0.5) - 0.5),
			Ly * (fract(d.y / Ly - 0.5) - 0.5) );

		float r2 = d.x * d.x + d.y * d.y;
		float sigma2 = sigma * sigma;

		if (r2 < sigma2 * (4.0 * 4.0)) // Ignore everething that's fairly far
		{
			float ri2 = sigma2 / r2;
			float ri6 = ri2 * ri2 * ri2;
			vec2 force = 24.0 * ri2 * ri6 * (2.0 * ri6 - 1.0) * d;

			forcesSh[iLoc] = force;
		}
		else
			forcesSh[iLoc] = vec2(0.0, 0.0);
	}
	else
		forcesSh[iLoc] = vec2(0.0, 0.0);
	memoryBarrierShared();

	int offset = 32 >> 1; 
#pragma unroll
	for (; offset > 0; offset >>= 1) 
	{
		if (iLoc < offset)
			forcesSh[iLoc] += forcesSh[iLoc + offset];
		memoryBarrierShared();
	}

	if (iLoc == 0)
		forces[ix * Nrad32 + iGlob] = forcesSh[0];
}