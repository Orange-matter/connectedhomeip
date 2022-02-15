#include "StaticPersistentStorage.h"

#include <fstream>

using namespace ::chip;

constexpr const char kFabricIdKey[]        = "FabricId";

std::string GetStaticFilename()
{
    char* matterHome = std::getenv("MATTER_HOME");
    if (matterHome == NULL) matterHome=std::getenv("HOME");
    std::string home(matterHome);
    return home + "/.matter/store.ini";
}

CHIP_ERROR StaticPersistentStorage::Init()
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    std::ifstream ifs;
    ifs.open(GetStaticFilename(), std::ifstream::in);
    VerifyOrExit(ifs.is_open(), err = CHIP_ERROR_OPEN_FAILED);

    mConfig.parse(ifs);
    ifs.close();
exit:
    return err;
}

FabricId StaticPersistentStorage::GetFabricId()
{
    CHIP_ERROR err = CHIP_NO_ERROR;

    uint64_t fabricId;
    uint16_t size = static_cast<uint16_t>(sizeof(fabricId));
    err           = SyncGetKeyValue(kFabricIdKey, &fabricId, size);
    if (err == CHIP_NO_ERROR)
    {
        return static_cast<FabricId>(Encoding::LittleEndian::HostSwap64(fabricId));
    }

    return 1UL;
}
