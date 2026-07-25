// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "Benchmarker.h"

Benchmarker::Benchmarker(bool manual_start_) : manual_start(manual_start_)
{
	t_start = std::chrono::high_resolution_clock::now();
}

Benchmarker::~Benchmarker()
{
}

void Benchmarker::Start()
{
	t_start = std::chrono::high_resolution_clock::now();
}

void Benchmarker::End()
{
	t_end = std::chrono::high_resolution_clock::now();
}

float Benchmarker::CalcMs()
{
	if(!manual_start) t_end = std::chrono::high_resolution_clock::now();
	float duration = std::chrono::duration<float, std::milli>(t_end - t_start).count();
	if(!manual_start) t_start = std::chrono::high_resolution_clock::now();
	return duration;
}