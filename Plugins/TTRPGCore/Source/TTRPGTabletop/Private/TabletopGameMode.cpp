#include "TabletopGameMode.h"
#include "STabletopView.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"

ATabletopGameMode::ATabletopGameMode()
{
    PlayerControllerClass = ATabletopPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    HUDClass = nullptr;
}
void ATabletopPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController() || !GetWorld()->GetGameViewport()) return;
    bShowMouseCursor = true;
    TabletopView = SNew(STabletopView);
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(TabletopView.ToSharedRef(), 10);
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(TabletopView);
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
}
void ATabletopPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (TabletopView && GetWorld() && GetWorld()->GetGameViewport())
        GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(TabletopView.ToSharedRef());
    TabletopView.Reset();
    Super::EndPlay(EndPlayReason);
}
