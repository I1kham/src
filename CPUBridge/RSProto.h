#ifndef _RSProto_h_
#define _RSProto_h_
#include "../rheaCommonLib/Protocol/ProtocolSocketServer.h"
#include "CPUBridgeEnumAndDefine.h"

namespace cpubridge
{
	class Server;
}

/******************************************************************************
* 
**/
class RSProto
{
public:
	static const u16 FILE_EVA_DTS					= 0x0001;
	static const u16 FILE_MACHINE_CONFIG			= 0x0002;
	static const u16 FILE_SMU						= 0x0003;
	static const u16 FILE_CPU						= 0x0004;
	static const u16 FILE_GUI						= 0x0005;

public:
	enum class eDecounter : u8
	{
		WATER_FILTER	= 0x01,
		COFFEE_BREWER	= 0x02,
		COFFEE_GROUND	= 0x03
	};

public:
				RSProto (rhea::ProtocolSocketServer *serverTCP, const HSokServerClient hClient);
				~RSProto ();

	void		onMessageRCV (cpubridge::Server *server, u16 what, u16 userValue, const u8 *payload, u32 payloadLen, rhea::ISimpleLogger *logger);
	void		onSMUStateChanged (const cpubridge::sCPUStatus &status, rhea::ISimpleLogger *logger);
	void		sendTemperature (u8 temperatureEsp, u8 temperatureCamCaffe, u8 temperatureSol, u8 temperatureIce, u8 temperatureMilker, rhea::ISimpleLogger *logger UNUSED_PARAM);
	void		sendFileAnswer (u32 fileID, u16 userValue, const u8 *fullPathName);

	void		sendDecounterProdotto (u8 prodotto1_n, u32 decValue, rhea::ISimpleLogger *logger UNUSED_PARAM);
	void		sendDecounterOther (eDecounter which, u32 decValue, rhea::ISimpleLogger *logger UNUSED_PARAM);


	bool		isItMe (const HSokServerClient hClientIN) const					{ return hClientIN == hClient; }

public:
	u64			nextTimeSendTemperature_msec;

private:
	static const u8  MACHINE_TYPE_TT = 0x01;
	//static const u8  MACHINE_TYPE_FS_LUCE = 0x02;

	static const u8  VAR_TYPE_u32 = 0x01;

	static const u32 VAR_STATO_MACCHINA				= 0x00000001;
	static const u32 VAR_TEMPERATURA_ESPRESSO		= 0x00000004;
	static const u32 VAR_TEMPERATURA_SOLUBILE		= 0x00000005;
	static const u32 VAR_TEMPERATURA_CAM_CAFFE		= 0x00000006;
	static const u32 VAR_TEMPERATURA_BANCO_GHIACCIO = 0x00000007;
	static const u32 VAR_TEMPERATURA_MILKER			= 0x00000008;

	static const u32 VAR_DECOUNTER_PRODOTTO			= 0x00000002;
	static const u32 VAR_DECOUNTER_OTHER			= 0x00000003;





private:
	enum class eAnswerToFileRequest
	{
		notAvail = 0,
		wait = 1,
		sendFile = 2
	};

private:
	void		priv_send (u16 what, u16 userValue, const void *payload, u32 payloadLen);
	void		priv_reset();
	void		priv_sendLastSMUState (rhea::ISimpleLogger *logger);
	void		priv_RSProtoSentMeAFile (cpubridge::Server *server, u32 fileID, const u8 *fileFullPath, rhea::ISimpleLogger *logger);
	void		priv_RSProtoWantsAFile (cpubridge::Server *server, u32 fileID, u16 userValue, rhea::ISimpleLogger *logger);

private:
	rhea::ProtocolSocketServer	*serverTCP;
	HSokServerClient			hClient;
	u8							lastStatus[3];
	u8							lastTemperature[5];
};

#endif //_RSProto_h_
