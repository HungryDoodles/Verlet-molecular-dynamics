#version 430 core

uniform double Lx;
uniform double Ly;
uniform double dt;
uniform int N;
uniform int Nrad32; 
uniform double sigma;
uniform double epsilon;

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
// Y --> this comp
// X --> other comp
shared Component compsFetch[32];
shared dvec2 forces[32];


dvec2 Accel(uint iComp, Component ci)
{
	Component cj = compsFetch[iComp];
	dvec2 d = ci.p - cj.p;

	//Separation(d, Vector2r{ Lx, Ly });
	if (abs(d.x) > 0.5 * Lx)
		d.x *= 1.0 - Lx / abs(d.x);
	if (abs(d.y) > 0.5 * Ly)
		d.y *= 1.0 - Ly / abs(d.y);

	double r = length(d);

	//F(r, force, potential);
	double rinv = sigma / r;
	double r3 = rinv * rinv * rinv;
	double r6 = r3 * r3;
	double g = 24.0 * rinv * r6 * (2.0 * r6 - 1.0);
	double force = g * rinv; 

	return force * d;
}


void main() 
{
	uint ix = gl_GlobalInvocationID.x;
	uint iy = gl_GlobalInvocationID.y;
	uint iLoc = gl_LocalInvocationID.x;

	if (ix >= N) return;
	if (iy >= N) return;

	Component thisComp = comps[iy];

	compsFetch[iLoc] = comps[ix];
	barrier();

	if (ix != iy)
		forces[iLoc] = Accel(iLoc, thisComp); // forces RECEIVED from iComp
	else
		forces[iLoc] = 0;
	barrier();

	uint locN = min(N - gl_WorkGroupID.x * gl_WorkGroupSize.x, 32);

	int offset = 32 >> 1;
	for (; offset > 0; offset >> 1) 
	{
		if (iLoc < offset && iLoc + offset < locN)
			forces[iLoc] += forces[iLoc + offset];
		barrier();
	}

	if (iLoc == 0)
		forces[Nrad32 * iy + gl_WorkGroupID.x] = forces[0];
}