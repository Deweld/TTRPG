#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "TabletopGameMode.generated.h"

class SWidget;

UCLASS()
class TTRPGTABLETOP_API ATabletopPlayerController : public APlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
private:
    TSharedPtr<SWidget> TabletopView;
};

UCLASS()
class TTRPGTABLETOP_API ATabletopGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ATabletopGameMode();
};
