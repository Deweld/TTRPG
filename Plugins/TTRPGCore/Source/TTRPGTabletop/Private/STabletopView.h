#pragma once

#include "CoreMinimal.h"
#include "TabletopModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"

class SVerticalBox;

class STabletopMap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(STabletopMap) {}
        SLATE_ARGUMENT(TSharedPtr<TTRPG::Tabletop::FBoardModel>, Model)
        SLATE_EVENT(FSimpleDelegate, OnChanged)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(640, 480); }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual int32 OnPaint(const FPaintArgs&, const FGeometry&, const FSlateRect&, FSlateWindowElementList&, int32, const FWidgetStyle&, bool) const override;
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnMouseMove(const FGeometry&, const FPointerEvent&) override;
    virtual FReply OnMouseWheel(const FGeometry&, const FPointerEvent&) override;
    virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override;
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;
    void ResetView() { Zoom = 1.f; Pan = FVector2D::ZeroVector; }
    int32 ZoomPercent() const { return FMath::RoundToInt(Zoom * 100); }
private:
    float CellSize(const FGeometry&) const;
    FVector2D Origin(const FGeometry&) const;
    FIntPoint CellAt(const FGeometry&, FVector2D Local) const;
    void ApplyTool(FIntPoint Cell);
    TSharedPtr<TTRPG::Tabletop::FBoardModel> Model;
    FSimpleDelegate Changed;
    FVector2D Pan = FVector2D::ZeroVector;
    FVector2D LastPointer;
    FVector2D PressPointer;
    FIntPoint Hover = FIntPoint(-1, -1);
    FIntPoint LastBrush = FIntPoint(-1, -1);
    int32 DragId = INDEX_NONE;
    float Zoom = 1.f;
    bool bPanning = false;
    bool bPainting = false;
};

class STabletopView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STabletopView) {}
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
private:
    TSharedRef<SWidget> Label(const FString& Text, int32 Size = 11) const;
    TSharedRef<SWidget> Action(const FString& Text, TFunction<void()> Callback);
    TSharedRef<SWidget> Tool(const FString& Text, TTRPG::Tabletop::ETool ToolId);
    void RefreshRoster();
    void SwitchRole(TTRPG::Tabletop::ERole Role);
    void SaveMap();
    void LoadMap();
    TSharedPtr<TTRPG::Tabletop::FBoardModel> Model;
    TSharedPtr<STabletopMap> Map;
    TSharedPtr<SVerticalBox> Roster;
};
