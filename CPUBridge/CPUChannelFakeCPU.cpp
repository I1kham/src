#include "CPUChannelFakeCPU.h"
#include "../rheaCommonLib/rheaUtils.h"
#include "../rheaCommonLib/rheaDateTime.h"

using namespace cpubridge;


//*****************************************************************
CPUChannelFakeCPU::CPUChannelFakeCPU()
{
	da3 = NULL;
	bShowDialogStopSelezione = true;
	statoPreparazioneBevanda = eStatoPreparazioneBevanda::doing_nothing;
	VMCState = eVMCState::DISPONIBILE;
	buzzerIsRunnigUntilTime_mSec = 0;
	memset(&runningSel, 0, sizeof(runningSel));
	memset(&cleaning, 0, sizeof(cleaning));
	memset(&testModem, 0, sizeof(testModem));
	memset(&testAssorbGruppo, 0, sizeof(testAssorbGruppo));

	memset(utf16_cpuMessage1, 0x00, sizeof(utf16_cpuMessage1));
	memset(utf16_cpuMessage2, 0x00, sizeof(utf16_cpuMessage2));

	memset (&dataAuditInProgress, 0x00, sizeof(dataAuditInProgress));

    rhea::string::strUTF8toUTF16 ((const u8*)"CPU msg example 1", utf16_cpuMessage1, sizeof(utf16_cpuMessage1));
    rhea::string::strUTF8toUTF16 ((const u8*)"CPU msg example 1", utf16_cpuMessage2, sizeof(utf16_cpuMessage2));
	
    //msg in ebraico
    /*u32 i = 0;
    utf16_cpuMessage1[i++] = 0x0031;
    utf16_cpuMessage1[i++] = 0x003d;
    utf16_cpuMessage1[i++] = 0x05dc;
    utf16_cpuMessage1[i++] = 0x05b0;
    utf16_cpuMessage1[i++] = 0x05d0;
    utf16_cpuMessage1[i++] = 0x05b7;
    utf16_cpuMessage1[i++] = 0x05e4;
    utf16_cpuMessage1[i++] = 0x05e9;
    utf16_cpuMessage1[i++] = 0x05c1;
    utf16_cpuMessage1[i++] = 0x05b5;
    utf16_cpuMessage1[i++] = 0x05e8;
    utf16_cpuMessage1[i++] = 0x0000;
    */

	for (u8 i = 0; i < 32; i++)
		decounterVari[i] = 1000 + i;

    /*
	//Det gør ondt her
	{
		u32 i = 0;
		utf16_cpuMessage2[i++] = 0x0044;
		utf16_cpuMessage2[i++] = 0x0065;
		utf16_cpuMessage2[i++] = 0x0074;
		utf16_cpuMessage2[i++] = 0x0020;
		utf16_cpuMessage2[i++] = 0x0067;
		utf16_cpuMessage2[i++] = 0x00f8;
		utf16_cpuMessage2[i++] = 0x0072;
		utf16_cpuMessage2[i++] = 0x0020;
		utf16_cpuMessage2[i++] = 0x006f;
		utf16_cpuMessage2[i++] = 0x006e;
		utf16_cpuMessage2[i++] = 0x0064;
		utf16_cpuMessage2[i++] = 0x0074;
		utf16_cpuMessage2[i++] = 0x0020;
		utf16_cpuMessage2[i++] = 0x0068;
		utf16_cpuMessage2[i++] = 0x0065;
		utf16_cpuMessage2[i++] = 0x0072;
		utf16_cpuMessage2[i++] = 0x00;
	}
    */

	/*
	//� 是从哪里来的？
	{
		u32 i = 0;
		utf16_cpuMessage2[i++] = 0x4f60;
		utf16_cpuMessage2[i++] = 0x662f;
		utf16_cpuMessage2[i++] = 0x4ece;
		utf16_cpuMessage2[i++] = 0x54ea;
		utf16_cpuMessage2[i++] = 0x91cc;
		utf16_cpuMessage2[i++] = 0x6765;
		utf16_cpuMessage2[i++] = 0x7684;
		utf16_cpuMessage2[i++] = 0xff1f;
		utf16_cpuMessage2[i++] = 0x00;
	}*/
	
	utf16_curCPUMessage = utf16_cpuMessage2;
	curCPUMessageImportanceLevel = 1;
	timeToSwapCPUMsgMesc = 0;
	
	for (u8 i = 0; i < 10; i++)
	{
		macine[i].reset();
		macine[i].posizioneMacina = 100 + (u16)rhea::randomU32(100);
	}

	timeToEndTestSelezioneMSec = 0;
}

//*****************************************************************
CPUChannelFakeCPU::~CPUChannelFakeCPU()
{
	if (NULL != da3)
	{
		RHEAFREE(rhea::getSysHeapAllocator(), da3);
		da3 = NULL;
	}
}

//*****************************************************************
bool CPUChannelFakeCPU::open(rhea::ISimpleLogger *logger)
{
	assert(logger != NULL);

	logger->log("CPUChannelFakeCPU::open\n");
	logger->incIndent();
	logger->log("OK\n");
	logger->decIndent();
	return true;
}

//*****************************************************************
void CPUChannelFakeCPU::close(rhea::ISimpleLogger *logger)
{
	logger->log("CPUChannelFakeCPU::close\n");
}

//*****************************************************************
void CPUChannelFakeCPU::priv_updateCPUMessageToBeSent(u64 timeNowMSec)
{
	if (timeNowMSec < timeToSwapCPUMsgMesc)
		return;
	timeToSwapCPUMsgMesc = timeNowMSec + 5000; //20000;
	if (utf16_curCPUMessage == utf16_cpuMessage1)
	{
		curCPUMessageImportanceLevel = 1;
		utf16_curCPUMessage = utf16_cpuMessage2;
	}
	else
	{
		curCPUMessageImportanceLevel = 1;
		utf16_curCPUMessage = utf16_cpuMessage1;
	}
}

//*****************************************************************
void CPUChannelFakeCPU::priv_DA3_reload()
{
	if (NULL != da3)
		RHEAFREE(rhea::getSysHeapAllocator(), da3);
	u8 s[256];
	sprintf_s((char*)s, sizeof(s), "%s/current/da3/vmcDataFile.da3", rhea::getPhysicalPathToAppFolder());
	u32 sizeOfBuffer = 0;
	this->da3 = rhea::fs::fileCopyInMemory(s, rhea::getSysHeapAllocator(), &sizeOfBuffer);
}

//*****************************************************************
u32 CPUChannelFakeCPU::priv_utils_giveMeAUTF16StringWithStrangeChar (u16 *out_message, u32 sizeOf_outMessage UNUSED_PARAM) const
{
	//Det g�r ondt her
	u32 i = 0;
	out_message[i++] = 0x0044;
	out_message[i++] = 0x0065;
	out_message[i++] = 0x0074;
	out_message[i++] = 0x0020;
	out_message[i++] = 0x0067;
	out_message[i++] = 0x00f8;
	out_message[i++] = 0x0072;
	out_message[i++] = 0x0020;
	out_message[i++] = 0x006f;
	out_message[i++] = 0x006e;
	out_message[i++] = 0x0064;
	out_message[i++] = 0x0074;
	out_message[i++] = 0x0020;
	out_message[i++] = 0x0068;
	out_message[i++] = 0x0065;
	out_message[i++] = 0x0072;
	
	out_message[i++] = 0x0000;
	
	return i*2;
}

//*****************************************************************
u32 CPUChannelFakeCPU::priv_utils_giveMeAUTF16StringWithStrangeChar2 (u16 *out_message, u32 sizeOf_outMessage UNUSED_PARAM) const
{
	//1=?????????
	u32 i = 0;
	out_message[i++] = 0x0031;
	out_message[i++] = 0x003d;
	out_message[i++] = 0x05dc;
	out_message[i++] = 0x05b0;
	out_message[i++] = 0x05d0;
	out_message[i++] = 0x05b7;
	out_message[i++] = 0x05e4;
	out_message[i++] = 0x05e9;
	out_message[i++] = 0x05c1;
	out_message[i++] = 0x05b5;
	out_message[i++] = 0x05e8;
	
	out_message[i++] = 0x0000;
	
	return i*2;
}

//*****************************************************************
u32 CPUChannelFakeCPU::priv_utils_giveMeAnExtendedASCIIStringWithStrangeChar (u8 *out_message, u32 sizeOf_outMessage UNUSED_PARAM) const
{
	//St�bchen nie
	// 0x53	83
	// 0x74	116
	// 0xE4	228
	// 0x62	98
	// 0x63	99
	// 0x68	104
	// 0x65	101
	// 0x6E	110
	// 0x20	32
	// 0x6E	110
	// 0x69	105
	// 0x65	101

	u32 i = 0;
	out_message[i++] = 0x53;
	out_message[i++] = 0x74;
	out_message[i++] = 0xE4;
	out_message[i++] = 0x62;
	out_message[i++] = 0x63;
	out_message[i++] = 0x68;
	out_message[i++] = 0x65;
	out_message[i++] = 0x6E;
	out_message[i++] = 0x20;
	out_message[i++] = 0x6E;
	out_message[i++] = 0x69;
	out_message[i++] = 0x65;
	out_message[i++] = 0x00;

	return i;
}


/*****************************************************************
 * Qui facciamo finta di mandare il msg ad una vera CPU e forniamo una risposta d'ufficio sempre valida
 */
bool CPUChannelFakeCPU::sendAndWaitAnswer(const u8 *bufferToSend, u16 nBytesToSend UNUSED_PARAM, u8 *out_answer, u16 *in_out_sizeOfAnswer, rhea::ISimpleLogger *logger, u64 timeoutRCVMsec UNUSED_PARAM)
{
	const eCPUCommand cpuCommand = (eCPUCommand)bufferToSend[1];
    //u8 msgLen = bufferToSend[2];
	u32 ct = 0;

	for (u8 i=0; i<10; i++)
		macine[i].update(rhea::getTimeNowMSec());

	switch (cpuCommand)
	{
	default:
        DBGBREAK;
		logger->log("CPUChannelFakeCPU::sendAndWaitAnswer() => ERR, cpuCommand not supported [%d]\n", (u8)cpuCommand);
		*in_out_sizeOfAnswer = 0;
		return false;
		break;

	case eCPUCommand::snackCommand:
		return priv_handleSnackCommand (bufferToSend, nBytesToSend, out_answer, in_out_sizeOfAnswer, logger, timeoutRCVMsec);
		break;

	case eCPUCommand::readDataAudit:
		{
			if (NULL == dataAuditInProgress.f)
			{
				dataAuditInProgress.fileOffset = 0;
				dataAuditInProgress.fileSize = 0;

				u8 s[256];
				rhea::string::utf8::spf (s, sizeof(s), "%s/current/eva-dts.log", rhea::getPhysicalPathToAppFolder());
				dataAuditInProgress.f = rhea::fs::fileOpenForReadBinary (s);
				assert (NULL != dataAuditInProgress.f);
				if (NULL != dataAuditInProgress.f)
					dataAuditInProgress.fileSize = static_cast<u32>(rhea::fs::filesize (dataAuditInProgress.f));
			}

			const u32 MAX_TO_READ = sizeof(dataAuditInProgress.buffer);
			u32 nToRead = MAX_TO_READ;
			if (dataAuditInProgress.fileOffset + nToRead > dataAuditInProgress.fileSize)
				nToRead = dataAuditInProgress.fileSize - dataAuditInProgress.fileOffset;

			if (nToRead)
			{
				rhea::fs::fileRead (dataAuditInProgress.f, dataAuditInProgress.buffer, nToRead);
				dataAuditInProgress.fileOffset += nToRead;
			}

			u8 finished = 0;
			if (dataAuditInProgress.fileOffset >= dataAuditInProgress.fileSize)
			{
				finished = 1;
				rhea::fs::fileClose (dataAuditInProgress.f);
				dataAuditInProgress.f = NULL;
			}

			//rispondo # L [len] [finished] [unused] [payload....] [ck]
			out_answer[ct++] = '#';
			out_answer[ct++] = (u8)cpuCommand;
			out_answer[ct++] = 0; //lunghezza
			out_answer[ct++] = finished;
			out_answer[ct++] = 0;

			if (nToRead)
			{
				memcpy (&out_answer[ct], dataAuditInProgress.buffer, nToRead);
				ct += nToRead;
			}
			out_answer[2] = (u8)ct + 1;
			out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
			*in_out_sizeOfAnswer = out_answer[2];
		}
		return true;

	case eCPUCommand::requestPriceHoldingPriceList:
		{
			u8 firstPriceLine = bufferToSend[3];
			u8 numPrices = bufferToSend[4];

			out_answer[ct++] = '#';
			out_answer[ct++] = (u8)cpuCommand;
			out_answer[ct++] = 0; //lunghezza
			out_answer[ct++] = firstPriceLine;
			out_answer[ct++] = numPrices;
			for (u8 i = 0; i < numPrices;i++)
			{
				u16 price = 300 + firstPriceLine + i;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], price);
				ct += 2;
			}

			out_answer[2] = (u8)ct + 1;
			out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
			*in_out_sizeOfAnswer = out_answer[2];
		}
		return true;

	case eCPUCommand::writePartialVMCDataFile:
		{
            //const u8 packet_uno_di = bufferToSend[3];
            //const u8 packet_num_toto = bufferToSend[4];
			const u8 packet_offset = bufferToSend[5];

			out_answer[ct++] = '#';
			out_answer[ct++] = 'X';
			out_answer[ct++] = 0; //lunghezza
			out_answer[ct++] = packet_offset;

			out_answer[2] = (u8)ct + 1;
			out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
			*in_out_sizeOfAnswer = out_answer[2];

			//simulo estrma lentezza della CPU in risposta
			//rhea::thread::sleepMSec(8000);
		}
		return true;

	case eCPUCommand::getVMCDataFileTimeStamp:
		{
			cpubridge::sCPUVMCDataFileTimeStamp ts;
			ts.setInvalid();
			out_answer[ct++] = '#';
			out_answer[ct++] = 'T';
			out_answer[ct++] = 0; //lunghezza
			ts.writeToBuffer(&out_answer[ct]);
			ct += ts.getLenInBytes();
			
			out_answer[2] = (u8)ct + 1;
			out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
			*in_out_sizeOfAnswer = out_answer[2];
		}
		return true;


	case eCPUCommand::startSelWithPaymentAlreadyHandled_V:
		{
			out_answer[ct++] = '#';
			out_answer[ct++] = (u8)cpuCommand;
			out_answer[ct++] = 0; //lunghezza
			out_answer[ct++] = bufferToSend[3];
			out_answer[ct++] = bufferToSend[4];
			out_answer[ct++] = bufferToSend[5];
			out_answer[ct++] = bufferToSend[6];
			out_answer[ct++] = bufferToSend[7];
			
			out_answer[2] = (u8)ct + 1;
			out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
			*in_out_sizeOfAnswer = out_answer[2];

			//hanno richiesto una selezione!!!
			statoPreparazioneBevanda = eStatoPreparazioneBevanda::wait;
			runningSel.selNum = bufferToSend[3];
			runningSel.timeStartedMSec = rhea::getTimeNowMSec();
			VMCState = eVMCState::PREPARAZIONE_BEVANDA;
			return true;
		}
		break;

	case eCPUCommand::getNomeSelezioneCPU_d:
		{
			//ricevuto:	# d [len] [numSel] [ck]
			//rispondo: # d [len] [numSel] [isUnicode] [msgLenInByte] [msg�..] [ck]
			const u8 selNum = bufferToSend[3];
			
			out_answer[ct++] = '#';
			out_answer[ct++] = (u8)cpuCommand;
			out_answer[ct++] = 0; //lunghezza
			out_answer[ct++] = selNum;
			out_answer[ct++] = 0;	//is unicode
			out_answer[ct++] = 5;
			out_answer[ct++] = 'S';
			out_answer[ct++] = 'E';
			out_answer[ct++] = 'L';
			out_answer[ct++] = ' ';
			out_answer[ct++] = ('0' + selNum);
	
			/*out_answer[ct++] = ('0' + (selNum / 100));
			out_answer[ct++] = ('0' + ((selNum % 100) / 10));
			out_answer[ct++] = ('0' + (selNum % 10)); */
			out_answer[2] = (u8)ct + 1;
			out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
			*in_out_sizeOfAnswer = out_answer[2];
			return true;
		}
		break;

	case eCPUCommand::checkStatus_B:
		//ho ricevuto una richiesta di stato, rispondo in maniera appropriata
		{
			priv_updateCPUMessageToBeSent(rhea::getTimeNowMSec());

			const u8 tastoPremuto = bufferToSend[3];

			if (statoPreparazioneBevanda == eStatoPreparazioneBevanda::doing_nothing)
			{
				if (tastoPremuto != 0)
				{
					//hanno richiesto una selezione!!!
					statoPreparazioneBevanda = eStatoPreparazioneBevanda::wait;
					runningSel.selNum = tastoPremuto;
					runningSel.timeStartedMSec = rhea::getTimeNowMSec();
					VMCState = eVMCState::PREPARAZIONE_BEVANDA;
				}
			}
			else
			{
				//sto facendo una selezione, rispondo "eStatoPreparazioneBevanda::wait" per un paio di secondi, poi vado in running per un altro paio di secondo
				const u64 timeElapsedMSec = rhea::getTimeNowMSec() - runningSel.timeStartedMSec;
				bool bFinished = false;
				if (timeElapsedMSec < 1500)
				{
					statoPreparazioneBevanda = eStatoPreparazioneBevanda::wait;
					if (tastoPremuto != 0)
						bFinished = true; //simula il tasto stop
				}
				else if (timeElapsedMSec < 12000)
				{
					statoPreparazioneBevanda = eStatoPreparazioneBevanda::running;
					if (tastoPremuto != 0)
						bFinished = true; //simula il tasto stop
				}
				else
				{
					//ok, � tempo di terminare la selezione
					bFinished = true;
				}

				if (bFinished)
				{
					VMCState = eVMCState::DISPONIBILE;
					statoPreparazioneBevanda = eStatoPreparazioneBevanda::doing_nothing;
				}
			}

			priv_buildAnswerTo_checkStatus_B(out_answer, in_out_sizeOfAnswer);
			*in_out_sizeOfAnswer = out_answer[2];
			return true;
		}
		break;

	case eCPUCommand::initialParam_C:
		//ho ricevuto i parametri iniziali, devo rispondere in maniera appropriata
		{
			assert(*in_out_sizeOfAnswer >= 116);

			u16 year, month, day;
			rhea::Date::getDateNow(&year, &month, &day);

			u8 hour, minute, seconds;
			rhea::Time24::getTimeNow(&hour, &minute, &seconds);

			out_answer[ct++] = '#';
			out_answer[ct++] = 'C';
			out_answer[ct++] = 0; //lunghezza

			out_answer[ct++] = (u8)(year - 2000);
			out_answer[ct++] = (u8)(month);
			out_answer[ct++] = (u8)(day);

			out_answer[ct++] = (u8)(hour);
			out_answer[ct++] = (u8)(minute);
			out_answer[ct++] = (u8)(seconds);

			//versione sw	8 caratteri del tipo "1.4 WIP"
			out_answer[ct++] = 'F';
			out_answer[ct++] = 'A';
			out_answer[ct++] = 'K';
			out_answer[ct++] = 'E';
			out_answer[ct++] = ' ';
			out_answer[ct++] = 'C';
			out_answer[ct++] = 'P';
			out_answer[ct++] = 'U';

			//98 btyes composti da 49 prezzi ciascuno da 2 bytes (byte basso byte alto)
            {
                u8 prices[49];
                memset (prices, 0, sizeof(prices));
                for (u8 i = 0; i < 49; i++)
                    prices[i] = (i+1); //questo per dare prezzo 1 a bevanda 1, prezzo 2 a bevanda 2...

                /*prices[0] = 0;
                prices[1] = 0;
                prices[2] = 0;
                */
                for (u8 i = 0; i < 49; i++)
                {
                    out_answer[ct++] = prices[i];
                    out_answer[ct++] = 0;
                }
            }

			//Da qui in poi sono dati nuovi, introdotti a dicembre 2018
			//115			versione protocollo.Inizialmente = 1, potrebbe cambiare in futuro
			out_answer[ct++] = CPU_REPORTED_PROTOCOL_VERSION;

			//116 ck
			out_answer[2] = (u8)ct+1;
			out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
			ct++;

			*in_out_sizeOfAnswer = out_answer[2];
			return true;
		}
		break;

	case eCPUCommand::getExtendedConfigInfo:
		{
			//tipo gruppo: 'V' = variflex, 'M'= Micro, 'N'= non presente

			//const u8 modelloMacchina = 0x82;	const u8 machine_type = (u8)cpubridge::eCPUMachineType::instant;	const u8 isInduzione = 0;	const u8 tipoGruppo='N';
			//const u8 modelloMacchina = 0x82;	const u8 machine_type = (u8)cpubridge::eCPUMachineType::espresso1;	const u8 isInduzione = 1;	const u8 tipoGruppo='V';
			const u8 modelloMacchina = 0x82;	const u8 machine_type = (u8)cpubridge::eCPUMachineType::espresso2;	const u8 isInduzione = 1;	const u8 tipoGruppo='V';
			//const u8 modelloMacchina = 0x82;	const u8 machine_type = (u8)cpubridge::eCPUMachineType::espresso2;		const u8 isInduzione = 0;	const u8 tipoGruppo='V';


			//MACCHINA: Minibona, espresso, induzione, gruppo micro
			//const u8 modelloMacchina = 0x56; const u8 tipoGruppo='M'; const u8 machine_type = (u8)cpubridge::eCPUMachineType::espresso2; const u8 isInduzione = 0;

			//MACCHINA: Brewmatic, espresso, induzione, gruppo variflex
			//const u8 modelloMacchina = 91; const u8 tipoGruppo='V'; const u8 machine_type = (u8)cpubridge::eCPUMachineType::espresso2; const u8 isInduzione = 1;
			
			out_answer[ct++] = '#';
			out_answer[ct++] = (u8)cpuCommand;
			out_answer[ct++] = 0; //lunghezza
			out_answer[ct++] = 0x02;	//versione
			out_answer[ct++] = machine_type;
			out_answer[ct++] = modelloMacchina;	
			out_answer[ct++] = isInduzione; 
			out_answer[ct++] = tipoGruppo;		

			out_answer[2] = (u8)ct + 1;
			out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
			*in_out_sizeOfAnswer = out_answer[2];
		}
		return true;

	case eCPUCommand::restart:
		*in_out_sizeOfAnswer = 0;
		return true;

	case eCPUCommand::getMilkerVer:
		out_answer[ct++] = '#';
		out_answer[ct++] = (u8)cpuCommand;
		out_answer[ct++] = 0; //lunghezza
		out_answer[ct++] = 'x';
		out_answer[ct++] = 'y';
		out_answer[ct++] = 'z';

		out_answer[2] = (u8)ct + 1;
		out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
		*in_out_sizeOfAnswer = out_answer[2];
		return true;

	case eCPUCommand::programming:
		{
			// [#] [P] [len] [subcommand] [optional_data] [ck]
			const eCPUProgrammingCommand subcommand = (eCPUProgrammingCommand)bufferToSend[3];
			switch (subcommand)
			{
			default:
				return false;

			case eCPUProgrammingCommand::attivazioneMotore:
			case eCPUProgrammingCommand::scivolo_brewmatic:
				//rispondo con lo stesso msg che ho ricevuto
				{
					const u8 len = bufferToSend[2];
					memcpy (out_answer, bufferToSend, len);
					*in_out_sizeOfAnswer = len;
					return true;
				}
				break;

			case eCPUProgrammingCommand::getLastGrinderSpeed:
				//# P [len] 0x23 [velocit� macina LSB MSB] [ck]
				{
					const u16 speed = rhea::randomU32(1000);
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					rhea::utils::bufferWriteU16_LSB_MSB (&out_answer[ct], speed);
					ct += 2;

					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
					return true;
				}
				break;

			case eCPUProgrammingCommand::testSelezione:
				if (timeToEndTestSelezioneMSec == 0)
				{
					timeToEndTestSelezioneMSec = rhea::getTimeNowMSec() + 8000;
					VMCState = eVMCState::TEST_ATTUATORE_SELEZIONE;
				}
				
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = bufferToSend[4];
				out_answer[ct++] = bufferToSend[5];

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
				break;

			case eCPUProgrammingCommand::setTime:
			case eCPUProgrammingCommand::setDate:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = bufferToSend[4];
				out_answer[ct++] = bufferToSend[5];
				out_answer[ct++] = bufferToSend[6];

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
				break;

			case eCPUProgrammingCommand::setFattoreCalibrazioneMotore:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = bufferToSend[4]; //motore
				out_answer[ct++] = bufferToSend[5];
				out_answer[ct++] = bufferToSend[6];

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
				break;

			case eCPUProgrammingCommand::getStatoGruppo:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = (rhea::random01() > 0.8f) ? 0 : 1;
				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
				break;

			case eCPUProgrammingCommand::calcolaImpulsiMacina:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = bufferToSend[4];
				out_answer[ct++] = bufferToSend[5];
				out_answer[ct++] = bufferToSend[6];

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
				break;

			case eCPUProgrammingCommand::getTime:
			{
				rhea::DateTime dt;
				dt.setNow();
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = dt.time.getHour();
				out_answer[ct++] = dt.time.getMin();
				out_answer[ct++] = dt.time.getSec();

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
			}
			break;

			case eCPUProgrammingCommand::getDate:
			{
				rhea::DateTime dt;
				dt.setNow();
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = (u8)(dt.date.getYear() -2000);
				out_answer[ct++] = dt.date.getMonth();
				out_answer[ct++] = dt.date.getDay();

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
			}
			break;

			case eCPUProgrammingCommand::getNomiLinguaCPU:
			{
				const char isUnicode = 1;
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = isUnicode;

				if (!isUnicode)
				{
					u8 i2 = ct;
					for (u8 i = 0; i < 64; i++)
						out_answer[ct++] = ' ';
					out_answer[i2] = 'E';
					out_answer[i2 + 1] = 'N';
					out_answer[i2 + 2] = '1';

					i2 += 32;
					out_answer[i2] = 'E';
					out_answer[i2 + 1] = 'N';
					out_answer[i2 + 2] = '2';
				}
				else
				{
					const u8 startOfMsg1 = ct;
					for (u8 i = 0; i < 64; i++)
					{
						out_answer[ct++] = ' ';
						out_answer[ct++] = 0x00;
					}

					u8 i2= startOfMsg1;
					out_answer[i2++] = 'E'; out_answer[i2++] = 0x00;
					out_answer[i2++] = 'N'; out_answer[i2++] = 0x00;
					out_answer[i2++] = '1'; out_answer[i2++] = 0x00;

					//האלפבית העבר
					{
						const u16 utf16Seq[] = { 0x05d4, 0x05d0, 0x05dc, 0x05e4, 0x05d1, 0x05d9, 0x05ea, 0x0020, 0x05d4, 0x05e2, 0x05d1, 0x05e8, 0x0000 };
						i2 = startOfMsg1 + 64;
						rhea::string::utf16::utf16SequenceToU8Buffer_LSB_MSB(utf16Seq, &out_answer[i2], 64, false);
					}
				}

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
			}
			break;

			case eCPUProgrammingCommand::getStatoCalcoloImpulsi:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = 0;
				if (rhea::random01() > 0.7)
				{
					out_answer[ct++] = 0;
					out_answer[ct++] = 0;
				}
				else
				{
					const u16 impulsi = (u16)(50 + rhea::randomU32(200));
					out_answer[ct++] = (u8)(impulsi & 0x00FF);
					out_answer[ct++] = (u8)((impulsi & 0xFF00) >> 8);
				}

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;
				break;

			// si vuole iniziare un cleaning di qualche dispositivo
			case eCPUProgrammingCommand::cleaning:
				//fingo un cleaning
				memset(&cleaning, 0, sizeof(cleaning));
				cleaning.freeBuffer8[0] = 0x00;	cleaning.freeBuffer8[1] = 0x01;	cleaning.freeBuffer8[2] = 0x02;	cleaning.freeBuffer8[3] = 0x03;
				cleaning.freeBuffer8[4] = 0x04;	cleaning.freeBuffer8[5] = 0x05;	cleaning.freeBuffer8[6] = 0x06;	cleaning.freeBuffer8[7] = 0x07;
				cleaning.cleaningType = (eCPUProg_cleaningType)bufferToSend[4];
				cleaning.fase = 1;
				cleaning.timeToEnd = rhea::getTimeNowMSec() + 4000;
				cleaning.prevState = this->VMCState;

				if (cleaning.cleaningType == eCPUProg_cleaningType::sanitario)
					this->VMCState = eVMCState::LAVAGGIO_SANITARIO;
				else if (cleaning.cleaningType == eCPUProg_cleaningType::descaling)
					this->VMCState = eVMCState::DESCALING;
				else if (cleaning.cleaningType == eCPUProg_cleaningType::milker || cleaning.cleaningType == eCPUProg_cleaningType::milkerQuick)
				{
					priv_DA3_reload();
					if (da3[69] == 2)
					{
						this->VMCState = eVMCState::LAVAGGIO_MILKER_INDUX;
						cleaning.timeToEnd = rhea::getTimeNowMSec() + 12000;
					}
					else
						this->VMCState = eVMCState::LAVAGGIO_MILKER_VENTURI;
				}
				else
					this->VMCState = eVMCState::LAVAGGIO_MANUALE;

				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getSelectionParam:
				//rcv: # P [len] 0x34 [selNum] [paramID] [ck]
				{
					const u16 paramValue = bufferToSend[5] + 100; //rispondo con un valore fittizio

					//snd: # P [len] 0x34 [selNum] [paramID] [value16 LSB-MSB] [ck]
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					out_answer[ct++] = (u8)bufferToSend[4];
					out_answer[ct++] = (u8)bufferToSend[5];
					rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], paramValue);
					ct+=2;
					
					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
					return true;
				}
				break;

			case eCPUProgrammingCommand::setSelectionParam:
				//rcv: # P [len] 0x33 [selNum] [paramID] [value16 LSB-MSB] [ck]
				{
					//fingo di aver fatto qualcosa e rispondo "tutto ok" riportando lo stesso identico msg che ho ricevuto
					//snd: # P [len] 0x33 [selNum] [paramID] [value16 LSB-MSB] [ck]
					*in_out_sizeOfAnswer = out_answer[2]; 
					return true;
				}
				break;

			//periodicamente la SMU fa delle query per conoscere lo stato del cleaning. La simulazione dell'avanzamento del processo di cleaning
			//si trova dentro priv_advanceFakeCleaning()
			case eCPUProgrammingCommand::querySanWashingStatus:
				if (cleaning.cleaningType == eCPUProg_cleaningType::sanitario ||
					cleaning.cleaningType == eCPUProg_cleaningType::milker ||
					cleaning.cleaningType == eCPUProg_cleaningType::milkerQuick ||
					cleaning.cleaningType == eCPUProg_cleaningType::descaling)
				{
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					out_answer[ct++] = cleaning.fase; //fase
					out_answer[ct++] = cleaning.btn1;
					out_answer[ct++] = cleaning.btn2;
					memcpy (&out_answer[ct], cleaning.freeBuffer8, 8);
					ct += 8;

					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
					return true;
				}
				break;

			case eCPUProgrammingCommand::setDecounter:
				{
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					const u8 whichOne = out_answer[ct++] = bufferToSend[4]; //which one
					const u16 value = rhea::utils::bufferReadU16_LSB_MSB(&bufferToSend[5]);
					out_answer[ct++] = bufferToSend[5];	//value LSB
					out_answer[ct++] = bufferToSend[6]; //value MSB

					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];

					decounterVari[whichOne] = value;
				}
				return true;

			case eCPUProgrammingCommand::getAllDecounterValues:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto1]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto2]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto3]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto4]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto5]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto6]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto7]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto8]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto9]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::prodotto10]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::waterFilter]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::coffeeBrewer]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::coffeeGround]); ct += 2;
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::blocking_counter]); ct += 2;
				
				//questo decounter � codificato in 3 byte
				rhea::utils::bufferWriteU24_LSB_MSB(&out_answer[ct], decounterVari[(u8)cpubridge::eCPUProg_decounter::maintenance_counter]); ct += 3;

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getPosizioneMacina:
				{
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					out_answer[ct++] = bufferToSend[4]; //macina

					u8 macinaIndex = bufferToSend[4] - 11;
					if (macinaIndex >= 10)
						macinaIndex = 0;

					u16 pos = macine[macinaIndex].posizioneMacina;
					//simulo il fatto che la posizione � un po' ballerina
					if (rhea::random01() < 0.4f)
						pos++;
					rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], pos);

					ct += 2;
					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
				}
				return true;

			case eCPUProgrammingCommand::setMotoreMacina:
				{
					u8 macinaIndex = bufferToSend[4] - 11;
					if (macinaIndex >= 10)
						macinaIndex = 0;

					macine[macinaIndex].tipoMovimentoMacina = bufferToSend[5];


					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					out_answer[ct++] = bufferToSend[4]; //macina
					out_answer[ct++] = bufferToSend[5]; //tipo di movimento
					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
				}
				return true;

			case eCPUProgrammingCommand::EVAresetPartial:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::EVAresetTotals:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getVoltAndTemp:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;

				out_answer[ct++] = 30; //temp_camera
				out_answer[ct++] = 31; //temp_tBollitore
				out_answer[ct++] = 32; //temp_cappuccinatore
				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], 220); //voltage
				ct += 2;

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getCPUOFFReportDetails:
				{
					const u8 startIndex = bufferToSend[4];
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;

					out_answer[ct++] = startIndex; //start index
					if (startIndex == 0)
					{
						out_answer[ct++] = 6; //last index

						out_answer[ct++] = '7'; out_answer[ct++] = 'A';  out_answer[ct++] = 16; out_answer[ct++] = 32; out_answer[ct++] = 3; out_answer[ct++] = 1; out_answer[ct++] = 20; out_answer[ct++] = 1;
						out_answer[ct++] = '8'; out_answer[ct++] = ' '; out_answer[ct++] = 18; out_answer[ct++] = 2; out_answer[ct++] = 4; out_answer[ct++] = 1; out_answer[ct++] = 20; out_answer[ct++] = 0;
					}
					else
					{
						out_answer[ct++] = 19; //last index

						out_answer[ct++] = '9'; out_answer[ct++] = 'B';  out_answer[ct++] = 8; out_answer[ct++] = 49; out_answer[ct++] = 5; out_answer[ct++] = 1; out_answer[ct++] = 20; out_answer[ct++] = 0;
						out_answer[ct++] = '5'; out_answer[ct++] = 'C'; out_answer[ct++] = 1; out_answer[ct++] = 32; out_answer[ct++] = 6; out_answer[ct++] = 1; out_answer[ct++] = 20; out_answer[ct++] = 1;
					}
					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
				}
				return true;

			case eCPUProgrammingCommand::getLastFluxInformation:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;

				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], 80 + (u16)rhea::randomU32(800)); //last flux
				ct += 2;

				rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], 80 + (u16)rhea::randomU32(250)); //last grinder position
				ct += 2;

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getStringVersionAndModel:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;

				out_answer[ct++] = 0; //is unicode

				memset(&out_answer[ct], ' ', 32);
				{
					u8 i = ct;
					out_answer[i++] = 'V'; out_answer[i++] = 'E'; out_answer[i++] = 'R'; out_answer[i++] = ' ';
					out_answer[i++] = '0'; out_answer[i++] = '.'; out_answer[i++] = '1';
					out_answer[i++] = ' '; 
					out_answer[i++] = ','; out_answer[i++] = ' ';
					out_answer[i++] = 'M'; out_answer[i++] = 'O'; out_answer[i++] = 'D'; out_answer[i++] = 'E'; out_answer[i++] = 'L';
					out_answer[i++] = '1'; out_answer[i++] = '2'; out_answer[i++] = '3';
				}
				ct += 32;

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::startModemTest:
				//fingo un modem test. CPU deve andare in stato TEST_MODEM(22) e rimanerci per un po' di tempo
				testModem.timeToEndMSec = rhea::getTimeNowMSec() + 5000;

				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getTimeNextLavaggioCappuccinatore:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = 12;
				out_answer[ct++] = 34;

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::startTestAssorbGruppo:
				//fingo un test di "assorbimento gruppo"
				testAssorbGruppo.timeToEndMSec = rhea::getTimeNowMSec() + 5000;
				testAssorbGruppo.fase = 0;
				testAssorbGruppo.esito = 0;
				testAssorbGruppo.result12[0] = 1;
				testAssorbGruppo.result12[1] = 12;
				testAssorbGruppo.result12[2] = 123;
				testAssorbGruppo.result12[3] = 1234;
				testAssorbGruppo.result12[4] = 12345;
				testAssorbGruppo.result12[5] = 8;
				testAssorbGruppo.result12[6] = 87;
				testAssorbGruppo.result12[7] = 876;
				testAssorbGruppo.result12[8] = 8765;
				testAssorbGruppo.result12[9] = 54321;
				testAssorbGruppo.result12[10] = 65535;
				testAssorbGruppo.result12[11] = 4637;

				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getStatusTestAssorbGruppo:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = testAssorbGruppo.fase;
				out_answer[ct++] = testAssorbGruppo.esito;
				for (u8 i = 0; i < 12; i++)
				{
					rhea::utils::bufferWriteU16_LSB_MSB (&out_answer[ct], testAssorbGruppo.result12[i]);
					ct += 2;
				}

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::startTestAssorbMotoriduttore:
				//fingo un test di "assorbimento motoriduttore"
				testAssorbGruppo.timeToEndMSec = rhea::getTimeNowMSec() + 5000;
				testAssorbGruppo.fase = 0;
				testAssorbGruppo.esito = 0;
				testAssorbGruppo.result12[0] = 1;
				testAssorbGruppo.result12[1] = 12;
				testAssorbGruppo.result12[2] = 123;
				testAssorbGruppo.result12[3] = 1234;

				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getStatusTestAssorbMotoriduttore:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				out_answer[ct++] = testAssorbGruppo.fase;
				out_answer[ct++] = testAssorbGruppo.esito;
				for (u8 i = 0; i < 2; i++)
				{
					rhea::utils::bufferWriteU16_LSB_MSB(&out_answer[ct], testAssorbGruppo.result12[i]);
					ct += 2;
				}

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::getCupSensorLiveValue:
				out_answer[ct++] = '#';
				out_answer[ct++] = 'P';
				out_answer[ct++] = 0; //lunghezza
				out_answer[ct++] = (u8)subcommand;
				
				rhea::utils::bufferWriteU16_LSB_MSB (&out_answer[ct], (u16)rhea::randomU32(0xffff));
				ct += 2;

				out_answer[2] = (u8)ct + 1;
				out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
				*in_out_sizeOfAnswer = out_answer[2];
				return true;

			case eCPUProgrammingCommand::caffeCortesia:
				//fingo una selezione
				statoPreparazioneBevanda = eStatoPreparazioneBevanda::wait;
				runningSel.selNum = 1;
				runningSel.timeStartedMSec = rhea::getTimeNowMSec();
				VMCState = eVMCState::PREPARAZIONE_BEVANDA;
				return true;

			case eCPUProgrammingCommand::activate_cpu_buzzer:
				//in: # P [len] 0x27 [byte1] [byte2] [ck]
				{
					const u8 b1 = bufferToSend[4];
					const u8 b2 = bufferToSend[5];
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					out_answer[ct++] = b1;
					out_answer[ct++] = b2;

					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];

					if (b1 == 0)
						buzzerIsRunnigUntilTime_mSec = rhea::getTimeNowMSec() + (u64)(b2 & 0x0F)*100;
					else
					{
						const u64 numRepeat = (b1 & 0x7f);
						const u64 dsecUp = (b2 & 0x0f);
						const u64 dsecDown = ((b2 & 0xf0) >> 4);
						const u64 totalMSec = 100 * ((dsecUp * numRepeat) + (dsecDown * (numRepeat-1)));
						buzzerIsRunnigUntilTime_mSec = rhea::getTimeNowMSec() + totalMSec;
					}
				}
				return true;

			case eCPUProgrammingCommand::get_cpu_buzzer_status:
				//# P [len] 0x28 [status] [ck]
				{
					u8 buzzerRunning = 0;
					if (rhea::getTimeNowMSec() <= buzzerIsRunnigUntilTime_mSec)
						buzzerRunning = 1;
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					out_answer[ct++] = buzzerRunning;

					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
				}
				return true;


			case eCPUProgrammingCommand::stop_jug:
				//# P [len] 0x2A [ck]
				{
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;

					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
				}
				return true;

			case eCPUProgrammingCommand::get_jug_current_repetition:
				//# P [len] 0x29 [n] [m] [ck]
				{
					u8 n = 0;
					u8 m = 0;

					if (eStatoPreparazioneBevanda::running == statoPreparazioneBevanda)
					{
						if (NULL == da3)
							priv_DA3_reload();

						m = da3[0xb1 + 100 * (runningSel.selNum-1)];
						if (m > 1)
							n = 1;
						else
							m = 0;
					}

					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					out_answer[ct++] = n;
					out_answer[ct++] = m;
					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
				}
				return true;

			case eCPUProgrammingCommand::ask_msg_from_table_language:
				//# P [len] 0x31 [tabellaID] [rigaNum] [lingua1o2] [ck]
				{
					const u8 tableID = bufferToSend[4];
					const u8 rigaNum = bufferToSend[5];
					const u8 lingua1or2 = bufferToSend[6];

					/*
					const u8 isUnicode = 0;
					u8 msg[32];
					const u8 msgLen = static_cast<u8>(priv_utils_giveMeAnExtendedASCIIStringWithStrangeChar (msg, sizeof(msg)));
					*/

					const u8 isUnicode = 1;
					u16 msg[64];
					const u8 msgLen = static_cast<u8>(priv_utils_giveMeAUTF16StringWithStrangeChar2 (msg, sizeof(msg)));
					

					//snd: # P [len] 0x31 [tabellaID] [rigaNum] [lingua1o2] [isUnicode] [msg_utf16_LSB_MSB...] [ck]
					out_answer[ct++] = '#';
					out_answer[ct++] = 'P';
					out_answer[ct++] = 0; //lunghezza
					out_answer[ct++] = (u8)subcommand;
					out_answer[ct++] = tableID;
					out_answer[ct++] = rigaNum;
					out_answer[ct++] = lingua1or2;
					out_answer[ct++] = isUnicode;
					if (isUnicode)
					{
						for (u8 i = 0; i < msgLen / 2; i++)
						{
							const u16 u = msg[i];
							const u8 lsb = ((u & 0x00FF));
							const u8 msb = ((u & 0xFF00) >> 8);
							out_answer[ct++] = lsb;
							out_answer[ct++] = msb;
						}
					}
					else
					{
						memcpy (&out_answer[ct], msg, msgLen);
						ct += msgLen;
					}

					out_answer[2] = (u8)ct + 1;
					out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
					*in_out_sizeOfAnswer = out_answer[2];
				}
				return true;

			} //switch (subcommand)
		}
		break;
	}
	return false;
}

//*****************************************************************
u32 CPUChannelFakeCPU::waitForAMessage (u8 *out_answer UNUSED_PARAM, u32 sizeOf_outAnswer UNUSED_PARAM, rhea::ISimpleLogger *logger UNUSED_PARAM, u64 timeoutRCVMsec UNUSED_PARAM)
{
    return 0;
}

//*****************************************************************
void CPUChannelFakeCPU::priv_advanceFakeCleaning()
{
	if (cleaning.cleaningType == eCPUProg_cleaningType::sanitario)
	{
		//cleaning sanitario del gruppo
		if (rhea::getTimeNowMSec() >= cleaning.timeToEnd)
		{
			cleaning.timeToEnd += 2000;
			cleaning.btn1 = cleaning.btn2 = 0;
			++cleaning.fase;

			if (cleaning.fase == 3 || cleaning.fase == 12)
			{
				cleaning.btn1 = 10;
				cleaning.timeToEnd += 3000;
			}
			if (cleaning.fase == 11 || cleaning.fase == 13)
			{
				cleaning.btn1 = 10;
				cleaning.btn2 = 1;
				cleaning.timeToEnd += 3000;
			}

			if (cleaning.fase >= 19)
			{
				cleaning.cleaningType = eCPUProg_cleaningType::invalid;
				this->VMCState = cleaning.prevState;
			}
		}
	}
	else if (cleaning.cleaningType == eCPUProg_cleaningType::milker || cleaning.cleaningType == eCPUProg_cleaningType::milkerQuick)
	{
		//cleaning del milker
		if (cleaning.fase == 1)
		{
			cleaning.btn1 = 10;
			cleaning.btn2 = 1;
		}

		if (rhea::getTimeNowMSec() >= cleaning.timeToEnd)
		{
			cleaning.timeToEnd += 2000;
			cleaning.btn1 = cleaning.btn2 = 0;
			++cleaning.fase;

			if (cleaning.fase >= 6)
			{
				cleaning.cleaningType = eCPUProg_cleaningType::invalid;
				this->VMCState = cleaning.prevState;
			}
		}
	}
	else if (cleaning.cleaningType == eCPUProg_cleaningType::descaling)
	{
		//descaling
		if (rhea::getTimeNowMSec() >= cleaning.timeToEnd)
		{
			cleaning.timeToEnd += 2000;
			cleaning.btn1 = cleaning.btn2 = 0;
			++cleaning.fase;

			if (cleaning.fase == 1 || cleaning.fase == 3 || cleaning.fase == 4)
			{
				cleaning.btn1 = 10;
				cleaning.timeToEnd += 3000;
			}

			if (cleaning.fase >= 19)
			{
				cleaning.cleaningType = eCPUProg_cleaningType::invalid;
				this->VMCState = cleaning.prevState;
			}
		}
	}
	else if (cleaning.cleaningType != eCPUProg_cleaningType::invalid)
	{
		//tutti gli altri cleaning
		if (rhea::getTimeNowMSec() >= cleaning.timeToEnd)
		{
			if (rhea::getTimeNowMSec() >= cleaning.timeToEnd)
			{
				cleaning.timeToEnd += 2000;
				cleaning.btn1 = cleaning.btn2 = 0;
				++cleaning.fase;

				if (cleaning.fase >= 4)
				{
					cleaning.cleaningType = eCPUProg_cleaningType::invalid;
					this->VMCState = cleaning.prevState;
				}
			}
		}
	}
}

//*****************************************************************
void CPUChannelFakeCPU::priv_buildAnswerTo_checkStatus_B(u8 *out_answer, u16 *in_out_sizeOfAnswer)
{
    bool CPUFLAG_isMilkerAlive = true;
	bool CPUFLAG_isFreevend = false;
	bool CPUFLAG_isTestvend = false;
				
	memset(out_answer, 0, *in_out_sizeOfAnswer);
	
	//gestione fake del cleaning
	if (cleaning.cleaningType != eCPUProg_cleaningType::invalid)
		priv_advanceFakeCleaning();

	//gestione fake del "test selezione"
	if (VMCState == eVMCState::TEST_ATTUATORE_SELEZIONE)
	{
		if (rhea::getTimeNowMSec() > timeToEndTestSelezioneMSec)
		{
			timeToEndTestSelezioneMSec = 0;
			VMCState = eVMCState::DISPONIBILE;
		}
	}

	//gestione fake del "test modem"
	if (testModem.timeToEndMSec > 0)
	{
		VMCState = eVMCState::TEST_MODEM;
		if (rhea::getTimeNowMSec() >= testModem.timeToEndMSec)
		{
			testModem.timeToEndMSec = 0;
			VMCState = eVMCState::DISPONIBILE;
		}
	}

	//gestione fake del "test assorbimento gruppo" e "assorbimento motoriduttore"
	if (testAssorbGruppo.timeToEndMSec > 0)
	{
		const u64 timeNowMSec = rhea::getTimeNowMSec();
		if (timeNowMSec >= testAssorbGruppo.timeToEndMSec)
		{
			testAssorbGruppo.fase++;
			if (testAssorbGruppo.fase >= 5)
			{
				testAssorbGruppo.fase = 5;
				testAssorbGruppo.timeToEndMSec = 0;
			}
			else
				testAssorbGruppo.timeToEndMSec = timeNowMSec + 1000;
		}
	}
	

	u32 ct = 0;

	/*
	0		#
	1		B
	2		lunghezza in byte del messaggio
	*/
	out_answer[ct++] = '#';
	out_answer[ct++] = 'Z';
	//out_answer[ct++] = 'B';
	out_answer[ct++] = 0; //lunghezza

	/*
	3		eVMCState
	4		VMCerrorCode
	5		VMCerrorType
	*/
	out_answer[ct++] = (u8)VMCState;
	out_answer[ct++] = 0;
	out_answer[ct++] = 0;

	/*
	6	??
	7	??
	8	??
	*/
	out_answer[ct++] = 0;
	out_answer[ct++] = 0;
	out_answer[ct++] = 0;

	//9		CupAbsentStatus_flag & bShowDialogStopSelezione && statoPreparazioneBevanda
	out_answer[ct] = ((u8)statoPreparazioneBevanda << 5);
	if (bShowDialogStopSelezione)
		out_answer[ct] |= 0x10;
	++ct;

	//10		selection_CPU_current
	out_answer[ct++] = 0;


	//messaggio di CPU
	if (out_answer[1] == 'B')
	{
		memset(&out_answer[ct], 0x00, 32);
		priv_utils_giveMeAnExtendedASCIIStringWithStrangeChar (&out_answer[ct], 32);
		ct += 32;
	}
	else
	{
		//11-74		32 unicode char col messaggio di stato
		assert(ct == 11);
		memset(&out_answer[ct], 0x00, 64);

		u32 n = (u32)rhea::string::utf16::lengthInBytes(utf16_curCPUMessage) / 2;
		for (u8 i = 0; i < n; i++)
		{
			out_answer[ct++] = (u8)(utf16_curCPUMessage[i] & 0x00FF);
			out_answer[ct++] = (u8)((utf16_curCPUMessage[i] & 0xFF00) >> 8);
		}

		ct = 11 + 64;
	}

	/*
	75		6 byte con lo stato di disponibilit�   delle 48 selezioni
	76		ATTENZIONE che il bit a zero significa che la selezione � ¨ disponibile, il bit
	77		a 1 significa che NON � ¨ disponibile
	78
	79
	80
	*/
	const u8 selAvailability[6][8] = { 
		{ 0, 0, 0, 0, 0, 0,	0, 0 },	//01-08
		{ 0, 0, 0, 0, 0, 0,	0, 0 },	//09-16
		{ 0, 0, 0, 0, 0, 0,	0, 0 },	//17-24
		{ 0, 0, 0, 0, 0, 0,	0, 0 },	//25-32
		{ 0, 0, 0, 0, 0, 0,	0, 0 },	//33-40
		{ 0, 0, 0, 0, 0, 0,	0, 0 }	//41-48
	};

	for (u8 i2 = 0; i2 < 6; i2++)
	{
		u8 b = 0;
		for (u8 i3 = 0; i3 < 8; i3++)
		{
			if (selAvailability[i2][i3] != 0)
				b |= (0x01 << i3);
		}
		out_answer[ct++] = b;
	}

	/*
	81		beepTime dSec LSB
	82		beepTime dSec MSB
	*/
	out_answer[ct++] = 3;
	out_answer[ct++] = 0;


	//83		linguaggio (default=='0' o ML=='1')
	out_answer[ct++] = '0';

	//84-85		se linguaggio ML=='1', questi 2 byte indicano la lingua in ASCII
	out_answer[ct++] = 'G';
	out_answer[ct++] = 'B';

	//86		1 byte per indicare importanza del msg di CPU (0=poco importante, 1=importante)
	out_answer[ct++] = curCPUMessageImportanceLevel;

	//87-94		8 byte stringa con l'attuale credito
	out_answer[ct++] = '0';
	out_answer[ct++] = '.';
	out_answer[ct++] = '0';
	out_answer[ct++] = '0';
	out_answer[ct++] = ' ';
	out_answer[ct++] = ' ';
	out_answer[ct++] = ' ';
	out_answer[ct++] = ' ';


	if (CPU_REPORTED_PROTOCOL_VERSION >= 4)
	{
		//protocol version 4
		//1 byte per indicare se btn prog � ¨ cliccato
		out_answer[ct++] = 0;

		//protocol version 5
		if (CPU_REPORTED_PROTOCOL_VERSION >= 5)
		{
			//1 byte a mo' di 8 bit flag per usi futuri
			u8 newFCPUFlag1 = 0;
			out_answer[ct] = 0;
			
			newFCPUFlag1 |= 0x01; //indica che CPU � ¨ pronta per fornire il data-audit

			//protocol version 6
			if (CPU_REPORTED_PROTOCOL_VERSION >= 6)
			{
				bool bSimulaStatoTelemetria = false;
				if (bSimulaStatoTelemetria)
					newFCPUFlag1 |= 0x02; //indica se CPU � ¨ in telemetria oppure no (default: no)
			}

			//protocol version 7
			if (CPU_REPORTED_PROTOCOL_VERSION >= 7)
			{
				if (CPUFLAG_isMilkerAlive)	newFCPUFlag1 |= 0x04;
				if (CPUFLAG_isFreevend)		newFCPUFlag1 |= 0x08;
				if (CPUFLAG_isTestvend)		newFCPUFlag1 |= 0x10;
			}

			out_answer[ct++] = newFCPUFlag1;
		}
	}


	//116 ck
	out_answer[2] = (u8)ct+1;
	out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
	ct++;
	(*in_out_sizeOfAnswer) = (u16)ct;
}


//*****************************************************************
bool CPUChannelFakeCPU::priv_handleSnackCommand (const u8 *bufferToSend, u16 nBytesToSend, u8 *out_answer, u16 *in_out_sizeOfAnswer, rhea::ISimpleLogger *logger, u64 timeoutRCVMsec)
{
	assert ((eCPUCommand)bufferToSend[1] == eCPUCommand::snackCommand);

	// [#] [Y] [len] [subcommand] [optional_data] [ck]
	const eSnackCommand subcommand = (eSnackCommand)bufferToSend[3];
	u32 ct = 0;
	out_answer[ct++] = '#';
	out_answer[ct++] = 'Y';
	out_answer[ct++] = 0; //lunghezza
	out_answer[ct++] = (u8)subcommand;

	switch (subcommand)
	{
	default:
		return false;

	case eSnackCommand::machineStatus:
		//snd: # Y [len] 0x03 [stato] [stato_sel_1-8]... [stato_sel_41-48] [ck]
		out_answer[ct++] = 1;
		out_answer[ct++] = 0xff;
		out_answer[ct++] = 0xff;
		out_answer[ct++] = 0xff;
		out_answer[ct++] = 0xff;
		out_answer[ct++] = 0xff;
		out_answer[ct++] = 0xff;
		break;

	case eSnackCommand::enterProg:
	case eSnackCommand::exitProg:
		//snd: # Y [len] 0x04 [esito] [ck]
		out_answer[ct++] = 1;
		break;

		//case eSnackCommand::getPrices = 0x01,
		//case eSnackCommand::setPrices = 0x02,

	}


	//checksumn
	out_answer[2] = (u8)ct + 1;
	out_answer[ct] = rhea::utils::simpleChecksum8_calc(out_answer, ct);
	*in_out_sizeOfAnswer = out_answer[2];
	return true;
}