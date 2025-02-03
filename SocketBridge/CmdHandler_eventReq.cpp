#include "CmdHandler_eventReq.h"
#include "SocketBridgeEnumAndDefine.h"
#include "CmdHandler/CmdHandler_eventReqCPUMessage.h"
#include "CmdHandler/CmdHandler_eventReqCPUStatus.h"
#include "CmdHandler/CmdHandler_eventReqCreditUpdated.h"
#include "CmdHandler/CmdHandler_eventReqSelAvailability.h"
#include "CmdHandler/CmdHandler_eventReqSelPrices.h"
#include "CmdHandler/CmdHandler_eventReqSelStatus.h"
#include "CmdHandler/CmdHandler_eventReqStartSel.h"
#include "CmdHandler/CmdHandler_eventReqStopSel.h"
#include "CmdHandler/CmdHandler_eventReqClientList.h"
#include "CmdHandler/CmdHandler_eventReqCPUbtnProgPressed.h"
#include "CmdHandler/CmdHandler_eventReqDataAudit.h"
#include "CmdHandler/CmdHandler_eventReqCPUIniParam.h"
#include "CmdHandler/CmdHandler_eventReqVMCDataFile.h"
#include "CmdHandler/CmdHandler_eventReq_T_VMCDataFileTimestamp.h"
#include "CmdHandler/CmdHandler_eventReqWriteLocalVMCDataFile.h"
#include "CmdHandler/CmdHandler_eventReqCPUProgrammingCmd.h"
#include "CmdHandler/CmdHandler_eventReq_P0x03_CPUSanWashingStatus.h"
#include "CmdHandler/CmdHandler_eventReqBtnPressed.h"
#include "CmdHandler/CmdHandler_eventReqPartialVMCDataFile.h"
#include "CmdHandler/CmdHandler_eventReqCPUExtendedConfigInfo.h"
#include "CmdHandler/CmdHandler_eventReq_P0x04_SetDecounter.h"
#include "CmdHandler/CmdHandler_eventReq_P0x06_GetAllDecounters.h"
#include "CmdHandler/CmdHandler_eventReq_P0x10_getAperturaVGrind.h"
#include "CmdHandler/CmdHandler_eventReq_P0x11_SetMotoreMacina.h"
#include "CmdHandler/CmdHandler_eventReqSetAperturaVGrind.h"
#include "CmdHandler/CmdHandler_eventReq_P0x14_StartDisintallation.h"
#include "CmdHandler/CmdHandler_eventReq_P0x15_RecalcFasciaOrariaFV.h"
#include "CmdHandler/CmdHandler_eventReqEnterDA3SyncStatus.h"
#include "CmdHandler/CmdHandler_eventReq_V_StartSelAlreadyPaid.h"
#include "CmdHandler/CmdHandler_eventReq_AliChina_onlineStatus.h"
#include "CmdHandler/CmdHandler_eventReqStartSelForceJug.h"
#include "CmdHandler/CmdHandler_eventReqActivateBuzzer.h"

using namespace socketbridge;

/***************************************************
 * Factory
 *
 */
CmdHandler_eventReq* CmdHandler_eventReqFactory::spawnFromSocketClientEventType(rhea::Allocator *allocator, const HSokBridgeClient &identifiedClientHandle, eEventType eventType, u16 handlerID, u64 dieAfterHowManyMSec)
{
    //ora che abbiamo l'eventtype, cerchiamo la classe che gestisce questo evento
#define CHECK(TClass) \
    if (eventType == TClass::EVENT_TYPE_FROM_SOCKETCLIENT)\
        return RHEANEW(allocator, TClass)(identifiedClientHandle, handlerID, dieAfterHowManyMSec);\


	CHECK(CmdHandler_eventReqCPUMessage)
	CHECK(CmdHandler_eventReqCPUStatus)
	CHECK(CmdHandler_eventReqCreditUpdated)
	CHECK(CmdHandler_eventReqSelAvailability)
	CHECK(CmdHandler_eventReqSelPrices)
	CHECK(CmdHandler_eventReqSelStatus)
	CHECK(CmdHandler_eventReqStartSel)
	CHECK(CmdHandler_eventReqStopSel)
	CHECK(CmdHandler_eventReqClientList);
	CHECK(CmdHandler_eventReqDataAudit);
	CHECK(CmdHandler_eventReqCPUIniParam);
	CHECK(CmdHandler_eventReqVMCDataFile);
	CHECK(CmdHandler_eventReq_T_VMCDataFileTimestamp);
	CHECK(CmdHandler_eventReqWriteLocalVMCDataFile);
	CHECK(CmdHandler_eventReqCPUProgrammingCmd);
	CHECK(CmdHandler_eventReq_P0x03_CPUSanWashingStatus);
	CHECK(CmdHandler_eventReqBtnPressed);
	CHECK(CmdHandler_eventReqPartialVMCDataFile);
	CHECK(CmdHandler_eventReqCPUExtendedConfigInfo);
	CHECK(CmdHandler_eventReq_P0x04_SetDecounter);
	CHECK(CmdHandler_eventReq_P0x06_GetAllDecounters);
	CHECK(CmdHandler_eventReq_P0x10_getAperturaVGrind);
	CHECK(CmdHandler_eventReq_P0x11_SetMotoreMacina);
	CHECK(CmdHandler_eventReqSetAperturaVGrind);
	CHECK(CmdHandler_eventReq_P0x14_StartDisintallation);
	CHECK(CmdHandler_eventReq_P0x15_RecalcFasciaOrariaFV);
	CHECK(CmdHandler_eventReqEnterDA3SyncStatus);
	CHECK(CmdHandler_eventReq_V_StartSelAlreadyPaid);
	CHECK(CmdHandler_eventReq_AliChina_onlineStatus);
	CHECK(CmdHandler_eventReqStartSelForceJug);
	CHECK(CmdHandler_eventReqActivateBuzzer);
#undef CHECK

    return NULL;
}

//***************************************************
CmdHandler_eventReq* CmdHandler_eventReqFactory::spawnFromCPUBridgeEventID(rhea::Allocator *allocator, const HSokBridgeClient &identifiedClientHandle, u16 eventID, u16 handlerID, u64 dieAfterHowManyMSec)
{
#define CHECK(TClass) \
    if (eventID == TClass::EVENT_ID_FROM_CPUBRIDGE)\
        return RHEANEW(allocator, TClass)(identifiedClientHandle, handlerID, dieAfterHowManyMSec);\

	CHECK(CmdHandler_eventReqCPUMessage)
	CHECK(CmdHandler_eventReqCPUStatus)
	CHECK(CmdHandler_eventReqCreditUpdated)
	CHECK(CmdHandler_eventReqSelAvailability)
	CHECK(CmdHandler_eventReqSelPrices)
	CHECK(CmdHandler_eventReqSelStatus)
	CHECK(CmdHandler_eventReqStartSel)
	CHECK(CmdHandler_eventReqStopSel)
	CHECK(CmdHandler_eventReqClientList);
	CHECK(CmdHandler_eventReqCPUBtnProgPressed);
	CHECK(CmdHandler_eventReqDataAudit);
	CHECK(CmdHandler_eventReqCPUIniParam);
	CHECK(CmdHandler_eventReqVMCDataFile);
	CHECK(CmdHandler_eventReq_T_VMCDataFileTimestamp);
	CHECK(CmdHandler_eventReqWriteLocalVMCDataFile);
	CHECK(CmdHandler_eventReqCPUProgrammingCmd);
	CHECK(CmdHandler_eventReq_P0x03_CPUSanWashingStatus);
	CHECK(CmdHandler_eventReqBtnPressed);
	CHECK(CmdHandler_eventReqPartialVMCDataFile);
	CHECK(CmdHandler_eventReqCPUExtendedConfigInfo);
	CHECK(CmdHandler_eventReq_P0x04_SetDecounter);
	CHECK(CmdHandler_eventReq_P0x06_GetAllDecounters);
	CHECK(CmdHandler_eventReq_P0x10_getAperturaVGrind);
	CHECK(CmdHandler_eventReq_P0x11_SetMotoreMacina);
	CHECK(CmdHandler_eventReqSetAperturaVGrind);
	CHECK(CmdHandler_eventReq_P0x14_StartDisintallation);
	CHECK(CmdHandler_eventReq_P0x15_RecalcFasciaOrariaFV);
	CHECK(CmdHandler_eventReqEnterDA3SyncStatus);
	CHECK(CmdHandler_eventReq_V_StartSelAlreadyPaid);
	CHECK(CmdHandler_eventReqStartSelForceJug);
	CHECK(CmdHandler_eventReqActivateBuzzer);

#undef CHECK

	return NULL;

}
