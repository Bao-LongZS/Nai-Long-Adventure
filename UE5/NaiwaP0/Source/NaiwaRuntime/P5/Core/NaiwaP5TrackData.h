#pragma once

#include "NaiwaP5Types.h"

class FNaiwaP5TrackData
{
public:
	/** Speed-only numeric baseline from the M0 paper fixture; it is not level geometry. */
	static const FNaiwaP5BaselineFixture& GetM0BaselineFixture();
	/** Provisional event markers for the M1 engineering graybox. */
	static const TArray<FNaiwaP5RouteEvent>& GetM1PrototypeEvents();
	/** Full-length, provisional M2 route fixture; graybox data only. */
	static const FNaiwaP5RouteFixture& GetM2PrototypeFixture();
	static const FNaiwaP5RouteFixture& GetM1PrototypeFixture();
	static constexpr double M1PrototypeLengthMeters = 100.0;
	static constexpr int32 M1PrototypeLengthMm = 100000;
	static constexpr int32 M1PrototypeSpeedMmPerSecond = 9000;
	static constexpr int32 M1TreeClearanceMm = 1200;
	static uint32 GetM1PrototypeFixtureHash();
	static double GetRouteSpeedMetersPerSecond(const FNaiwaP5RouteFixture& Fixture, int32 DistanceMm);
	static double GetRouteSurfaceHeightMeters(const FNaiwaP5RouteFixture& Fixture, int32 DistanceMm);
	static ENaiwaP5SurfaceKind GetRouteSurfaceKind(const FNaiwaP5RouteFixture& Fixture, int32 DistanceMm);
};
