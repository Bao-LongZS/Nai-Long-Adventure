#include "NaiwaP5TrackData.h"

namespace
{
	uint32 HashByte(uint32 Hash, const uint8 Byte)
	{
		return (Hash ^ Byte) * 16777619u;
	}

	uint32 HashInt32LittleEndian(uint32 Hash, const int32 Value)
	{
		const uint32 Bits = static_cast<uint32>(Value);
		Hash = HashByte(Hash, static_cast<uint8>(Bits & 0xFFu));
		Hash = HashByte(Hash, static_cast<uint8>((Bits >> 8) & 0xFFu));
		Hash = HashByte(Hash, static_cast<uint8>((Bits >> 16) & 0xFFu));
		Hash = HashByte(Hash, static_cast<uint8>((Bits >> 24) & 0xFFu));
		return Hash;
	}

	uint32 HashRouteId(uint32 Hash, const FName RouteId)
	{
		const FTCHARToUTF8 RouteIdUtf8(*RouteId.ToString());
		for (int32 Index = 0; Index < RouteIdUtf8.Length(); ++Index)
		{
			Hash = HashByte(Hash, static_cast<uint8>(RouteIdUtf8.Get()[Index]));
		}
		return Hash;
	}

	uint32 ComputeBaselineFixtureHash(const FNaiwaP5BaselineFixture& Fixture)
	{
		uint32 Hash = HashRouteId(2166136261u, Fixture.RouteId);
		Hash = HashInt32LittleEndian(Hash, Fixture.Version);
		Hash = HashInt32LittleEndian(Hash, Fixture.RouteLengthMm);
		Hash = HashInt32LittleEndian(Hash, Fixture.SpeedSegments.Num());
		for (const FNaiwaP5TrackSpeedSegment& Segment : Fixture.SpeedSegments)
		{
			Hash = HashInt32LittleEndian(Hash, Segment.StartDistanceMm);
			Hash = HashInt32LittleEndian(Hash, Segment.EndDistanceMm);
			Hash = HashInt32LittleEndian(Hash, Segment.BaseSpeedMmPerSecond);
		}
		return Hash;
	}

	uint32 ComputeRouteFixtureHash(const FNaiwaP5RouteFixture& Fixture)
	{
		uint32 Hash = HashRouteId(2166136261u, Fixture.RouteId);
		Hash = HashInt32LittleEndian(Hash, Fixture.Version);
		Hash = HashInt32LittleEndian(Hash, Fixture.RouteLengthMm);
		Hash = HashInt32LittleEndian(Hash, Fixture.Segments.Num());
		for (const FNaiwaP5RouteSegment& Segment : Fixture.Segments)
		{
			Hash = HashInt32LittleEndian(Hash, Segment.StartDistanceMm);
			Hash = HashInt32LittleEndian(Hash, Segment.EndDistanceMm);
			Hash = HashInt32LittleEndian(Hash, Segment.BaseSpeedMmPerSecond);
			Hash = HashInt32LittleEndian(Hash, Segment.StartHeightMm);
			Hash = HashInt32LittleEndian(Hash, Segment.EndHeightMm);
			Hash = HashInt32LittleEndian(Hash, static_cast<int32>(Segment.Surface));
		}
		Hash = HashInt32LittleEndian(Hash, Fixture.Events.Num());
		for (const FNaiwaP5RouteEvent& Event : Fixture.Events)
		{
			Hash = HashInt32LittleEndian(Hash, Event.Id);
			Hash = HashInt32LittleEndian(Hash, Event.DistanceMm);
			Hash = HashInt32LittleEndian(Hash, static_cast<int32>(Event.Kind));
			Hash = HashInt32LittleEndian(Hash, Event.RequiredClearanceMm);
			Hash = HashInt32LittleEndian(Hash, Event.HitGapPenaltyMm);
			Hash = HashInt32LittleEndian(Hash, Event.WidthMm);
		}
		return Hash;
	}

	const FNaiwaP5RouteSegment* FindRouteSegment(const FNaiwaP5RouteFixture& Fixture,
		const int32 DistanceMm)
	{
		const FNaiwaP5RouteSegment* PreviousSegment = nullptr;
		for (const FNaiwaP5RouteSegment& Segment : Fixture.Segments)
		{
			if (DistanceMm >= Segment.StartDistanceMm && DistanceMm < Segment.EndDistanceMm)
			{
				return &Segment;
			}
			if (DistanceMm < Segment.StartDistanceMm)
			{
				return PreviousSegment ? PreviousSegment : &Segment;
			}
			PreviousSegment = &Segment;
		}
		return Fixture.Segments.IsEmpty() ? nullptr : &Fixture.Segments.Last();
	}
}

const FNaiwaP5BaselineFixture& FNaiwaP5TrackData::GetM0BaselineFixture()
{
	static const FNaiwaP5BaselineFixture Fixture = []()
	{
		FNaiwaP5BaselineFixture Result;
		Result.RouteId = FName(TEXT("P5_Main_1200m_Baseline"));
		Result.Version = 1;
		Result.RouteLengthMm = 1200000;
		Result.SpeedSegments = {
			{ 0, 800000, 15000 },
			{ 800000, 872000, 9000 },
			{ 872000, 1200000, 12500 }
		};
		Result.FixtureHash = ComputeBaselineFixtureHash(Result);
		return Result;
	}();
	return Fixture;
}

const FNaiwaP5RouteFixture& FNaiwaP5TrackData::GetM1PrototypeFixture()
{
	static const FNaiwaP5RouteFixture Fixture = []()
	{
		FNaiwaP5RouteFixture Result;
		Result.RouteId = FName(TEXT("P5_M1_100m_Graybox"));
		Result.Version = 1;
		Result.RouteLengthMm = M1PrototypeLengthMm;
		Result.Segments = {
			{ 0, M1PrototypeLengthMm, M1PrototypeSpeedMmPerSecond, 0, 0, ENaiwaP5SurfaceKind::Snow }
		};
		Result.Events = {
			{ 1, 14000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 2, 30000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 3, 42000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 4, 58000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 5, 70000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 6, 86000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 7, 96000, ENaiwaP5RouteEventKind::Coin, 0, 0 }
		};
		Result.FixtureHash = ComputeRouteFixtureHash(Result);
		return Result;
	}();
	return Fixture;
}

const TArray<FNaiwaP5RouteEvent>& FNaiwaP5TrackData::GetM1PrototypeEvents()
{
	return GetM1PrototypeFixture().Events;
}

const FNaiwaP5RouteFixture& FNaiwaP5TrackData::GetM2PrototypeFixture()
{
	static const FNaiwaP5RouteFixture Fixture = []()
	{
		FNaiwaP5RouteFixture Result;
		Result.RouteId = FName(TEXT("P5_Main_1800m_M2Graybox"));
		Result.Version = 5;
		Result.RouteLengthMm = 1800000;
		Result.Segments = {
			{ 0, 80000, 22000, 0, -14000, ENaiwaP5SurfaceKind::Snow },
			{ 80000, 160000, 18000, -14000, -2000, ENaiwaP5SurfaceKind::Snow },
			{ 160000, 240000, 24000, -2000, -24000, ENaiwaP5SurfaceKind::Snow },
			{ 240000, 320000, 18000, -24000, -5000, ENaiwaP5SurfaceKind::Snow },
			{ 320000, 400000, 25000, -5000, -28000, ENaiwaP5SurfaceKind::Snow },
			{ 400000, 480000, 19000, -28000, -8000, ENaiwaP5SurfaceKind::Snow },
			{ 480000, 560000, 25000, -8000, -30000, ENaiwaP5SurfaceKind::Snow },
			{ 560000, 640000, 19000, -30000, -9000, ENaiwaP5SurfaceKind::Snow },
			{ 640000, 720000, 26000, -9000, -36000, ENaiwaP5SurfaceKind::Snow },
			{ 720000, 800000, 19000, -36000, -10000, ENaiwaP5SurfaceKind::Snow },
			{ 800000, 872000, 11000, -10000, 8000, ENaiwaP5SurfaceKind::Snow },
			{ 872000, 930000, 24000, 8000, -12000, ENaiwaP5SurfaceKind::Snow },
			{ 930000, 980000, 26000, -12000, -30000, ENaiwaP5SurfaceKind::Snow },
			{ 980000, 1020000, 21000, -30000, -36000, ENaiwaP5SurfaceKind::Ice },
			{ 1020000, 1080000, 19000, -36000, -13000, ENaiwaP5SurfaceKind::Snow },
			{ 1080000, 1140000, 26000, -13000, -34000, ENaiwaP5SurfaceKind::Snow },
			{ 1140000, 1160000, 19000, -34000, -24000, ENaiwaP5SurfaceKind::Snow },
			{ 1160000, 1200000, 26000, -24000, 3000, ENaiwaP5SurfaceKind::JumpRamp },
			{ 1200000, 1280000, 23000, 3000, -18000, ENaiwaP5SurfaceKind::Snow },
			{ 1280000, 1360000, 26000, -18000, -2000, ENaiwaP5SurfaceKind::Snow },
			{ 1360000, 1440000, 21000, -2000, -22000, ENaiwaP5SurfaceKind::Snow },
			{ 1440000, 1480000, 24000, -22000, -8000, ENaiwaP5SurfaceKind::Snow },
			{ 1480000, 1500000, 26000, -8000, 3000, ENaiwaP5SurfaceKind::JumpRamp },
			{ 1524000, 1600000, 22000, -8000, -26000, ENaiwaP5SurfaceKind::Snow },
			{ 1600000, 1680000, 26000, -26000, -4000, ENaiwaP5SurfaceKind::Snow },
			{ 1680000, 1720000, 22000, -4000, -12000, ENaiwaP5SurfaceKind::Snow },
			{ 1720000, 1740000, 26000, -12000, -4000, ENaiwaP5SurfaceKind::JumpRamp },
			{ 1756000, 1800000, 24000, -10000, -20000, ENaiwaP5SurfaceKind::Snow }
		};
		Result.Events = {
			{ 1, 25000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 2, 75000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 3, 125000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 39, 150000, ENaiwaP5RouteEventKind::Kangaroo, 1800, 0, 2500 },
			{ 4, 175000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 5, 190000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 6, 225000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 7, 275000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 8, 325000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 9, 345000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 10, 375000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 11, 425000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 12, 455000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 13, 475000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 40, 500000, ENaiwaP5RouteEventKind::Kangaroo, 1800, 0, 2500 },
			{ 14, 515000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 15, 545000, ENaiwaP5RouteEventKind::Rock, 600, 3000 },
			{ 16, 575000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 17, 615000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 18, 625000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 19, 675000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 20, 695000, ENaiwaP5RouteEventKind::Rock, 600, 3000 },
			{ 21, 725000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 22, 755000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 23, 775000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 24, 825000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 25, 850000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 26, 875000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 27, 900000, ENaiwaP5RouteEventKind::Rock, 600, 3000 },
			{ 28, 925000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 41, 930000, ENaiwaP5RouteEventKind::Kangaroo, 1800, 0, 2500 },
			{ 29, 945000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 30, 975000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 31, 1005000, ENaiwaP5RouteEventKind::IceGap, 1400, 0, 2000 },
			{ 32, 1025000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 33, 1045000, ENaiwaP5RouteEventKind::Rock, 600, 3000 },
			{ 34, 1075000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 35, 1085000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 36, 1125000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 37, 1150000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 38, 1175000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 42, 1225000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 43, 1260000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 44, 1275000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 45, 1325000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 46, 1345000, ENaiwaP5RouteEventKind::Rock, 600, 3000 },
			{ 47, 1375000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 48, 1410000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 49, 1425000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 50, 1475000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 51, 1500000, ENaiwaP5RouteEventKind::CliffLaunch, 0, 0 },
			{ 52, 1512000, ENaiwaP5RouteEventKind::CliffGap, 1000, 0, 24000 },
			{ 53, 1535000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 54, 1575000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 55, 1600000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 },
			{ 56, 1625000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 57, 1675000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 58, 1695000, ENaiwaP5RouteEventKind::Rock, 600, 3000 },
			{ 59, 1725000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 60, 1740000, ENaiwaP5RouteEventKind::CliffLaunch, 0, 0 },
			{ 61, 1748000, ENaiwaP5RouteEventKind::CliffGap, 1000, 0, 16000 },
			{ 62, 1780000, ENaiwaP5RouteEventKind::Coin, 0, 0 },
			{ 63, 1785000, ENaiwaP5RouteEventKind::Tree, M1TreeClearanceMm, 3000 }
		};
		Result.FixtureHash = ComputeRouteFixtureHash(Result);
		return Result;
	}();
	return Fixture;
}

uint32 FNaiwaP5TrackData::GetM1PrototypeFixtureHash()
{
	return GetM1PrototypeFixture().FixtureHash;
}

double FNaiwaP5TrackData::GetRouteSpeedMetersPerSecond(const FNaiwaP5RouteFixture& Fixture,
	const int32 DistanceMm)
{
	const FNaiwaP5RouteSegment* Segment = FindRouteSegment(Fixture,
		FMath::Clamp(DistanceMm, 0, FMath::Max(0, Fixture.RouteLengthMm - 1)));
	return Segment ? static_cast<double>(Segment->BaseSpeedMmPerSecond) / 1000.0 : 0.0;
}

double FNaiwaP5TrackData::GetRouteSurfaceHeightMeters(const FNaiwaP5RouteFixture& Fixture,
	const int32 DistanceMm)
{
	const FNaiwaP5RouteSegment* Segment = FindRouteSegment(Fixture,
		FMath::Clamp(DistanceMm, 0, Fixture.RouteLengthMm));
	if (!Segment)
	{
		return 0.0;
	}
	const int32 SegmentLength = FMath::Max(1, Segment->EndDistanceMm - Segment->StartDistanceMm);
	const double Alpha = FMath::Clamp(static_cast<double>(DistanceMm - Segment->StartDistanceMm) /
		static_cast<double>(SegmentLength), 0.0, 1.0);
	return FMath::Lerp(static_cast<double>(Segment->StartHeightMm),
		static_cast<double>(Segment->EndHeightMm), Alpha) / 1000.0;
}

ENaiwaP5SurfaceKind FNaiwaP5TrackData::GetRouteSurfaceKind(const FNaiwaP5RouteFixture& Fixture,
	const int32 DistanceMm)
{
	const FNaiwaP5RouteSegment* Segment = FindRouteSegment(Fixture,
		FMath::Clamp(DistanceMm, 0, FMath::Max(0, Fixture.RouteLengthMm - 1)));
	return Segment ? Segment->Surface : ENaiwaP5SurfaceKind::Snow;
}
