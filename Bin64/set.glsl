#version 430 core

uniform int N;
uniform int Nrad32;

layout(local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

struct Component
{
	dvec2 p;
	dvec2 v;
	dvec2 a;
};

layout(std430, binding = 1) buffer ComponentsLayout
{
	Component comps[];
};
layout(std430, binding = 2) buffer SumLayout
{
	dvec2 forces[];
};

shared dvec2 acc[32];

void main() 
{
	uint ix = gl_GlobalInvocationID.x;
	uint iy = gl_GlobalInvocationID.y;
	uint iLoc = gl_LocalInvocation.x;

	if (ix >= Nrad32) return;
	if (iy >= N) return;

	int Nrad1024 = Nrad32 / 32;
	if (Nrad32 % 32 != 0)
		Nrad1024++;

	int radIndex = Nrad32 / Nrad1024;
	int sumoffset = radIndex * Nrad1024;

	dvec2 accSum = dvec2(0.0, 0.0);
	for (int i = 0; i < Nrad1024; ++i)
	{
		int ind = sumoffset + i;
		if (sumoffset + i < Nrad32)
			accSum += forces[sumoffset + i];
		else break;
	}

	acc[iLoc] = accSum;
	barrier();

	uint locN = min(Nrad32 - gl_WorkGroupID.x * gl_WorkGroupSize.x, 32);

	int offset = 32 >> 1;
	for (; offset > 0; offset >> 1)
	{
		if (iLoc < offset)
			acc[iLoc] += acc[iLoc + offset];
		barrier();
	}

	if (iLoc == 0)
		comps[iy].a = acc[0];
}