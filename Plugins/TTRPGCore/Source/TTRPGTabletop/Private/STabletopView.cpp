#include "STabletopView.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using namespace TTRPG::Tabletop;
namespace
{
const FLinearColor Background(0.018f, 0.025f, 0.034f);
const FLinearColor Panel(0.033f, 0.044f, 0.056f);
const FLinearColor Ink(0.83f, 0.86f, 0.87f);
const FLinearColor Muted(0.39f, 0.47f, 0.52f);
const FLinearColor Teal(0.12f, 0.70f, 0.59f);
const FLinearColor Gold(0.87f, 0.58f, 0.22f);
const FSlateBrush* White() { return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")); }
FSlateFontInfo Font(int32 Size, bool bBold = false) { return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size); }
FLinearColor TokenColor(const FToken& T)
{
    return T.Kind == ETokenKind::Prop ? Gold : T.Kind == ETokenKind::NPC ? FLinearColor(0.77f, 0.29f, 0.25f)
        : T.bOwnedByPlayer ? Teal : FLinearColor(0.36f, 0.49f, 0.83f);
}
FString SavePath() { return FPaths::ProjectSavedDir() / TEXT("TabletopPrototype/Map.json"); }
}

void STabletopMap::Construct(const FArguments& Args)
{
    Model = Args._Model; Changed = Args._OnChanged;
    SetClipping(EWidgetClipping::ClipToBounds);
    ForceVolatile(true);
}
float STabletopMap::CellSize(const FGeometry& G) const
{
    return FMath::Max(8.f, FMath::Min(float(G.GetLocalSize().X / (FBoardModel::Columns + 2)), float(G.GetLocalSize().Y / (FBoardModel::Rows + 2)))) * Zoom;
}
FVector2D STabletopMap::Origin(const FGeometry& G) const
{
    return (G.GetLocalSize() - FVector2D(FBoardModel::Columns, FBoardModel::Rows) * CellSize(G)) * .5 + Pan;
}
FIntPoint STabletopMap::CellAt(const FGeometry& G, FVector2D Local) const
{
    const FVector2D P = (Local - Origin(G)) / CellSize(G);
    return FIntPoint(FMath::FloorToInt(P.X), FMath::FloorToInt(P.Y));
}
int32 STabletopMap::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
    auto Box = [&](FVector2D P, FVector2D S, FLinearColor Color, int32 L = 0, const FSlateBrush* Brush = nullptr)
    {
        FSlateDrawElement::MakeBox(Out, Layer + L, G.ToPaintGeometry(S, FSlateLayoutTransform(P)), Brush ? Brush : White(), ESlateDrawEffect::None, Color);
    };
    auto Text = [&](FVector2D P, const FString& Value, int32 Size, FLinearColor Color, int32 L = 4)
    {
        FSlateDrawElement::MakeText(Out, Layer + L, G.ToPaintGeometry(FVector2D(1, 1), FSlateLayoutTransform(P)), Value, Font(Size, true), ESlateDrawEffect::None, Color);
    };
    const float S = CellSize(G);
    const FVector2D O = Origin(G);
    Box(FVector2D::ZeroVector, G.GetLocalSize(), Background);
    Box(O - FVector2D(5, 5), FVector2D(FBoardModel::Columns * S + 10, FBoardModel::Rows * S + 10), FLinearColor(0.08f, 0.12f, 0.14f));
    for (int32 Y = 0; Y < FBoardModel::Rows; ++Y)
    {
        for (int32 X = 0; X < FBoardModel::Columns; ++X)
        {
            const FIntPoint C(X, Y);
            const FVector2D P = O + FVector2D(X, Y) * S;
            const bool bRim = X == 0 || X == FBoardModel::Columns - 1 || Y == 0 || Y == FBoardModel::Rows - 1;
            const bool bPath = (X >= 11 && X <= 12) || (Y >= 10 && Y <= 12);
            const float Variation = ((X * 7 + Y * 13) % 5) * .007f;
            FLinearColor Tile = bRim ? FLinearColor(.035f, .072f, .066f) : bPath ? FLinearColor(.14f + Variation, .15f + Variation, .145f)
                : FLinearColor(.080f + Variation, .11f + Variation, .103f);
            Box(P, FVector2D(S - 1, S - 1), Tile, 1);
            if (Model->Walls.Contains(C))
            {
                Box(P + FVector2D(2, 4), FVector2D(S - 3, S - 4), FLinearColor(.022f, .031f, .035f), 2);
                Box(P + FVector2D(2, 1), FVector2D(S - 5, S - 6), FLinearColor(.25f, .28f, .28f), 2);
                Box(P + FVector2D(2, 1), FVector2D(S - 5, 2), FLinearColor(.37f, .40f, .38f), 3);
                Box(P + FVector2D(S * .5, 2), FVector2D(1, S - 7), FLinearColor(.14f, .17f, .17f), 3);
            }
        }
    }
    for (int32 X = 0; X < FBoardModel::Columns; ++X)
        Text(O + FVector2D((X + .38f) * S, -19), FString::Chr(TCHAR('A' + X)), 9, Muted);
    for (int32 Y = 0; Y < FBoardModel::Rows; ++Y)
        Text(O + FVector2D(-25, (Y + .3f) * S), FString::FromInt(Y + 1), 9, Muted);

    if (Model->IsInside(Hover))
    {
        const bool bValid = Model->Tool == ETool::Erase || Model->IsAvailable(Hover, DragId);
        FLinearColor Color = bValid ? Teal : Gold; Color.A = .24f;
        Box(O + FVector2D(Hover) * S, FVector2D(S - 1, S - 1), Color, 3);
    }
    // Height-relative rounding keeps token circles visible at every zoom level.
    const FSlateRoundedBoxBrush Circle(FLinearColor::White);
    const FSlateRoundedBoxBrush Square(FLinearColor::White, 4.f);
    for (const FToken& T : Model->Tokens)
    {
        if (!Model->IsVisible(T)) continue;
        const FVector2D P = O + FVector2D(T.Cell) * S;
        const bool bSelected = T.Id == Model->SelectedId;
        const FLinearColor Color = TokenColor(T);
        const FSlateBrush* Shape = T.Kind == ETokenKind::Prop ? &Square : &Circle;
        if (bSelected) Box(P + FVector2D(1, 1), FVector2D(S - 3, S - 3), FLinearColor(.90f, .82f, .57f), 5, Shape);
        Box(P + FVector2D(4, 6), FVector2D(S - 9, S - 9), FLinearColor(0, 0, 0, .7f), 6, Shape);
        Box(P + FVector2D(4, 3), FVector2D(S - 9, S - 9), T.bHidden ? Color * .4f : Color, 7, Shape);
        Text(P + FVector2D(S * .32f, S * .15f), T.Kind == ETokenKind::Prop ? TEXT("+") : T.Name.Left(1), FMath::Clamp(int32(S * .30f), 9, 24), Background, 8);
        const FString Name = T.bHidden ? T.Name + TEXT(" [DM]") : T.Name;
        const float LabelW = FMath::Max(S, Name.Len() * 5.7f + 10);
        Box(P + FVector2D((S - LabelW) * .5f, S - 5), FVector2D(LabelW, 16), FLinearColor(.025f, .036f, .046f, .94f), 8, &Square);
        Text(P + FVector2D((S - LabelW) * .5f + 5, S - 5), Name, 8, bSelected ? Gold : Ink, 9);
    }
    if (DragId != INDEX_NONE && Model->IsInside(Hover))
        Box(O + FVector2D(Hover) * S + FVector2D(5, 5), FVector2D(S - 11, S - 11), FLinearColor(.2f, .8f, .65f, .5f), 10, &Circle);
    return Layer + 11;
}
void STabletopMap::ApplyTool(FIntPoint Cell)
{
    if (!Model->IsInside(Cell)) return;
    if (Model->Tool == ETool::Wall || Model->Tool == ETool::Erase)
        Model->PaintWall(Cell, Model->Tool == ETool::Wall);
    else if (Model->Tool != ETool::Select)
        Model->Place(Model->Tool == ETool::Hero ? ETokenKind::Hero : Model->Tool == ETool::NPC ? ETokenKind::NPC : ETokenKind::Prop, Cell);
    Changed.ExecuteIfBound();
}
FReply STabletopMap::OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    LastPointer = PressPointer = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    Hover = CellAt(G, LastPointer);
    if (E.GetEffectingButton() == EKeys::RightMouseButton || E.GetEffectingButton() == EKeys::MiddleMouseButton)
    {
        bPanning = true;
        return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
    }
    if (E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
    if (Model->Tool != ETool::Select)
    {
        ApplyTool(Hover); LastBrush = Hover;
        bPainting = Model->Tool == ETool::Wall || Model->Tool == ETool::Erase;
    }
    else if (const FToken* T = Model->TokenAt(Hover))
    {
        const int32 Id = T->Id;
        DragId = Model->CanControl(*T) ? Id : INDEX_NONE;
        Model->Select(Id);
    }
    else if (Model->SelectedId != INDEX_NONE) Model->Move(Model->SelectedId, Hover);
    Changed.ExecuteIfBound();
    return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this));
}
FReply STabletopMap::OnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
    const FVector2D P = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    if (bPanning) Pan += P - LastPointer;
    LastPointer = P; Hover = CellAt(G, P);
    if (bPainting && Hover != LastBrush)
    {
        if (Model->IsInside(LastBrush) && Model->IsInside(Hover))
        {
            Model->PaintStroke(LastBrush, Hover, Model->Tool == ETool::Wall);
            Changed.ExecuteIfBound();
        }
        else ApplyTool(Hover);
        LastBrush = Hover;
    }
    return FReply::Handled();
}
FReply STabletopMap::OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E)
{
    if (!HasMouseCapture()) return FReply::Unhandled();
    const FVector2D P = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    if (E.GetEffectingButton() == EKeys::LeftMouseButton && DragId != INDEX_NONE && (P - PressPointer).Size() > 4)
        Model->Move(DragId, CellAt(G, P));
    DragId = INDEX_NONE; bPanning = false; bPainting = false;
    Changed.ExecuteIfBound();
    return FReply::Handled().ReleaseMouseCapture();
}
void STabletopMap::OnMouseCaptureLost(const FCaptureLostEvent&)
{
    DragId = INDEX_NONE; bPanning = false; bPainting = false;
}
FReply STabletopMap::OnMouseWheel(const FGeometry& G, const FPointerEvent& E)
{
    const FVector2D P = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    const FVector2D BoardPoint = (P - Origin(G)) / CellSize(G);
    Zoom = FMath::Clamp(Zoom * FMath::Pow(1.15f, E.GetWheelDelta()), .6f, 2.8f);
    Pan += P - (Origin(G) + BoardPoint * CellSize(G));
    Hover = CellAt(G, P);
    return FReply::Handled();
}
FReply STabletopMap::OnKeyDown(const FGeometry&, const FKeyEvent& E)
{
    if (E.GetKey() == EKeys::Home) ResetView();
    else if (E.GetKey() == EKeys::Escape) { Model->Tool = ETool::Select; Model->SelectedId = INDEX_NONE; }
    else return FReply::Unhandled();
    Changed.ExecuteIfBound();
    return FReply::Handled();
}

TSharedRef<SWidget> STabletopView::Label(const FString& Text, int32 Size) const
{
    return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(Size, true)).ColorAndOpacity(Size > 14 ? Ink : Muted).AutoWrapText(true);
}
TSharedRef<SWidget> STabletopView::Action(const FString& Text, TFunction<void()> Callback)
{
    return SNew(SButton).ContentPadding(FMargin(12, 9)).ButtonColorAndOpacity(FLinearColor(.10f, .14f, .17f))
        .OnClicked_Lambda([Callback]() { Callback(); return FReply::Handled(); })
        [SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(10, true)).ColorAndOpacity(Ink)];
}
TSharedRef<SWidget> STabletopView::Tool(const FString& Text, ETool Id)
{
    return SNew(SButton).ContentPadding(FMargin(12, 11))
        .ButtonColorAndOpacity_Lambda([this, Id]() { return Model->Tool == Id ? FLinearColor(.10f, .37f, .32f) : FLinearColor(.065f, .087f, .105f); })
        .OnClicked_Lambda([this, Id]() { Model->Tool = Id; Model->Status = Id == ETool::Select ? TEXT("Select a token, then click a cell or drag to move.") : TEXT("Click the grid to place. Wall tools also support dragging."); return FReply::Handled(); })
        [SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(11)).ColorAndOpacity(Ink)];
}
void STabletopView::SwitchRole(ERole Role) { Model->SetRole(Role); RefreshRoster(); }
void STabletopView::RefreshRoster()
{
    if (!Roster) return;
    Roster->ClearChildren();
    for (const FToken& T : Model->Tokens)
    {
        if (!Model->IsVisible(T)) continue;
        const int32 Id = T.Id;
        Roster->AddSlot().AutoHeight().Padding(0, 3)
        [SNew(SButton).ContentPadding(FMargin(10, 8))
            .ButtonColorAndOpacity_Lambda([this, Id]() { return Model->SelectedId == Id ? FLinearColor(.15f, .24f, .25f) : FLinearColor(.06f, .077f, .09f); })
            .OnClicked_Lambda([this, Id]() { Model->Select(Id); return FReply::Handled(); })
            [SNew(STextBlock).Text(FText::FromString(T.Name + (T.bHidden ? TEXT("  [DM]") : T.bOwnedByPlayer ? TEXT("  / you") : TEXT(""))))
                .Font(Font(11)).ColorAndOpacity(TokenColor(T))]];
    }
}
void STabletopView::SaveMap()
{
    if (Model->Role != ERole::DM) return;
    const FString Path = SavePath();
    const FString Temp = Path + TEXT(".tmp");
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
    if (FFileHelper::SaveStringToFile(Model->ToJson(), *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        && IFileManager::Get().Move(*Path, *Temp, true, true)) Model->Log(TEXT("Map saved locally. Tokens, walls and HP included."));
    else Model->Status = TEXT("Map save failed. Check the project's Saved folder permissions.");
}
void STabletopView::LoadMap()
{
    if (Model->Role != ERole::DM) return;
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *SavePath())) Model->Status = TEXT("No saved map yet. Use Save map first.");
    else { Model->LoadJson(Json); RefreshRoster(); }
}
void STabletopView::Construct(const FArguments&)
{
    Model = MakeShared<FBoardModel>();
    auto DMOnly = [this]() { return Model->Role == ERole::DM ? EVisibility::Visible : EVisibility::Collapsed; };
    auto CanEdit = [this]() { const FToken* T = Model->Find(Model->SelectedId); return T && Model->CanControl(*T) && T->Kind != ETokenKind::Prop; };
    ChildSlot
    [SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Background).Padding(0)
    [SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(22, 16))
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("WAYFARER"))).Font(Font(21, true)).ColorAndOpacity(Ink)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Label(TEXT("T A B L E T O P   L A B"), 8)]]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 18, 0)[Label(TEXT("LOCAL SANDBOX  /  24 x 18 GRID"), 9)]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3, 0)
            [SNew(SButton).ContentPadding(FMargin(18, 12)).ButtonColorAndOpacity_Lambda([this]() { return Model->Role == ERole::DM ? FLinearColor(.16f, .42f, .35f) : Panel; })
                .OnClicked_Lambda([this]() { SwitchRole(ERole::DM); return FReply::Handled(); })[SNew(STextBlock).Text(FText::FromString(TEXT("DM VIEW"))).Font(Font(11, true)).ColorAndOpacity(Ink)]]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(3, 0)
            [SNew(SButton).ContentPadding(FMargin(18, 12)).ButtonColorAndOpacity_Lambda([this]() { return Model->Role == ERole::Player ? FLinearColor(.16f, .42f, .35f) : Panel; })
                .OnClicked_Lambda([this]() { SwitchRole(ERole::Player); return FReply::Handled(); })[SNew(STextBlock).Text(FText::FromString(TEXT("PLAYER VIEW"))).Font(Font(11, true)).ColorAndOpacity(Ink)]]]]
        + SVerticalBox::Slot().FillHeight(1).Padding(12)
        [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SBox).WidthOverride(210)
            [SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(16)
            [SNew(SScrollBox)
                + SScrollBox::Slot()[SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[Label(TEXT("THE WATCHTOWER"), 16)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 20)[Label(TEXT("Ruined outpost / encounter 01"), 9)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[Label(TEXT("TABLE TOOLS"), 9)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Tool(TEXT("Select / move"), ETool::Select)]
                    + SVerticalBox::Slot().AutoHeight()[SNew(SVerticalBox).Visibility_Lambda(DMOnly)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Tool(TEXT("+  Player token"), ETool::Hero)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Tool(TEXT("+  NPC token"), ETool::NPC)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Tool(TEXT("+  Supply crate"), ETool::Prop)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 12, 0, 3)[Tool(TEXT("Paint wall"), ETool::Wall)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Tool(TEXT("Erase wall"), ETool::Erase)]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 16, 0, 3)[Action(TEXT("Save map"), [this]() { SaveMap(); })]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)[Action(TEXT("Load saved map"), [this]() { LoadMap(); })]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 20, 0, 8)[Label(TEXT("ON THE MAP"), 9)]
                    + SVerticalBox::Slot().AutoHeight()[SAssignNew(Roster, SVerticalBox)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 0)[Label(TEXT("Teal: your characters\nBlue: party members\nCoral: NPCs   Gold: objects"), 9)]
                ]]]]
            + SHorizontalBox::Slot().FillWidth(1).Padding(12, 0)
            [SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight().Padding(6, 0, 6, 10)
                [SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Model->Role == ERole::DM ? TEXT("ENCOUNTER MAP  /  DM CONTROL") : TEXT("ENCOUNTER MAP  /  PLAYER CONTROL")); }).Font(Font(10, true)).ColorAndOpacity(Muted)]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0)[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(FString::Printf(TEXT("%d%%"), Map ? Map->ZoomPercent() : 100)); }).Font(Font(10)).ColorAndOpacity(Teal)]
                    + SHorizontalBox::Slot().AutoWidth()[Action(TEXT("Fit map"), [this]() { Map->ResetView(); })]]
                + SVerticalBox::Slot().FillHeight(1)[SAssignNew(Map, STabletopMap).Model(Model).OnChanged(FSimpleDelegate::CreateSP(this, &STabletopView::RefreshRoster))]
                + SVerticalBox::Slot().AutoHeight().Padding(8, 14, 8, 4)[Label(TEXT("DRAG token   /   RIGHT-DRAG pan   /   WHEEL zoom   /   HOME fit"), 9)]]
            + SHorizontalBox::Slot().AutoWidth()
            [SNew(SBox).WidthOverride(240)
            [SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(18)
            [SNew(SScrollBox)
                + SScrollBox::Slot()[SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[Label(TEXT("TOKEN INSPECTOR"), 9)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 14, 0, 6)[SNew(STextBlock).Text_Lambda([this]() { const FToken* T = Model->Find(Model->SelectedId); return FText::FromString(T ? T->Name : TEXT("Nothing selected")); }).Font(Font(21, true)).ColorAndOpacity(Ink).AutoWrapText(true)]
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]()
                    {
                        const FToken* T = Model->Find(Model->SelectedId);
                        return FText::FromString(T ? FString::Printf(TEXT("CELL %c%d  /  %s\n%s"), TCHAR('A' + T->Cell.X), T->Cell.Y + 1,
                            T->Kind == ETokenKind::Prop ? TEXT("OBJECT") : T->Kind == ETokenKind::NPC ? TEXT("NPC") : TEXT("CHARACTER"),
                            Model->CanControl(*T) ? TEXT("You can control this token") : TEXT("View only / another participant")) : TEXT("Choose a token on the map\nor in the roster."));
                    }).Font(Font(10)).ColorAndOpacity(Muted).AutoWrapText(true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 18)[SNew(STextBlock).Text_Lambda([this]() { const FToken* T = Model->Find(Model->SelectedId); return FText::FromString(T && T->Kind != ETokenKind::Prop ? FString::Printf(TEXT("%d  HP"), T->HP) : TEXT("--")); }).Font(Font(28, true)).ColorAndOpacity(Teal)]
                    + SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox).IsEnabled_Lambda(CanEdit)
                        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 4, 0)[Action(TEXT("- 1 HP"), [this]() { Model->ChangeHP(-1); })]
                        + SHorizontalBox::Slot().FillWidth(1).Padding(4, 0, 0, 0)[Action(TEXT("+ 1 HP"), [this]() { Model->ChangeHP(1); })]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNew(SBox).IsEnabled_Lambda(CanEdit)[Action(TEXT("Roll d20"), [this]() { Model->RollD20(); })]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 8)[SNew(SBox).Visibility_Lambda(DMOnly)
                        [SNew(SButton).ContentPadding(FMargin(12, 10)).ButtonColorAndOpacity(FLinearColor(.20f, .15f, .08f))
                        .OnClicked_Lambda([this]() { Model->ToggleHidden(); RefreshRoster(); return FReply::Handled(); })
                        [SNew(STextBlock).Text_Lambda([this]() { const FToken* T = Model->Find(Model->SelectedId); return FText::FromString(T && T->bHidden ? TEXT("Reveal to players") : TEXT("Hide from players")); }).Font(Font(10, true)).ColorAndOpacity(Gold)]]]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 25, 0, 12)[Label(TEXT("TABLE ACTIVITY"), 9)]
                    + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(FString::Join(Model->Activity, TEXT("\n\n"))); }).Font(Font(10)).ColorAndOpacity(Ink).AutoWrapText(true)]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 25, 0, 0)[Label(TEXT("LOCAL PREVIEW\nRole switching is for testing.\nNo multiplayer connection."), 9)]
                ]]]]
        ]
        + SVerticalBox::Slot().AutoHeight()
        [SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(22, 12))
            [SNew(STextBlock).Text_Lambda([this]() { return FText::FromString(Model->Status); }).Font(Font(11)).ColorAndOpacity(Ink).AutoWrapText(true)]]
    ]];
    RefreshRoster();
}
