#include "NaiwaP4UpgradeCatalog.h"

#include "Engine/DataTable.h"

namespace
{
	bool IsCanonicalIdentifier(const FName Identifier)
	{
		const FString Text = Identifier.ToString();
		if (Text.IsEmpty() || Text == TEXT("None"))
		{
			return false;
		}

		bool bPreviousWasUnderscore = true;
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const TCHAR Character = Text[Index];
			const bool bIsLowerAscii = Character >= TEXT('a') && Character <= TEXT('z');
			const bool bIsDigit = Character >= TEXT('0') && Character <= TEXT('9');
			const bool bIsUnderscore = Character == TEXT('_');
			if (Index == 0 && !bIsLowerAscii)
			{
				return false;
			}
			if (bIsUnderscore)
			{
				if (bPreviousWasUnderscore || Index == Text.Len() - 1)
				{
					return false;
				}
			}
			else if (!bIsLowerAscii && !bIsDigit)
			{
				return false;
			}
			bPreviousWasUnderscore = bIsUnderscore;
		}

		return true;
	}

	bool HasDuplicateIds(const TArray<FName>& Ids)
	{
		TSet<FName> Seen;
		for (const FName Id : Ids)
		{
			if (!IsCanonicalIdentifier(Id) || Seen.Contains(Id))
			{
				return true;
			}
			Seen.Add(Id);
		}
		return false;
	}
}

bool FNaiwaP4UpgradeCatalog::Load(const UDataTable* DataTable, FString& OutError)
{
	OutError.Reset();
	if (!DataTable)
	{
		OutError = TEXT("No upgrade DataTable was supplied.");
		return false;
	}

	if (DataTable->GetRowStruct() != FNaiwaP4UpgradeRow::StaticStruct())
	{
		OutError = FString::Printf(
			TEXT("Unexpected upgrade row struct '%s'; expected '%s'."),
			*GetNameSafe(DataTable->GetRowStruct()),
			*FNaiwaP4UpgradeRow::StaticStruct()->GetName());
		return false;
	}

	TArray<FName> CandidateIds = DataTable->GetRowNames();
	CandidateIds.Sort([](const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
	});
	if (CandidateIds.IsEmpty())
	{
		OutError = TEXT("The upgrade DataTable has no rows.");
		return false;
	}

	TMap<FName, FNaiwaP4UpgradeRow> CandidateRows;
	CandidateRows.Reserve(CandidateIds.Num());
	for (const FName CardId : CandidateIds)
	{
		if (!IsCanonicalIdentifier(CardId))
		{
			OutError = FString::Printf(TEXT("Upgrade row name '%s' is not a lowercase snake_case ID."), *CardId.ToString());
			return false;
		}

		const FNaiwaP4UpgradeRow* Row = DataTable->FindRow<FNaiwaP4UpgradeRow>(CardId, TEXT("P4 upgrade catalog load"), false);
		if (!Row)
		{
			OutError = FString::Printf(TEXT("Upgrade row '%s' could not be read."), *CardId.ToString());
			return false;
		}
		if (Row->Title.ToString().IsEmpty() || !IsCanonicalIdentifier(Row->EffectKind) || Row->RedlineGroup.IsNone())
		{
			OutError = FString::Printf(TEXT("Upgrade row '%s' has an empty title, invalid effect ID, or missing redline group."), *CardId.ToString());
			return false;
		}
		if (!FMath::IsFinite(Row->P0) || !FMath::IsFinite(Row->P1) || !FMath::IsFinite(Row->P2))
		{
			OutError = FString::Printf(TEXT("Upgrade row '%s' contains a non-finite parameter."), *CardId.ToString());
			return false;
		}
		if (HasDuplicateIds(Row->PrerequisiteCardIds) || HasDuplicateIds(Row->ExcludedCardIds))
		{
			OutError = FString::Printf(TEXT("Upgrade row '%s' contains a malformed or duplicate prerequisite/exclusion ID."), *CardId.ToString());
			return false;
		}

		CandidateRows.Add(CardId, *Row);
	}

	for (const FName CardId : CandidateIds)
	{
		const FNaiwaP4UpgradeRow& Row = CandidateRows.FindChecked(CardId);
		if (Row.PrerequisiteCardIds.Contains(CardId) || Row.ExcludedCardIds.Contains(CardId))
		{
			OutError = FString::Printf(TEXT("Upgrade row '%s' references itself."), *CardId.ToString());
			return false;
		}

		for (const FName ReferencedId : Row.PrerequisiteCardIds)
		{
			if (!CandidateRows.Contains(ReferencedId))
			{
				OutError = FString::Printf(TEXT("Upgrade row '%s' requires unknown card '%s'."), *CardId.ToString(), *ReferencedId.ToString());
				return false;
			}
			if (Row.ExcludedCardIds.Contains(ReferencedId))
			{
				OutError = FString::Printf(TEXT("Upgrade row '%s' both requires and excludes card '%s'."), *CardId.ToString(), *ReferencedId.ToString());
				return false;
			}
		}

		for (const FName ReferencedId : Row.ExcludedCardIds)
		{
			if (!CandidateRows.Contains(ReferencedId))
			{
				OutError = FString::Printf(TEXT("Upgrade row '%s' excludes unknown card '%s'."), *CardId.ToString(), *ReferencedId.ToString());
				return false;
			}
		}
	}

	TMap<FName, uint8> VisitState;
	TArray<FName> VisitStack;
	TFunction<bool(FName)> VisitPrerequisites;
	VisitPrerequisites = [&](const FName CardId)
	{
		const uint8* ExistingState = VisitState.Find(CardId);
		if (ExistingState && *ExistingState == 2u)
		{
			return true;
		}
		if (ExistingState && *ExistingState == 1u)
		{
			const int32 CycleStart = VisitStack.Find(CardId);
			TArray<FString> CycleNames;
			for (int32 Index = CycleStart; Index < VisitStack.Num(); ++Index)
			{
				CycleNames.Add(VisitStack[Index].ToString());
			}
			CycleNames.Add(CardId.ToString());
			OutError = FString::Printf(TEXT("Upgrade prerequisite cycle detected: %s."), *FString::Join(CycleNames, TEXT(" -> ")));
			return false;
		}

		VisitState.Add(CardId, 1u);
		VisitStack.Add(CardId);
		const FNaiwaP4UpgradeRow& Row = CandidateRows.FindChecked(CardId);
		TArray<FName> SortedPrerequisites = Row.PrerequisiteCardIds;
		SortedPrerequisites.Sort([](const FName Left, const FName Right)
		{
			return Left.ToString() < Right.ToString();
		});
		for (const FName PrerequisiteId : SortedPrerequisites)
		{
			if (!VisitPrerequisites(PrerequisiteId))
			{
				return false;
			}
		}
		VisitStack.Pop();
		VisitState.Add(CardId, 2u);
		return true;
	};

	for (const FName CardId : CandidateIds)
	{
		if (!VisitPrerequisites(CardId))
		{
			return false;
		}
	}

	RowsById = MoveTemp(CandidateRows);
	OrderedCardIds = MoveTemp(CandidateIds);
	return true;
}

void FNaiwaP4UpgradeCatalog::Reset()
{
	RowsById.Reset();
	OrderedCardIds.Reset();
}

const FNaiwaP4UpgradeRow* FNaiwaP4UpgradeCatalog::Find(const FName CardId) const
{
	return RowsById.Find(CardId);
}
