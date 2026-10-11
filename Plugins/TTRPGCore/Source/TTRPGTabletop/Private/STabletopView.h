#pragma once

#include "CoreMinimal.h"
#include "TabletopModel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SLeafWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/Texture2D.h"

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
    bool LoadBackground();
    bool ImportBackground(const FString& Path);
    float LightRange = 30;
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
    TStrongObjectPtr<UTexture2D> BackgroundTexture;
    FSlateBrush BackgroundBrush;
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
    TSharedRef<SWidget> Action(const FString& Text, TFunction<void()> Callback, bool Compact = false);
    TSharedRef<SWidget> Tool(const FString& Text, TTRPG::Tabletop::ETool ToolId);
    void RefreshRoster();
    void SwitchRole(TTRPG::Tabletop::ERole Role);
    void SaveMap();
    void LoadMap();
    void Refresh();
    void ChangeMap(int32 Id);
    TSharedRef<SWidget> Number(const FString& Name, TFunction<float()> Get, TFunction<void(float)> Set, float Min, float Max, TFunction<bool()> Enabled = {});
    TSharedRef<SWidget> Check(const FString& Name, TFunction<bool()> Get, TFunction<void(bool)> Set, TFunction<bool()> Enabled = {});
    TSharedRef<SWidget> Field(const FString& Name, TFunction<FString()> Get, TFunction<void(FString)> Set, TFunction<bool()> Enabled = {});
    TSharedRef<SWidget> MapsPanel();
    TSharedRef<SWidget> ToolsPanel();
    TSharedRef<SWidget> SettingsPanel();
    TSharedRef<SWidget> SheetPanel();
    TSharedRef<SWidget> VisionPanel();
    TSharedRef<SWidget> CombatPanel();
    TSharedRef<SWidget> EntryPanel();
    TSharedRef<SWidget> GameplayPanel();
    TSharedRef<SWidget> TokenPanel();
    TSharedRef<SWidget> StatusPanel();
    TSharedRef<SWidget> LightingPanel();
    TSharedRef<SWidget> ScriptPanel();
    TSharedRef<SWidget> ScriptLibrary();
    TSharedRef<SWidget> ScriptHelp();
    TSharedRef<SWidget> LiveText(TFunction<FString()> Get, int32 Size = 11);
    TSharedRef<SWidget> Guard(TSharedRef<SWidget> Content, TFunction<bool()> Enabled);
    void Enter(TTRPG::Tabletop::ERole Role);
    void Workspace(TTRPG::Tabletop::EWorkspace Value);
    void RefreshActions();
    void NewScript();
    void SaveScriptDraft(bool Run);
    virtual void Tick(const FGeometry&, double, float) override;
    TSharedPtr<SVerticalBox> MacroList;
    TSharedPtr<SVerticalBox> ScriptList;
    TSharedPtr<SVerticalBox> ApprovalList;
    TSharedPtr<SVerticalBox> LinkList;
    uint64 SeenRevision = MAX_uint64;
    int32 EntryPlayer = 1;
    int32 DeleteMapId = INDEX_NONE;
    bool bDeleteScript = false;
    FString AttributeName = TEXT("mana");
    float AttributeValue = 10;
    FString ConditionName = TEXT("Prone");
    FString BarName = TEXT("Mana");
    FString BarValue = TEXT("mana");
    FString BarMaximum = TEXT("maxMana");
    FString LinkName;
    FString LinkUrl = TEXT("https://");
    bool bLinkPlayerVisible = true;
    TTRPG::Tabletop::FTabletopScript ScriptDraft;
    TSharedPtr<SVerticalBox> MapList;
    int32 LeftPage = 0;
    int32 RightPage = 0;
    FString NewMapName = TEXT("New map");
    int32 NewWidth = 20;
    int32 NewHeight = 16;
    bool bNewHex = false;
    FString ImportPath;
    TSharedPtr<TTRPG::Tabletop::FBoardModel> Model;
    TSharedPtr<STabletopMap> Map;
    TSharedPtr<SVerticalBox> Roster;
};
