#include "PersistentStorage.h"

class StaticPersistentStorage : public PersistentStorage
{
public:
    CHIP_ERROR Init();
    // Return the stored Fabric id, or the default one if nothing is stored.
    chip::FabricId GetFabricId();
};