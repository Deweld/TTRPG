#include "TTRPGCreateMapCommandlet.h"
#include "TabletopGameMode.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "FileHelpers.h"
#include "Misc/PackageName.h"

UTTRPGCreateMapCommandlet::UTTRPGCreateMapCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true; LogToConsole = true;
}
int32 UTTRPGCreateMapCommandlet::Main(const FString& Params)
{
    const FString Path = TEXT("/TTRPGCore/Maps/TabletopPrototype");
    if (FPackageName::DoesPackageExist(Path))
    {
        UE_LOG(LogTemp, Display, TEXT("Tabletop map already exists; preserving it."));
        return 0;
    }
    UWorld* World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
    if (!World) return 1;
    World->GetWorldSettings()->DefaultGameMode = ATabletopGameMode::StaticClass();
    if (!UEditorLoadingAndSavingUtils::SaveMap(World, Path)) return 2;
    UE_LOG(LogTemp, Display, TEXT("Created %s. Play this level to open the tabletop."), *Path);
    return 0;
}
