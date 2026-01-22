#pragma once
#ifdef _WIN32
#include <stdexcept>
#include <Windows.h>

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		throw std::runtime_error("HRESULT Error");
	}
}
#endif
