// VoxelNoise.cpp

#include "VoxelNoise.h"

namespace VoxelNoise
{
	// Integer hash -> [0,1) float. Deterministic, no lookup tables (mobile-friendly:
	// no cache-unfriendly permutation table reads like classic Perlin).
	static FORCEINLINE uint32 Hash(int32 Seed, int32 X, int32 Y, int32 Z)
	{
		uint32 H = static_cast<uint32>(Seed);
		H = H * 374761393u + static_cast<uint32>(X) * 668265263u;
		H = (H ^ (H >> 13)) * 1274126177u;
		H = H + static_cast<uint32>(Y) * 2246822519u;
		H = (H ^ (H >> 16)) * 3266489917u;
		H = H + static_cast<uint32>(Z) * 3812015801u;
		H = H ^ (H >> 15);
		return H;
	}

	static FORCEINLINE float HashToUnitFloat(uint32 H)
	{
		return (H & 0x00FFFFFF) / static_cast<float>(0x01000000); // [0, 1)
	}

	static FORCEINLINE float SmoothStep(float T)
	{
		return T * T * (3.0f - 2.0f * T);
	}

	float Sample2D(int32 Seed, float X, float Y)
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const float FracX = X - X0;
		const float FracY = Y - Y0;

		const float V00 = HashToUnitFloat(Hash(Seed, X0, Y0, 0));
		const float V10 = HashToUnitFloat(Hash(Seed, X0 + 1, Y0, 0));
		const float V01 = HashToUnitFloat(Hash(Seed, X0, Y0 + 1, 0));
		const float V11 = HashToUnitFloat(Hash(Seed, X0 + 1, Y0 + 1, 0));

		const float SX = SmoothStep(FracX);
		const float SY = SmoothStep(FracY);

		const float Top = FMath::Lerp(V00, V10, SX);
		const float Bottom = FMath::Lerp(V01, V11, SX);
		return FMath::Lerp(Top, Bottom, SY) * 2.0f - 1.0f; // remap [0,1) -> [-1,1)
	}

	float Sample3D(int32 Seed, float X, float Y, float Z)
	{
		const int32 X0 = FMath::FloorToInt(X);
		const int32 Y0 = FMath::FloorToInt(Y);
		const int32 Z0 = FMath::FloorToInt(Z);
		const float FracX = X - X0;
		const float FracY = Y - Y0;
		const float FracZ = Z - Z0;

		float Corners[8];
		int32 Index = 0;
		for (int32 DZ = 0; DZ <= 1; ++DZ)
		{
			for (int32 DY = 0; DY <= 1; ++DY)
			{
				for (int32 DX = 0; DX <= 1; ++DX)
				{
					Corners[Index++] = HashToUnitFloat(Hash(Seed, X0 + DX, Y0 + DY, Z0 + DZ));
				}
			}
		}

		const float SX = SmoothStep(FracX);
		const float SY = SmoothStep(FracY);
		const float SZ = SmoothStep(FracZ);

		const float X00 = FMath::Lerp(Corners[0], Corners[1], SX);
		const float X10 = FMath::Lerp(Corners[2], Corners[3], SX);
		const float X01 = FMath::Lerp(Corners[4], Corners[5], SX);
		const float X11 = FMath::Lerp(Corners[6], Corners[7], SX);

		const float Y0Interp = FMath::Lerp(X00, X10, SY);
		const float Y1Interp = FMath::Lerp(X01, X11, SY);

		return (FMath::Lerp(Y0Interp, Y1Interp, SZ)) * 2.0f - 1.0f;
	}

	float FractalBrownianMotion2D(int32 Seed, float X, float Y, int32 Octaves, float Lacunarity, float Gain)
	{
		float Sum = 0.0f;
		float Amplitude = 1.0f;
		float Frequency = 1.0f;
		float MaxAmplitude = 0.0f;

		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			Sum += Sample2D(Seed + Octave, X * Frequency, Y * Frequency) * Amplitude;
			MaxAmplitude += Amplitude;
			Amplitude *= Gain;
			Frequency *= Lacunarity;
		}

		return MaxAmplitude > 0.0f ? Sum / MaxAmplitude : 0.0f;
	}

	float FractalBrownianMotion3D(int32 Seed, float X, float Y, float Z, int32 Octaves, float Lacunarity, float Gain)
	{
		float Sum = 0.0f;
		float Amplitude = 1.0f;
		float Frequency = 1.0f;
		float MaxAmplitude = 0.0f;

		for (int32 Octave = 0; Octave < Octaves; ++Octave)
		{
			Sum += Sample3D(Seed + Octave, X * Frequency, Y * Frequency, Z * Frequency) * Amplitude;
			MaxAmplitude += Amplitude;
			Amplitude *= Gain;
			Frequency *= Lacunarity;
		}

		return MaxAmplitude > 0.0f ? Sum / MaxAmplitude : 0.0f;
	}
}
