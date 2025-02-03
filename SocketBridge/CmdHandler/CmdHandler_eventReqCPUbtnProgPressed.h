#ifndef _CmdHandler_eventReqCPUBtnProgPressed_h_
#define _CmdHandler_eventReqCPUBtnProgPressed_h_
#include "../CmdHandler_eventReq.h"



namespace socketbridge
{
    /*********************************************************
     * CmdHandler_eventReqCPUBtnProgPressed
     *
     *	Utilizzato per inviare automaticamente l'evento 'eEventType::btnProgPressed' ai client collegati.
	 *	Questo evento viene scatenato quando l'utente preme il btn fisico per entrare in menu prog
     *  La notifica "CPUBRIDGE_NOTIFY_BTN_PROG_PRESSED" viene generata spontaneamente da CPUBridge e, in risposta a questa notifica,
     *  SocketBrdige invia l'evento eEventType::btnProgPressed a tutti i client collegati
     */
    class CmdHandler_eventReqCPUBtnProgPressed : public CmdHandler_eventReq
    {
    public:
        static const eEventType EVENT_TYPE_FROM_SOCKETCLIENT = eEventType::btnProgPressed;
		static const u16		EVENT_ID_FROM_CPUBRIDGE = CPUBRIDGE_NOTIFY_BTN_PROG_PRESSED;

		CmdHandler_eventReqCPUBtnProgPressed(const HSokBridgeClient &identifiedClientHandle, u16 handlerID, u64 dieAfterHowManyMSec) :
                        CmdHandler_eventReq(identifiedClientHandle, handlerID, dieAfterHowManyMSec)
                    {  }

        bool		needToPassDownToCPUBridge() const                                                                                                       { return false; }
        void        passDownRequestToCPUBridge(cpubridge::sSubscriber &from UNUSED_PARAM, const u8 *payload UNUSED_PARAM, u16 payloadLen UNUSED_PARAM)      { }

        void		handleRequestFromSocketBridge(socketbridge::Server *server UNUSED_PARAM, HSokServerClient &hClient UNUSED_PARAM)                        { }

        void        onCPUBridgeNotification (socketbridge::Server *server, HSokServerClient &hClient, const rhea::thread::sMsg &msgFromCPUBridge);
    };
} // namespace socketbridge

#endif // _CmdHandler_eventReqCPUBtnProgPressed_h_
