#include "Holodeck.h"
#include "Conversion.h"

const bool USE_RHS = true;

FRotator RPYToRotator(float Roll, float Pitch, float Yaw){
	/* Takes right-handed Roll, Pitch, Yaw, and creates left-handed rotation
		about X, Y, Z in the fixed frame, or Z, Y, X about current frame. 
		
	I don't trust UE4 with rotations after I found this:
	https://github.com/EpicGames/UnrealEngine/blob/release/Engine/Source/Runtime/Core/Public/Math/Rotator.h#L484
	(Combining 2 rotations by adding euler angle's *shiver*)
	*/
	FMatrix R;
	
	float SP, SY, SR;
	float CP, CY, CR;
	FMath::SinCos(&SP, &CP, FMath::DegreesToRadians(Pitch));
	FMath::SinCos(&SY, &CY, FMath::DegreesToRadians(Yaw));
	FMath::SinCos(&SR, &CR, FMath::DegreesToRadians(Roll));

	// Column, then row index
	// First (X) Column
	R.M[0][0] = CY*CP;
	R.M[0][1] = CP*SY;
	R.M[0][2] = -SP;

	// Second (Y) Column
	R.M[1][0] = CY*SP*SR - CR*SY;
	R.M[1][1] = CY*CR + SY*SP*SR;
	R.M[2][1] = CP*SR;

	// Third (Z) Column
	R.M[2][0] = SY*SR + CY*CR*SP;
	R.M[2][1] = CR*SY*SP - CY*SR;
	R.M[2][2] = CP*CR;

	// Flip y-axes in and out to make it left-handed
	R.M[0][1] *= -1;
	R.M[1][0] *= -1;
	R.M[2][1] *= -1;
	R.M[1][2] *= -1;
	
	return R.Rotator();
}

FVector RotatorToRPY(FRotator Rot){
	/* Takes left-handed rotation about X, Y, Z in the fixed frame, 
		or Z, Y, X about current frame and creates a right-handed Roll, Pitch, Yaw.
		
		Taken from here: http://eecs.qmul.ac.uk/~gslabaugh/publications/euler.pdf
	*/
	FMatrix R = FRotationMatrix::Make(Rot);

	// Flip y-axes in and out to make it right-handed
	R.M[0][1] *= -1;
	R.M[1][0] *= -1;
	R.M[2][1] *= -1;
	R.M[1][2] *= -1;

	float R31 = R.M[0][2];
	float R12 = R.M[1][0];
	float R13 = R.M[2][0];
	float theta, phi, psi;
	// If we're in a singularity, assume phi = 0
	if(FMath::IsNearlyEqual(R31, -1.0f)){
		phi = 0;
		theta = 90;
		psi = phi + UKismetMathLibrary::DegAtan2(R12, R13);
	}
	else if(FMath::IsNearlyEqual(R31, 1.0f)){
		phi = 0;
		theta = -90;
		psi = phi + UKismetMathLibrary::DegAtan2(-R12, -R13);
	}
	// Otherwise do normal calculations
	else{
		float R11 = R.M[0][0];
		float R21 = R.M[0][1];
		float R32 = R.M[1][2];
		float R33 = R.M[2][2];

		theta = -UKismetMathLibrary::DegAsin(R31);
		float ct = UKismetMathLibrary::DegCos(theta);

		psi = UKismetMathLibrary::DegAtan2(R32/ct, R33/ct);
		phi = UKismetMathLibrary::DegAtan2(R21/ct, R11/ct);
	}
	// roll, pitch, yaw
	FVector temp(psi, theta, phi);
	UE_LOG(LogHolodeck, Warning, TEXT("Euler: %s"), *temp.ToString());
	return FVector(psi, theta, phi);
}

FVector ConvertLinearVector(FVector Vector, ConvertType Type) {

	float ScaleFactor = 1.0;
	if (Type == UEToClient)
		ScaleFactor /= UEUnitsPerMeter;
	else if(Type == ClientToUE)
		ScaleFactor *= UEUnitsPerMeter;

	Vector.X *= ScaleFactor;
	Vector.Y *= ScaleFactor;
	Vector.Z *= ScaleFactor;

	if(USE_RHS)
		Vector.Y *= -1;

	return Vector;
}

FVector ConvertAngularVector(FVector Vector, ConvertType Type) {

	float ScaleFactor = 1.0;
	if (Type == UEToClient)
		ScaleFactor /= UEUnitsPerMeter;
	else if (Type == ClientToUE)
		ScaleFactor *= UEUnitsPerMeter;

	Vector.X *= ScaleFactor;
	Vector.Y *= ScaleFactor;
	Vector.Z *= ScaleFactor;

	if (USE_RHS) {
		Vector.X *= -1;
		Vector.Z *= -1;
	}

	return Vector;
}


FRotator ConvertAngularVector(FRotator Rotator, ConvertType Type) {

	if (USE_RHS) {
		Rotator.Roll *= -1;
		Rotator.Yaw *= -1;
	}

	return Rotator;
}

FVector ConvertTorque(FVector Vector, ConvertType Type) {

	float ScaleFactor = 1.0;
	if (Type == UEToClient)
		ScaleFactor /= UEUnitsPerMeterSquared;
	else if (Type == ClientToUE)
		ScaleFactor *= UEUnitsPerMeterSquared;

	Vector.X *= ScaleFactor;
	Vector.Y *= ScaleFactor;
	Vector.Z *= ScaleFactor;

	if (USE_RHS) {
		Vector.X *= -1;
		Vector.Z *= -1;
	}

	return Vector;
}

float ConvertClientDistanceToUnreal(float client) {
	return client * UEUnitsPerMeter;
}

float ConvertUnrealDistanceToClient(float unreal) {
	return unreal / UEUnitsPerMeter;
}