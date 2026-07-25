// MIT License (c) 2021 BYU FRoStLab see LICENSE file

#pragma once
#include <random>
#include <array>

/**
 * Sample from a mean 0 multivariate normal distribution
 * with covariance as specified in the constructor
 */
template<int N>
class HOLODECK_API MultivariateNormal
{
static_assert(N > 0, "MVN: N must be > 0");

public:
	MultivariateNormal(){}
	~MultivariateNormal(){}

    /*
    * Set up covariance from a std deviation
    * Can be a float => covariance is diagonal with that #^2
    * Can be a 3-array => covariance is diagonal with that vector^2
    * Can be a FJsonValue 3-array => covariance is diagonal with that vector^2 
    */
    void initSigma(float sigma){
        for(int i=0;i<N;i++){
            sqrtCov[i][i] = sigma;
        }
        uncertain = true;
    }
    void initSigma(std::array<float,N> sigma){
        for(int i=0;i<N;i++){
            sqrtCov[i][i] = sigma[i];
        }
        uncertain = true;
    }
    void initSigma(TArray<TSharedPtr<FJsonValue>> sigma){
        verifyf(sigma.Num() == N, TEXT("Sigma has size %d and should be %d"), sigma.Num(), N);
        
        for(int i=0;i<N;i++){
            sqrtCov[i][i] = sigma[i]->AsNumber();
        }
        uncertain = true;
    }


    /*
    * Set up covariance from a covariance
    * Can be a float => covariance is diagonal with that #
    * Can be a 3-array => covariance is diagonal with that vector
    * Can be 3x3-array => covariance is that array
    * Can be a FJsonValue 3-array => covariance is diagonal with that vector
    * Can be a FJsonValue 3x3-array => covariance is that array
    */
    void initCov(float cov){
        for(int i=0;i<N;i++){
            sqrtCov[i][i] = FMath::Sqrt(cov);
        }
        uncertain = true;
    }
    void initCov(std::array<float,N> cov){
        for(int i=0;i<N;i++){
            sqrtCov[i][i] = FMath::Sqrt(cov[i]);
        }
        uncertain = true;
    }
    void initCov(std::array<std::array<float,N>,N> cov){        
        sqrtCov = cov;

        bool success = Cholesky(sqrtCov);
        if(success){
            uncertain = true;
        }
        else{
            UE_LOG(LogHolodeck, Warning, TEXT("MVN: Encountered a non-positive definite covariance"));
        }
    }
    void initCov(TArray<TSharedPtr<FJsonValue>> cov){
        verifyf(cov.Num() == N, TEXT("Cov has size %d and should be %d"), cov.Num(), N);

        double temp;
        bool is2D = !(cov[0]->TryGetNumber(temp)); 

        if(is2D){
            std::array<std::array<float,N>,N> parsed;
            for(int i=0;i<N;i++){
                TArray<TSharedPtr<FJsonValue>> row = cov[i]->AsArray();
                verifyf(row.Num() == N, TEXT("Cov Row has size %d and should be %d"), cov.Num(), N);
                for(int j=0;j<N;j++){
                    parsed[i][j] = row[j]->AsNumber();
                }
            }
            initCov(parsed);
        }
        else{
            for(int i=0;i<N;i++){
                sqrtCov[i][i] = FMath::Sqrt(cov[i]->AsNumber());
            }
        }
        uncertain = true;
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
                sam[i] = dist(gen);
            }

            // shift by our covariance
            for(int i=0;i<N;i++){
                for(int j=0;j<N;j++){
                    result[i] += sqrtCov[i][j]*sam[j];
                }
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
    float sampleRayleigh(){
        // Samples from a rayleigh distribution
        verifyf(N == 1, TEXT("Can't use MVN size %d with Rayleigh Noise"), N);

        // sample
        float x = sampleFloat();
        float y = sampleFloat();

        return sqrt(x*x + y*y);
    }

    /*
    * Computes cholesky decomp in place
    * returns false if matrix isn't positive definite
    */
    static bool Cholesky(std::array<std::array<float,N>,N>& A){
        for(int32 I=0;I<N;++I){
            for(int32 J = I; J < N; ++J){
                float Sum = A[I][J];
                for(int32 K = I - 1; K >= 0; --K){
                    Sum -= A[I][K] * A[J][K];
                }
                if (I == J){
                    if (Sum <= 0){
                        // Not positive definite (rounding?)
                        return false;
                    }
                    A[I][J] = FMath::Sqrt(Sum);
                }
                else{
                    A[J][I] = Sum / A[I][I];
                }
            }
        }

        for(int32 I=0;I<N;++I){
            for(int32 J=0;J<I;++J){
                A[J][I] = 0;
            }
        }

        return true;
    }

    std::array<std::array<float,N>,N> getSqrtCov(){ return sqrtCov; }
    bool isUncertain(){ return uncertain;}

private:
    bool uncertain = false;
    std::array<std::array<float,N>,N> sqrtCov = {{{{0}}}};
    std::normal_distribution<float> dist{0.0f, 1.0f};
    std::random_device rd{};
    std::mt19937 gen{ rd() };
};
