// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "GraphEditor.h"

class FNiagaraOverviewGraphViewModel;
struct FActionMenuContent;
class FMenuBuilder;
class UEdGraph;
class UEdGraphNode;

class SNiagaraOverviewGraph : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraOverviewGraph) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraOverviewGraphViewModel> InViewModel);

	~SNiagaraOverviewGraph();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	void ViewModelSelectionChanged();

	void GraphSelectionChanged(const TSet<UObject*>& SelectedNodes);

	/** Called to create context menu when right-clicking on graph */
	FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);

	void OnCreateComment();

	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	void CreateAddEmitterMenuContent(FMenuBuilder& MenuBuilder, UEdGraph* InGraph);

	void ZoomToFit();

	void ZoomToFitAll();

private:
	TSharedPtr<FNiagaraOverviewGraphViewModel> ViewModel;
	TSharedPtr<SGraphEditor> GraphEditor;

	bool bUpdatingViewModelSelectionFromGraph;
	bool bUpdatingGraphSelectionFromViewModel;

	int32 ZoomToFitFrameDelay;
};