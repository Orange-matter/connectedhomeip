#include "StaticPersistentStorage.h"

#include <fstream>

using namespace ::chip;

std::string GetStaticFilename()
{
    char* matterHome = std::getenv("MATTER_HOME");
    if (matterHome == NULL) matterHome=std::getenv("HOME");
    std::string home(matterHome);
    return home + "/.matter/store.ini";
}

CHIP_ERROR StaticPersistentStorage::Init(const char * name)
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
