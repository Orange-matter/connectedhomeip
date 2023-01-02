#include <app/CommandHandler.h>
#include <app-common/zap-generated/cluster-objects.h>
#include <app/util/af.h>

using namespace chip;
using namespace chip::app;
using namespace chip::app::Clusters;
using Status = Protocols::InteractionModel::Status;


bool emberAfBasicInformationClusterMfgSpecificPingCallback(
    chip::app::CommandHandler * commandObj, 
    const chip::app::ConcreteCommandPath & commandPath,
    const chip::app::Clusters::BasicInformation::Commands::MfgSpecificPing::DecodableType & commandData)
{
    ChipLogDetail(Test, "Ping-App: Responding to command");
    commandObj->AddStatus(commandPath, Status::Success);
    ChipLogDetail(Test, "Ping-App: Response sent");
    return true;
}
