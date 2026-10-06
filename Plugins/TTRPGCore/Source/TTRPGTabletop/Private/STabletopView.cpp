#include "STabletopView.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

using namespace TTRPG::Tabletop;
namespace
{
const FLinearColor Background(.018f,.025f,.034f), Panel(.033f,.044f,.056f), Ink(.83f,.86f,.87f), Muted(.45f,.53f,.58f), Teal(.12f,.70f,.59f);
const FSlateBrush* White(){return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));}
FSlateFontInfo Font(int32 Size,bool Bold=false){return FCoreStyle::GetDefaultFontStyle(Bold?TEXT("Bold"):TEXT("Regular"),Size);}
FString SavePath(){return FPaths::ProjectSavedDir()/TEXT("TabletopPrototype/Session.json");}
}
TSharedRef<SWidget> STabletopView::Label(const FString& Text,int32 Size) const
{
    return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(Size,Size>=14)).ColorAndOpacity(Size>=14?Ink:Muted).AutoWrapText(true);
}
TSharedRef<SWidget> STabletopView::Action(const FString& Text,TFunction<void()> Callback,bool Compact)
{
    return SNew(SButton).ContentPadding(FMargin(Compact?2:10,8)).ButtonColorAndOpacity(FLinearColor(.09f,.15f,.17f))
        .OnClicked_Lambda([Callback](){Callback(); return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(Compact?10:11,true)).ColorAndOpacity(Ink)];
}
TSharedRef<SWidget> STabletopView::Tool(const FString& Text,ETool Id)
{
    return SNew(SButton).ContentPadding(FMargin(10,9))
        .ButtonColorAndOpacity_Lambda([this,Id](){return Model->Tool==Id?FLinearColor(.10f,.37f,.32f):Panel;})
        .OnClicked_Lambda([this,Id](){if(Model->Role==ERole::DM || Id==ETool::Select) Model->Tool=Id; return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(11)).ColorAndOpacity(Ink)];
}
TSharedRef<SWidget> STabletopView::Number(const FString& Name,TFunction<float()> Get,TFunction<void(float)> Set,float Min,float Max,TFunction<bool()> Enabled)
{
    return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,3)[Label(Name,10)]
        +SVerticalBox::Slot().AutoHeight()[SNew(SSpinBox<float>).Value_Lambda([Get](){return Get();})
            .MinValue(Min).MaxValue(Max).MinSliderValue(Min).MaxSliderValue(Max).Delta(Max<=5?.1f:1.f).Font(Font(12))
            .IsEnabled_Lambda([Enabled](){return !Enabled || Enabled();})
            .OnValueChanged_Lambda([Set,Min,Max](float V){if(FMath::IsFinite(V)) Set(FMath::Clamp(V,Min,Max));})
            .OnValueCommitted_Lambda([Set,Min,Max](float V,ETextCommit::Type){if(FMath::IsFinite(V)) Set(FMath::Clamp(V,Min,Max));})];
}
TSharedRef<SWidget> STabletopView::Check(const FString& Name,TFunction<bool()> Get,TFunction<void(bool)> Set,TFunction<bool()> Enabled)
{
    return SNew(SCheckBox).IsChecked_Lambda([Get](){return Get()?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
        .IsEnabled_Lambda([Enabled](){return !Enabled || Enabled();})
        .OnCheckStateChanged_Lambda([Set](ECheckBoxState S){Set(S==ECheckBoxState::Checked);})
        [SNew(STextBlock).Text(FText::FromString(Name)).Font(Font(11)).ColorAndOpacity(Ink).AutoWrapText(true)];
}
TSharedRef<SWidget> STabletopView::Field(const FString& Name,TFunction<FString()> Get,TFunction<void(FString)> Set,TFunction<bool()> Enabled)
{
    return SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(0,6,0,3)[Label(Name,10)]
        +SVerticalBox::Slot().AutoHeight()[SNew(SEditableTextBox).Text_Lambda([Get](){return FText::FromString(Get());})
            .Font(Font(11)).IsEnabled_Lambda([Enabled](){return !Enabled || Enabled();})
            .OnTextCommitted_Lambda([Set](const FText& T,ETextCommit::Type){Set(T.ToString());})];
}
void STabletopView::RefreshRoster()
{
    if(!Roster) return; Roster->ClearChildren();
    for(const FToken& T:Model->Tokens)
    {
        if(!Model->IsVisible(T)) continue; const int32 Id=T.Id;
        Roster->AddSlot().AutoHeight().Padding(0,3)
        [SNew(SButton).ContentPadding(FMargin(8,7)).ButtonColorAndOpacity_Lambda([this,Id](){return Model->SelectedId==Id?FLinearColor(.12f,.29f,.26f):Panel;})
        .OnClicked_Lambda([this,Id](){Model->Select(Id); return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(T.Name+(T.bHidden?TEXT(" [DM]"):T.OwnerId==Model->ViewingPlayerId?TEXT(" / you"):TEXT(""))))
            .Font(Font(11)).ColorAndOpacity(T.OwnerId==Model->ViewingPlayerId?Teal:Ink)]];
    }
}
void STabletopView::Refresh()
{
    RefreshRoster(); if(!MapList) return; MapList->ClearChildren();
    for(const FMapData& M:Model->Maps)
    {
        if(Model->Role!=ERole::DM && (!Model->Player() || M.Id!=Model->Player()->MapId)) continue;
        const int32 Id=M.Id;
        MapList->AddSlot().AutoHeight().Padding(0,4)
        [SNew(SButton).ContentPadding(FMargin(10,9)).ButtonColorAndOpacity_Lambda([this,Id](){return Model->ActiveMapId==Id?FLinearColor(.12f,.30f,.26f):Panel;})
        .OnClicked_Lambda([this,Id](){ChangeMap(Id); return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(FString::Printf(TEXT("%s\n%d x %d / %s"),*M.Name,M.Width,M.Height,M.Grid==EGrid::Hex?TEXT("HEX"):TEXT("SQUARE"))))
            .Font(Font(11)).ColorAndOpacity(Ink).AutoWrapText(true)]];
    }
}
void STabletopView::ChangeMap(int32 Id)
{
    if(Model->SwitchMap(Id)){Map->LoadBackground(); Map->ResetView(); Refresh();}
}
void STabletopView::SwitchRole(ERole Role)
{
    Model->SetRole(Role); Map->LoadBackground(); Map->ResetView(); Refresh();
}
void STabletopView::SaveMap()
{
    if(Model->Role!=ERole::DM) return;
    FString Path=SavePath(), Temp=Path+TEXT(".tmp"); IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    if(FFileHelper::SaveStringToFile(Model->ToJson(),*Temp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)
        && IFileManager::Get().Move(*Path,*Temp,true,true)) Model->Log(TEXT("Session saved: maps, player assignments, sheets, lights, and turns."));
    else Model->Status=TEXT("Session save failed. Existing save has been preserved where possible.");
}
void STabletopView::LoadMap()
{
    if(Model->Role!=ERole::DM) return;
    FString Json;
    FString Path=SavePath(); if(!FPaths::FileExists(Path)) Path=FPaths::ProjectSavedDir()/TEXT("TabletopPrototype/Map.json");
    if(IFileManager::Get().FileSize(*Path)>4*1024*1024 || !FFileHelper::LoadFileToString(Json,*Path)) Model->Status=TEXT("No readable session save. Save a session first.");
    else if(Model->LoadJson(Json)){Map->LoadBackground(); Map->ResetView(); Refresh();}
}
TSharedRef<SWidget> STabletopView::MapsPanel()
{
    auto DM=[this](){return Model->Role==ERole::DM;};
    auto DMVisibility=[this](){return Model->Role==ERole::DM?EVisibility::Visible:EVisibility::Collapsed;};
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("MAP LIBRARY"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[SAssignNew(MapList,SVerticalBox)]
    +SVerticalBox::Slot().AutoHeight()[SNew(SVerticalBox).Visibility_Lambda(DMVisibility)
        +SVerticalBox::Slot().AutoHeight().Padding(0,12)[Label(TEXT("PLAYER ASSIGNMENT"),12)]
        +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){const auto* P=Model->Player(); const auto* M=P?Model->FindMap(P->MapId):nullptr; return FText::FromString(P&&M?P->Name+TEXT("\nCurrently: ")+M->Name:TEXT("No player"));}).Font(Font(11)).ColorAndOpacity(Ink).AutoWrapText(true)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,7)[Action(TEXT("Bring this player to this map"),[this](){Model->AssignPlayer(Model->ViewingPlayerId,Model->ActiveMapId); Refresh();})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,0)[Label(TEXT("CREATE MAP"),12)]
        +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Name"),[this](){return NewMapName;},[this](FString V){NewMapName=V.Left(80);})]
        +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Columns"),[this](){return float(NewWidth);},[this](float V){NewWidth=FMath::RoundToInt(V);},4,64)]
        +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Rows"),[this](){return float(NewHeight);},[this](float V){NewHeight=FMath::RoundToInt(V);},4,64)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Check(TEXT("Hex grid"),[this](){return bNewHex;},[this](bool V){bNewHex=V;})]
        +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Create map"),[this](){if(Model->AddMap(NewMapName,NewWidth,NewHeight,bNewHex?EGrid::Hex:EGrid::Square)!=INDEX_NONE){Map->ResetView(); Map->LoadBackground(); Refresh();}})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,18,0,4)[Action(TEXT("Save session"),[this](){SaveMap();})]
        +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Load saved session"),[this](){LoadMap();})]];
}
TSharedRef<SWidget> STabletopView::ToolsPanel()
{
    TSharedRef<SVerticalBox> Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(TEXT("MAP TOOLS"),15)];
    Box->AddSlot().AutoHeight().Padding(0,6)[Tool(TEXT("Select / move"),ETool::Select)];
    const TArray<TPair<FString,ETool>> Items={{TEXT("+ Player character"),ETool::Hero},{TEXT("+ NPC"),ETool::NPC},{TEXT("+ Crate"),ETool::Prop},
        {TEXT("Paint blocking wall"),ETool::Wall},{TEXT("Erase wall"),ETool::Erase},{TEXT("Place / update light"),ETool::Light},{TEXT("Remove light"),ETool::EraseLight}};
    for(const auto& Item:Items) Box->AddSlot().AutoHeight().Padding(0,3)
        [SNew(SBox).Visibility_Lambda([this](){return Model->Role==ERole::DM?EVisibility::Visible:EVisibility::Collapsed;})[Tool(Item.Key,Item.Value)]];
    Box->AddSlot().AutoHeight().Padding(0,8)[Number(TEXT("New light range (units)"),[this](){return Map?Map->LightRange:30;},[this](float V){Map->LightRange=V;},1,500,[this](){return Model->Role==ERole::DM;})];
    Box->AddSlot().AutoHeight().Padding(0,16,0,4)[Label(TEXT("VISIBLE TOKENS"),12)];
    Box->AddSlot().AutoHeight()[SAssignNew(Roster,SVerticalBox)];
    return Box;
}
TSharedRef<SWidget> STabletopView::SettingsPanel()
{
    auto DM=[this](){return Model->Role==ERole::DM;};
    return SNew(SVerticalBox).IsEnabled_Lambda(DM)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("MAP SETTINGS"),15)]
    +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Map name"),[this](){return Model->Map().Name;},[this](FString V){if(Model->Role==ERole::DM && !V.TrimStartAndEnd().IsEmpty()){Model->Map().Name=V.Left(80); Model->Touch(); Refresh();}})]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Columns (safe resize)"),[this](){return float(Model->Map().Width);},[this](float V){Model->ResizeMap(FMath::RoundToInt(V),Model->Map().Height); Map->ResetView(); Refresh();},4,64)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Rows (safe resize)"),[this](){return float(Model->Map().Height);},[this](float V){Model->ResizeMap(Model->Map().Width,FMath::RoundToInt(V)); Map->ResetView(); Refresh();},4,64)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,10)[Check(TEXT("Hex grid (off = square)"),[this](){return Model->Map().Grid==EGrid::Hex;},[this](bool V){Model->Map().Grid=V?EGrid::Hex:EGrid::Square; Model->Touch(); Map->ResetView(); Refresh();})]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Distance units per cell"),[this](){return Model->Map().UnitsPerCell;},[this](float V){Model->Map().UnitsPerCell=V; Model->Touch();},.1f,100)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,5)[Check(TEXT("Dynamic token vision"),[this](){return Model->Map().bVision;},[this](bool V){Model->Map().bVision=V; Model->Touch(); RefreshRoster();})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,5)[Check(TEXT("Darkness / require light"),[this](){return Model->Map().bDarkness;},[this](bool V){Model->Map().bDarkness=V; Model->Touch();})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,12,0,5)[Check(TEXT("Limit player movement"),[this](){return Model->Map().bLimitMovement;},[this](bool V){Model->Map().bLimitMovement=V; Model->Touch();})]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Distance allowance (units)"),[this](){return Model->Map().MovementLimit;},[this](float V){Model->Map().MovementLimit=V; Model->Touch();},0,1000)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[Action(TEXT("Refresh movement allowances"),[this](){Model->ResetMovement();})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,0)[Label(TEXT("BACKGROUND IMAGE"),12)]
    +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Local PNG / JPEG file path"),[this](){return ImportPath;},[this](FString V){ImportPath=V;})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Action(TEXT("Import image"),[this](){Map->ImportBackground(ImportPath);})]
    +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Use block background"),[this](){Model->Map().ImagePath.Empty(); Model->Touch(); Map->LoadBackground();})]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Image scale"),[this](){return Model->Map().ImageScale;},[this](float V){Model->Map().ImageScale=V; Model->Touch();},.1f,5)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Image offset X (cells)"),[this](){return Model->Map().ImageX;},[this](float V){Model->Map().ImageX=V; Model->Touch();},-64,64)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Image offset Y (cells)"),[this](){return Model->Map().ImageY;},[this](float V){Model->Map().ImageY=V; Model->Touch();},-64,64)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Grid opacity (0 to 1)"),[this](){return Model->Map().GridOpacity;},[this](float V){Model->Map().GridOpacity=V; Model->Touch();},0,1)];
}
TSharedRef<SWidget> STabletopView::SheetPanel()
{
    auto Can=[this](){const FToken* T=Model->Selected(); return T && Model->CanControl(*T) && T->Kind!=ETokenKind::Prop;};
    auto DM=[this,Can](){return Can() && Model->Role==ERole::DM;};
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("CHARACTER SHEET"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[SNew(STextBlock).Text_Lambda([this](){const auto* T=Model->Selected(); return FText::FromString(T?FString::Printf(TEXT("%s / cell %d, %d"),Model->CanControl(*T)?TEXT("You control this character"):TEXT("View only"),T->Cell.X+1,T->Cell.Y+1):TEXT("Select a visible token."));}).Font(Font(10)).ColorAndOpacity(Muted).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Name"),[this](){auto* T=Model->Selected(); return T?T->Name:TEXT("");},[this](FString V){if(auto* T=Model->Selected()){Model->UpdateSheet(T->Sheet,V,T->HP); RefreshRoster();}},Can)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Current HP"),[this](){auto* T=Model->Selected(); return T?float(T->HP):0;},[this](float V){if(auto* T=Model->Selected()) Model->UpdateSheet(T->Sheet,T->Name,FMath::Min(FMath::RoundToInt(V),T->Sheet.MaxHP));},0,9999,Can)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Maximum HP / DM"),[this](){auto* T=Model->Selected(); return T?float(T->Sheet.MaxHP):1;},[this](float V){if(auto* T=Model->Selected()){auto Sheet=T->Sheet; Sheet.MaxHP=FMath::RoundToInt(V); Model->UpdateSheet(Sheet,T->Name,FMath::Min(T->HP,Sheet.MaxHP));}},1,9999,DM)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Defense / DM"),[this](){auto* T=Model->Selected(); return T?float(T->Sheet.Defense):0;},[this](float V){if(auto* T=Model->Selected()){auto Sheet=T->Sheet; Sheet.Defense=FMath::RoundToInt(V); Model->UpdateSheet(Sheet,T->Name,T->HP);}},0,999,DM)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Movement speed (units / turn) / DM"),[this](){auto* T=Model->Selected(); return T?T->Sheet.Speed:0;},[this](float V){if(auto* T=Model->Selected()){auto Sheet=T->Sheet; Sheet.Speed=V; Model->UpdateSheet(Sheet,T->Name,T->HP);}},0,1000,DM)]
    +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Notes"),[this](){auto* T=Model->Selected(); return T?T->Sheet.Notes:TEXT("");},[this](FString V){if(auto* T=Model->Selected()){auto Sheet=T->Sheet; Sheet.Notes=V.Left(4096); Model->UpdateSheet(Sheet,T->Name,T->HP);}},Can)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,12)[SNew(SBox).IsEnabled_Lambda(Can)[Action(TEXT("Roll d20"),[this](){Model->RollD20();})]]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){const auto* T=Model->Selected(); float R=T?Model->RemainingMovement(*T):-1; return FText::FromString(R<0?TEXT("Movement: unrestricted outside combat"):FString::Printf(TEXT("Movement remaining: %.1f units"),R));}).Font(Font(11,true)).ColorAndOpacity(Teal).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,16)[Label(TEXT("Edits commit on Enter or focus change.\nMechanical fields are DM-controlled."),10)];
}
TSharedRef<SWidget> STabletopView::VisionPanel()
{
    auto DM=[this](){return Model->Role==ERole::DM && Model->Selected();};
    auto Own=[this](){const auto* T=Model->Selected(); return T && Model->CanControl(*T);};
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("TOKEN VISION"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Label(TEXT("Enable dynamic vision in map Settings.\nFacing: 0 east, 90 south, 180 west."),10)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("View range (units)"),[this](){auto* T=Model->Selected(); return T?T->VisionRange:0;},[this](float V){if(auto* T=Model->Selected()){T->VisionRange=V; Model->Touch();}},0,500,DM)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("View cone (degrees; 360 = all)"),[this](){auto* T=Model->Selected(); return T?T->VisionAngle:360;},[this](float V){if(auto* T=Model->Selected()){T->VisionAngle=V; Model->Touch();}},1,360,DM)]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Facing (degrees)"),[this](){auto* T=Model->Selected(); return T?T->Facing:0;},[this](float V){if(auto* T=Model->Selected()){T->Facing=V; Model->Touch(); RefreshRoster();}},0,360,Own)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,12)[Check(TEXT("Darkvision / ignores darkness"),[this](){auto* T=Model->Selected(); return T && T->bDarkvision;},[this](bool V){if(auto* T=Model->Selected()){T->bDarkvision=V; Model->Touch();}},DM)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Check(TEXT("Hidden from players"),[this](){auto* T=Model->Selected(); return T && T->bHidden;},[this](bool V){if(auto* T=Model->Selected()){T->bHidden=V; Model->Touch(); RefreshRoster();}},DM)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,16)[Label(TEXT("Player view combines sight from that player's characters on their assigned map. Walls block sight and lights. No explored fog is retained yet."),11)];
}
TSharedRef<SWidget> STabletopView::CombatPanel()
{
    auto DM=[this](){return Model->Role==ERole::DM;};
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("COMBAT TRACKER"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,10)[SNew(STextBlock).Text_Lambda([this](){const auto& M=Model->Map(); if(!M.bCombat) return FText::FromString(TEXT("Outside combat")); const auto* T=Model->Find(Model->ActiveTurnId()); return FText::FromString(FString::Printf(TEXT("Round %d\nActive: %s"),M.Round,T && Model->IsVisible(*T)?*T->Name:TEXT("DM turn")));}).Font(Font(15,true)).ColorAndOpacity(Teal).AutoWrapText(true)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SBox).IsEnabled_Lambda(DM)[Action(TEXT("Start / end combat"),[this](){Model->ToggleCombat();})]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SBox).IsEnabled_Lambda(DM)[Action(TEXT("Next turn"),[this](){Model->NextTurn();})]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,6)[SNew(SBox).IsEnabled_Lambda(DM)[Action(TEXT("Refresh movement"),[this](){Model->ResetMovement();})]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,14)[Check(TEXT("Selected token: out-of-turn exception"),[this](){auto* T=Model->Selected(); return T && T->bOutOfTurn;},[this](bool V){if(auto* T=Model->Selected()){T->bOutOfTurn=V; Model->Touch();}},[this](){return Model->Role==ERole::DM && Model->Selected();})]
    +SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Text_Lambda([this](){FString S; for(int32 Id:Model->Map().TurnOrder){auto* T=Model->Find(Id); if(T && Model->IsVisible(*T)) S+=FString::Printf(TEXT("%s %s\n"),Id==Model->ActiveTurnId()?TEXT(">"):TEXT("-"),*T->Name);} return FText::FromString(S);}).Font(Font(12)).ColorAndOpacity(Ink)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,16)[Label(TEXT("Player movement is allowed on the character's turn. Exceptions still spend the current allowance. DM repositioning ignores distance and turn limits."),11)];
}
void STabletopView::Construct(const FArguments&)
{
    Model=MakeShared<FBoardModel>();
    // Create the map before building settings whose callbacks refer to it.
    SAssignNew(Map,STabletopMap).Model(Model).OnChanged(FSimpleDelegate::CreateSP(this,&STabletopView::RefreshRoster));
    auto LeftTabs=SNew(SHorizontalBox);
    const TArray<FString> LeftNames={TEXT("Maps"),TEXT("Tools"),TEXT("Settings")};
    for(int32 I=0; I<3; ++I) LeftTabs->AddSlot().FillWidth(1).Padding(2)[Action(LeftNames[I],[this,I](){LeftPage=I;},true)];
    auto RightTabs=SNew(SHorizontalBox);
    const TArray<FString> RightNames={TEXT("Sheet"),TEXT("Vision"),TEXT("Combat")};
    for(int32 I=0; I<3; ++I) RightTabs->AddSlot().FillWidth(1).Padding(2)[Action(RightNames[I],[this,I](){RightPage=I;},true)];
    ChildSlot[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Background).Padding(0)
    [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(16,12))
        [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[Label(TEXT("WAYFARER"),21)]
                +SVerticalBox::Slot().AutoHeight()[Label(TEXT("MAPS / VISION / CHARACTERS"),9)]]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(5,0)[Label(TEXT("Preview identity:"),10)]
            +SHorizontalBox::Slot().AutoWidth().Padding(3,0)[Action(TEXT("Mira"),[this](){Model->SetPlayer(1); Map->LoadBackground(); Map->ResetView(); Refresh();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(3,0)[Action(TEXT("Rowan"),[this](){Model->SetPlayer(2); Map->LoadBackground(); Map->ResetView(); Refresh();})]
            +SHorizontalBox::Slot().AutoWidth().Padding(12,0,3,0)[Action(TEXT("DM VIEW"),[this](){SwitchRole(ERole::DM);})]
            +SHorizontalBox::Slot().AutoWidth().Padding(3,0)[Action(TEXT("PLAYER VIEW"),[this](){SwitchRole(ERole::Player);})]]]
        +SVerticalBox::Slot().FillHeight(1).Padding(10)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(260)[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(10)
            [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[LeftTabs]
                +SVerticalBox::Slot().FillHeight(1).Padding(5,12)[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return LeftPage;})
                    +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[MapsPanel()]]
                    +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[ToolsPanel()]]
                    +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[SettingsPanel()]]]]]]
            +SHorizontalBox::Slot().FillWidth(1).Padding(10,0)[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(4,0,4,8)[SNew(SHorizontalBox)
                    +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([this](){const auto& M=Model->Map();return FText::FromString(FString::Printf(TEXT("%s\n%s / %s / %.1f units per cell"),*M.Name,Model->Role==ERole::DM?TEXT("DM"):TEXT("PLAYER"),M.Grid==EGrid::Hex?TEXT("HEX"):TEXT("SQUARE"),M.UnitsPerCell));}).Font(Font(12,true)).ColorAndOpacity(Ink)]
                    +SHorizontalBox::Slot().AutoWidth()[Action(TEXT("Fit map"),[this](){Map->ResetView();})]]
                +SVerticalBox::Slot().FillHeight(1)[Map.ToSharedRef()]
                +SVerticalBox::Slot().AutoHeight().Padding(5,12)[Label(TEXT("DRAG token / RIGHT-DRAG pan / WHEEL zoom / HOME fit"),10)]]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(290)[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(10)
            [SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight()[RightTabs]
                +SVerticalBox::Slot().FillHeight(1).Padding(5,12)[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return RightPage;})
                    +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[SheetPanel()]]
                    +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[VisionPanel()]]
                    +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[CombatPanel()]]]
                +SVerticalBox::Slot().AutoHeight().Padding(5,8)[Label(TEXT("LOCAL PREVIEW / identity buttons are for testing, not authentication."),9)]]]]]
        +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Panel).Padding(FMargin(16,10))
            [SNew(STextBlock).Text_Lambda([this](){return FText::FromString(Model->Status);}).Font(Font(11)).ColorAndOpacity(Ink).AutoWrapText(true)]]]];
    Refresh();
}
