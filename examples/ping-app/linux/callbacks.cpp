#include <app/CommandHandler.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/util/af.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;


bool emberAfBasicInformationClusterMfgSpecificPingCallback(
    chip::app::CommandHandler * commandObj, 
    const chip::app::ConcreteCommandPath & commandPath,
    const chip::app::Clusters::BasicInformation::Commands::MfgSpecificPing::DecodableType & commandData)
{
    ChipLogDetail(Test, "Ping-App: Responding to command");
    emberAfSendDefaultResponse(emberAfCurrentCommand(), EMBER_ZCL_STATUS_SUCCESS);
    ChipLogDetail(Test, "Ping-App: Response sent");
    return true;
}