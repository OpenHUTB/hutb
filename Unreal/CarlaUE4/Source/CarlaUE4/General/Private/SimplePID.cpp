// MIT License (c) 2019 BYU PCCL see LICENSE file

#include "Holodeck.h"
#include "SimplePID.h"

SimplePID::SimplePID() {
	Integrator = 0.0f;
	Differentiator = 0.0f;
	LastError = 0.0f;
	LastState = 0.0f;
	Tau = 0.0f; // Connor: What does tau represent?
}

SimplePID::SimplePID(const float P, const float I, const float D)
		: P(P), I(I), D(D), Tau(0.05) {
	Integrator = 0.0f;
	Differentiator = 0.0f;
	LastError = 0.0f;
	LastState = 0.0f;
}

float SimplePID::ComputePID(float Desired, float Current, float Delta) {
	// Get the error
	float Error = Desired - Current;

	// If dt is 0 or the error is massive, that's a probelm
	if (Delta == 0.0 || MyAbs(Error) > 9999999) {
		LastError = Error;
		LastState = Current;
		return 0.0;
	}

	Integrator += Delta / 2.0 * (Error + LastError);

	// Derivative
	// Noise reduction (See "Small Unmanned Aircraft". Chapter 6. Slide 31/33)
	// d/dx w.r.t. error:: differentiator_ = (2*tau_ - dt)/(2*tau_ + dt)*differentiator_ + 2/(2*tau_ + dt)*(error - last_error_);
	if (Delta > 0.0)
		Differentiator = (2.0 * Tau - Delta) / (2.0 * Tau + Delta) * Differentiator + 2.0 / (2.0 * Tau + Delta) * (Current - LastState);

	LastError = Error;
	LastState = Current;

	// Note the negative der. term.  This is because now the differentiator is in the feedback loop rather than the forward loop
	return P * Error + I * Integrator - D * Differentiator;
}

float SimplePID::ComputePIDDirect(float XC, float X, float XDot, float Delta) {
	//XC is the desired, X is the current
	float Error = XC - X;

	if (Delta == 0 || MyAbs(Error) > 9999999) {
		LastError = Error;
		LastState = X;
	}

	Integrator += Delta / 2.0 * (Error + LastError);

	LastError = Error;
	LastState = X;
	return P * Error + I * Integrator - D * XDot;
}

float SimplePID::ComputePIDDirect(float XC, float X, float XDot, float Delta, bool bIsAngle) {
	// XC is the desired, X is the current
	float Error = XC - X;

	if (bIsAngle) {
		if (Error >= PI) 
			Error -= (2.0 * PI);
		else if (Error <= -PI)
			Error += (2 * PI);
	}

	if (Delta == 0 || MyAbs(Error) > 9999999) {
		LastError = Error;
		LastState = X;
	}

	Integrator += Delta / 2.0 * (Error + LastError);
	LastError = Error;
	LastState = X;

	return P * Error + I * Integrator - D * XDot;
}

void SimplePID::SetGains(const float NewP, const float NewI, const float NewD) {
	P = NewP;
	I = NewI;
	D = NewD;

	Tau = 0.05; // time for force to go from 0 to 63.2% of maximum desired force
	return;
}

float SimplePID::MyAbs(float x) {
	if (x < 0)
		return -x;
	else
		return x;
}
