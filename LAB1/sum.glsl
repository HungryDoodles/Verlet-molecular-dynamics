#version 430 core

uniform int N;
uniform int Nrad32;

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

shared vec2 partsum[32];

void main()
{
	uint iy = gl_GlobalInvocationID.y;
	uint ix = gl_LocalInvocationID.x;

	if (iy >= N) return;

	int Nrad1024 = Nrad32 / 32;
	if (Nrad32 % 32 != 0)
		Nrad1024++;

	uint strideOffset = Nrad1024 * ix;

	vec2 accSum = vec2(0.0, 0.0);
	
	for (int i = 0; i < Nrad1024 && i + strideOffset < Nrad32; ++i)
		accSum += forces[(i + strideOffset) + iy * Nrad32];

	partsum[ix] = accSum;
	memoryBarrierShared();

	int offset = 32 >> 1;
#pragma unroll
	for (; offset > 0; offset >>= 1)
	{
		if (ix < offset)
			partsum[ix] += partsum[ix + offset];
		memoryBarrierShared();
	}

	if (ix == 0)
		comps[iy].a = partsum[0];
}