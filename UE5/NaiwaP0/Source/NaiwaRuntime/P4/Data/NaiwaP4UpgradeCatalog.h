#pragma once

#include "CoreMinimal.h"
#include "NaiwaP4DataTypes.h"

class UDataTable;

/** Immutable, deterministically ordered runtime snapshot of a P4 upgrade table. */
class NAIWARUNTIME_API FNaiwaP4UpgradeCatalog
{
public:
	/**
	 * Replaces the catalog only if every row and cross-reference is valid.
	 * Card IDs are DataTable row names and are returned in ordinal name order.
	 */
	bool Load(const UDataTable* DataTable, FString& OutError);
	void Reset();

	const FNaiwaP4UpgradeRow* Find(const FName CardId) const;
	const TArray<FName>& GetOrderedCardIds() const { return OrderedCardIds; }
	int32 Num() const { return OrderedCardIds.Num(); }

private:
	TMap<FName, FNaiwaP4UpgradeRow> RowsById;
	TArray<FName> OrderedCardIds;
};
