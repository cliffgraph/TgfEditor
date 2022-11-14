
#include "pch.h"
#include "CTimeCount.h"

CTimeCount::CTimeCount()
{
#ifdef _WIN32
	::QueryPerformanceFrequency(&m_QPF);
#endif
	m_Begin = getCount();
	return;
}

CTimeCount::~CTimeCount()
{
	// do nothing
	return;
}

void CTimeCount::ResetBegin()
{
	m_Begin = getCount();
	return;
}

uint32_t CTimeCount::GetTime()
{
	return getCount() - m_Begin;

}

uint32_t CTimeCount::getCount()	// ミリ秒単位
{
#ifdef _WIN32
	LARGE_INTEGER step;
	::QueryPerformanceCounter(&step);
	uint32_t msec = static_cast<uint32_t>(
		(static_cast<double>(step.QuadPart) / m_QPF.QuadPart) * 1000);
	return msec;
#endif
#ifdef	__linux
	struct timespec tmp;
	clock_gettime(CLOCK_MONOTONIC, &tmp);
	uint32_t msec = static_cast<uint32_t>(
		static_cast<uint32_t>(tmp.tv_sec) * 1000ULL
		+ static_cast<uint64_t>(tmp.tv_nsec / 1000000L));	// ナノ単位からミリ単位に。
	return msec;
#endif

}
