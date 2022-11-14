
#pragma once
#include "pch.h"

class CTimeCount
{
private:
#ifdef _WIN32
	LARGE_INTEGER m_QPF;
#endif
	uint32_t m_Begin;

private:
	uint32_t getCount();
public:
	CTimeCount();
	virtual ~CTimeCount();
public:
	void ResetBegin();
	uint32_t GetTime();
};

