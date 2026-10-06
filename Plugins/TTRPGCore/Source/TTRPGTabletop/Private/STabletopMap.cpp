#include "STabletopView.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Modules/ModuleManager.h"

using namespace TTRPG::Tabletop;
namespace
{
const FSlateBrush* White() {return FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));}
FString ImageFolder() {return FPaths::ProjectSavedDir()/TEXT("TabletopPrototype/Images");}
UTexture2D* Decode(const FString& Path)
{
    const int64 Size=IFileManager::Get().FileSize(*Path);
    if(Size<=0 || Size>32*1024*1024) return nullptr;
    TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*Path)) return nullptr;
    auto& Module=FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    const EImageFormat Format=Module.DetectImageFormat(Bytes.GetData(),Bytes.Num());
    if(Format!=EImageFormat::PNG && Format!=EImageFormat::JPEG) return nullptr;
    auto Wrapper=Module.CreateImageWrapper(Format);
    if(!Wrapper || !Wrapper->SetCompressed(Bytes.GetData(),Bytes.Num()) || Wrapper->GetWidth()<1 || Wrapper->GetHeight()<1
        || Wrapper->GetWidth()>8192 || Wrapper->GetHeight()>8192 || int64(Wrapper->GetWidth())*Wrapper->GetHeight()>16777216) return nullptr;
    FImage Raw; if(!Module.DecompressImage(Bytes.GetData(),Bytes.Num(),Raw)) return nullptr;
    return FImageUtils::CreateTexture2DFromImage(Raw);
}
}
void STabletopMap::Construct(const FArguments& Args)
{
    Model=Args._Model; Changed=Args._OnChanged; SetClipping(EWidgetClipping::ClipToBounds); ForceVolatile(true);
}
bool STabletopMap::LoadBackground()
{
    BackgroundTexture.Reset(); BackgroundBrush=FSlateBrush();
    const FString File=Model->Map().ImagePath;
    if(File.IsEmpty()) return true;
    if(FPaths::GetCleanFilename(File)!=File || File.Contains(TEXT(":"))) return false;
    UTexture2D* Texture=Decode(ImageFolder()/File);
    if(!Texture) {Model->Status=TEXT("Background image unavailable; map data is intact."); return false;}
    BackgroundTexture.Reset(Texture); BackgroundBrush.SetResourceObject(Texture);
    BackgroundBrush.ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY()); BackgroundBrush.DrawAs=ESlateBrushDrawType::Image;
    return true;
}
bool STabletopMap::ImportBackground(const FString& Input)
{
    if(Model->Role!=ERole::DM) return false;
    const FString Path=Input.TrimStartAndEnd().TrimQuotes();
    UTexture2D* Texture=Decode(Path);
    if(!Texture) {Model->Status=TEXT("Import needs a PNG/JPEG up to 32 MB and 16 megapixels (max side 8192)."); return false;}
    TStrongObjectPtr<UTexture2D> Guard(Texture);
    IFileManager::Get().MakeDirectory(*ImageFolder(),true);
    const FString File=FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".")+FPaths::GetExtension(Path).ToLower();
    if(IFileManager::Get().Copy(*(ImageFolder()/File),*Path)!=COPY_OK) {Model->Status=TEXT("Could not copy the background into local map storage."); return false;}
    Model->Map().ImagePath=File; Model->Map().ImageScale=1; Model->Map().ImageX=0; Model->Map().ImageY=0;
    BackgroundTexture=MoveTemp(Guard); BackgroundBrush.SetResourceObject(Texture);
    BackgroundBrush.ImageSize=FVector2D(Texture->GetSizeX(),Texture->GetSizeY()); BackgroundBrush.DrawAs=ESlateBrushDrawType::Image;
    Model->Log(TEXT("Background imported. Adjust image scale/offset and grid opacity in Settings.")); return true;
}
float STabletopMap::CellSize(const FGeometry& G) const
{
    FVector2D E=Model->Map().Extent()+FVector2D(2,2);
    return FMath::Max(3.f,FMath::Min(float(G.GetLocalSize().X/E.X),float(G.GetLocalSize().Y/E.Y)))*Zoom;
}
FVector2D STabletopMap::Origin(const FGeometry& G) const {return (G.GetLocalSize()-Model->Map().Extent()*CellSize(G))*.5+Pan;}
FIntPoint STabletopMap::CellAt(const FGeometry& G,FVector2D P) const {return Model->Map().CellFromPoint((P-Origin(G))/CellSize(G));}
int32 STabletopMap::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const
{
    const auto& M=Model->Map(); const float S=CellSize(G); const FVector2D O=Origin(G);
    auto Box=[&](FVector2D P,FVector2D Size,FLinearColor C,int32 L,const FSlateBrush* B=nullptr)
    {FSlateDrawElement::MakeBox(Out,Layer+L,G.ToPaintGeometry(Size,FSlateLayoutTransform(P)),B?B:White(),ESlateDrawEffect::None,C);};
    auto Text=[&](FVector2D P,const FString& Value,int32 Size,FLinearColor C,int32 L)
    {FSlateDrawElement::MakeText(Out,Layer+L,G.ToPaintGeometry(FVector2D(1,1),FSlateLayoutTransform(P)),Value,FCoreStyle::GetDefaultFontStyle(TEXT("Bold"),Size),ESlateDrawEffect::None,C);};
    auto Polygon=[&](const TArray<FVector2D>& Points,FLinearColor C,int32 L)
    {
        TArray<FSlateVertex> V; TArray<SlateIndex> I;
        for(FVector2D P:Points) V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(G.GetAccumulatedRenderTransform(),FVector2f(P),FVector2f(0,0),C.ToFColorSRGB()));
        for(int32 N=1; N<Points.Num()-1; ++N) {I.Add(0); I.Add(N); I.Add(N+1);}
        FSlateDrawElement::MakeCustomVerts(Out,Layer+L,FSlateApplication::Get().GetRenderer()->GetResourceHandle(*White()),V,I,nullptr,0,0);
    };
    auto Points=[&](FIntPoint Cell,double Inset=0.)
    {
        TArray<FVector2D> P; FVector2D Center=O+M.Center(Cell)*S;
        if(M.Grid==EGrid::Square)
        {
            double R=S*.5-Inset; P={Center+FVector2D(-R,-R),Center+FVector2D(R,-R),Center+FVector2D(R,R),Center+FVector2D(-R,R)};
        }
        else for(int32 I=0; I<6; ++I)
        {double A=FMath::DegreesToRadians(-90.+I*60.); P.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*(S*.5773502692-Inset));}
        return P;
    };
    Box(FVector2D::ZeroVector,G.GetLocalSize(),FLinearColor(.018f,.025f,.034f),0);
    Box(O,M.Extent()*S,FLinearColor(.045f,.075f,.07f),1);
    if(BackgroundTexture.IsValid())
        Box(O+FVector2D(M.ImageX,M.ImageY)*S,M.Extent()*S*M.ImageScale,FLinearColor::White,2,&BackgroundBrush);
    for(int32 Y=0; Y<M.Height; ++Y) for(int32 X=0; X<M.Width; ++X)
    {
        FIntPoint C(X,Y); auto P=Points(C);
        const bool Visible=Model->IsCellVisible(C);
        if(!Visible) {Polygon(P,FLinearColor(.015f,.02f,.028f),6); continue;}
        if(!BackgroundTexture.IsValid())
        {
            float V=((X*7+Y*13)%5)*.007f;
            Polygon(P,FLinearColor(.08f+V,.11f+V,.10f),2);
        }
        if(M.bVision && M.bDarkness && Model->Role==ERole::DM && !Model->IsLit(M,C)) Polygon(P,FLinearColor(0,0,0,.42f),3);
        const FVector2D FirstPoint=P[0]; P.Add(FirstPoint);
        FSlateDrawElement::MakeLines(Out,Layer+4,G.ToPaintGeometry(),P,ESlateDrawEffect::None,FLinearColor(.5f,.62f,.60f,M.GridOpacity),true,1.f);
        if(M.Walls.Contains(C))
        {
            Polygon(Points(C,2),FLinearColor(.25f,.28f,.28f),5);
            const FVector2D Center=O+M.Center(C)*S;
            Box(Center+FVector2D(-S*.28,-S*.25),FVector2D(S*.55,2),FLinearColor(.45f,.48f,.45f),6);
        }
    }
    const FSlateRoundedBoxBrush Circle(FLinearColor::White), Square(FLinearColor::White,4.f);
    for(const FMapLight& L:M.Lights) if(Model->IsCellVisible(L.Cell))
    {
        FVector2D P=O+M.Center(L.Cell)*S;
        Box(P-FVector2D(S*.17,S*.17),FVector2D(S*.34,S*.34),FLinearColor(1,.65f,.14f),7,&Circle);
        Text(P-FVector2D(4,7),TEXT("*"),11,FLinearColor(.12f,.08f,.02f),8);
    }
    if(Model->IsInside(Hover) && Model->IsCellVisible(Hover))
    {FLinearColor Color=Model->IsAvailable(Hover,DragId)?FLinearColor(.12f,.70f,.59f,.25f):FLinearColor(.87f,.58f,.22f,.25f); Polygon(Points(Hover,1),Color,7);}
    for(const FToken& T:Model->Tokens)
    {
        if(!Model->IsVisible(T)) continue;
        FVector2D P=O+M.Center(T.Cell)*S-FVector2D(S*.5,S*.5);
        FLinearColor Color=T.Kind==ETokenKind::Prop?FLinearColor(.87f,.58f,.22f):T.Kind==ETokenKind::NPC?FLinearColor(.77f,.29f,.25f):T.OwnerId==Model->ViewingPlayerId?FLinearColor(.12f,.7f,.59f):FLinearColor(.36f,.49f,.83f);
        const FSlateBrush* Shape=T.Kind==ETokenKind::Prop?&Square:&Circle;
        if(T.Id==Model->SelectedId) Box(P+FVector2D(S*.05,S*.05),FVector2D(S*.9,S*.9),FLinearColor(.9f,.82f,.57f),9,Shape);
        Box(P+FVector2D(S*.15,S*.12),FVector2D(S*.7,S*.7),T.bHidden?Color*.4f:Color,10,Shape);
        if(S>=18) Text(P+FVector2D(S*.33,S*.14),T.Kind==ETokenKind::Prop?TEXT("+"):T.Name.Left(1),FMath::Clamp(int32(S*.28f),8,22),FLinearColor(.02f,.03f,.04f),11);
        if(S>=25)
        {
            FString Name=T.Name.Left(14)+(T.bHidden?TEXT(" [DM]"):TEXT("")); float W=Name.Len()*5.7f+10;
            Box(P+FVector2D((S-W)*.5,S-3),FVector2D(W,16),FLinearColor(.02f,.03f,.04f,.94f),12,&Square);
            Text(P+FVector2D((S-W)*.5+5,S-3),Name,8,FLinearColor(.85f,.88f,.87f),13);
        }
        if(T.Id==Model->SelectedId && T.Kind!=ETokenKind::Prop)
        {
            FVector2D Center=O+M.Center(T.Cell)*S; float A=FMath::DegreesToRadians(T.Facing);
            TArray<FVector2D> Line={Center,Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*S*.6};
            FSlateDrawElement::MakeLines(Out,Layer+14,G.ToPaintGeometry(),Line,ESlateDrawEffect::None,FLinearColor(1,.83f,.3f),true,2.f);
        }
    }
    if(DragId!=INDEX_NONE && Model->IsInside(Hover) && Model->IsCellVisible(Hover)) Polygon(Points(Hover,3),FLinearColor(.2f,.8f,.65f,.4f),15);
    return Layer+16;
}
void STabletopMap::ApplyTool(FIntPoint Cell)
{
    if (!Model->IsInside(Cell)) return;
    if (Model->Tool == ETool::Wall || Model->Tool == ETool::Erase)
        Model->PaintWall(Cell, Model->Tool == ETool::Wall);
    else if (Model->Tool == ETool::Light || Model->Tool == ETool::EraseLight)
        Model->PlaceLight(Cell, LightRange, Model->Tool == ETool::EraseLight);
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



