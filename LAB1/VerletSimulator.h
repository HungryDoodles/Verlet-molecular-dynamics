#pragma once
#include <iostream>
#include <random>
#include <chrono>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <map>
#include "ISimulator.h"
#include "Types.h"
#include "inipp.h"
#include "IniHelpers.h"
#include <GL/glew.h>
#include <GL/glut.h>
#include "GLHelpers.h"

template<typename real>
struct VerletProperties
{
	int N;
	real Lx, Ly, dt, dt2;
	real virial, xFlux, yFlux, pe, ke, _time;
	real sigma = 1.0;
	real epsilon = 4.0;
	real collisionRadiusThreshold = 0.95;
	real initPoxScale = 0.5;

	int nAvg, nSet;
	int bUseAdaptiveTimeStep = true;
	bool bSimulate = false;
	int bSimulateOnGPU = false;
	int edgeCondition = 0;
	real ATSPathThreshold = 0.00015;
	real explosionProtectionThreshold = 50.0;

	long long collisionsNum = 0;
	long long doubleCollisions = 0;
	long long tripleCollisions = 0;

	int numInBox = 0;

	real cumulativeForce = 0;
};

struct VerletGPUProperties
{
	GLProgram drawProgram;
	GLProgram prestepProgram;
	GLProgram accelProgram;
	GLProgram sumProgram;
	GLProgram stepProgram;

	GLBuffer compSSBO;
	GLBuffer accelSSBO;

	GLuint vao = 0;

	int Nrad32;
};

template<typename real>
class VerletSimulator : private VerletProperties<real>, private VerletGPUProperties, virtual public ISimulator<real>
{
private:
	std::vector<Component<real>> comps;
	std::map<std::string, real> stats;

	std::uniform_real_distribution<real> random = std::uniform_real_distribution<real>(real(-1.0), real(1.0));
	std::random_device rd;

	void InitPosCPU(int nRow, real vMax)
	{
		int nCol = N / nRow + ((N % nRow == 0) ? 0 : 1);
		real ay = Ly / nRow;
		real ax = Lx / nRow;
		real vxSum = 0;
		real vySum = 0;

		int i = 0;

		for (int ix = 0; ix < nCol; ++ix)
			for (int iy = 0; iy < nRow; ++iy)
			{
				comps[i].p.y = ay * (iy + real(0.5)) * initPoxScale;
				comps[i].p.x = ax * (ix + real(0.5)) * initPoxScale;
				vxSum += (comps[i].v.x = random(rd) * vMax);
				vySum += (comps[i].v.y = random(rd) * vMax);
				++i;
				if (i >= N)
					goto cycleBreak;
			}
	cycleBreak:
		return;
	}
	void GPUCleanup() 
	{
		drawProgram.DeleteProgram();
		accelProgram.DeleteProgram();
		sumProgram.DeleteProgram();
		stepProgram.DeleteProgram();
		compSSBO.DeleteBuffer();
		accelSSBO.DeleteBuffer();
		if (vao != 0)
		{
			glDeleteVertexArrays(1, &vao);
			vao = 0;
		}
	}
	void InitGPU() 
	{
		GPUCleanup();

		{
			GLint uboSize = 0;
			glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &uboSize);
			cout << "Max UBO size: " << uboSize << endl;
		}

		Nrad32 = N / 32 + ((N % 32 != 0) ? 1 : 0);

		cout << "Compiling draw program" << endl;
		drawProgram.Initialize("vert.glsl", GL_VERTEX_SHADER, "frag.glsl", GL_FRAGMENT_SHADER, { "Lx", "Ly" });
		glUseProgram(drawProgram.Get());
		//glUniform1d(drawProgram["Lx"], double(Lx));
		//glUniform1d(drawProgram["Ly"], double(Ly));
		glUniform1f(drawProgram["Lx"], float(Lx));
		glUniform1f(drawProgram["Ly"], float(Ly));
		glUseProgram(0);

		cout << "Compiling prestep program" << endl;
		prestepProgram.Initialize("prestep.glsl", GL_COMPUTE_SHADER, { "Lx", "Ly", "dt", "N" });
		glUseProgram(prestepProgram.Get());
		//glUniform1d(prestepProgram["Lx"], double(Lx));
		//glUniform1d(prestepProgram["Ly"], double(Ly));
		glUniform1f(prestepProgram["Lx"], float(Lx));
		glUniform1f(prestepProgram["Ly"], float(Ly));
		glUniform1i(prestepProgram["N"], N);
		glUseProgram(0);

		cout << "Compiling accel program" << endl;
		accelProgram.Initialize("accel.glsl", GL_COMPUTE_SHADER, { "Lx", "Ly", "dt", "N", "Nrad32", "sigma", "epsilon" });
		glUseProgram(accelProgram.Get());
		//glUniform1d(accelProgram["Lx"], double(Lx));
		//glUniform1d(accelProgram["Ly"], double(Ly));
		glUniform1f(accelProgram["Lx"], float(Lx));
		glUniform1f(accelProgram["Ly"], float(Ly));
		glUniform1i(accelProgram["N"], N);
		glUniform1i(accelProgram["Nrad32"], Nrad32);
		//glUniform1d(accelProgram["sigma"], double(sigma));
		//glUniform1d(accelProgram["epsilon"], double(epsilon));
		glUniform1f(accelProgram["sigma"], float(sigma));
		glUniform1f(accelProgram["epsilon"], float(epsilon));
		glUseProgram(0);

		cout << "Compiling sum program" << endl;
		sumProgram.Initialize("sum.glsl", GL_COMPUTE_SHADER, { "N", "Nrad32" });
		glUseProgram(sumProgram.Get());
		glUniform1i(sumProgram["N"], N);
		glUniform1i(sumProgram["Nrad32"], Nrad32);
		glUseProgram(0);

		cout << "Compiling step program" << endl;
		stepProgram.Initialize("step.glsl", GL_COMPUTE_SHADER, { "dt", "N" });
		glUseProgram(stepProgram.Get());
		glUniform1i(stepProgram["N"], N);
		glUseProgram(0);

		//compSSBO.Initialize(GL_SHADER_STORAGE_BUFFER, sizeof(Component<double>) * N, comps.data(), GL_DYNAMIC_COPY, 2);
		compSSBO.Initialize(GL_SHADER_STORAGE_BUFFER, sizeof(Component<float>) * N, comps.data(), GL_DYNAMIC_COPY, 2);
		accelSSBO.Initialize(GL_SHADER_STORAGE_BUFFER, sizeof(Vector2f) * N * Nrad32, nullptr, GL_DYNAMIC_COPY, 3);

		
		glGenVertexArrays(1, &vao);
	}
	void InitializeConfig(const std::string& filename)
	{
		inipp::Ini<char> ini;
		{
			struct stat buffer;
			if (stat(filename.c_str(), &buffer) == 0)
				ini.parse(std::ifstream(filename));
		}
		if (!ini.errors.empty())
		{
			cout << "There were initialization errors" << endl;
			while (!ini.errors.empty())
			{
				cout << ini.errors.back() << endl;
				ini.errors.pop_back();
			}
		}

		InitializeValue("VERLET", "N", N, 10, ini);
		InitializeValue("VERLET", "Lx", Lx, real(1.0), ini);
		InitializeValue("VERLET", "Ly", Ly, real(1.0), ini);
		InitializeValue("VERLET", "dt", dt, real(0.0167), ini);
		InitializeValue("VERLET", "nAvg", nAvg, 4, ini);
		InitializeValue("VERLET", "nSet", nSet, 4, ini);
		InitializeValue("VERLET", "bUseAdaptiveTimeStep", bUseAdaptiveTimeStep, 1, ini);
		InitializeValue("VERLET", "bSimulateOnGPU", bSimulateOnGPU, 0, ini);
		InitializeValue("VERLET", "sigma", sigma, real(sigma), ini);
		InitializeValue("VERLET", "epsilon", epsilon, real(epsilon), ini);
		InitializeValue("VERLET", "collisionRadiusThreshold", collisionRadiusThreshold, real(collisionRadiusThreshold), ini);
		InitializeValue("VERLET", "ATSPathThreshold", ATSPathThreshold, real(0.00015), ini);
		InitializeValue("VERLET", "edgeCondition", edgeCondition, 0, ini);
		InitializeValue("VERLET", "explosionProtectionThreshold", explosionProtectionThreshold, real(explosionProtectionThreshold), ini);

		comps = std::vector<Component<real>>(N);

		int nRow;
		real vMax;
		InitializeValue("VERLET", "nRow", nRow, 2, ini);
		InitializeValue("VERLET", "vMax", vMax, real(0.5), ini);
		InitializeValue("VERLET", "initPoxScale", initPoxScale, real(0.5), ini);

		InitPosCPU(nRow, vMax);
		if (bSimulateOnGPU)
			InitGPU();

		ini.generate(std::ofstream(filename));
	}

	void Transport_HoleInABox(Vector2<real>& P, Vector2<real>& flux,
		Vector2<real>& V, Vector2<real>& L)
	{
		if (P.x < 0) 
		{
			V.x = abs(V.x);
		}
		if (P.x > L.x)
		{
			if (P.x < L.x * 1.05)
			{
				if (!(P.y > 0.25 * L.y && P.y < 0.75 * L.y))
					V.x = -abs(V.x);
			}
			else if (P.x < L.x * 1.1)
			{
				V.x = abs(V.x);
			}
		}

		if (P.y < 0)
		{
			V.y = abs(V.y);
		}
		if (P.y > L.y)
		{
			V.y = -abs(V.y);
		}
	}
	void Transport_HoleInABox_PhaseY(Vector2<real>& P, Vector2<real>& flux,
		Vector2<real>& V, Vector2<real>& L)
	{
		if (P.x < 0)
		{
			V.x = abs(V.x);
		}
		if (P.x > L.x)
		{
			if (P.x < L.x * 1.05)
			{
				if (!(P.y > 0.25 * L.y && P.y < 0.75 * L.y))
					V.x = -abs(V.x);
			}
			else if (P.x < L.x * 1.1)
			{
				V.x = abs(V.x);
			}
		}

		if (P.y < 0)
			P.y += L.y;
		if (P.y > L.y)
			P.y -= L.y;
	}
	void Transport_HoleInABox_NonEuclidean(Vector2<real>& P, Vector2<real>& flux,
		Vector2<real>& V, Vector2<real>& L)
	{
		if (P.x < 0)
			P.x += L.x;
		if (P.x > L.x)
		{
			if (P.x < L.x * 1.05)
			{
				if (!(P.y > 0.25 * L.y && P.y < 0.75 * L.y))
					P.x -= L.x;
			}
			else if (P.x < L.x * 1.1)
			{
				V.x = abs(V.x);
			}
		}

		if (P.y < 0)
			P.y += L.y;
		if (P.y > L.y)
			P.y -= L.y;
	}
	void Transport_PhaseXY(Vector2<real>& P, Vector2<real>& flux,
		Vector2<real>& V, Vector2<real>& L)
	{
		if (P.x < 0)
			P.x += L.x;
		if (P.x > L.x)
			P.x -= L.x;

		if (P.y < 0)
			P.y += L.y;
		if (P.y > L.y)
			P.y -= L.y;
	}
	void Transport_PhaseX(Vector2<real>& P, Vector2<real>& flux,
		Vector2<real>& V, Vector2<real>& L)
	{
		if (P.x < 0)
			P.x += L.x;
		if (P.x > L.x)
			P.x -= L.x;

		if (P.y < 0)
			V.y = abs(V.y);
		if (P.y > L.y)
			V.y = -abs(V.y);
	}
	void Transport_Closed(Vector2<real>& P, Vector2<real>& flux,
		Vector2<real>& V, Vector2<real>& L)
	{
		if (P.x < 0)
			V.x = abs(V.x);
		if (P.x > L.x)
			V.x = -abs(V.x);

		if (P.y < 0)
			V.y = abs(V.y);
		if (P.y > L.y)
			V.y = -abs(V.y);
	}
	void Transport_Tube(Vector2<real>& P, Vector2<real>& flux,
		Vector2<real>& V, Vector2<real>& L)
	{
		const real boxWindow = 0.5;
		const real forcedXSpeed = 10.0;
		const real distancePenalty = 1.5;

		if (P.x < 0)
			V.x = max(abs(V.x), forcedXSpeed);
		if (P.x > L.x)
		{
			if (P.y > 0.5 * (1.0 - boxWindow) * L.y && P.y < 0.5 * (1.0 + boxWindow) * L.y)
			{
				P.y = (P.y - real(0.5) * L.y) / boxWindow + real(0.5) * L.y;
				P.x -= L.x * distancePenalty;
			}
			else
			{
				V.x = -abs(V.x);
			}
		}

		if (P.y < 0)
			V.y = abs(V.y);
		if (P.y > L.y)
			V.y = -abs(V.y);
	}

	void Separation(Vector2<real>& d, Vector2<real>& L)
	{
		switch (edgeCondition) 
		{ case 2: case 3: case 6: return; }

		if (edgeCondition != 4 && std::abs(d.x) > real(0.5) * L.x)
			d.x *= real(1.0) - L.x / std::abs(d.x);
		if (edgeCondition != 1 && std::abs(d.y) > real(0.5) * L.y)
			d.y *= real(1.0) - L.y / std::abs(d.y);
	}
	void F(real& r, real& force, real& potential)
	{
		real rinv = sigma / r;
		real r3 = rinv * rinv * rinv;
		real r6 = r3 * r3;
		real g = real(24.0) * rinv * r6 * (real(2.0) * r6 - real(1.0));
		force = g * rinv;
		potential = epsilon * r6 * (r6 - real(1.0));
	}
	void Accel(Vector2<real>& L, real& pe)
	{
#pragma omp parallel for
		for (int i = 0; i < N; ++i)
			comps[i].a = { 0.0, 0.0 };
		for (int i = 0; i < N - 1; ++i)
			for (int j = i + 1; j < N; ++j)
			{
				Component<real>& ci = comps[i];
				Component<real>& cj = comps[j];
				if (ci.p.x > L.x || cj.p.x > L.x)
					continue;
				Vector2<real> d = ci.p - cj.p;
				Separation(d, Vector2r{ Lx, Ly });
				real r = d.Size();
				real force, potential;
				F(r, force, potential);
				ci.a += force * d;
				cj.a -= force * d;

				if (ci.p.x < Lx && cj.p.x < Lx)
					pe += potential;
			}
	}
	void Verlet()
	{
		for (int i = 0; i < N; ++i)
		{
			Component<real>& c = comps[i];
			Vector2<real> newP = c.p + c.v * dt + real(0.5) * c.a * dt2;
			c.v += real(0.5) * c.a * dt;
			switch (edgeCondition)
			{
			case 0:
				Transport_PhaseXY(newP, Vector2<real>{ xFlux, yFlux }, c.v, Vector2<real>{ Lx, Ly }); break;
			case 1:
				Transport_PhaseX(newP, Vector2<real>{ xFlux, yFlux }, c.v, Vector2<real>{ Lx, Ly }); break;
			case 2:
				Transport_Closed(newP, Vector2<real>{ xFlux, yFlux }, c.v, Vector2<real>{ Lx, Ly }); break;
			case 3:
				Transport_HoleInABox(newP, Vector2<real>{ xFlux, yFlux }, c.v, Vector2<real>{ Lx, Ly }); break;
			case 4:
				Transport_HoleInABox_PhaseY(newP, Vector2<real>{ xFlux, yFlux }, c.v, Vector2<real>{ Lx, Ly }); break;
			case 5:
				Transport_HoleInABox_NonEuclidean(newP, Vector2<real>{ xFlux, yFlux }, c.v, Vector2<real>{ Lx, Ly }); break;
			case 6:
				Transport_Tube(newP, Vector2<real>{ xFlux, yFlux }, c.v, Vector2<real>{ Lx, Ly }); break;
			}
			c.p = newP;
		}
		Accel(Vector2<real>{ Lx, Ly }, pe);

		// Explosion protection
		real maxForce;
		real garbage;
		real r = sigma * explosionProtectionThreshold;
		F(r, maxForce, garbage);
		for (int i = 0; i < N; ++i)
		{
			auto thisForce = comps[i].a;
			if (thisForce.SizeSqr() >= maxForce * maxForce)
				comps[i].a = thisForce.Normalized() * maxForce;
		}

		numInBox = 0;
		for (int i = 0; i < N; ++i)
		{
			Component<real>& c = comps[i];
			c.v += real(0.5) * c.a * dt;
			if (c.p.x < Lx) 
				ke += real(0.5) * c.v.SizeSqr();
			virial += c.p * c.a;
			if (c.p.x < Lx * 1.05)
				++numInBox;
		}
	}
	void AdjustTimeStep()
	{
		real Lmin = min(Lx, Ly);
		real Amax = 0;
		real Vmax = 0;
		for (int i = 0; i < N; ++i)
		{
			Vmax = max(comps[i].v.SizeSqr(), Vmax);
			Amax = max(comps[i].a.SizeSqr(), Amax);
		}
		Amax = sqrt(sqrt(Amax));
		Vmax = sqrt(Vmax);

		dt = (Lmin / (Amax * 2 + Vmax)) * ATSPathThreshold;
		dt2 = dt * dt;
	}
	void CountCollisions() 
	{
		real sigma2 = sigma * sigma;
		real thresh2 = collisionRadiusThreshold * collisionRadiusThreshold;
		for (int i = 0; i < N - 1; ++i)
		{
			int numColls = 0;
			for (int j = i + 1; j < N; j++)
			{
				// sigma^2 >= r^2/thresh^2 = collision
				Vector2r r = comps[i].p - comps[j].p;
				if (sigma2 >= r.SizeSqr() / thresh2)
					numColls++;
			}
			if (numColls > 0)
				collisionsNum++;
			switch (numColls) 
			{
			case 1:
				doubleCollisions++;
				break;
			case 2:
				tripleCollisions++;
				break;
			}
		}
	}
	void UpdateStats() 
	{
		ke /= nAvg;
		pe /= nAvg;
		real E = (pe + ke) / N;
		real T = ke / N;
		real pFlux = ((xFlux / (real(2.0) * Lx)) + (yFlux / (real(2.0) * Ly))) / (dt * nAvg);
		real pvirial = (N * T) / (Lx * Ly) + real(0.5) * virial / (nAvg * Lx * Ly);
		real doubleCollsPerc = real(100) * real(doubleCollisions) / real(collisionsNum);
		real tripleCollsPerc = real(100) * real(tripleCollisions) / real(collisionsNum);

		stats["E"] = E;
		stats["T"] = T;
		stats["pFlux"] = pFlux;
		stats["pvirial"] = pvirial;
		stats["Time"] = real(_time);
		stats["Hits double"] = doubleCollsPerc;
		stats["Hits triple"] = tripleCollsPerc;
		stats["In box"] = real(numInBox);
		
	}
	
	void AccelGPU()
	{
		glUseProgram(accelProgram.Get());
		//glUniform1d(accelProgram["dt"], double(dt));
		glUniform1f(accelProgram["dt"], float(dt));
		glDispatchCompute(N, Nrad32, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(sumProgram.Get());
		glDispatchCompute(1, N, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		glUseProgram(0);
	}
	void PrestepGPU() 
	{
		glUseProgram(prestepProgram.Get());
		//glUniform1d(prestepProgram["dt"], double(dt));
		glUniform1f(prestepProgram["dt"], float(dt));
		glDispatchCompute(Nrad32, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);
	}
	void StepGPU()
	{
		glUseProgram(stepProgram.Get());
		//glUniform1d(stepProgram["dt"], double(dt));
		glUniform1f(stepProgram["dt"], float(dt));
		glDispatchCompute(Nrad32, 1, 1);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		glUseProgram(0);
	}
	void VerletGPU() 
	{
		PrestepGPU();
		AccelGPU();
		StepGPU();
	}

public:
	virtual void Initialize(const std::string& configFilename) override
	{
		InitializeConfig(configFilename);

		pe = 0;
		Accel(Vector2r{ Lx, Ly }, pe);
		_time = 0;
		pe = 0;
		ke = 0;
		xFlux = yFlux = 0;
		virial = 0;
		collisionsNum = doubleCollisions = tripleCollisions = 0;

		if (bSimulateOnGPU)
			AccelGPU();
	}
	virtual void Update() override
	{
		if (bSimulate)
		{
			for (int iAvg = 0; iAvg < nAvg; ++iAvg)
			{
				if (bUseAdaptiveTimeStep)
					AdjustTimeStep();
				if (bSimulateOnGPU) 
				{
					VerletGPU();
				}
				else
				{
					Verlet();
					CountCollisions();
				}
			}
			_time += nAvg * dt;
			UpdateStats();
			ResetStats();
		}
	}
	virtual void ResetStats() override
	{
		ke = pe = 0;
		xFlux = yFlux = 0;
		virial = 0;
	}

	virtual bool GetSimulate() const override { return bSimulate; }
	virtual void SetSimulate(bool newSimulate) override { bSimulate = newSimulate; }

	virtual int GetN() const override { return N; }
	virtual real GetDt() const override { return dt; }

	virtual const std::vector<Component<real>>& GetComponents() const override { return comps; }
	virtual const std::map<std::string, real>& GetStats() const override { return stats; }
	virtual Vector2<real> GetDims() const override { return { Lx, Ly }; }

	VerletProperties<real> GetAsProperties() 
	{ return (VerletProperties<real>)*this; }

	virtual void SetGPUSimulation(bool newGPUSim) 
	{
		bSimulateOnGPU = newGPUSim;
		InitGPU();
	}
	virtual bool GetGPUSimulation() const { return bSimulateOnGPU; }

	virtual void Draw() 
	{
		if (vao != 0)
		{
			glUseProgram(drawProgram.Get());
			glBindVertexArray(vao);
			glDrawArrays(GL_TRIANGLES, 0, N * 3);
			glBindVertexArray(0);
			glUseProgram(0);
		}
	}
};