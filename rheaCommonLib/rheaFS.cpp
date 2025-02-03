#include <limits.h>
#include "rhea.h"
#include "rheaString.h"


using namespace rhea;

//**************************************************************
void fs::sanitizePath (const u8 *utf8_path, u8* out_utf8sanitizedPath, u32 sizeOfOutSanitzed)
{
	if (NULL == utf8_path)
	{
		out_utf8sanitizedPath[0] = 0;
		return;
	}
	if (utf8_path[0] == 0x00)
	{
		out_utf8sanitizedPath[0] = 0;
		return;
	}

    strcpy_s ((char*)out_utf8sanitizedPath, sizeOfOutSanitzed, (const char*)utf8_path);
	fs::sanitizePathInPlace(out_utf8sanitizedPath);
}

bool fs_isValidHexSymbol (u8 c)
{
	if (c >= '0' && c <= '9') return true;
	if (c == 'A' || c == 'a') return true;
	if (c == 'B' || c == 'b') return true;
	if (c == 'C' || c == 'c') return true;
	if (c == 'D' || c == 'd') return true;
	if (c == 'E' || c == 'e') return true;
	if (c == 'F' || c == 'f') return true;
	return false;
}

//**************************************************************
void fs::sanitizePathInPlace(u8 *utf8_path, u32 nBytesToCheck)
{
	if (u32MAX == nBytesToCheck)
		nBytesToCheck = (u32)strlen((const char*)utf8_path);

	for (u32 i = 0; i < nBytesToCheck; i++)
	{
		if (utf8_path[i] == '\\')
			utf8_path[i] = '/';
	}

	//eventuali %20 li trasforma in blank
	if (nBytesToCheck > 2)
	{
		for (u32 i = 0; i < nBytesToCheck - 2; i++)
		{
			if (utf8_path[i] == '%')
			{
				/*if (utf8_path[i + 1] == '2' && utf8_path[i + 2] == '0')
				{
					utf8_path[i] = ' ';
					memcpy(&utf8_path[i + 1], &utf8_path[i + 3], nBytesToCheck - i - 3);
					nBytesToCheck -= 2;
					utf8_path[nBytesToCheck] = 0;
				}*/
				
				if (fs_isValidHexSymbol(utf8_path[i + 1]) && fs_isValidHexSymbol(utf8_path[i + 2]))
				{
					char hex[4] = { (char)utf8_path[i + 1], (char)utf8_path[i + 2], 0, 0 };
					u32 num = 32;
					rhea::string::ansi::hexToInt (hex, &num);
					utf8_path[i] = (u8)num;
					memcpy(&utf8_path[i + 1], &utf8_path[i + 3], nBytesToCheck - i - 3);
					nBytesToCheck -= 2;
					utf8_path[nBytesToCheck] = 0;
				}
			}
		}
	}

	u32 i = 0, t = 0;
	while (i < nBytesToCheck)
	{
		if (utf8_path[i] == '/')
		{
			utf8_path[t++] = utf8_path[i++];
			while (utf8_path[i] == '/')
				++i;
		}
		else if (utf8_path[i] == '.')
		{
			//se xxx/./yyy
			if (i > 0 && utf8_path[i - 1] == '/' && utf8_path[i + 1] == '/')
				i += 2;
			//se xxx/../yyy
			else if (i > 0 && utf8_path[i - 1] == '/' && utf8_path[i + 1] == '.')
			{
				i += 3;
				if (t >= 2)
					t -= 2;
				while (t && utf8_path[t] != '/')
					--t;
				if (utf8_path[t] == '/')
					++t;
			}
			else
				utf8_path[t++] = utf8_path[i++];
		}
		else
			utf8_path[t++] = utf8_path[i++];
	}
	utf8_path[t] = 0;
	if (t > 1 && utf8_path[t - 1] == '/')
		utf8_path[t - 1] = 0;
}

//******************************************** 
void fs::filePath_GoBack (const u8 *pathSenzaSlashIN, u8 *out, u32 sizeofout)
{
	assert (NULL != out && sizeofout > 1);
	out[0] ='/'; 
	out[1] = 0;
	if (NULL == pathSenzaSlashIN || (NULL != pathSenzaSlashIN && pathSenzaSlashIN[0] == 0))
		return;
	if (pathSenzaSlashIN[1] == 0)
	{
		assert (pathSenzaSlashIN[0]=='/');
		return;
	}

	const u32 MAXSIZE = 1024;
	u8 pathSenzaSlash[MAXSIZE];
	fs::sanitizePath(pathSenzaSlashIN, pathSenzaSlash, sizeof(pathSenzaSlash));

	string::utf8::Iter parser;
	parser.setup (pathSenzaSlash);
	parser.toLast();

	assert(parser.getCurChar() != '/');

	while (parser.getCurChar() != '/')
	{
		if (!parser.backOneChar())
			break;
	}

	if (parser.getCurChar() == '/')
	{
		if (parser.backOneChar())
			parser.copyStrFromXToCurrentPosition (0, out, sizeofout, true);
	}

}

//**************************************************************************
void fs::extractFileExt (const u8 *utf8_filename, u8 *out, u32 sizeofout)
{
	assert (out && sizeofout >=3);
	out[0] = 0;

	u32 len = (u32)strlen((const char*)utf8_filename);
	if (len > 0)
	{
		u32 i = len;
		while (i-- > 0)
		{
			if (utf8_filename[i] == '.')
			{
				if (i < len - 1)
				{
					u32 numBytesToCopy = len - i - 1;
					if (numBytesToCopy >= sizeofout-1)
					{
						DBGBREAK;
						numBytesToCopy = sizeofout -2;
					}
					memcpy (out, &utf8_filename[i+1], numBytesToCopy);
					out[numBytesToCopy] = 0;
				}
				return;
			}
		}
	}
}

//**************************************************************************
void fs::extractFileNameWithExt (const u8 *utf8_filename, u8 *out, u32 sizeofout)
{
	assert (out && sizeofout >=3);
	out[0] = 0;

	u32 len = (u32)strlen((const char*)utf8_filename);
	if (len > 0)
	{
		u32 i = len;
		while (i-- > 0)
		{
			if (utf8_filename[i]=='/' || utf8_filename[i]=='\\')
			{
				u32 numBytesToCopy = len - i - 1;
				if (numBytesToCopy >= sizeofout-1)
				{
					DBGBREAK;
					numBytesToCopy = sizeofout -2;
				}
				memcpy (out, &utf8_filename[i+1], numBytesToCopy);
				out[numBytesToCopy] = 0;
				return;
			}
		}
		
		u32 numBytesToCopy = len;
		if (numBytesToCopy >= sizeofout-1)
		{
			DBGBREAK;
			numBytesToCopy = sizeofout -2;
		}
		memcpy (out, utf8_filename, numBytesToCopy);
		out[numBytesToCopy] = 0;
		return;
	}
}

//**************************************************************************
void fs::extractFileNameWithoutExt (const u8 *utf8_filename, u8 *out, u32 sizeofout)
{
	fs::extractFileNameWithExt (utf8_filename, out, sizeofout);

	u32 len = (u32)strlen((const char*)out);
	while (len--)
	{
		if (out[len] == '.')
		{
			out[len] = 0;
			return;
		}
	}
}

//**************************************************************************
void fs::extractFilePathWithSlash (const u8 *utf8_filename, u8 *out, u32 sizeofout)
{
	assert (out && sizeofout >=3);
	out[0] = 0;

	u32 len = (u32)strlen((const char*)utf8_filename);
	while (len-- > 0)
	{
		if (utf8_filename[len]=='/' || utf8_filename[len]=='\\')
		{
			u32 numBytesToCopy = len+1;
			if (numBytesToCopy >= sizeofout)
			{
				DBGBREAK;
				numBytesToCopy = sizeofout -1;
			}
			memcpy (out, utf8_filename, numBytesToCopy);
			out[numBytesToCopy] = 0;
			return;
		}
	}
}

//**************************************************************************
void fs::extractFilePathWithOutSlash (const u8 *utf8_filename, u8 *out, u32 sizeofout)
{
	assert (out && sizeofout >=3);
	out[0] = 0;

	u32 len = (u32)strlen((const char*)utf8_filename);
	while (len-- > 0)
	{
		if (utf8_filename[len]=='/' || utf8_filename[len]=='\\')
		{
			u32 numBytesToCopy = len;
			if (numBytesToCopy >= sizeofout)
			{
				DBGBREAK;
				numBytesToCopy = sizeofout -1;
			}
			memcpy (out, utf8_filename, numBytesToCopy);
			out[numBytesToCopy] = 0;
			return;
		}
	}
}

//*********************************************
bool FS_doesFileNameMatchJolly (const u8 *utf8_filename, const u8 *utf8_strJolly)
{
    assert (NULL != utf8_filename && NULL != utf8_strJolly);

	string::utf8::Iter parserFilename;
	parserFilename.setup (utf8_filename);

    string::utf8::Iter parserJolly;
    parserJolly.setup (utf8_strJolly);

    while (1)
    {
        if (parserJolly.getCurChar().isEOF() || parserFilename.getCurChar().isEOF())
        {
            if (parserJolly.getCurChar().isEOF() && parserFilename.getCurChar().isEOF())
                return true;
            return false;
        }

        if (parserJolly.getCurChar() == '?')
        {
            //il char jolly  ?, quindi va bene un char qualunque
            parserFilename.advanceOneChar();
            parserJolly.advanceOneChar();
        }
        else if (parserJolly.getCurChar() == '*')
        {
            //il char jolly  un *, quindi prendo il prox char jolly e lo cerco nel filename
            parserJolly.advanceOneChar();
            if (parserJolly.getCurChar().isEOF())
                return true;

            //cerco il char jolly
            while (1)
            {
                parserFilename.advanceOneChar();
                if (parserFilename.getCurChar().isEOF())
                    return false;
                if (parserFilename.getCurChar() == parserJolly.getCurChar())
                {
                    if (fs::doesFileNameMatchJolly (parserFilename.getPointerToCurrentPosition(), parserJolly.getPointerToCurrentPosition()))
                        return true;
                }
            }
        }
        else
        {
            //il carattere jolly  un char normale, quindi deve essere uguale al char del filename
            if (parserFilename.getCurChar() != parserJolly.getCurChar())
                return false;
            parserFilename.advanceOneChar();
            parserJolly.advanceOneChar();
        }

    }
    return true;
}

//*********************************************
bool fs::doesFileNameMatchJolly (const u8 *utf8_filename, const u8 *utf8_strJolly)
{
	//la stringa dei jolly potrebbe contenere pi� di una sequenza. Le sequenze sono separate da spazio
	//es: *.txt *.bmp
	bool ret = false;

	u8 jolly[128];
	string::utf8::Iter parserJolly;
	parserJolly.setup (utf8_strJolly);

	u8 ct = 0;
	while (1)
	{
		const UTF8Char ch = parserJolly.getCurChar();
		if (ch.isEOF())
		{
			jolly[ct] = 0x00;
			if (FS_doesFileNameMatchJolly(utf8_filename, jolly))
				ret = true;
			break;
		}
		else if (ch.isEqual(' '))
		{
			jolly[ct] = 0x00;
			if (FS_doesFileNameMatchJolly(utf8_filename, jolly))
				ret = true;
			
			parserJolly.advanceOneChar();
			ct = 0;
		}
		else
		{
			memcpy (&jolly[ct], ch.data, ch.length());
			ct += ch.length();
			parserJolly.advanceOneChar();
		}
	}

	return ret;

}

//**************************************************************************
void fs::findComposeFullFilePathAndName(const OSFileFind &ff, const u8 *pathNoSlash, u8 *out, u32 sizeofOut)
{
	sprintf_s((char*)out, sizeofOut, "%s/", pathNoSlash);

	const u32 n = string::utf8::lengthInBytes(out);
	fs::findGetFileName(ff, &out[n], sizeofOut - n);
}

//**************************************************************************
void fs::deleteAllFileInFolderRecursively(const u8 *pathSenzaSlash, bool bAlsoRemoveFolder)
{
	if (!folderExists(pathSenzaSlash))
		return;

	OSFileFind ff;
	if (fs::findFirst(&ff, pathSenzaSlash, (const u8*)"*.*"))
	{
		do
		{
			u8 s[512];
			findComposeFullFilePathAndName(ff, pathSenzaSlash, s, sizeof(s));

			if (fs::findIsDirectory(ff))
			{
				const u8 *fname = fs::findGetFileName(ff);
				if (fname[0] != '.')
					fs::deleteAllFileInFolderRecursively(s, true);
			}
			else
				fs::fileDelete(s);
		} while (fs::findNext(ff));
		fs::findClose(ff);
	}	

	if (bAlsoRemoveFolder)
		fs::folderDelete(pathSenzaSlash);
}

//**************************************************************************
u64 fs::filesize(const u8 *utf8_srcFullFileNameAndPath)
{
	u64 ret = 0;
	FILE *f = fileOpenForReadBinary(utf8_srcFullFileNameAndPath);
	if (f)
	{
		ret = filesize(f);
        rhea::fs::fileClose(f);
	}
	return ret;
}

//**************************************************************************
u64 fs::filesize(FILE *fp)
{
	long prev = ftell(fp);
	fseek(fp, 0L, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, prev, SEEK_SET);
	return sz;
}

//**************************************************************************
void fs::fileCopyInChunkWithPreallocatedBuffer (FILE *fSRC, u32 numBytesToCopy, FILE *fDST, void *buffer, u32 BUFFER_SIZE)
{
	while (numBytesToCopy >= BUFFER_SIZE)
	{
		fread (buffer, BUFFER_SIZE, 1, fSRC);
		fwrite(buffer, BUFFER_SIZE, 1, fDST);
		numBytesToCopy -= BUFFER_SIZE;
	}
	if (numBytesToCopy)
	{
		fread (buffer, (size_t)numBytesToCopy, 1, fSRC);
		fwrite(buffer, (size_t)numBytesToCopy, 1, fDST);
	}
}

//**************************************************************************
bool fs_do_open_and_copy_fileCopy (const u8 *utf8_srcFullFileNameAndPath, const u8 *utf8_dstFullFileNameAndPath, void *buffer, u32 BUFFER_SIZE)
{
	FILE *fSRC = fs::fileOpenForReadBinary (utf8_srcFullFileNameAndPath);
	if (NULL == fSRC)
		return false;

	FILE *fDST = fs::fileOpenForWriteBinary(utf8_dstFullFileNameAndPath);
	if (NULL == fDST)
	{
        rhea::fs::fileClose(fSRC);
		return false;
	}

	fs::fileCopyInChunkWithPreallocatedBuffer (fSRC, (u32)fs::filesize(fSRC), fDST, buffer, BUFFER_SIZE);
    rhea::fs::fileClose(fSRC);
    fflush(fDST);

#ifdef LINUX
	fsync (fileno(fDST));
#endif
	
    rhea::fs::fileClose(fDST);

#ifdef LINUX
    sync();
#endif
	return true;
}

//**************************************************************************
bool fs::fileCopy (const u8 *utf8_srcFullFileNameAndPath, const u8 *utf8_dstFullFileNameAndPath)
{
    const u32 BUFFER_SIZE = 1024*1024;
	rhea::Allocator *allocator = rhea::getScrapAllocator();
	void *buffer = RHEAALLOC(allocator, BUFFER_SIZE);
	bool ret = fs_do_open_and_copy_fileCopy(utf8_srcFullFileNameAndPath, utf8_dstFullFileNameAndPath, buffer, BUFFER_SIZE);
	RHEAFREE(allocator, buffer);
	return ret;
}

//**************************************************************************
bool fs_folderCopy_with_buffer (const u8 *utf8_srcFullPathNoSlash, const u8 *utf8_dstFullPathNoSlash, void *buffer, u32 BUFFER_SIZE, u8* const *elencoPathDaEscludere)
{
	if (!fs::folderCreate(utf8_dstFullPathNoSlash))
	{
		DBGBREAK;
		return false;
	}

	if (!fs::folderExists(utf8_srcFullPathNoSlash))
		return false;

	bool ret = true;
	OSFileFind ff;
	if (fs::findFirst(&ff, utf8_srcFullPathNoSlash, (const u8*)"*.*"))
	{
		do
		{
			if (fs::findIsDirectory(ff))
			{
				const u8 *dirname = fs::findGetFileName(ff);
				if (dirname[0] != '.')
				{
					char src[1024], dst[1024];
					sprintf_s(src, sizeof(src), "%s/%s", utf8_srcFullPathNoSlash, dirname);

					bool bSkipFolder = false;
					const char* *p = (const char**)elencoPathDaEscludere;
					while (p)
					{
						if (NULL == p[0])
							break;
						if (strcasecmp(p[0], src) == 0)
						{
							bSkipFolder = true;
							break;
						}
						p++;
					}

					if (!bSkipFolder)
					{
						sprintf_s(dst, sizeof(dst), "%s/%s", utf8_dstFullPathNoSlash, dirname);
						if (!fs_folderCopy_with_buffer((const u8*)src, (const u8*)dst, buffer, BUFFER_SIZE, elencoPathDaEscludere))
							ret = false;
					}
				}
			}
			else
			{
				const u8 *fname = fs::findGetFileName(ff);
				
				char src[1024], dst[1024];
				sprintf_s(src, sizeof(src), "%s/%s", utf8_srcFullPathNoSlash, fname);
				sprintf_s(dst, sizeof(dst), "%s/%s", utf8_dstFullPathNoSlash, fname);
				fs_do_open_and_copy_fileCopy((const u8*)src, (const u8*)dst, buffer, BUFFER_SIZE);
			}
		} while (fs::findNext(ff));
		fs::findClose(ff);
	}

	return ret;
}


//**************************************************************************
bool fs::folderCopy(const u8 *utf8_srcFullPathNoSlash, const u8 *utf8_dstFullPathNoSlash, u8* const *elencoPathDaEscludere)
{
	//alloco un buffer per il file copy
    const u32 BUFFER_SIZE = 1024*1024;
	rhea::Allocator *allocator = rhea::getScrapAllocator();
	
	void *buffer = RHEAALLOC(allocator, BUFFER_SIZE);

	bool ret = fs_folderCopy_with_buffer(utf8_srcFullPathNoSlash, utf8_dstFullPathNoSlash, buffer, BUFFER_SIZE, elencoPathDaEscludere);

	RHEAFREE(allocator, buffer);
	return ret;

}

//*********************************************
u8* fs::fileCopyInMemory(const u8 *utf8_srcFullFileNameAndPath, rhea::Allocator *allocator, u32 *out_sizeOfAllocatedBuffer)
{
	FILE *f = fs::fileOpenForReadBinary(utf8_srcFullFileNameAndPath);
	if (NULL == f)
	{
		*out_sizeOfAllocatedBuffer = 0;
		return NULL;
	}

	u8 *ret = fs::fileCopyInMemory(f, allocator, out_sizeOfAllocatedBuffer);
    rhea::fs::fileClose(f);
	return ret;
}

//*********************************************
u32 fs::fileReadInPreallocatedBuffer (const u8 *srcFullFileNameAndPath, void *dstBuffer, u32 sizeOfDstBuffer)
{
	if (NULL == dstBuffer || 0 == sizeOfDstBuffer)
	{
		DBGBREAK;
		return 0;
	}

	FILE *f = fs::fileOpenForReadBinary(srcFullFileNameAndPath);
	if (NULL == f)
		return 0;

	const u32 fsize = static_cast<u32>(filesize(f));
	u32 nToRead = fsize;
	if (nToRead > sizeOfDstBuffer)
		nToRead = sizeOfDstBuffer;
	fileRead (f, dstBuffer, nToRead);
	fileClose(f);
	return nToRead;
}


//*********************************************
u8* fs::fileCopyInMemory (FILE *f, rhea::Allocator *allocator, u32 *out_sizeOfAllocatedBuffer)
{
	u32 fsize = (u32)fs::filesize(f);

	*out_sizeOfAllocatedBuffer = (u32)fsize;
	u8 *buffer = RHEAALLOCT(u8*,allocator, (u32)fsize);
	if (NULL == buffer)
	{
		*out_sizeOfAllocatedBuffer = 0;
		return NULL;
	}

	u32 CHUNK = 1024 * 1024;
	u32 ct = 0;
	while (fsize >= CHUNK)
	{
		fread(&buffer[ct], CHUNK, 1, f);
		fsize -= CHUNK;
		ct += CHUNK;
	}

	if (fsize)
		fread(&buffer[ct], fsize, 1, f);

	return buffer;

}

//*********************************************
u32 fs::fileRead (FILE *f, void *out_bufferIN, u32 numBytesToRead)
{
	const u32 CHUNK_SIZE = 1024;

    u8 *out_buffer = (u8*)out_bufferIN;
	u32 ct = 0;
	while (numBytesToRead >= CHUNK_SIZE)
	{
		const u32 n = fread(&out_buffer[ct], CHUNK_SIZE, 1, f);
		if (n == 0)
			return ct;
		numBytesToRead -= CHUNK_SIZE;
		ct += CHUNK_SIZE;
	}
	if (numBytesToRead)
	{
		fread(&out_buffer[ct], numBytesToRead, 1, f);
		ct += numBytesToRead;
	}

	return ct;
}

//*********************************************
u32 fs::fileWrite (FILE *f, const void *bufferIN, u32 numBytesToWrite)
{
	const u32 CHUNK_SIZE = 1024;

    const u8 *buffer = (const u8*)bufferIN;
	u32 ct = 0;
	while (numBytesToWrite >= CHUNK_SIZE)
	{
		const u32 n = fwrite(&buffer[ct], CHUNK_SIZE, 1, f);
		if (n == 0)
			return ct;
		numBytesToWrite -= CHUNK_SIZE;
		ct += CHUNK_SIZE;
	}
	if (numBytesToWrite)
	{
		fwrite(&buffer[ct], numBytesToWrite, 1, f);
		ct += numBytesToWrite;
	}

	return ct;
}

//**************************************************************************
bool fs::findFirstFileInFolderWithJolly (const u8 *utf8_path, const u8 *utf8_jolly, bool bReturnFullPathAndName, u8 *out_filename, u32 sizeOfOutFilename)
{
    assert (sizeOfOutFilename > 0);

    OSFileFind ff;
    if (!fs::findFirst (&ff, utf8_path, utf8_jolly))
    {
        out_filename[0] = 0x00;
        return false;
    }

    do
    {
        if (fs::findIsDirectory(ff))
            continue;

        if (bReturnFullPathAndName)
            fs::findComposeFullFilePathAndName (ff, utf8_path, out_filename, sizeOfOutFilename);
        else
            fs::findGetFileName (ff, out_filename, sizeOfOutFilename);
        fs::findClose(ff);
        return true;

    } while (fs::findNext(ff));

    fs::findClose(ff);
    return false;
}

//**************************************************************************
bool fs::fileCopyAndKeepSameName (const u8 *utf8_srcFullFileNameAndPath, const u8 *utf8_dstPathNOFilename)
{
    u8 fileNameOnly[128];
    fs::extractFileNameWithExt (utf8_srcFullFileNameAndPath, fileNameOnly, sizeof(fileNameOnly));

    u8 dstFullFileNameAndPath[512];
    rhea::string::utf8::spf (dstFullFileNameAndPath, sizeof(dstFullFileNameAndPath), "%s/%s", utf8_dstPathNOFilename, fileNameOnly);
    return fs::fileCopy (utf8_srcFullFileNameAndPath, dstFullFileNameAndPath);
}

