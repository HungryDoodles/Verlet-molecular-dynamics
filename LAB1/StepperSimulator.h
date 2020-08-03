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

template<typename real>
struct StepperProperties
{
	int N;
	real Lx, Ly;
	real dt;
	int xWrap, yWrap;

	real particleRadius;
	real particleMass;
	real maxRandV;
	int depenetrationSteps;
	real depenetrationBonus = 0.000001;
	real ATSMultiplier = 0.9;

	int doubleCollisions = 0;
	int tripleCollisions = 0;
	int quadCollisions = 0;
	int doubleCollisionsMax = 0;
	int tripleCollisionsMax = 0;
	int quadCollisionsMax = 0;

	int nAvg;
	int bUseAdaptiveTimeStep = true;
	bool bSimulate = false;
	int bGPUSim = false;
};

template<typename real>
struct CollisionInfo 
{
	Component<real> const* otherComponent;
	Vector2<real> impactPoint;
	Vector2<real> impactNormal;
};

template<typename real>
class StepperSimulator : private StepperProperties<real>, virtual public ISimulator<real> 
{
	std::vector<Component<real>> components0;
	std::vector<Component<real>> components;
	std::map<std::string, real> stats;

	void InitializeConfig(const std::string& configFilename) 
	{
		inipp::Ini<char> ini;
		{
			struct stat buffer;
			if (stat(configFilename.c_str(), &buffer) == 0)
				ini.parse(std::ifstream(configFilename));
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

		InitializeValue("STEPPER", "N", N, 10, ini);
		InitializeValue("STEPPER", "Lx", Lx, real(1.0), ini);
		InitializeValue("STEPPER", "Ly", Ly, real(1.0), ini);
		InitializeValue("STEPPER", "dt", dt, real(0.016667), ini);
		InitializeValue("STEPPER", "xWrap", xWrap, 0, ini);
		InitializeValue("STEPPER", "yWrap", yWrap, 0, ini);
		InitializeValue("STEPPER", "particleRadius", particleRadius, real(0.1), ini);
		InitializeValue("STEPPER", "particleMass", particleMass, real(1.0), ini);
		InitializeValue("STEPPER", "nAvg", nAvg, 5, ini);
		InitializeValue("STEPPER", "bUseAdaptiveTimeStep", bUseAdaptiveTimeStep, 1, ini);
		InitializeValue("STEPPER", "maxRandV", maxRandV, real(1.0), ini);
		InitializeValue("STEPPER", "depenetrationSteps", depenetrationSteps, 5, ini);
		InitializeValue("STEPPER", "depenetrationBonus", depenetrationBonus, real(0.000001), ini);
		InitializeValue("STEPPER", "ATSMultiplier", ATSMultiplier, real(0.9), ini);

		ini.generate(std::ofstream(configFilename));
	}
	void GenerateParticles()
	{
		std::uniform_real_distribution<real> rand(real(0.0), real(1.0));
		std::uniform_real_distribution<real> randdual(real(-1.0), real(1.0));
		std::random_device rdev;
		components0 = components = std::vector<Component<real>>(N);

		for (int i = 0; i < N; ++i)
		{
			Component<real> newComp; 
			newComp.p = { rand(rdev) * Lx * 0.5, rand(rdev) * Ly * 0.5 };
			newComp.v = { randdual(rdev) * maxRandV, randdual(rdev) * maxRandV };
			newComp.a = { 0, 0 };

			components[i] = components0[i] = newComp;
		}
	}

	bool FindAllCollisionsWith(std::vector<CollisionInfo<real>>& outInfo, Component<real>* collider, std::vector<Component<real>>& comps, bool bClearVector = true)
	{
		const real doubleRadiusSqr = particleRadius * particleRadius * 2;
		bool bCollisionFound = false;

		if (bClearVector) outInfo.clear();

		for (int i = 0; i < N; ++i) if (&comps[i] != collider)
		{
			CollisionInfo<real> newCollision;
			Component<real>* other = &comps[i];
			Vector2<real> d = other->p - collider->p;

			if (d.SizeSqr() > doubleRadiusSqr)
				continue;

			newCollision.otherComponent = other;
			newCollision.impactPoint = d * real(0.5);
			newCollision.impactNormal = d.Normalized();

			outInfo.push_back(newCollision);

			bCollisionFound = true;
		}

		if (!xWrap) 
		{
			if (collider->p.x - particleRadius < 0) 
			{
				CollisionInfo<real> newCollision;
				newCollision.otherComponent = nullptr;
				newCollision.impactPoint = Vector2<real>{ 0, collider->p.y };
				newCollision.impactNormal = Vector2<real>{ -1, 0 };

				outInfo.push_back(newCollision);

				bCollisionFound = true;
			}
			else if (collider->p.x + particleRadius > Lx) 
			{
				CollisionInfo<real> newCollision;
				newCollision.otherComponent = nullptr;
				newCollision.impactPoint = Vector2<real>{ Lx, collider->p.y };
				newCollision.impactNormal = Vector2<real>{ 1, 0 };

				outInfo.push_back(newCollision);

				bCollisionFound = true;
			}
		}
		if (!yWrap)
		{
			if (collider->p.y - particleRadius < 0)
			{
				CollisionInfo<real> newCollision;
				newCollision.otherComponent = nullptr;
				newCollision.impactPoint = Vector2<real>{ collider->p.x, 0 };
				newCollision.impactNormal = Vector2<real>{ 0, -1 };

				outInfo.push_back(newCollision);

				bCollisionFound = true;
			}
			else if (collider->p.y + particleRadius > Ly)
			{
				CollisionInfo<real> newCollision;
				newCollision.otherComponent = nullptr;
				newCollision.impactPoint = Vector2<real>{ collider->p.x, Ly };
				newCollision.impactNormal = Vector2<real>{ 0, 1 };

				outInfo.push_back(newCollision);

				bCollisionFound = true;
			}
		}

		return bCollisionFound;
	}
	void Step() 
	{
		for (int i = 0; i < N; ++i)
		{
			components[i].p += components[i].v * dt;

			if (xWrap)
			{
				if (components[i].p.x < 0)
					components[i].p.x = Lx + components[i].p.x;
				if (components[i].p.x > Lx)
					components[i].p.x = components[i].p.x - Lx;
			}
			if (yWrap)
			{
				if (components[i].p.y < 0)
					components[i].p.y = Ly + components[i].p.y;
				if (components[i].p.y > Ly)
					components[i].p.y = components[i].p.y - Ly;
			}
		}
		components0 = components;
	}
	void Interact() 
	{
		std::vector<CollisionInfo<real>> colInfo;
		colInfo.reserve(N - 1);

		for (int i = 0; i < N; ++i) 
		{
			Component<real>* thisComp = &components0[i];
			Vector2<real> cumulativeV = { 0, 0 };

			if (FindAllCollisionsWith(colInfo, thisComp, components0)) 
			{
				int numDirectCollisions = 0;

				for (CollisionInfo<real>& col : colInfo) if (col.otherComponent) 
				{
					Vector2<real> d = thisComp->p - col.otherComponent->p;
					Vector2<real> dv = thisComp->v - col.otherComponent->v;

					if (d * dv > 0) // Ignore separating collisions
						continue;

					if (real ssqr = d.SizeSqr() > 0.000001)
					{
						cumulativeV += (d * dv) / d.SizeSqr() * d;
						numDirectCollisions++;
						break;
					}
				}
				switch(numDirectCollisions)
				{
				case 1: doubleCollisions++;
					break;
				case 2: tripleCollisions++;
					break;
				case 3: quadCollisions++;
					break;
				}
				if (numDirectCollisions != 0) components[i].v -= cumulativeV / sqrt(real(numDirectCollisions));

				for (CollisionInfo<real>& col : colInfo) if (!col.otherComponent)
					components[i].v += -2 * (components[i].v * col.impactNormal) * col.impactNormal;
			}
		}
	}
	void UpdateTimestep() 
	{
		// v * dt < rad
		real maxV = 0;
		for (int i = 0; i < N; ++i)
			maxV = max(maxV, components[i].v.SizeSqr());

		dt = min(particleRadius / (sqrt(maxV) + 0.0001), real(0.01667)) * ATSMultiplier;
	}
	void Depenetrate(std::vector<Component<real>>& comps)
	{
		if (depenetrationSteps <= 0)
			return;

		std::vector<CollisionInfo<real>> colInfo;
		colInfo.reserve(N - 1);

		for (int i = 0; i < N; ++i) 
		{
			Component<real>* thisComp = &comps[i];
			for (int n = 0;
				n < depenetrationSteps && FindAllCollisionsWith(colInfo, thisComp, comps);
				++n)
			{
				for (auto& col : colInfo) 
				{
					if (col.otherComponent)
					{
						Vector2<real> d = col.otherComponent->p - thisComp->p;
						real depenetrationCoef = d.Size() - particleRadius;
						thisComp->p -= col.impactNormal * depenetrationCoef * (depenetrationBonus + 1);
					}
					else
					{
						thisComp->p = col.impactPoint - col.impactNormal * particleRadius * (depenetrationBonus + 1);
					}
				}
			}
		}
	}
	void CollectStats()
	{
		real E = 0;
		real I = 0;

		for (int i = 0; i < N; ++i) 
		{
			E += components[i].v.SizeSqr();
			I += components[i].v.Size();
		}
		E *= particleMass * 0.5;
		I *= particleMass;

		doubleCollisionsMax = max(doubleCollisionsMax, doubleCollisions);
		tripleCollisionsMax = max(tripleCollisionsMax, tripleCollisions);
		quadCollisionsMax = max(quadCollisionsMax, quadCollisions);

		stats["E"] = E;
		stats["I"] = I;
		stats["ColDoubles"] = real(doubleCollisionsMax);
		stats["ColTriples"] = real(tripleCollisionsMax);
		stats["ColQuadruples"] = real(quadCollisionsMax);
	}

public:
	/*Begin ISimulator interface*/
	virtual void Initialize(const std::string& configFilename) 
	{
		InitializeConfig(configFilename);

		GenerateParticles();
		Depenetrate(components0);
		components = components0;
		doubleCollisionsMax = doubleCollisions =
			tripleCollisionsMax = tripleCollisions =
			quadCollisionsMax = quadCollisions = 0;
	}
	virtual void Update() 
	{
		if (bSimulate) 
		{
			for (int i = 0; i < nAvg; ++i)
			{
				Interact();
				Depenetrate(components);
				if (bUseAdaptiveTimeStep) UpdateTimestep();
				Step();
			}
			CollectStats();
			ResetStats();
		}

	}
	virtual void ResetStats() 
	{
		doubleCollisions = tripleCollisions = quadCollisions = 0;
	}
	virtual bool GetSimulate() const { return bSimulate; }
	virtual void SetSimulate(bool newSimulate) { bSimulate = newSimulate; }

	virtual int GetN() const { return N; }
	virtual real GetDt() const { return dt; }

	virtual const std::vector<Component<real>>& GetComponents() const override { return components; }
	virtual const std::map<std::string, real>& GetStats() const override { return stats; }
	virtual Vector2<real> GetDims() const { return { Lx, Ly }; }

	virtual void SetGPUSimulation(bool newGPUSim) { /*Unsupported*/ }
	virtual bool GetGPUSimulation() const { return false; }
	/*End ISimulator interface*/
};