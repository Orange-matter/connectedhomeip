#include "ExamplePersistentStorage.h"

class StaticPersistentStorage : public PersistentStorage
{
public:
    CHIP_ERROR Init(const char * name = nullptr);
};
