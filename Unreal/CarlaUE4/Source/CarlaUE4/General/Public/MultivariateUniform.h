// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#pragma once
#include <random>
#include <cmath>
#include <array>

/**
 * Sample from a min 0 multivariate uniform distribution
 * with maximum as specified in the constructor
 *
 * TODO: This library has NOT been fully tested. Beware before using!
 */
template<int N>
class HOLODECK_API MultivariateUniform
{
static_assert(N > 0, "UNIFORM: N must be > 0");

public:
	MultivariateUniform(){}
	~MultivariateUniform(){}

    /*
    * Set up covariance from a std deviation
    * Can be a float => covariance is diagonal with that #^2
    * Can be a 3-array => covariance is diagonal with that vector^2
    * Can be a FJsonValue 3-array => covariance is diagonal with that vector^2 
    */
    void initBounds(float max_){
        for(int i=0;i<N;i++){
            max[i] = max_;
        }
        if(max_ != 0) uncertain = true;
    }
    void initBounds(std::array<float,N> max_){
        for(int i=0;i<N;i++){
            max[i] = max_[i];
            if(max_[i] != 0) uncertain = true;
        }
    }
    void initBounds(TArray<TSharedPtr<FJsonValue>> max_){
        verifyf(max_.Num() == N, TEXT("Sigma has size %d and should be %d"), max_.Num(), N);
        
        for(int i=0;i<N;i++){
            max[i] = max_[i]->AsNumber();
            if(max_[i] != 0) uncertain = true;
        }
    }

    /* 
    * Different ways to sample, with different return types
    * Can return as std::array, TArray, FVector (requires N=3), or float (requires N=1)
    * All functions draw on sampleArray. If cov hasn't been set, returns 0s
    */
    std::array<float,N> sampleArray(){
        std::array<float,N> sam;
        std::array<float,N> result = {0};

        if(uncertain){
            // sample from N(0,1);
            for(int i=0;i<N;i++){
                sam[i] = dist(gen)*max[i];
            }
        }

        return result;
    };
    TArray<float> sampleTArray(){
        // sample
        std::array<float,N> sample = sampleArray();

        // put into TArray
        TArray<float> result;
        result.Append(sample.data(), N);

        return result;
    }
    FVector sampleFVector(){
        verifyf(N == 3, TEXT("Can't use MVN size %d with FVector samples"), N);

        // sample
        std::array<float,N> sample = sampleArray();

        // put into TArray
        FVector result = FVector(sample[0], sample[1], sample[2]);

        return result;
    }
    float sampleFloat(){
        verifyf(N == 1, TEXT("Can't use MVN size %d with float samples"), N);

        // sample
        std::array<float,N> sample = sampleArray();

        return sample[0];
    }
    float sampleExponential(){
        // Samples from a exponential distribution using max as the scale parameter
        verifyf(N == 1, TEXT("Can't use MVN size %d with Exponential Noise"), N);

        // sample
        if(uncertain){
            float x = dist(gen);
            // https://en.wikipedia.org/wiki/Exponential_distribution#Generating_exponential_variates
            return -max[0]*std::log(x);
        }
        else{
            return 0;
        }
    }
    float exponentialPDF(float x){
        if(uncertain) return std::exp(-x/max[0]) / max[0];
        else return 1;
    }
    float exponentialScaledPDF(float x){
        if(uncertain) return std::exp(-x/max[0]);
        else return 1;
    }

    bool isUncertain(){ return uncertain;}

private:
    bool uncertain = false;
    std::array<float,N> max = {{0}};
    std::uniform_real_distribution<float> dist{0.0f, 1.0f};
    std::random_device rd{};
    std::mt19937 gen{ rd() };
};
