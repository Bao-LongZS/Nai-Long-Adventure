#pragma once

#include "../Core/NaiwaP5Types.h"

class FNaiwaP5SkinCatalog
{
public:
	/** Shared P5/P6 skin identity and economy rows frozen in the interface baseline. */
	static const TArray<FNaiwaP5SkinRow>& GetRows();
	static constexpr int32 CatalogVersion = 1;
};
