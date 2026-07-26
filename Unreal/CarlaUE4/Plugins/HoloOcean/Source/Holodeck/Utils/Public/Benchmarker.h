// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once
#include <chrono>
#include <ctime>

/**
 * 
 */
class HOLODECK_API Benchmarker
{
public:
	// If manual start is false, CalcMs will automatically end your iteration and start the next one.
	Benchmarker(bool manual_start_=false);
	~Benchmarker();

	/**
	  * Start
	  *
	  * Start before the function you want to benchmark
	  */
	void Start();

	/**
	  * End
	  *
	  * End after the function you want to benchmark
	  */
	void End();

	/**
	  * CalculateAvg
	  *
	  * Calculates the total time for the function
	  */
	float CalcMs();

private:
	bool manual_start;
	std::chrono::high_resolution_clock::time_point t_start;
	std::chrono::high_resolution_clock::time_point t_end;
};
