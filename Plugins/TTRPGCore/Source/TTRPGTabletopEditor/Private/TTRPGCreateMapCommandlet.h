#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "TTRPGCreateMapCommandlet.generated.h"

UCLASS()
class UTTRPGCreateMapCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UTTRPGCreateMapCommandlet();
    virtual int32 Main(const FString& Params) override;
};
