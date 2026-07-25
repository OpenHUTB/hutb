// MIT License (c) 2019 BYU PCCL see LICENSE file

#pragma once

/**
  * A simple PID controller.
  */
class HOLODECK_API SimplePID {
public:
	/**
	  * Default Constructor.
	  */
	SimplePID();

	/**
	  * Full Constructor.
	  * Constructs the PID controller with values for P, I, D, and Tau.
	  */
	SimplePID(const float P, const float I, const float D);

	/**
	  * Default Destructor.
	  */
	~SimplePID() = default;

	/**
	  * ComputePID
	  * Computes the control
	  * @param Desired the desired value
	  * @param Current the current value
	  * @param Delta the delta time
	  * @return the PID
	  */
	float ComputePID(float Desired, float Current, float Delta);

	/**
	  * ComputePIDDirect
	  * Computes the PID directly?
	  * @param XC desired
	  * @param X current
	  * @param XDot the gradient (angular velocity?)
	  * @param Delta the change in time
	  * @return force or torque
	  */
	float ComputePIDDirect(float XC, float X, float XDot, float Delta);


	/**
	  * ComputePIDDirect
	  * Computes the PID directly?
	  * @param XC desired
	  * @param X current
	  * @param XDot the gradient (angular velocity?)
	  * @param Delta the change in time
	  * @param bIsAngle ?
	  * @return Force or torque
	  */
	float ComputePIDDirect(float XC, float X, float XDot, float Delta, bool bIsAngle);


	/**
	* SetGains
	*
	* Used for late initialization or a redo
	*/
	void SetGains(const float P, const float I, const float D);

private:
	float Integrator;
	float Differentiator;
	float LastError;
	float LastState;

	float P;
	float I;
	float D;
	float Tau;

	float MyAbs(float I);
};
