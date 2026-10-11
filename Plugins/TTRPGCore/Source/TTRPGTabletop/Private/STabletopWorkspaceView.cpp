#include "STabletopView.h"
#include "HAL/PlatformProcess.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
using namespace TTRPG::Tabletop;
namespace
{
const FLinearColor Back(.018f,.025f,.034f),PanelColor(.033f,.044f,.056f),InkColor(.83f,.86f,.87f),Accent(.12f,.70f,.59f);
const FSlateBrush* White(){return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));}
FSlateFontInfo Font(int32 N,bool Bold=false){return FCoreStyle::GetDefaultFontStyle(Bold?TEXT("Bold"):TEXT("Regular"),N);}
}
TSharedRef<SWidget> STabletopView::LiveText(TFunction<FString()> Get,int32 Size)
{
    return SNew(STextBlock).Text_Lambda([Get](){return FText::FromString(Get());}).Font(Font(Size)).ColorAndOpacity(InkColor).AutoWrapText(true);
}
TSharedRef<SWidget> STabletopView::Guard(TSharedRef<SWidget> Content,TFunction<bool()> Enabled)
{return SNew(SBox).IsEnabled_Lambda([Enabled](){return Enabled();})[Content];}
void STabletopView::Tick(const FGeometry& G,double Time,float Delta)
{
    SCompoundWidget::Tick(G,Time,Delta);
    if(SeenRevision!=Model->Revision){SeenRevision=Model->Revision;Refresh();}
}
void STabletopView::Enter(ERole Role)
{
    if(Model->EnterSession(Role,EntryPlayer))
    {
        if(Role==ERole::DM)LoadMap();
        LeftPage=3;RightPage=0;NewScript();Map->LoadBackground();Map->ResetView();Refresh();
    }
}
void STabletopView::Workspace(EWorkspace Value)
{
    if(!Model->SetWorkspace(Value))return;
    LeftPage=Value==EWorkspace::Gameplay?3:Value==EWorkspace::TokenEdit?1:0;
    RightPage=Value==EWorkspace::TokenEdit?3:0;DeleteMapId=INDEX_NONE;
    if(ScriptDraft.MapId!=Model->ActiveMapId || ScriptDraft.Name.IsEmpty())NewScript();Refresh();
}
void STabletopView::NewScript()
{
    ScriptDraft=FTabletopScript();ScriptDraft.Name=TEXT("New macro");ScriptDraft.MapId=Model->ActiveMapId;
    ScriptDraft.OwnerId=Model->IsDM()?0:Model->ViewingPlayerId;
    ScriptDraft.Source=TEXT("local t = token(self_id)\nif t then\n  log(t.name .. \" rolled \" .. roll(20))\nelse\n  log(\"Select a token first\")\nend");
    bDeleteScript=false;
}
void STabletopView::SaveScriptDraft(bool Run)
{
    int32 Id=Model->SaveScript(ScriptDraft);
    if(Id!=INDEX_NONE){ScriptDraft.Id=Id;if(Run)Model->RunScript(Id);Refresh();}
}
TSharedRef<SWidget> STabletopView::EntryPanel()
{
    return SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Back).HAlign(HAlign_Center).VAlign(VAlign_Center)
    [SNew(SBox).WidthOverride(650)[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(PanelColor).Padding(36)
    [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[Label(TEXT("WAYFARER"),32)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,12)[Label(TEXT("Choose how you enter this session"),20)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Label(TEXT("DMs build maps, run encounters, and preview a player's view. Players use their characters, sheets, and macros."),13)]
        +SVerticalBox::Slot().AutoHeight().Padding(0,24,0,8)[Action(TEXT("Enter as DM"),[this](){Enter(ERole::DM);})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Label(TEXT("PLAYER CHARACTER"),11)]
        +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,4,0)[Action(TEXT("Mira"),[this](){EntryPlayer=1;})]
            +SHorizontalBox::Slot().FillWidth(1).Padding(4,0,0,0)[Action(TEXT("Rowan"),[this](){EntryPlayer=2;})]]
        +SVerticalBox::Slot().AutoHeight().Padding(0,8)[LiveText([this](){return EntryPlayer==1?TEXT("Selected: Mira's player"):TEXT("Selected: Rowan's player");})]
        +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Enter as Player"),[this](){Enter(ERole::Player);})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,24,0,0)[Label(TEXT("Local prototype session. Player entry stays player-only. DM entry restores your saved session when available."),11)]
    ]]];
}
TSharedRef<SWidget> STabletopView::GameplayPanel()
{
    auto DM=[this](){return Model->IsDM();};
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("GAME PLAY"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[LiveText([this](){return Model->Map().bCombat?TEXT("COMBAT / turns and movement budgets"):TEXT("PLAY / free exploration");},13)]
    +SVerticalBox::Slot().AutoHeight()[SNew(SBox).Visibility_Lambda([this](){return Model->IsDM()?EVisibility::Visible:EVisibility::Collapsed;})
        [Check(TEXT("Lock map editing"),[this](){return Model->bMapEditingLocked;},[this](bool V){Model->SetMapEditingLocked(V);Refresh();})]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,4)[Label(TEXT("MACROS"),12)]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(MacroList,SVerticalBox)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,5)[Action(TEXT("Open Script workspace"),[this](){Workspace(EWorkspace::Script);})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,14,0,4)[Label(TEXT("LINKS"),12)]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(LinkList,SVerticalBox)]
    +SVerticalBox::Slot().AutoHeight()[SNew(SVerticalBox).Visibility_Lambda([this](){return Model->IsDM()?EVisibility::Visible:EVisibility::Collapsed;})
        +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Link label"),[this](){return LinkName;},[this](FString V){LinkName=V;})]
        +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Website URL"),[this](){return LinkUrl;},[this](FString V){LinkUrl=V;})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Check(TEXT("Visible to players"),[this](){return bLinkPlayerVisible;},[this](bool V){bLinkPlayerVisible=V;})]
        +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Add link"),[this](){if(Model->AddMapLink(LinkName,LinkUrl,bLinkPlayerVisible)){LinkName.Empty();Refresh();}})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,4)[Label(TEXT("TRIGGER ACTIVITY"),12)]
        +SVerticalBox::Slot().AutoHeight()[LiveText([this](){FString S;for(const auto& L:Model->TriggerLog)S+=L+TEXT("\n");return S.IsEmpty()?TEXT("No trigger events yet."):S;})]];
}
TSharedRef<SWidget> STabletopView::TokenPanel()
{
    auto DM=[this](){return Model->IsDM() && Model->Selected();};
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("TOKEN INSTANCE"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[LiveText([this](){const auto* T=Model->Selected();return T?FString::Printf(TEXT("%s / ID %d\nOwner: %d"),*T->Name,T->Id,T->OwnerId):TEXT("Select a token on the board or roster.");})]
    +SVerticalBox::Slot().AutoHeight()[Guard(SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Owner: DM / unowned"),[this](){Model->SetOwner(Model->SelectedId,0);})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Action(TEXT("Owner: Mira's player"),[this](){Model->SetOwner(Model->SelectedId,1);})]
        +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Owner: Rowan's player"),[this](){Model->SetOwner(Model->SelectedId,2);})],DM)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,4)[Label(TEXT("INSTANCE ATTRIBUTES"),12)]
    +SVerticalBox::Slot().AutoHeight()[LiveText([this](){FString S;const auto* T=Model->Selected();if(T)for(const auto& A:T->Attributes)S+=FString::Printf(TEXT("%s = %.1f\n"),*A.Key,A.Value);return S.IsEmpty()?TEXT("No extra attributes."):S;})]
    +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Attribute key"),[this](){return AttributeName;},[this](FString V){AttributeName=V;})]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Value"),[this](){return AttributeValue;},[this](float V){AttributeValue=V;},-1000000,1000000)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,5)[Action(TEXT("Set instance attribute"),[this](){Model->SetAttribute(Model->SelectedId,AttributeName,AttributeValue);})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,4)[Label(TEXT("DEFAULTS FOR NEW TOKENS"),12)]
    +SVerticalBox::Slot().AutoHeight()[Guard(Action(TEXT("Use selected token as defaults"),[this](){Model->CaptureTokenDefaults(Model->SelectedId);}),DM)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,5)[Guard(Action(TEXT("Apply defaults to this instance"),[this](){Model->ApplyTokenDefaults(Model->SelectedId);}),DM)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Label(TEXT("Existing instances keep their overrides. Associated sheet fields are on the Sheet tab."),11)];
}
TSharedRef<SWidget> STabletopView::StatusPanel()
{
    auto DM=[this](){return Model->IsDM() && Model->Selected();};
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("CONDITIONS & BARS"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[LiveText([this](){const auto* T=Model->Selected();if(!T)return FString(TEXT("Select a token."));return T->Conditions.IsEmpty()?TEXT("No conditions"):FString::Join(T->Conditions,TEXT(", "));})]
    +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Condition / status"),[this](){return ConditionName;},[this](FString V){ConditionName=V;})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,5)[Action(TEXT("Add condition"),[this](){Model->SetCondition(Model->SelectedId,ConditionName,true);})]
    +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Remove condition"),[this](){Model->SetCondition(Model->SelectedId,ConditionName,false);})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,16,0,4)[Label(TEXT("STATUS BARS"),12)]
    +SVerticalBox::Slot().AutoHeight()[LiveText([this](){FString S;const auto* T=Model->Selected();if(T)for(const auto& B:T->Bars){float V=0,M=0;Model->GetAttribute(T->Id,B.ValueAttribute,V);Model->GetAttribute(T->Id,B.MaxAttribute,M);S+=FString::Printf(TEXT("%s: %.1f / %.1f\n"),*B.Label,V,M);}return S;})]
    +SVerticalBox::Slot().AutoHeight()[Guard(SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Bar label"),[this](){return BarName;},[this](FString V){BarName=V;})]
        +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Current value attribute"),[this](){return BarValue;},[this](FString V){BarValue=V;})]
        +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Maximum attribute"),[this](){return BarMaximum;},[this](FString V){BarMaximum=V;})]
        +SVerticalBox::Slot().AutoHeight().Padding(0,7)[Action(TEXT("Add / replace named bar"),[this](){if(const auto* T=Model->Selected()){auto Bars=T->Bars;Bars.RemoveAll([this](const FStatusBar& B){return B.Label==BarName;});Bars.Add({BarName,BarValue,BarMaximum});Model->SetStatusBars(T->Id,Bars);}})]
        +SVerticalBox::Slot().AutoHeight()[Action(TEXT("Remove named bar"),[this](){if(const auto* T=Model->Selected()){auto Bars=T->Bars;Bars.RemoveAll([this](const FStatusBar& B){return B.Label==BarName;});Model->SetStatusBars(T->Id,Bars);}})],DM)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,9)[Label(TEXT("Create custom attributes in Token Edit first, then bind them here. Owners can edit their current resources; the DM defines their limits."),11)];
}
TSharedRef<SWidget> STabletopView::LightingPanel()
{
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("DYNAMIC LIGHTING"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,10)[Guard(Check(TEXT("Token vision enabled"),[this](){return Model->Map().bVision;},[this](bool V){Model->Map().bVision=V;Model->Touch();}),[this](){return Model->CanEditMap();})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,4)[Guard(Check(TEXT("Darkness / requires light"),[this](){return Model->Map().bDarkness;},[this](bool V){Model->Map().bDarkness=V;Model->Touch();}),[this](){return Model->CanEditMap();})]
    +SVerticalBox::Slot().AutoHeight()[Number(TEXT("Light range"),[this](){return Map->LightRange;},[this](float V){Map->LightRange=V;},1,500,[this](){return Model->CanEditMap();})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[Tool(TEXT("Place / update light"),ETool::Light)]
    +SVerticalBox::Slot().AutoHeight()[Tool(TEXT("Remove light"),ETool::EraseLight)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[Tool(TEXT("Paint blocking wall"),ETool::Wall)]
    +SVerticalBox::Slot().AutoHeight()[Tool(TEXT("Erase wall"),ETool::Erase)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[Tool(TEXT("Select / move"),ETool::Select)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,14)[Label(TEXT("Select a token and use the Vision tab to set its cone, range, facing, and darkvision."),11)];
}
TSharedRef<SWidget> STabletopView::ScriptLibrary()
{
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("SCRIPTS & TRIGGERS"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Action(TEXT("New macro"),[this](){NewScript();})]
    +SVerticalBox::Slot().AutoHeight()[SAssignNew(ScriptList,SVerticalBox)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,15)[Label(TEXT("Macros run when clicked. DM triggers run on map or token events during Game Play."),11)];
}
TSharedRef<SWidget> STabletopView::ScriptPanel()
{
    auto Edit=[this](){return ScriptDraft.Id==0 || Model->IsDM() || ScriptDraft.OwnerId==Model->ViewingPlayerId;};
    auto DM=[this](){return Model->IsDM();};
    auto Events=SNew(SHorizontalBox);
    const TArray<FString> Names={TEXT("Manual"),TEXT("On move"),TEXT("Turn start"),TEXT("Condition added")};
    const TArray<FString> Keys={TEXT("manual"),TEXT("move"),TEXT("turn_start"),TEXT("condition_added")};
    for(int32 I=0;I<4;++I)Events->AddSlot().FillWidth(1).Padding(2)[Guard(Action(Names[I],[this,Keys,I](){ScriptDraft.Event=Keys[I];},true),[this,I](){return I==0 || Model->IsDM();})];
    auto Source=SNew(SMultiLineEditableTextBox).Text_Lambda([this](){return FText::FromString(ScriptDraft.Source);})
        .Font(Font(12)).IsReadOnly_Lambda([Edit](){return !Edit();})
        .OnTextChanged_Lambda([this,Edit](const FText& V){if(Edit())ScriptDraft.Source=V.ToString();});
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("LUA SCRIPT"),18)]
    +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Name"),[this](){return ScriptDraft.Name;},[this](FString V){ScriptDraft.Name=V;},Edit)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,10)[Events]
    +SVerticalBox::Slot().AutoHeight()[LiveText([this](){return FString::Printf(TEXT("Event: %s / %s / %s"),*ScriptDraft.Event,ScriptDraft.TokenId?*FString::Printf(TEXT("token %d"),ScriptDraft.TokenId):TEXT("map"),ScriptDraft.OwnerId?TEXT("player macro"):TEXT("DM script"));})]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Guard(Action(TEXT("Bind to map"),[this](){ScriptDraft.MapId=Model->ActiveMapId;ScriptDraft.TokenId=0;},true),Edit)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Guard(Action(TEXT("Bind to selected token"),[this](){if(const auto* T=Model->Selected()){ScriptDraft.MapId=T->MapId;ScriptDraft.TokenId=T->Id;}},true),Edit)]]
    +SVerticalBox::Slot().AutoHeight()[Field(TEXT("Requires condition (optional)"),[this](){return ScriptDraft.Condition;},[this](FString V){ScriptDraft.Condition=V;},Edit)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1)[Check(TEXT("Enabled"),[this](){return ScriptDraft.bEnabled;},[this](bool V){ScriptDraft.bEnabled=V;},Edit)]
        +SHorizontalBox::Slot().FillWidth(1)[Check(TEXT("Players may run this macro"),[this](){return ScriptDraft.bPlayerCallable;},[this](bool V){ScriptDraft.bPlayerCallable=V;},DM)]]
    +SVerticalBox::Slot().AutoHeight().Padding(0,8)[Label(TEXT("STARTER SCRIPTS / replaces this draft's code"),10)]
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Guard(Action(TEXT("Roll d20"),[this](){ScriptDraft.Source=TEXT("log(roll(20))");},true),Edit)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Guard(Action(TEXT("Add Prone"),[this](){ScriptDraft.Source=TEXT("add_condition(self_id, \"Prone\")");},true),Edit)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Guard(Action(TEXT("Inspect token"),[this](){ScriptDraft.Source=TEXT("local hero = token(self_id)\nlog(hero.name)\nlog(hero.hp)");},true),Edit)]]
    +SVerticalBox::Slot().FillHeight(1).Padding(0,8)[Source]
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Guard(Action(TEXT("Save script"),[this](){SaveScriptDraft(false);}),Edit)]
        +SHorizontalBox::Slot().FillWidth(1).Padding(2)[Guard(Action(TEXT("Save & run macro"),[this](){SaveScriptDraft(true);}),[this,Edit](){return Edit() && ScriptDraft.Event==TEXT("manual");})]
        +SHorizontalBox::Slot().AutoWidth().Padding(2)[Guard(Action(TEXT("Delete..."),[this](){bDeleteScript=true;}),[this,Edit](){return ScriptDraft.Id>0 && Edit();})]]
    +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox).Visibility_Lambda([this](){return bDeleteScript?EVisibility::Visible:EVisibility::Collapsed;})
        +SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("Confirm script deletion"),[this](){if(Model->DeleteScript(ScriptDraft.Id))NewScript();bDeleteScript=false;Refresh();})]
        +SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("Keep script"),[this](){bDeleteScript=false;})]];
}
TSharedRef<SWidget> STabletopView::ScriptHelp()
{
    return SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("QUICK REFERENCE"),15)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,10)[Label(TEXT("Select a starter, save it, then run it from Game Play. Bind to a token to reuse it without selecting that token each time."),12)]
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("log(roll(20))\nRoll a 20-sided die.\n\nadd_condition(self_id, \"Prone\")\nAdd a status to the current token.\n\nremove_condition(self_id, \"Prone\")\nRemove that status.\n\nlocal hero = token(self_id)\nRead the selected or event token.\n\nlocal other = token(2)\nRead another visible token by ID.\n\nset_attr(self_id, \"hp\", 10)\nSet an allowed attribute.\n\nlog(map().name)\nRead the current map.\n\nif hero.hp < 5 then\n  log(\"Low health\")\nend"),12)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,16)[Label(TEXT("Token fields: name, hp, maxHP, defense, speed, x, y, custom attributes, conditions. Token IDs appear in Token Edit.\n\nPlayer macros see visible tokens and can change their own current resources and conditions. DM triggers can act across the current map."),11)]
    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("A failed script applies no changes. Loops, memory, and output are bounded. No files, shell commands, or web requests."),10)];
}
void STabletopView::RefreshActions()
{
    if(MacroList)
    {
        MacroList->ClearChildren();int32 Count=0;
        for(const auto& S:Model->Scripts)if(S.Event==TEXT("manual") && Model->CanReadScript(S))
        {int32 Id=S.Id;bool Enabled=S.bEnabled;++Count;MacroList->AddSlot().AutoHeight().Padding(0,3)[Guard(Action(S.Name,[this,Id](){Model->RunScript(Id);Refresh();}),[Enabled](){return Enabled;})];}
        if(!Count)MacroList->AddSlot().AutoHeight()[Label(TEXT("No macros on this map yet."),11)];
    }
    if(ScriptList)
    {
        ScriptList->ClearChildren();
        for(const auto& S:Model->Scripts)if(Model->CanReadScript(S))
        {int32 Id=S.Id;ScriptList->AddSlot().AutoHeight().Padding(0,4)[Action(S.Name+TEXT("\n")+S.Event+(S.Condition.IsEmpty()?TEXT(""):TEXT(" / ")+S.Condition),[this,Id](){const auto* Found=Model->Scripts.FindByPredicate([Id](const FTabletopScript& V){return V.Id==Id;});if(Found){ScriptDraft=*Found;bDeleteScript=false;}})];}
    }
    if(LinkList)
    {
        LinkList->ClearChildren();
        for(int32 I=0;I<Model->Map().Links.Num();++I)
        {const auto& L=Model->Map().Links[I];if(!Model->IsDM() && !L.bPlayerVisible)continue;const FString Url=L.Url;
            auto Row=SNew(SHorizontalBox);
            Row->AddSlot().FillWidth(1)[Action(L.Name,[Url](){if(Url.StartsWith(TEXT("http://")) || Url.StartsWith(TEXT("https://")))FPlatformProcess::LaunchURL(*Url,nullptr,nullptr);})];
            if(Model->IsDM())Row->AddSlot().AutoWidth()[Action(TEXT("x"),[this,I](){Model->RemoveMapLink(I);Refresh();},true)];
            LinkList->AddSlot().AutoHeight().Padding(0,3)[Row];}
    }
    if(ApprovalList)
    {
        ApprovalList->ClearChildren();
        for(int32 I=Model->ForcedMoves.Num()-1;I>=0;--I)
        {
            const auto& P=Model->ForcedMoves[I];if(P.MapId!=Model->ActiveMapId || (!Model->IsDM() && P.PlayerId!=Model->ViewingPlayerId))continue;
            const auto* T=Model->Find(P.TargetId);FString Name=T && Model->IsVisible(*T)?T->Name:TEXT("Target");
            FString State=P.State==EMoveProposalState::Pending?TEXT("Pending"):P.State==EMoveProposalState::Approved?TEXT("Approved"):P.State==EMoveProposalState::Stale?TEXT("Stale"):TEXT("Rejected");
            ApprovalList->AddSlot().AutoHeight().Padding(0,6)[Label(FString::Printf(TEXT("#%d %s\n%s to %d, %d"),P.Id,*State,*Name,P.Destination.X+1,P.Destination.Y+1),11)];
            if(Model->IsDM() && P.State==EMoveProposalState::Pending)
            {int32 Id=P.Id;ApprovalList->AddSlot().AutoHeight()[SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("Approve"),[this,Id](){Model->ResolveForcedMove(Id,true);Refresh();},true)]
                +SHorizontalBox::Slot().FillWidth(1)[Action(TEXT("Reject"),[this,Id](){Model->ResolveForcedMove(Id,false);Refresh();},true)]];}
        }
    }
}
void STabletopView::Construct(const FArguments&)
{
    Model=MakeShared<FBoardModel>();NewScript();
    SAssignNew(Map,STabletopMap).Model(Model).OnChanged(FSimpleDelegate::CreateSP(this,&STabletopView::Refresh));
    auto Modes=SNew(SHorizontalBox);
    const TArray<FString> ModeNames={TEXT("Map Editor"),TEXT("Game Play"),TEXT("Token Edit"),TEXT("Script")};
    for(int32 I=0;I<4;++I)
    {
        EWorkspace Id=EWorkspace(I);
        Modes->AddSlot().AutoWidth().Padding(3)[SNew(SButton).ContentPadding(FMargin(18,9))
            .Visibility_Lambda([this,I](){return I!=0 || Model->IsDM()?EVisibility::Visible:EVisibility::Collapsed;})
            .ButtonColorAndOpacity_Lambda([this,Id](){return Model->Workspace==Id?FLinearColor(.10f,.37f,.32f):PanelColor;})
            .OnClicked_Lambda([this,Id](){Workspace(Id);return FReply::Handled();})[SNew(STextBlock).Text(FText::FromString(ModeNames[I])).Font(Font(12,true)).ColorAndOpacity(InkColor)]];
    }
    auto LeftTabs=SNew(SVerticalBox).Visibility_Lambda([this](){return Model->Workspace==EWorkspace::Script?EVisibility::Collapsed:EVisibility::Visible;});
    auto First=SNew(SHorizontalBox),Second=SNew(SHorizontalBox),Third=SNew(SHorizontalBox);
    const TArray<FString> LeftNames={TEXT("Maps"),TEXT("Layout / tokens"),TEXT("Image / grid"),TEXT("Actions"),TEXT("Lighting")};
    for(int32 I=0;I<5;++I)
    {
        auto Button=SNew(SBox).Visibility_Lambda([this,I](){return I==0 || I==3 || Model->CanEditMap() || (I==1 && Model->Workspace==EWorkspace::TokenEdit)?EVisibility::Visible:EVisibility::Collapsed;})[Action(LeftNames[I],[this,I](){LeftPage=I;},true)];
        (I<2?First:I<4?Second:Third)->AddSlot().FillWidth(1).Padding(2)[Button];
    }
    LeftTabs->AddSlot().AutoHeight()[First];LeftTabs->AddSlot().AutoHeight()[Second];LeftTabs->AddSlot().AutoHeight()[Third];
    auto RightTabs=SNew(SVerticalBox).Visibility_Lambda([this](){return Model->Workspace==EWorkspace::Script?EVisibility::Collapsed:EVisibility::Visible;});
    auto RightFirst=SNew(SHorizontalBox),RightSecond=SNew(SHorizontalBox);
    const TArray<FString> RightNames={TEXT("Sheet"),TEXT("Combat"),TEXT("Status"),TEXT("Token"),TEXT("Vision")};
    for(int32 I=0;I<5;++I)(I<3?RightFirst:RightSecond)->AddSlot().FillWidth(1).Padding(1)[Action(RightNames[I],[this,I](){RightPage=I;},true)];
    RightTabs->AddSlot().AutoHeight()[RightFirst];RightTabs->AddSlot().AutoHeight()[RightSecond];
    auto LeftPane=SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[LeftTabs]
        +SVerticalBox::Slot().FillHeight(1).Padding(5,10)[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return Model->Workspace==EWorkspace::Script?5:LeftPage;})
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[MapsPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[ToolsPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[SettingsPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[GameplayPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[LightingPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[ScriptLibrary()]]];
    auto RightPane=SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[RightTabs]
        +SVerticalBox::Slot().FillHeight(1).Padding(5,10)[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return Model->Workspace==EWorkspace::Script?5:RightPage;})
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[SheetPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[CombatPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[StatusPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[TokenPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[VisionPanel()]]
            +SWidgetSwitcher::Slot()[SNew(SScrollBox)+SScrollBox::Slot()[ScriptHelp()]]];
    auto Board=SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight().Padding(4,0,4,8)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(1)[LiveText([this](){return Model->Map().Name+(Model->Map().Grid==EGrid::Hex?TEXT(" / HEX"):TEXT(" / SQUARE"));},15)]
            +SHorizontalBox::Slot().AutoWidth()[Action(TEXT("Fit map"),[this](){Map->ResetView();})]]
        +SVerticalBox::Slot().FillHeight(1)[Map.ToSharedRef()]
        +SVerticalBox::Slot().AutoHeight().Padding(5,10)[Label(TEXT("Drag to move / right-drag to pan / wheel to zoom. During your turn, drag another token to request a forced move."),10)];
    auto Session=SNew(SBorder).BorderImage(White()).BorderBackgroundColor(Back).Padding(0)
    [SNew(SVerticalBox)
        +SVerticalBox::Slot().AutoHeight()[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(PanelColor).Padding(FMargin(16,10))
            [SNew(SHorizontalBox)
                +SHorizontalBox::Slot().FillWidth(1)[SNew(SVerticalBox)
                    +SVerticalBox::Slot().AutoHeight()[Label(TEXT("WAYFARER"),22)]
                    +SVerticalBox::Slot().AutoHeight()[LiveText([this](){return Model->IsDM()?TEXT("DM SESSION"):Model->IsDMEntry()?TEXT("PLAYER PREVIEW / DM SESSION"):TEXT("PLAYER SESSION");},10)]]
                +SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SBox).Visibility_Lambda([this](){return Model->IsDMEntry()?EVisibility::Visible:EVisibility::Collapsed;})[Action(TEXT("Mira"),[this](){Model->SetPlayer(1);Map->LoadBackground();Map->ResetView();NewScript();Refresh();})]]
                +SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SBox).Visibility_Lambda([this](){return Model->IsDMEntry()?EVisibility::Visible:EVisibility::Collapsed;})[Action(TEXT("Rowan"),[this](){Model->SetPlayer(2);Map->LoadBackground();Map->ResetView();NewScript();Refresh();})]]
                +SHorizontalBox::Slot().AutoWidth().Padding(3)[SNew(SBox).WidthOverride(155).Visibility_Lambda([this](){return Model->IsDMEntry()?EVisibility::Visible:EVisibility::Collapsed;})
                    [SNew(SButton).ContentPadding(FMargin(14,9)).OnClicked_Lambda([this](){SwitchRole(Model->Role==ERole::DM?ERole::Player:ERole::DM);return FReply::Handled();})
                        [LiveText([this](){return Model->Role==ERole::DM?TEXT("View as player"):TEXT("Return to DM");})]]]]]
        +SVerticalBox::Slot().AutoHeight().Padding(10,3)[Modes]
        +SVerticalBox::Slot().FillHeight(1).Padding(10)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(280)[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(PanelColor).Padding(10)[LeftPane]]]
            +SHorizontalBox::Slot().FillWidth(1).Padding(12,0)[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return Model->Workspace==EWorkspace::Script?1:0;})
                +SWidgetSwitcher::Slot()[Board]+SWidgetSwitcher::Slot()[ScriptPanel()]]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(320)[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(PanelColor).Padding(10)[RightPane]]]]
        +SVerticalBox::Slot().AutoHeight()[SNew(SBox).MaxDesiredHeight(92)[SNew(SBorder).BorderImage(White()).BorderBackgroundColor(PanelColor).Padding(FMargin(16,10))[LiveText([this](){return Model->Status;})]]]
    ];
    ChildSlot[SNew(SWidgetSwitcher).WidgetIndex_Lambda([this](){return Model->HasEnteredSession()?1:0;})
        +SWidgetSwitcher::Slot()[EntryPanel()]+SWidgetSwitcher::Slot()[Session]];
    Refresh();
}
