#pragma once
#include "Types.h"
#include <string>
#include <map>

template<typename real>
struct ISimulator 
{
	virtual void Initialize(const std::string& configFilename) abstract;
	virtual void Update() abstract;
	virtual void ResetStats() abstract;
	virtual bool GetSimulate() const abstract;
	virtual void SetSimulate(bool newSimulate) abstract;
	virtual int GetN() const abstract;
	virtual real GetDt() const abstract;
	virtual const std::vector<Component<real>>& GetComponents() const abstract;
	virtual const std::map<std::string, real>& GetStats() const abstract;
	virtual Vector2<real> GetDims() const abstract;
	virtual void SetGPUSimulation(bool newGPUSim) abstract;
	virtual bool GetGPUSimulation() const abstract;
	virtual void Draw() {}
};