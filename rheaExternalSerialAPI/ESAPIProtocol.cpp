#include "ESAPIProtocol.h"
#include "ESAPI.h"
#include "../CPUBridge/CPUBridge.h"
#include "../rheaCommonLib/rheaUtils.h"
#include "../rheaCommonLib/rheaString.h"
#include "../rheaCommonLib/string/rheaUTF8String.h"

using namespace esapi;

//*********************************************************
Protocol::Protocol()
{
    rhea::rs232::setInvalid (rs232);
    localAllocator = NULL;
    logger = NULL;
    cpuBridgeSubscriber = NULL;
    waitableGrp = NULL;
    bufferOUT = NULL;
	commandC8InProgress = false;
    runningSel.status = cpubridge::eRunningSelStatus::finished_OK;
}

//*********************************************************
bool Protocol::setup (rhea::Allocator *allocator, cpubridge::sSubscriber *cpuBridgeSubscriber, const char *serialPort, OSWaitableGrp *waitableGrp, rhea::ISimpleLogger *loggerIN)
{
    this->localAllocator = allocator;
    this->logger = loggerIN;
    this->cpuBridgeSubscriber = cpuBridgeSubscriber;

    //apertura rs232
	logger->log ("esapi::Protocol::setup => opening com=%s   ", serialPort);
    const bool SERIAL_IS_BLOCKING = false;
    if (!rhea::rs232::open(&rs232, serialPort, eRS232BaudRate::b115200, false, false, eRS232DataBits::b8, eRS232Parity::No, eRS232StopBits::One, eRS232FlowControl::No, SERIAL_IS_BLOCKING))
    {
        logger->log ("FAILED. unable to open port [%s]\n", serialPort);
        logger->decIndent();
        return false;
    }
	logger->log ("OK\n");

#ifdef LINUX
    waitableGrp->addSerialPort (rs232, ESAPI_WAITABLEGRP_EVENT_FROM_RS232);
#endif

    //buffer
    bufferOUT = RHEAALLOCT(u8*, localAllocator, SIZE_OF_BUFFEROUT);
    rs232BufferIN.alloc (localAllocator, 1024);

    return true;
}

//*********************************************************
void Protocol::priv_unsetup()
{
    if (NULL == localAllocator)
        return;

    if (NULL != waitableGrp)
    {
#ifdef LINUX
        waitableGrp->removeSerialPort (rs232);
#endif
        waitableGrp = NULL;
    }

    rhea::rs232::close (rs232);

    if (bufferOUT)
    {
        RHEAFREE(localAllocator, bufferOUT);
        bufferOUT = NULL;
    }
    rs232BufferIN.free(localAllocator);

    localAllocator = NULL;
}

//*********************************************************
void Protocol::priv_onCPUNotify_RUNNING_SEL_STATUS(const rhea::thread::sMsg &msg)
{
    cpubridge::translateNotify_CPU_RUNNING_SEL_STATUS(msg, &runningSel.status);
}

//*********************************************************
bool Protocol::onMsgFromCPUBridge(UNUSED_PARAM cpubridge::sSubscriber &cpuBridgeSubscriber, const rhea::thread::sMsg &msg, u16 handlerID)
{
    if (handlerID == 0)
    {
        //in queso caso, CPUBridge ha mandato una notifica di sua spontanea volont� , non � una risposta ad una mia specifica richiesta.
        switch (msg.what)
        {
        default:
            return false;

        case CPUBRIDGE_NOTIFY_CPU_RUNNING_SEL_STATUS:
            priv_onCPUNotify_RUNNING_SEL_STATUS(msg);
            return true;

		case CPUBRIDGE_NOTIFY_READ_DATA_AUDIT_PROGRESS:
			//risposta a C8 (data audit)
			//rispondo: # C 8 [payloadLen] [payload] ... [payload] [ck]
			if (commandC8InProgress)
			{
				cpubridge::eReadDataFileStatus status;
				u16 totKbSoFar = 0;
				u16 fileID = 0;
				u8 buffer[256];
				u8 nBytesInBuffer = 0;
				cpubridge::translateNotify_READ_DATA_AUDIT_PROGRESS (msg, &status, &totKbSoFar, &fileID, &buffer[1], &nBytesInBuffer);

				if (nBytesInBuffer)
				{
					buffer[0] = nBytesInBuffer;
					rs232_esapiSendAnswer ('C', '8', buffer, 1 + nBytesInBuffer);
				}
			}
			return true;
		}
    }
    else
    {
        //in questo caso invece, � una risposta ad una mia domanda specifica
        switch (msg.what)
        {
        default:
            DBGBREAK;
            return false;
		
        case CPUBRIDGE_NOTIFY_BROWSER_URL_CHANGE:
            //lo ignoriamo
            return false;

		case CPUBRIDGE_NOTIFY_LOCK_STATUS:
			//risposta al comando C9
			//risposta al comando S5
            {
	            cpubridge::eLockStatus lockStatus;
	            cpubridge::translateNotify_MACHINE_LOCK(msg, &lockStatus);

				u8 buffer[2];
				buffer[0] = static_cast<u8>(lockStatus);

				if (0x0001 == handlerID)
				{
					//rispondo: # C 9 [lockStatus] [ck]
					rs232_esapiSendAnswer ('C', '9', buffer, 1);
				}
				else if (0x0002 == handlerID)
				{
					//rispondo: # S 5 [current_lock_status] [ck]
					rs232_esapiSendAnswer ('S', '5', buffer, 1);
				}
            }
            return true;

		case CPUBRIDGE_NOTIFY_GET_CPU_STRING_MODEL_AND_VER:
			//risposta al comando A5
            {
				u16		versionString[64];
				u8		len;
				char	buf[66];
				memset (versionString, 0, sizeof(versionString));
	            cpubridge::translateNotify_CPU_STRING_VERSION_AND_MODEL(msg, versionString, sizeof(versionString));

				if (false == commandCpuVerExtended)
				{
					for (len = 0; len < 8; len++)
						buf[len] = static_cast<char>(versionString[len]);
				}
				else
				{
					for (len = 0; len < sizeof(versionString) / sizeof(versionString[0]) && versionString[len] != 0; len++)
					{
						// empty loop
					}

					buf[0] = 'b';
					buf[1] = len;
					len += 2;
					for (u8 k = 0, i = 2; i < len; k++, i++)
						buf[i] = static_cast<char>(versionString[k]);
				}
					
				//rispondo: # A 5 [ASCII_0] ... [ASCII_7] [ck]
				rs232_esapiSendAnswer('A', '5', buf, len);
            }
            return true;

		case CPUBRIDGE_NOTIFY_CPU_RESTART:
			//risposta al comando #A3 restart
			//Arriviamo qui quando abbiamo inviato il comando di reboot alla CPU
			//A questo punto, dobbiamo fare il reboot di noistessi
            rhea::reboot();
			return true;

		case CPUBRIDGE_NOTIFY_QUERY_ID101:
            //risposta al comando  #A2
            {
	            u32 id101 = 0;
	            cpubridge::translateNotify_CPU_QUERY_ID101(msg, &id101);

                //rispondo: # A 2 'R' 'H' 'V' '2' '0' [id101 come stringa di 9 ASCCI char] [ck]
				char s[16];
                sprintf_s (s, sizeof(s), "RHV20%09d", id101);
                rs232_esapiSendAnswer ('A', '2', s, 14);
            }
            return true;

        case CPUBRIDGE_NOTIFY_CPU_NEW_LCD_MESSAGE:
            //risposta al comando  #C1
            {
	            cpubridge::sCPULCDMessage lcdMsg;
	            translateNotify_CPU_NEW_LCD_MESSAGE(msg, &lcdMsg);

	            const u16 msgLenInBytes = rhea::string::utf16::lengthInBytes(lcdMsg.utf16LCDString);
                const u32 n = esapi::buildAnswer_C1_getCPUScreenMsg (lcdMsg.utf16LCDString, msgLenInBytes, bufferOUT, SIZE_OF_BUFFEROUT);
                rs232_write (bufferOUT, n);
            }
            return true;

        case CPUBRIDGE_NOTIFY_CPU_SEL_AVAIL_CHANGED:
            //risposta al comando  #C2
            {
	            cpubridge::sCPUSelAvailability selAvail;
	            cpubridge::translateNotify_CPU_SEL_AVAIL_CHANGED(msg, &selAvail);

                const u32 n = esapi::buildAnswer_C2_getSelAvailability (selAvail, bufferOUT, SIZE_OF_BUFFEROUT);
                rs232_write (bufferOUT, n);
            }
            return true;

        case CPUBRIDGE_NOTIFY_CPU_RUNNING_SEL_STATUS:
            priv_onCPUNotify_RUNNING_SEL_STATUS(msg);
            return true;

        case CPUBRIDGE_NOTIFY_SINGLE_SEL_PRICE_STRING:
			//risposta al comando  #C3	
			{
                u8 data[32];
                u8 selNum = 0;
                cpubridge::translateNotify_CPU_SINGLE_SEL_PRICE(msg, &selNum, &data[2], sizeof(data)-2);

                //rispondo con # C 3 [selNum] [priceLen] [price?] [ck]
                data[0] = selNum;
                data[1] = (u8)rhea::string::utf8::lengthInBytes(&data[2]);

                rs232_esapiSendAnswer ('C', '3', data, data[1]+2);
            }
			return true;

		case CPUBRIDGE_NOTIFY_SELECTION_NAME_UTF16_LSB_MSB:
			//risposta al comando  #C5
			{
				u8 selNum = 0;
				u16 selNameUTF16_LSB_MSB[64];
				cpubridge::translateNotify_CPU_GET_CPU_SELECTION_NAME (msg, &selNum, selNameUTF16_LSB_MSB, sizeof(selNameUTF16_LSB_MSB));

				//rispondo # C 5 [selNum] [nameLenInBytes] [nameUTF16_LSB_MSB...] [ck]
				const u8 nameLenInBytes = rhea::string::utf16::lengthInBytes(selNameUTF16_LSB_MSB);

				u32 BYTES_TO_ALLOC = 6 + nameLenInBytes;
				u8 *answer = (u8*)RHEAALLOC(rhea::getScrapAllocator(), BYTES_TO_ALLOC);
				u16 ct = 0;
				answer[ct++] = '#';
				answer[ct++] = 'C';
				answer[ct++] = '5';
				answer[ct++] = selNum;
				answer[ct++] = nameLenInBytes;
				memcpy (&answer[ct], selNameUTF16_LSB_MSB, nameLenInBytes);
				ct += nameLenInBytes;

				answer[ct] = rhea::utils::simpleChecksum8_calc (answer, ct);
				ct++;

				rs232_write (answer, ct);
				RHEAFREE(rhea::getScrapAllocator(), answer);
			}
			return true;

		case CPUBRIDGE_NOTIFY_CPU_FULLSTATE:
			//risposta al comando  #C6
			//risposta al comando  #C7
			{
				cpubridge::sCPUStatus s;
				cpubridge::translateNotify_CPU_FULLSTATE (msg, &s);

				if (0x0001 == handlerID)
				{
					//comando #C6
					u8 status = 'N';
					if (s.isCupDetected())
						status = 'Y';

					//rispondo con # C 6 [status] [ck] 
					rs232_esapiSendAnswer ('C', '6', &status, 1);
				}
				else if (0x0002 == handlerID)
				{
					//comando #C7
					u8 status = static_cast<u8>(s.VMCstate);
					//rispondo con # C 7 [status] [ck] 
					rs232_esapiSendAnswer ('C', '7', &status, 1);
				}
			}
			return true;

		case CPUBRIDGE_NOTIFY_READ_DATA_AUDIT_PROGRESS:
			//risposta a C8 (data audit)
			//rispondo: # C 8 [payloadLen] [payload] ... [payload] [ck]
			if (commandC8InProgress)
			{
				//questo � l'ultimo messaggio che ricevo. Lo [status] in questo caso � "finishedOK" oppure "finishedKO"
				commandC8InProgress = false;

				cpubridge::eReadDataFileStatus status;
				u16 totKbSoFar = 0;
				u16 fileID = 0;
				u8 buffer[256];
				u8 nBytesInBuffer = 0;
				cpubridge::translateNotify_READ_DATA_AUDIT_PROGRESS(msg, &status, &totKbSoFar, &fileID, &buffer[1], &nBytesInBuffer);

				if (nBytesInBuffer)
				{
					buffer[0] = nBytesInBuffer;
					rs232_esapiSendAnswer ('C', '8', buffer, 1 + nBytesInBuffer);
				}

				//Segnalo la fine del processo riportando un "payloadLen] == 0
				u8 data[2] = {0, 0};
				rs232_esapiSendAnswer ('C', '8', data, 1);
			}
			return true;

		case CPUBRIDGE_NOTIFY_OVERWRITE_CPU_MESSAGE_ON_SCREEN:
			{
				u8		errorCode;

				cpubridge::translateNotify_OVERWRITE_CPU_MESSAGE_ON_SCREEN(msg, &errorCode);

				rs232_esapiSendAnswer('S', '6', &errorCode, 1);
			}
			return true;

		case CPUBRIDGE_NOTIFY_SELECTION_ENABLE:
			{
				u8		errorCode;

				cpubridge::translateNotify_SELECTION_ENABLE_DISABLE(msg, &errorCode);

				rs232_esapiSendAnswer('S', '7', &errorCode, 1);
			}
			return true;

		case CPUBRIDGE_NOTIFY_SET_SELECTION_PARAMU16:
			{
				u8							selNum, errorCode;
				cpubridge::eSelectionParam	param;
				u8							buffer[3];
				cpubridge::translateNotify_SET_SELECTION_PARAMU16(msg, &selNum, &param, &errorCode);

				buffer[0] = selNum;
				buffer[1] = (u8)param;
				buffer[2] = errorCode;
				rs232_esapiSendAnswer('P', 'S', buffer, 3);
			}
			return true;

		case CPUBRIDGE_NOTIFY_GET_SELECTION_PARAMU16:
			{
				u8							selNum, errorCode;
				cpubridge::eSelectionParam	param;
				u16							value;
				u8							buffer[6];
				cpubridge::translateNotify_GET_SELECTION_PARAMU16(msg, &selNum, &param, &errorCode, &value);

				buffer[0] = selNum;
				buffer[1] = (u8)param;
				buffer[2] = errorCode;
				buffer[3] = 0x10;			// fixed value;
				rhea::utils::bufferWriteU16_LSB_MSB(&buffer[4], value);

				rs232_esapiSendAnswer('P', 'G', buffer, 6);
			}
			return true;

		case CPUBRIDGE_NOTIFY_WRITE_VMCDATAFILE_PROGRESS:
			{
				cpubridge::eWriteCPUFWFileStatus	status;
				u16									totKbSoFar;
				u8									buffer;

				cpubridge::translateNotify_WRITE_CPUFW_PROGRESS(msg, &status, &totKbSoFar);

				if (cpubridge::eWriteCPUFWFileStatus::finishedOK == status)
				{
					dataUpdate.Reset();
				}
				else
				{
					if (cpubridge::eWriteCPUFWFileStatus::inProgress == status)
						buffer = 0;
					else
						buffer = (u8)status;

					rs232_esapiSendAnswer('X', '1', &buffer, 1);
				}
			}
			return true;
		case CPUBRIDGE_NOTIFY_WRITE_CPUFW_PROGRESS:
			{
				cpubridge::eWriteDataFileStatus status;
				u16								totKbSoFar;
				u8								buffer;

				cpubridge::translateNotify_WRITE_VMCDATAFILE_PROGRESS(msg, &status, &totKbSoFar);

				if (cpubridge::eWriteDataFileStatus::finishedOK == status)
				{
					dataUpdate.Reset();
				}
				else
				{
					if (cpubridge::eWriteDataFileStatus::inProgress == status)
						buffer = 0;
					else
						buffer = (u8)status;

					rs232_esapiSendAnswer('X', '1', &buffer, 1);
				}
			}
			return true;
        } //switch (msg.what)
	}
}

//*********************************************************
void Protocol::rs232_write (const u8 *buffer, u32 numBytesToSend)
{
    if (numBytesToSend>0)
        rhea::rs232::writeBuffer (rs232, buffer, numBytesToSend);
}

//*********************************************************
void Protocol::rs232_esapiSendAnswer (u8 c1, u8 c2, const void* optionalData, u32 numOfBytesInOptionalData)
{
	const u32 nBytesToSend = esapi::buildAnswer (c1, c2, optionalData, numOfBytesInOptionalData, bufferOUT, SIZE_OF_BUFFEROUT);
	if (nBytesToSend)
		rs232_write (bufferOUT, nBytesToSend);
}

//*********************************************************
void Protocol::rs232_esapiSendAnswerWithCrc16(u8 c1, u8 c2, const void* optionalData, u32 numOfBytesInOptionalData)
{
	const u32 nBytesToSend = esapi::buildAnswerWithCrc16(c1, c2, optionalData, numOfBytesInOptionalData, bufferOUT, SIZE_OF_BUFFEROUT);
	if (nBytesToSend)
		rs232_write(bufferOUT, nBytesToSend);
}

//*********************************************************
bool Protocol::rsr232_readSingleByte (u8 *out)
{
    return rhea::rs232::readBuffer(rs232, out, 1);
}

//*********************************************************
bool Protocol::rs232_read ()
{
    const u64 timeToExitMSec = rhea::getTimeNowMSec() + 300;
    while (rhea::getTimeNowMSec() < timeToExitMSec)
    {
	    //leggo tutto quello che posso dalla seriale e bufferizzo in [b]
        const u16 nBytesAvailInBuffer = (u16)(rs232BufferIN.SIZE - rs232BufferIN.numBytesInBuffer);

#ifdef _DEBUG
		if (0 == nBytesAvailInBuffer)
        {
			DBGBREAK;
        }
#endif

	    if (nBytesAvailInBuffer > 0)
	    {
		    const u32 nRead = rhea::rs232::readBuffer(rs232, &rs232BufferIN.buffer[rs232BufferIN.numBytesInBuffer], nBytesAvailInBuffer);
		    rs232BufferIN.numBytesInBuffer += (u16)nRead;
	    }
    
		if (0 == rs232BufferIN.numBytesInBuffer)
			return true;

        //provo ad estrarre un msg 'raw' dal mio buffer.
        //Cerco il carattere di inizio buffer ed eventualmente butto via tutto quello che c'� prima
        u32 i = 0;
        while (i < rs232BufferIN.numBytesInBuffer && rs232BufferIN.buffer[i] != (u8)'#')
            i++;

        if (rs232BufferIN.buffer[i] != (u8)'#')
        {
            rs232BufferIN.reset();
            return true;
        }

        rs232BufferIN.removeFirstNBytes(i);
        assert (rs232BufferIN.buffer[0] == (u8)'#');
        i = 0;

		//il comando valido pi� piccolo � di almeno 4 byte
        if (rs232BufferIN.numBytesInBuffer < 4)
            return true;

        const u8 commandChar = rs232BufferIN.buffer[1];
		u32 nBytesConsumed = 0;
        switch (commandChar)
        {
        default:
			//comando non riconosciuto, cancello # che e' in posizione 0 cosi'
			//al prossimo giro viene buttato via tutto fino al prossimo #
			nBytesConsumed = 1;
			logger->log ("esapi::Protocol() => invalid command char [%c]\n", commandChar);
            break;

        case 'A':
			nBytesConsumed = priv_rs232_handleCommand_A (rs232BufferIN);
			break;
        case 'C':
			nBytesConsumed = priv_rs232_handleCommand_C (rs232BufferIN);
			break;
		case 'P':
			nBytesConsumed = priv_rs232_handleCommand_P(rs232BufferIN);
			break;
		case 'S':
			nBytesConsumed = priv_rs232_handleCommand_S(rs232BufferIN);
			break;
        case 'R':
            //questo tipo di comandi (classe 'R') devono essere gestiti in autonomia dai singoli moduli
            return false;
		case 'X':
			// comandi per il trasferiemnto del file
			nBytesConsumed = priv_rs232_handleCommand_X(rs232BufferIN);
			break;
        }

		rs232BufferIN.removeFirstNBytes(nBytesConsumed);
    }

    return true;
}

/*********************************************************
 * ritorna il numero di bytes consumati
 */
u32 Protocol::priv_rs232_handleCommand_A (const sBuffer &b)
{
    assert (b.numBytesInBuffer >= 3 && b.buffer[0] == '#' && b.buffer[1] == 'A');
    const u8 commandCode = b.buffer[2];

    switch (commandCode)
    {
    default:
        logger->log ("esapi::Protocol() => invalid commandNum [%c][%c]\n", b.buffer[1], commandCode);
		return 2;

    case '1': 
        //Request API version
        //ricevuto: # A 1 [ck]
        //rispondo: # A 1 [api_ver_major] [api_ver_minor] [GPUmodel] [ck]
		{
			//parse del messaggio
			if (b.buffer[3] != rhea::utils::simpleChecksum8_calc (b.buffer, 3))
				return 2;

			//rispondo.
			const u8 data[4] = { ESAPI_VERSION_MAJOR, ESAPI_VERSION_MINOR, (u8)esapi::eGPUType::TS, 0 };
			rs232_esapiSendAnswer ('A', '1', data, 3);
		}
		return 4;

	case '2':
		//Request Machine ID
		//ricevuto: # A 2 [ck]
        //rispondo: # A 2 'R' 'H' 'V' '2' '0' [id101 come stringa di 9 ASCCI char] [ck]
		{
			//parse del messaggio
			if (b.buffer[3] != rhea::utils::simpleChecksum8_calc (b.buffer, 3))
				return 2;

            //chiedo a CPUBridge. Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
            cpubridge::ask_CPU_QUERY_ID101 (*cpuBridgeSubscriber, 0x01);
		}
		return 4;

	case '3':
		//Restart CPU & GPU
		//ricevuto: # A 3 [ck]
        //rispondo: # A 3 [ck]
		{
			//parse del messaggio
			if (b.buffer[3] != rhea::utils::simpleChecksum8_calc (b.buffer, 3))
				return 2;

			//reboot CPU
			cpubridge::ask_CPU_RESTART(*cpuBridgeSubscriber, 0x01);

			//rispondo
            rs232_esapiSendAnswer ('A', '3', NULL, 0);
		}
		return 4;

	case '4':
		//Request GPU version
		//ricevuto: # A 4 [ck]
        //rispondo: # A 4 [GPU_ver_major] [GPU_ver_minor] [GPU_ver_build] [ck]
		{
			//parse del messaggio
			if (b.buffer[3] != rhea::utils::simpleChecksum8_calc (b.buffer, 3))
				return 2;

			//la stringa con la versione di GPU � in un file in current/gpu/ver.txt nel formato x.y.zzzzzz
			u8 verMajor = 0;
			u8 verMinor = 0;
			u8 verBuild = 0;
			rhea::Allocator* allocator = rhea::getSysHeapAllocator();

			u8	fileName[256];
			sprintf_s((char*)fileName, sizeof(fileName), "%s/current/gpu/ver.txt", rhea::getPhysicalPathToAppFolder());
			
			u32 fileSize;
			u8* buffer = rhea::fs::fileCopyInMemory(fileName, allocator, &fileSize);
			if (NULL != buffer)
			{
				rhea::utf8::String s;
				s.setFrom(buffer, fileSize);

				rhea::Array<rhea::utf8::String> e;
				e.setup (allocator, 3);

				const u32 n = s.explode ('.', e);
                if (n >= 3)
				{
					verMajor = rhea::string::ansi::toU32(reinterpret_cast<const char*>(e[0].getBuffer()));
					verMinor = rhea::string::ansi::toU32(reinterpret_cast<const char*>(e[1].getBuffer()));
					verBuild = rhea::string::ansi::toU32(reinterpret_cast<const char*>(e[2].getBuffer()));
				}

				RHEAFREE(allocator, buffer);
			}

			//rispondo.
			const u8 data[3] = { verMajor, verMinor, verBuild };
			rs232_esapiSendAnswer ('A', '4', data, 3);
		}
		return 4;

	case '5':
		//Request CPU version
		if(b.buffer[3] == 'b')
		{
			// extended string version
			//ricevuto: # A 5 b [ck]
			//rispondo: # A 5 [ASCII_0] ... [ASCII_7] [ck]

			//parse del messaggio
			if (b.buffer[4] != rhea::utils::simpleChecksum8_calc(b.buffer, 4))
				return 4;

			commandCpuVerExtended = true;
			cpubridge::ask_CPU_STRING_VERSION_AND_MODEL(*cpuBridgeSubscriber, 0x01);
		}
		else
		{
			//ricevuto: # A 5 [ck]
			//rispondo: # A 5 [ASCII_0] ... [ASCII_7] [ck]

			//parse del messaggio
			if (b.buffer[3] != rhea::utils::simpleChecksum8_calc (b.buffer, 3))
				return 2;

            //chiedo a CPUBridge. Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
			commandCpuVerExtended = false;
			cpubridge::ask_CPU_STRING_VERSION_AND_MODEL (*cpuBridgeSubscriber, 0x01);
		}
		return 4;
		
	}
}

/********************************************************
 * ritorna il numero di bytes consumati
 */
u32 Protocol::priv_rs232_handleCommand_C (const sBuffer &b)
{
    assert (b.numBytesInBuffer >= 3 && b.buffer[0] == '#' && b.buffer[1] == 'C');
    const u8 commandCode = b.buffer[2];

    switch (commandCode)
    {
    default:
        logger->log ("esapi::Protocol => invalid commandNum [%c][%c]\n", b.buffer[1], commandCode);
		return 2;

    case '1': 
        //Query CPU screen message
        //ricevuto: # C 1 [ck]
        //rispondo: # C 1 [numByteInMsg] [messageUTF16_LSB_MSB] [ck]
        {
			//parse del messaggio
			if (b.buffer[3] != rhea::utils::simpleChecksum8_calc (b.buffer, 3))
				return 2;
			       
            //chiedo a CPUBridge. Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
            cpubridge::ask_CPU_QUERY_LCD_MESSAGE (*cpuBridgeSubscriber, 0x01);
        }
		return 4;
		
    case '2': 
        //Query selections availability
        //ricevuto: # C 2 [ck]
        //rispondo: # C 2 [avail1-8] [avail9-16] [avail17-24] [...] [avail121-128] [ck]
        {
			//parse del messaggio
			if (b.buffer[3] != rhea::utils::simpleChecksum8_calc (b.buffer, 3))
				return 2;

            //chiedo a CPUBridge. Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
            cpubridge::ask_CPU_QUERY_SEL_AVAIL (*cpuBridgeSubscriber, 0x01);
        }
		return 4;

	case '3':
        //Query selections prices
        //ricevuto: # C 3 [selNum] [ck]
        //rispondo: # C 3 [selNum] [priceLen] [price?] [ck]
        {
            if (b.numBytesInBuffer < 5)	//devo avere almeno 5 char nel buffer
                return 0;

            if (b.buffer[4] != rhea::utils::simpleChecksum8_calc (b.buffer, 4))
                return 2;

            const u8 selNum = b.buffer[3];
			//chiedo a CPUBridge. Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
            cpubridge::ask_CPU_QUERY_SINGLE_SEL_PRICE (*cpuBridgeSubscriber, 0x01, selNum);
		}
		return 4;

    case '4':
        //Query 12 LED status
        //ricevuto: # C 4 [ck]
        //rispondo: # C 4 [b1] [b2] [ck]
		{
			//questo comando non � supportato per le TS
			const u8 data[2] = { 0, 0 };
			rs232_esapiSendAnswer ('C', '4', data, 2);
		}
		return 4;

	case '5':
		//Get selection name
		//ricevuto: # C 5 [selNum] [ck]
		//rispondo:	# C 5 [selNum] [nameLenInBytes] [nameUTF16_LSB_MSB...] [ck]
		{
			if (b.numBytesInBuffer < 5)	//devo avere almeno 5 char nel buffer
				return 0;

			if (b.buffer[4] != rhea::utils::simpleChecksum8_calc (b.buffer, 4))
				return 2;

			u8 selNumber = b.buffer[3];

			if (selNumber < 1 || selNumber > 48)
				selNumber = 1;

			//chiedo a CPUBridge. Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
			cpubridge::ask_CPU_GET_CPU_SELECTION_NAME_UTF16_LSB_MSB (*cpuBridgeSubscriber, 0x01, selNumber);
		}
		return 5;

	case '6':
		//Get Cup Sensor Status
		//ricevuto: # C 6 [ck]
		//rispondo: # C 6 [status] [ck] 
		//				dove [status] ='Y' oppure 'N'
		{
			//chiedo a CPUBridge il suo stato perch� l'informazione sullo stato del cup-sensor ce l'ha lei.
			//Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
			cpubridge::ask_CPU_QUERY_FULLSTATE (*cpuBridgeSubscriber, 0x0001);
		}
		return 4;
		
	case '7':
		//Get CPU status
		//ricevuto: # C 7 [ck]
		//rispondo: # C 7 [status] [ck]
		{
			//chiedo a CPUBridge il suo stato 
			//Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
			cpubridge::ask_CPU_QUERY_FULLSTATE (*cpuBridgeSubscriber, 0x0002);
		}
		return 4;

	case '8':
		//Get daily sales data (eva-dts)
		//ricevuto: # C 8 [ck]
		//rispondo: # C 8 [payloadLen] [payload] ... [payload] [ck]
		if (!commandC8InProgress)
		{
			//chiedo a CPUBridge
			//Alla ricezione delle risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
			commandC8InProgress = true;
			cpubridge::ask_READ_DATA_AUDIT (*cpuBridgeSubscriber, 0x0001, true);
			
		}
		return 4;

	case '9':
		//Get machine locking status
		//ricevuto: # C 9 [ck]
		//rispondo: # C 9 [status] [ck]
		{
			//chiedo a CPUBridge
			//Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
			cpubridge::ask_GET_MACHINE_LOCK_STATUS (*cpuBridgeSubscriber, 0x0001);
		}
		return 4;

    } //switch
}

/********************************************************
 * ritorna il numero di bytes consumati
 */
u32 Protocol::priv_rs232_handleCommand_S (const sBuffer &b)
{
    assert (b.numBytesInBuffer >= 3 && b.buffer[0] == '#' && b.buffer[1] == 'S');
    const u8 commandCode = b.buffer[2];

    switch (commandCode)
    {
    default:
        logger->log ("esapi::Protocol => invalid commandNum [%c][%c]\n", b.buffer[1], commandCode);
		return 2;

    case '1': 
        //Start selection
        //ricevuto: # S 1 [sel_num] [ck]
        //rispondo: # S 1 [sel_num] [ck]
        {
			if (b.numBytesInBuffer < 5)	//devo avere almeno 5 char nel buffer
				return 0;

			if (b.buffer[4] != rhea::utils::simpleChecksum8_calc (b.buffer, 4))
				return 2;

			u8 selNumber = b.buffer[3];
			if (selNumber < 1 || selNumber > 48)
				selNumber = 0;

            //chiedo a CPUBridge di iniziare la selezione indicata. CPUBridge risponder� con una serie di notify_CPU_RUNNING_SEL_STATUS() per
            //indicare lo stato di avanzamento della selezione
			if (selNumber > 0)
			{
				runningSel.status = cpubridge::eRunningSelStatus::wait;
				cpubridge::ask_CPU_START_SELECTION (*cpuBridgeSubscriber, selNumber);
			}

			//rispondo
			rs232_esapiSendAnswer ('S', '1', &selNumber, 1);
        }
		return 5;

    case '2': 
        //Query selection status
        //ricevuto: # S 2 [ck]
        //rispondo: # S 2 [status] [ck]
		//		[status] can be one of the following:
		//		0x01 => waiting for payment
		//		0x02 => delivering in progress, STOP button is not available
		//		0x03 => finished KO
		//		0x04 => finished OK
		//		0x05 => delivering in progress, STOP button is available		
		{
			if (b.buffer[3] != rhea::utils::simpleChecksum8_calc (b.buffer, 3))
				return 2;

            //rispondo
			const u8 status = (u8)runningSel.status;
			rs232_esapiSendAnswer ('S', '2', &status, 1);
        }
		return 4;

	case '3':
		//start already paid selection
		//ricevo:	# S 3 [sel_num] [priceLSB] [priceMSB] [ck]
		//rispondo:	# S 3 [sel_num] [ck]
		{
			if (b.numBytesInBuffer < 7)	//devo avere almeno 7 char nel buffer
				return 0;

			if (b.buffer[6] != rhea::utils::simpleChecksum8_calc (b.buffer, 6))
				return 2;

			//chiedo a CPUBridge di iniziare la selezione indicata. CPUBrdige risponder� con una serie di notify_CPU_RUNNING_SEL_STATUS() per
			//indicare lo stato di avanzamento della selezione
			u8 selNum = b.buffer[3];
			if (selNum < 1 || selNum > 48)
				selNum = 0;
			else
			{
				const u16 price = rhea::utils::bufferReadU16_LSB_MSB(&b.buffer[4]);
				runningSel.status = cpubridge::eRunningSelStatus::wait;
				cpubridge::ask_CPU_START_SELECTION_WITH_PAYMENT_ALREADY_HANDLED (*cpuBridgeSubscriber, selNum, price, cpubridge::eGPUPaymentType::unknown, false);
			}

			//rispondo
			rs232_esapiSendAnswer ('S', '3', &selNum, 1);
		}
		return 7;

	case '4':
		//Send button press
        //ricevuto: # S 4 [btn] [ck]
        //rispondo: # S 4 [btn] [ck]
		{
			if (b.numBytesInBuffer < 5)	//devo avere almeno 5 char nel buffer
				return 0;

			if (b.buffer[4] != rhea::utils::simpleChecksum8_calc (b.buffer, 4))
				return 2;

			u8 btnNum =b.buffer[3];

			//inoltro a CPUBridge
			if (btnNum > 0 && btnNum <= 12)
				cpubridge::ask_CPU_SEND_BUTTON (*cpuBridgeSubscriber, btnNum);
			else
				btnNum = 0;

			//rispondo
			rs232_esapiSendAnswer ('S', '4', &btnNum, 1);
		}
		return 5;

    case '5': 
        //Lock or unlock the machine
        //ricevuto: # S 5 [desired_lock_status] [ck]
        //rispondo: # S 5 [current_lock_status] [ck]
        {
			if (b.numBytesInBuffer < 5)	//devo avere almeno 5 char nel buffer
				return 0;

			if (b.buffer[4] != rhea::utils::simpleChecksum8_calc (b.buffer, 4))
				return 1;

			const cpubridge::eLockStatus desiredLockStatus = static_cast<cpubridge::eLockStatus>(b.buffer[3]);

			//chiedo a CPUBridge
			//Alla ricezione della risposta da parte di CPUBridge, rispondo a mia volta lungo la seriale (vedi onMsgFromCPUBridge)
			cpubridge::ask_SET_MACHINE_LOCK_STATUS (*cpuBridgeSubscriber, 0x0002, desiredLockStatus);
        }
		return 5;

	case '6':
		//Display a text message on the screen
		//ricevuto: # S 6 [howLong_sec] [msgLen16 LSB-MSB] [UTF8_0] ... [UTF8_n] [ck]
		//rispondo: # S 6 [errorCode] [ck]
		{
			if (b.numBytesInBuffer < 7)	//devo avere almeno 7 char nel buffer
				return 0;

			const u16 msgByteLen = rhea::utils::bufferReadU16_LSB_MSB(&b.buffer[4]);
			const u16 packetLen = msgByteLen + 7;
			if (b.numBytesInBuffer < packetLen)
				return 0;
			
			if (b.buffer[packetLen - 1] != rhea::utils::simpleChecksum8_calc(b.buffer, packetLen - 1))
				return 1;

			const u8 timeSec = b.buffer[3];
			u8	*msgTxtUtf8 = RHEAALLOCT(u8*, rhea::getScrapAllocator(), msgByteLen+2);
			memcpy (msgTxtUtf8, &b.buffer[6], msgByteLen);
			msgTxtUtf8[msgByteLen] = 0;

            cpubridge::ask_OVERWRITE_CPU_MESSAGE_ON_SCREEN(*cpuBridgeSubscriber, 0x0002, msgTxtUtf8, timeSec);

			RHEAFREE(rhea::getScrapAllocator(), msgTxtUtf8);
			return packetLen;
		}
		break;

	case '7':
		//Enable/Disable selection
		//rcv: # S 7 [selNum] [enabled] [ck]
		//snd: # S 7 [errorCode] [ck]
		{
			if (b.numBytesInBuffer < 6)	//devo avere almeno 6 char nel buffer
				return 0;
			
			if(b.buffer[5] != rhea::utils::simpleChecksum8_calc(b.buffer, 5))
				return 1;

			const bool enable = (b.buffer[4] != 0);
			cpubridge::ask_SELECTION_ENABLE_DISABLE(*cpuBridgeSubscriber, 0x0002, b.buffer[3], enable);
			return 6;
		}
		break;
		
	} //switch
}

/********************************************************
 * ritorna il numero di bytes consumati
 */
u32 Protocol::priv_rs232_handleCommand_P(const sBuffer& b)
{
	assert(b.numBytesInBuffer >= 3 && b.buffer[0] == '#' && b.buffer[1] == 'P');
	const u8 commandCode = b.buffer[2];
	u32		ret = 0;

	switch (commandCode)
	{
	default:
		logger->log("esapi::Protocol => invalid commandNum [%c][%c]\n", b.buffer[1], commandCode);
		ret = 1;
		break;

	case 'G':
		// Get selection param
		//ricevuto: # P G [selnum] [paramId] [checksum]
		//risposta prevista: # P G [selnum] [paramId] [errorCode] 0x10 [vallue LSB-MSB] [checksum]
		if (b.numBytesInBuffer >= 6)	//devo avere almeno 6 char nel buffer
		{
			if (b.buffer[5] != rhea::utils::simpleChecksum8_calc(b.buffer, 5))
				ret = 1;
			else
			{
				ret = 6;
				const u8 selNum = b.buffer[3];
				const u8 paramId = b.buffer[4];
				const cpubridge::eSelectionParam param = static_cast<cpubridge::eSelectionParam>(paramId);

				// invio comando al bridge
				cpubridge::ask_GET_SELECTION_PARAMU16(*cpuBridgeSubscriber, 0x00002, selNum, param);
			}
		}
		break;

	case 'S':
		//ricevuto: # P S [selnum] [paramId] [dataType] [value (LSB...MSB)] [checksum]
		//risposta prevista: # P S [selnum] [paramId] [errorCode] [checksum]
		if (b.numBytesInBuffer >= 9)	//devo avere almeno 9 char nel buffer
		{
			if (b.buffer[8] != rhea::utils::simpleChecksum8_calc(b.buffer, 8))
				ret = 1;
			else
			{
				ret = 9;
				const u8  selNum = b.buffer[3];
				const u8  paramId = b.buffer[4];
				const u8  dataType = b.buffer[5];	// fisso 0x10
				const u16 value = rhea::utils::bufferReadU32_LSB_MSB(&b.buffer[6]);
	
				if (0x10 == dataType)
				{
					const cpubridge::eSelectionParam param = static_cast<cpubridge::eSelectionParam>(paramId);
					// invio comando al bridge
					cpubridge::ask_SET_SELECTION_PARAMU16(*cpuBridgeSubscriber, 0x00002, selNum, param, value);
				}
			}
			
		}
		break;
	}
	return ret;
}

/********************************************************
 * ritorna il numero di bytes consumati
 */
u32 Protocol::priv_rs232_handleCommand_X(const sBuffer& b)
{
	assert(b.numBytesInBuffer >= 3 && b.buffer[0] == '#' && b.buffer[1] == 'X');
	const u8 commandCode = b.buffer[2];
	u32		ret = 0;

	switch (commandCode)
	{
	default:
		logger->log("esapi::Protocol => invalid commandNum [%c][%c]\n", b.buffer[1], commandCode);
		ret = 1;
		break;

	case '0':
		// File trasfer header
		//ricevuto: # X 0 [fileId] [fileLen (4byte LSB...MSB)] [crc16 LSB-MSB]
		//rispondo: # X 0 [fileId] [errorCode] [crc16 LSB-MSB]
		if (10 >= b.numBytesInBuffer)	//devo avere almeno 10 char nel buffer
		{
			ret = 10;
				
			u8	buffer[3] = { 0, 0, 0 };
			const u16 crc = rhea::utils::bufferReadU16_LSB_MSB(&b.buffer[8]);
			if (crc == rhea::utils::Crc16_calc(b.buffer, 8))
			{
				const u8 fileType = b.buffer[3];
				const u32 fileLen = rhea::utils::bufferReadU32_LSB_MSB(&b.buffer[4]);
				rhea::utils::bufferWriteU16_LSB_MSB(&buffer[1], 500);

                if(false == dataUpdate.Open(fileType, fileLen, logger))
					buffer[0] = 1;
				else
					rhea::utils::bufferWriteU16_LSB_MSB(&buffer[1], dataUpdate.BlockMaxDim());
			}

			rs232_esapiSendAnswerWithCrc16('X', commandCode, buffer, 3);
		}
		break;

	case '1':	// blocco effettivo
		//rcv: # X 1 [blockNr16 LSB-MSB] [payloadLen16 LSB-MSB] [payload....] [crc16 LSB-MSB]
		if (b.numBytesInBuffer >= 9)
		{
			const u16 blockNr = rhea::utils::bufferReadU16_LSB_MSB(&b.buffer[3]);
			const u16 blockLen = rhea::utils::bufferReadU16_LSB_MSB(&b.buffer[5]);

			u8 errorCode = 0;
			if (b.numBytesInBuffer >= static_cast<u32>(blockLen) + 9)
			{
				ret = blockLen + 9;

				const u16 bufferLenNoCrc = blockLen + 7;
				const u16 crc = rhea::utils::bufferReadU16_LSB_MSB(&b.buffer[bufferLenNoCrc]);
				if (crc != rhea::utils::Crc16_calc(b.buffer, bufferLenNoCrc))
				{
					ret = 1;
					errorCode = 1;
				}
				else if (blockLen > 0)
				{
					//if ((blockNr % 40) == 0)
					//	logger->log("esapi::Protocol => blocco [%d][len=%d]\n", blockNr, blockLen);
                    if (false == dataUpdate.Append(blockNr, blockLen, &b.buffer[7], logger))
						errorCode = 1;
				}
				else //� implicito che qui blockLen==0
				{
                    if (dataUpdate.Complete(blockNr, cpuBridgeSubscriber, logger))
						logger->log("esapi::Protocol => COMPLETE\n");
					errorCode = 0;
				}
				rs232_esapiSendAnswerWithCrc16('X', commandCode, &errorCode, 1);
			}
		}
		break;
	}
	return ret;
}
