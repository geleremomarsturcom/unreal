// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackRoot.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraEmitter.h"
#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ScopedTransaction.h"

#include "Editor.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"
const double UNiagaraStackViewModel::MaxSearchTime = .02f; // search at 50 fps

bool UNiagaraStackViewModel::FTopLevelViewModel::IsValid() const
{
	return (SystemViewModel.IsValid() && EmitterHandleViewModel.IsValid() == false) ||
		(SystemViewModel.IsValid() == false && EmitterHandleViewModel.IsValid());
}

UNiagaraStackEditorData* UNiagaraStackViewModel::FTopLevelViewModel::GetStackEditorData() const
{
	if (SystemViewModel.IsValid())
	{
		return &SystemViewModel->GetEditorData().GetStackEditorData();
	}
	else if (EmitterHandleViewModel.IsValid())
	{
		return &EmitterHandleViewModel->GetEmitterViewModel()->GetEditorData().GetStackEditorData();
	}
	else
	{
		return nullptr;
	}
}

FText UNiagaraStackViewModel::FTopLevelViewModel::GetDisplayName() const
{
	if (SystemViewModel.IsValid())
	{
		return SystemViewModel->GetDisplayName();
	}
	else if (EmitterHandleViewModel.IsValid())
	{
		return EmitterHandleViewModel->GetNameText();
	}
	else
	{
		return FText();
	}
}

bool UNiagaraStackViewModel::FTopLevelViewModel::operator==(const FTopLevelViewModel& Other) const
{
	return Other.SystemViewModel == SystemViewModel && Other.EmitterHandleViewModel == EmitterHandleViewModel;
}

void UNiagaraStackViewModel::InitializeWithViewModels(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel, TSharedPtr<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel, FNiagaraStackViewModelOptions InOptions)
{
	Reset();

	Options = InOptions;
	SystemViewModel = InSystemViewModel;
	EmitterHandleViewModel = InEmitterHandleViewModel;

	TSharedPtr<FNiagaraSystemViewModel> SystemViewModelPinned = SystemViewModel.Pin();
	TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel = InEmitterHandleViewModel.IsValid() ? InEmitterHandleViewModel->GetEmitterViewModel() : TSharedPtr<FNiagaraEmitterViewModel>();

	if (SystemViewModelPinned.IsValid())
	{
		GEditor->RegisterForUndo(this);

		if (EmitterViewModel.IsValid())
		{
			EmitterViewModel->OnScriptCompiled().AddUObject(this, &UNiagaraStackViewModel::OnEmitterCompiled);
			EmitterViewModel->OnParentRemoved().AddUObject(this, &UNiagaraStackViewModel::EmitterParentRemoved);
		}
		SystemViewModelPinned->OnSystemCompiled().AddUObject(this, &UNiagaraStackViewModel::OnSystemCompiled);
		
		UNiagaraStackRoot* StackRoot = NewObject<UNiagaraStackRoot>(this);
		UNiagaraStackEntry::FRequiredEntryData RequiredEntryData(SystemViewModelPinned.ToSharedRef(), EmitterViewModel,
			UNiagaraStackEntry::FExecutionCategoryNames::System, UNiagaraStackEntry::FExecutionSubcategoryNames::Settings,
			SystemViewModelPinned->GetEditorData().GetStackEditorData());
		StackRoot->Initialize(RequiredEntryData, Options.GetIncludeSystemInformation(), Options.GetIncludeEmitterInformation());
		StackRoot->RefreshChildren();
		StackRoot->OnStructureChanged().AddUObject(this, &UNiagaraStackViewModel::EntryStructureChanged);
		StackRoot->OnDataObjectModified().AddUObject(this, &UNiagaraStackViewModel::EntryDataObjectModified);
		StackRoot->OnRequestFullRefresh().AddUObject(this, &UNiagaraStackViewModel::EntryRequestFullRefresh);
		StackRoot->OnRequestFullRefreshDeferred().AddUObject(this, &UNiagaraStackViewModel::EntryRequestFullRefreshDeferred);
		RootEntry = StackRoot;
		RootEntries.Add(RootEntry);

		bExternalRootEntry = false;
	}

	StructureChangedDelegate.Broadcast();
}

void UNiagaraStackViewModel::InitializeWithRootEntry(UNiagaraStackEntry* InRootEntry)
{
	Reset();
	bUsesTopLevelViewModels = true;

	RootEntry = InRootEntry;
	RootEntry->OnStructureChanged().AddUObject(this, &UNiagaraStackViewModel::EntryStructureChanged);
	RootEntries.Add(RootEntry);

	GEditor->RegisterForUndo(this);
	StructureChangedDelegate.Broadcast();
}

void UNiagaraStackViewModel::Reset()
{
	if (RootEntry != nullptr)
	{
		RootEntry->OnStructureChanged().RemoveAll(this);
		RootEntry->OnDataObjectModified().RemoveAll(this);
		RootEntry->OnRequestFullRefresh().RemoveAll(this);
		RootEntry->OnRequestFullRefreshDeferred().RemoveAll(this);
		if (bExternalRootEntry == false)
		{
			RootEntry->Finalize();
		}
		RootEntry = nullptr;
	}
	RootEntries.Empty();

	if (EmitterHandleViewModel.IsValid())
	{
		EmitterHandleViewModel.Pin()->GetEmitterViewModel()->OnScriptCompiled().RemoveAll(this);
		EmitterHandleViewModel.Reset();
	}

	if (SystemViewModel.IsValid())
	{
		SystemViewModel.Pin()->OnSystemCompiled().RemoveAll(this);
		SystemViewModel.Reset();
	}

	TopLevelViewModels.Empty();
	
	GEditor->UnregisterForUndo(this);

	CurrentFocusedSearchMatchIndex = -1;
	bRestartSearch = false;
	bRefreshPending = false;
	bUsesTopLevelViewModels = false;
}

void UNiagaraStackViewModel::Finalize()
{
	Reset();
}

void UNiagaraStackViewModel::BeginDestroy()
{
	checkf(HasAnyFlags(RF_ClassDefaultObject) || (SystemViewModel.IsValid() == false && EmitterHandleViewModel.IsValid() == false), TEXT("Stack view model not finalized."));
	Super::BeginDestroy();
}

void UNiagaraStackViewModel::Tick()
{
	if (RootEntry)
	{
		if (bRefreshPending)
		{
			RootEntry->RefreshChildren();
			OnSearchTextChanged(CurrentSearchText);
			bRefreshPending = false;
		}

		SearchTick();
	}
}

void UNiagaraStackViewModel::OnSearchTextChanged(const FText& SearchText)
{
	if (RootEntry && !CurrentSearchText.EqualTo(SearchText))
	{
		CurrentSearchText = SearchText;
		RestoreStackEntryExpansionPreSearch();
		// postpone searching until next tick; protects against crashes from the GC
		// also this can be triggered by multiple events, so better wait
		bRestartSearch = true;
	}
}

bool UNiagaraStackViewModel::IsSearching()
{
	return ItemsToSearch.Num() > 0;
}

const TArray<UNiagaraStackViewModel::FSearchResult>& UNiagaraStackViewModel::GetCurrentSearchResults()
{
	return CurrentSearchResults;
}

UNiagaraStackEntry* UNiagaraStackViewModel::GetCurrentFocusedEntry()
{
	if (CurrentFocusedSearchMatchIndex >= 0)
	{
		FSearchResult FocusedMatch = CurrentSearchResults[CurrentFocusedSearchMatchIndex];
		return FocusedMatch.GetEntry();
	}
	return nullptr;
}

void UNiagaraStackViewModel::AddSearchScrollOffset(int NumberOfSteps)
{
	CurrentFocusedSearchMatchIndex += NumberOfSteps;
	if (CurrentFocusedSearchMatchIndex >= CurrentSearchResults.Num())
	{
		CurrentFocusedSearchMatchIndex = 0;
	}
	if (CurrentFocusedSearchMatchIndex < 0)
	{
		CurrentFocusedSearchMatchIndex = CurrentSearchResults.Num() - 1;
	}
}

void UNiagaraStackViewModel::CollapseToHeaders()
{
	CollapseToHeadersRecursive(GetRootEntries());
	NotifyStructureChanged();
}

void UNiagaraStackViewModel::UndismissAllIssues()
{
	FScopedTransaction ScopedTransaction(LOCTEXT("UnDismissIssues", "Undismiss issues"));

	TArray<UNiagaraStackEditorData*> StackEditorDatas;
	for (TSharedRef<UNiagaraStackViewModel::FTopLevelViewModel> TopLevelViewModel : TopLevelViewModels)
	{
		if (TopLevelViewModel->IsValid())
		{
			StackEditorDatas.AddUnique(TopLevelViewModel->GetStackEditorData());
		}
	}

	for (UNiagaraStackEditorData* StackEditorData : StackEditorDatas)
	{
		StackEditorData->Modify();
		StackEditorData->UndismissAllIssues();
	}
	
	RootEntry->RefreshChildren();
}

bool UNiagaraStackViewModel::HasDismissedStackIssues()
{
	for (TSharedRef<FTopLevelViewModel> TopLevelViewModel : TopLevelViewModels)
	{
		if (TopLevelViewModel->IsValid() && TopLevelViewModel->GetStackEditorData()->GetDismissedStackIssueIds().Num() > 0)
		{
			return true;
		}
	}
	return false;
}

const TArray<TSharedRef<UNiagaraStackViewModel::FTopLevelViewModel>>& UNiagaraStackViewModel::GetTopLevelViewModels() const
{
	return TopLevelViewModels;
}

TSharedPtr<UNiagaraStackViewModel::FTopLevelViewModel> UNiagaraStackViewModel::GetTopLevelViewModelForEntry(UNiagaraStackEntry& InEntry) const
{
	if (InEntry.GetEmitterViewModel().IsValid())
	{
		const TSharedRef<FTopLevelViewModel>* MatchingTopLevelViewModel = TopLevelViewModels.FindByPredicate([&InEntry](const TSharedRef<FTopLevelViewModel>& TopLevelViewModel) 
		{ 
			return TopLevelViewModel->EmitterHandleViewModel.IsValid() && TopLevelViewModel->EmitterHandleViewModel->GetEmitterViewModel() == InEntry.GetEmitterViewModel(); 
		});

		if (MatchingTopLevelViewModel != nullptr)
		{
			return *MatchingTopLevelViewModel;
		}
	}
	else
	{
		const TSharedRef<FTopLevelViewModel>* MatchingTopLevelViewModel = TopLevelViewModels.FindByPredicate([&InEntry](const TSharedRef<FTopLevelViewModel>& TopLevelViewModel) 
		{ 
			return TopLevelViewModel->SystemViewModel == InEntry.GetSystemViewModel(); 
		});

		if (MatchingTopLevelViewModel != nullptr)
		{
			return *MatchingTopLevelViewModel;
		}
	}
	return TSharedPtr<FTopLevelViewModel>();
}

void UNiagaraStackViewModel::CollapseToHeadersRecursive(TArray<UNiagaraStackEntry*> Entries)
{
	for (UNiagaraStackEntry* Entry : Entries)
	{
		if (Entry->GetCanExpand())
		{
			if (Entry->IsA<UNiagaraStackItemGroup>())
			{
				Entry->SetIsExpanded(true);
			}
			else if (Entry->IsA<UNiagaraStackItem>())
			{
				Entry->SetIsExpanded(false);
			}
		}

		TArray<UNiagaraStackEntry*> Children;
		Entry->GetUnfilteredChildren(Children);
		CollapseToHeadersRecursive(Children);
	}
}

void UNiagaraStackViewModel::GetPathForEntry(UNiagaraStackEntry* Entry, TArray<UNiagaraStackEntry*>& EntryPath)
{
	GeneratePathForEntry(RootEntry, Entry, TArray<UNiagaraStackEntry*>(), EntryPath);
}

void UNiagaraStackViewModel::OnSystemCompiled()
{
	RootEntry->RefreshChildren();
	// when the entries are refreshed, make sure search results are still valid, the stack counts on them
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::OnEmitterCompiled()
{
	RootEntry->RefreshChildren();
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::EmitterParentRemoved()
{
	RootEntry->RefreshChildren();
}

void UNiagaraStackViewModel::SearchTick()
{
	// perform partial searches here, by processing a fixed number of entries (maybe more than one?)
	if (bRestartSearch)
	{
		// clear the search results
		for (auto SearchResult : GetCurrentSearchResults())
		{
			SearchResult.GetEntry()->SetIsSearchResult(false);
		}
		CurrentSearchResults.Empty();
		CurrentFocusedSearchMatchIndex = -1;
		// generates ItemsToSearch, these will be processed on tick, in batches
		if (CurrentSearchText.IsEmpty() == false)
		{
			GenerateTraversalEntries(RootEntry, TArray<UNiagaraStackEntry*>(), ItemsToSearch);
		}
		// we need to call the SearchCompletedDelegate to go through SynchronizeTreeExpansion in SNiagaraStack so that when exiting search we return the stack expansion to the state it was before searching
		else
		{
			SearchCompletedDelegate.Broadcast();
		}
		bRestartSearch = false;
	}

	if (IsSearching())
	{
		double SearchStartTime = FPlatformTime::Seconds();
		double CurrentSearchLoopTime = SearchStartTime;
		// process at least one item, but don't go over MaxSearchTime for the rest
		while (ItemsToSearch.Num() > 0 && CurrentSearchLoopTime - SearchStartTime < MaxSearchTime)
		{
			UNiagaraStackEntry* EntryToProcess = ItemsToSearch[0].GetEntry();
			ensure(EntryToProcess != nullptr); // should never happen so something went wrong if this is hit
			if (EntryToProcess != nullptr)
			{
				TArray<UNiagaraStackEntry::FStackSearchItem> SearchItems;
				EntryToProcess->GetSearchItems(SearchItems);
				TSet<FName> MatchedKeys;
				for (UNiagaraStackEntry::FStackSearchItem SearchItem : SearchItems)
				{
					if (!EntryToProcess->GetStackEditorDataKey().IsEmpty())
					{
						EntryToProcess->GetStackEditorData().SetStackEntryWasExpandedPreSearch(EntryToProcess->GetStackEditorDataKey(), EntryToProcess->GetIsExpanded());
					}

					if (ItemMatchesSearchCriteria(SearchItem)) 
					{
						if (MatchedKeys.Contains(SearchItem.Key) == false)
						{
							EntryToProcess->SetIsSearchResult(true);
							CurrentSearchResults.Add({ ItemsToSearch[0].EntryPath, SearchItem });
							MatchedKeys.Add(SearchItem.Key);
						}
					}
				}
			}
			ItemsToSearch.RemoveAt(0); // can't use RemoveAtSwap because we need to preserve the order
			CurrentSearchLoopTime = FPlatformTime::Seconds();
		}
		if (ItemsToSearch.Num() == 0)
		{
			SearchCompletedDelegate.Broadcast();
		}
	}
}

void UNiagaraStackViewModel::GenerateTraversalEntries(UNiagaraStackEntry* Root, TArray<UNiagaraStackEntry*> ParentChain,
	TArray<FSearchWorkItem>& TraversedArray)
{
	TArray<UNiagaraStackEntry*> Children;
	Root->GetUnfilteredChildren(Children);
	ParentChain.Add(Root);
	TraversedArray.Add(FSearchWorkItem{ParentChain});
	for (auto Child : Children)
	{
		GenerateTraversalEntries(Child, ParentChain, TraversedArray);
	}
}

bool UNiagaraStackViewModel::ItemMatchesSearchCriteria(UNiagaraStackEntry::FStackSearchItem SearchItem)
{
	// this is a simple text compare, we need to replace this with a complex search on future passes
	return SearchItem.Value.ToString().Contains(CurrentSearchText.ToString());
}

void UNiagaraStackViewModel::GeneratePathForEntry(UNiagaraStackEntry* Root, UNiagaraStackEntry* Entry, TArray<UNiagaraStackEntry*> CurrentPath, TArray<UNiagaraStackEntry*>& EntryPath)
{
	if (EntryPath.Num() > 0)
	{
		return;
	}
	TArray<UNiagaraStackEntry*> Children;
	Root->GetUnfilteredChildren(Children);
	CurrentPath.Add(Root);
	for (auto Child : Children)
	{
		if (Child == Entry)
		{
			EntryPath.Append(CurrentPath);
			return;
		}
		GeneratePathForEntry(Child, Entry, CurrentPath, EntryPath);
	}
}

void UNiagaraStackViewModel::RestoreStackEntryExpansionPreSearch()
{
	GenerateTraversalEntries(RootEntry, TArray<UNiagaraStackEntry*>(), ItemsToRestoreExpansionState);

	while (ItemsToRestoreExpansionState.Num() != 0)
	{
		UNiagaraStackEntry* EntryToProcess = ItemsToRestoreExpansionState[0].GetEntry();
		if (EntryToProcess->GetIsSearchResult() && !EntryToProcess->IsA<UNiagaraStackRoot>())
		{
			for (UNiagaraStackEntry* EntryToExpand : ItemsToRestoreExpansionState[0].EntryPath)
			{
				if (!EntryToExpand->IsA<UNiagaraStackRoot>())
				{
					UNiagaraStackEditorData& StackEditorData = EntryToExpand->GetStackEditorData();
					EntryToExpand->SetIsExpanded(StackEditorData.GetStackEntryWasExpandedPreSearch(EntryToExpand->GetStackEditorDataKey(), false));
				}
			}
		}
		ItemsToRestoreExpansionState.RemoveAtSwap(0);
	}
}

TArray<UNiagaraStackEntry*>& UNiagaraStackViewModel::GetRootEntries()
{
	return RootEntries;
}

UNiagaraStackViewModel::FOnStructureChanged& UNiagaraStackViewModel::OnStructureChanged()
{
	return StructureChangedDelegate;
}

UNiagaraStackViewModel::FOnSearchCompleted& UNiagaraStackViewModel::OnSearchCompleted()
{
	return SearchCompletedDelegate;
}

bool UNiagaraStackViewModel::GetShowAllAdvanced() const
{
	for (TSharedRef<FTopLevelViewModel> TopLevelViewModel : TopLevelViewModels)
	{
		if (TopLevelViewModel->IsValid() && TopLevelViewModel->GetStackEditorData()->GetShowAllAdvanced())
		{
			return true;
		}
	}
	return false;
}

void UNiagaraStackViewModel::SetShowAllAdvanced(bool bInShowAllAdvanced)
{
	for (TSharedRef<FTopLevelViewModel> TopLevelViewModel : TopLevelViewModels)
	{
		if (TopLevelViewModel->IsValid()) 
		{
			TopLevelViewModel->GetStackEditorData()->SetShowAllAdvanced(bInShowAllAdvanced);
		}
	}

	OnSearchTextChanged(CurrentSearchText);
	StructureChangedDelegate.Broadcast();
}

bool UNiagaraStackViewModel::GetShowOutputs() const
{
	for (TSharedRef<FTopLevelViewModel> TopLevelViewModel : TopLevelViewModels)
	{
		if (TopLevelViewModel->IsValid() && TopLevelViewModel->GetStackEditorData()->GetShowOutputs())
		{
			return true;
		}
	}
	return false;
}

void UNiagaraStackViewModel::SetShowOutputs(bool bInShowOutputs)
{
	for (TSharedRef<FTopLevelViewModel> TopLevelViewModel : TopLevelViewModels)
	{
		if (TopLevelViewModel->IsValid())
		{
			TopLevelViewModel->GetStackEditorData()->SetShowOutputs(bInShowOutputs);
		}
	}
	
	// Showing outputs changes indenting so a full refresh is needed.
	OnSearchTextChanged(CurrentSearchText);
	RootEntry->RefreshChildren();
}

bool UNiagaraStackViewModel::GetShowLinkedInputs() const
{
	for (TSharedRef<FTopLevelViewModel> TopLevelViewModel : TopLevelViewModels)
	{
		if (TopLevelViewModel->IsValid() && TopLevelViewModel->GetStackEditorData()->GetShowLinkedInputs())
		{
			return true;
		}
	}
	return false;
}

void UNiagaraStackViewModel::SetShowLinkedInputs(bool bInShowLinkedInputs)
{
	for (TSharedRef<FTopLevelViewModel> TopLevelViewModel : TopLevelViewModels)
	{
		if (TopLevelViewModel->IsValid())
		{
			TopLevelViewModel->GetStackEditorData()->SetShowLinkedInputs(bInShowLinkedInputs);
		}
	}

	// Showing linked inputs changes indenting so a full refresh is needed.
	OnSearchTextChanged(CurrentSearchText);
	RootEntry->RefreshChildren();
}

double UNiagaraStackViewModel::GetLastScrollPosition() const
{
	// TODO: Fix this with the new overview paradigm.
	if (EmitterHandleViewModel.IsValid())
	{
		return EmitterHandleViewModel.Pin()->GetEmitterViewModel()->GetEditorData().GetStackEditorData().GetLastScrollPosition();
	}
	return 0;
}

void UNiagaraStackViewModel::SetLastScrollPosition(double InLastScrollPosition)
{
	// TODO: Fix this with the new overview paradigm.
	if (EmitterHandleViewModel.IsValid())
	{
		EmitterHandleViewModel.Pin()->GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData().SetLastScrollPosition(InLastScrollPosition);
	}
}

void UNiagaraStackViewModel::NotifyStructureChanged()
{
	EntryStructureChanged();
}

void UNiagaraStackViewModel::PostUndo(bool bSuccess)
{
	RootEntry->RefreshChildren();
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::EntryStructureChanged()
{
	if (bUsesTopLevelViewModels)
	{
		RefreshTopLevelViewModels();
	}
	StructureChangedDelegate.Broadcast();
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::EntryDataObjectModified(UObject* ChangedObject)
{
	if (SystemViewModel.IsValid())
	{
		SystemViewModel.Pin()->NotifyDataObjectChanged(ChangedObject);
	}
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::EntryRequestFullRefresh()
{
	checkf(RootEntry != nullptr, TEXT("Can not process full refresh when the root entry doesn't exist"));
	RootEntry->RefreshChildren();
}

void UNiagaraStackViewModel::EntryRequestFullRefreshDeferred()
{
	bRefreshPending = true;
}

void UNiagaraStackViewModel::RefreshTopLevelViewModels()
{
	TArray<TSharedRef<FTopLevelViewModel>> CurrentTopLevelViewModels = TopLevelViewModels;
	TopLevelViewModels.Empty();

	TArray<UNiagaraStackEntry*> RootChildren;
	RootEntry->GetUnfilteredChildren(RootChildren);
	for (UNiagaraStackEntry* RootChild : RootChildren)
	{
		if (RootChild->IsA<UNiagaraStackSpacer>())
		{
			// Skip spacers they're not a good indication of top level object.  This will be unnecessary when spacers are removed.
			continue;
		}

		TSharedPtr<FTopLevelViewModel> TopLevelViewModel;
		if (RootChild->GetEmitterViewModel().IsValid())
		{
			TSharedPtr<FNiagaraEmitterHandleViewModel> RootChildEmitterHandleViewModel = RootChild->GetSystemViewModel()->GetEmitterHandleViewModelForEmitter(RootChild->GetEmitterViewModel()->GetEmitter());
			TSharedRef<FTopLevelViewModel>* CurrentTopLevelViewModelPtr = CurrentTopLevelViewModels.FindByPredicate(
				[&RootChildEmitterHandleViewModel](const TSharedRef<FTopLevelViewModel>& TopLevelViewModel) { return TopLevelViewModel->EmitterHandleViewModel == RootChildEmitterHandleViewModel; });
			if (CurrentTopLevelViewModelPtr != nullptr)
			{
				TopLevelViewModel = *CurrentTopLevelViewModelPtr;
			}
			else
			{
				TopLevelViewModel = MakeShared<FTopLevelViewModel>(RootChildEmitterHandleViewModel);
			}
		}
		else
		{
			TSharedRef<FTopLevelViewModel>* CurrentTopLevelViewModelPtr = CurrentTopLevelViewModels.FindByPredicate(
				[RootChild](const TSharedRef<FTopLevelViewModel>& TopLevelViewModel) { return TopLevelViewModel->SystemViewModel == RootChild->GetSystemViewModel(); });
			if (CurrentTopLevelViewModelPtr != nullptr)
			{
				TopLevelViewModel = *CurrentTopLevelViewModelPtr;
			}
			else
			{
				TopLevelViewModel = MakeShared<FTopLevelViewModel>(RootChild->GetSystemViewModel());
			}
		}
		if (TopLevelViewModels.ContainsByPredicate([&TopLevelViewModel](const TSharedRef<FTopLevelViewModel>& ExistingTopLevelViewModel) { return *TopLevelViewModel == *ExistingTopLevelViewModel; }) == false)
		{
			TopLevelViewModels.Add(TopLevelViewModel.ToSharedRef());
		}
	}
}

#undef LOCTEXT_NAMESPACE
