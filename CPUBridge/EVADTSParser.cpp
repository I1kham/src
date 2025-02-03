#include "EVADTSParser.h"
#include "../rheaCommonLib/rheaNetBufferView.h"

using namespace rhea;

EVADTSParser::ContatoreValNumValNum	EVADTSParser::MatriceContatori::contatoreAZero;


//*******************************************************
EVADTSParser::EVADTSParser()
{
	allocator = rhea::getScrapAllocator();
	selezioni.setup(allocator, 64);
	elencoPP1.setup(allocator, 8);
}

//*******************************************************
EVADTSParser::~EVADTSParser()
{
	priv_reset();
	selezioni.unsetup();
}

//*******************************************************
void EVADTSParser::priv_reset()
{
	VA1.reset();
	VA2.reset();
	VA3.reset();
	CA2.reset();
	matriceContatori.reset();
	elencoPP1.reset();
	
	for (u32 i = 0; i < selezioni.getNElem(); i++)
		RHEADELETE(allocator, selezioni[i]);
	selezioni.reset();
}

//*******************************************************
bool EVADTSParser::loadAndParse(const u8 *fullFilePathAndName)
{
	FILE *f = rhea::fs::fileOpenForReadBinary(fullFilePathAndName);
	if (NULL == f)
		return false;

	rhea::Allocator *allocator = rhea::getScrapAllocator();
	u32 bufferSize = 0;
	u8 *buffer = rhea::fs::fileCopyInMemory(f, allocator, &bufferSize);
	rhea::fs::fileClose(f);
	bool ret = parseFromMemory(buffer, 0, bufferSize);
	RHEAFREE(allocator, buffer);
	return ret;
}

//*******************************************************
bool EVADTSParser::parseFromMemory (const u8 *buffer, u32 firstByte, u32 nBytesToCheck)
{
	priv_reset();

	string::utf8::Iter iter1;
	iter1.setup(buffer, firstByte, nBytesToCheck);

	InfoSelezione *lastInfoSel = NULL;
	TempStr128 par[8];
	while (1)
	{
		string::utf8::Iter iter2;
		string::utf8::extractLine (iter1, &iter2);
		if (iter2.getBytesLeft() == 0)
			break;

		u8 line[256];
		//iter2.copyCurStr(line, sizeof(line));
		iter2.copyAllStr(line, sizeof(line));

		if (priv_checkTag(line, "VA1", 4, par))
		{
			VA1.valorizzaFromString_ValNumValNum(par);
		}
		else if (priv_checkTag(line, "VA2", 4, par))
		{
			VA2.valorizzaFromString_ValNumValNum(par);
		}
		else if (priv_checkTag(line, "VA3", 4, par))
		{
			VA3.valorizzaFromString_ValNumValNum(par);
		}
		else if (priv_checkTag(line, "CA2", 4, par))
		{
			CA2.valorizzaFromString_ValNumValNum(par);
		}
		else if (priv_checkTag(line, "PP1", 8, par))
		{
			const u32 n = elencoPP1.getNElem();
			elencoPP1[n].reset();
			elencoPP1[n].valorizzaFromString (par);
		}
		else if (priv_checkTag(line, "PA1", 2, par)) //a volte il terzo parametro (nome selezione) non esiste
		{
			//ho trovato il tag di inizio di una nuova selezione

			const int currentNewSelID = priv_toInt(par[0].s);
			if (selezioni.getNElem())
			{
				int lastSelID = selezioni(selezioni.getNElem() - 1)->id;
				//devo riempire eventuali "gap" tra l'id della selezione precedente e l'id di quella nuova.
				//Se per esempio saltiamo da selezione 3 a selezione 5, devo aggiungere d'ufficio la selezione 4
				while (lastSelID < (currentNewSelID - 1))
				{
					++lastSelID;

					InfoSelezione *p = RHEANEW(allocator, InfoSelezione)();
					p->id = lastSelID;
					p->price = 0;
					p->name.s[0] = 0;
					selezioni.append(p);
				}
			}


			lastInfoSel = RHEANEW(allocator, InfoSelezione)();
			lastInfoSel->id = currentNewSelID;
			lastInfoSel->price = priv_toInt(par[1].s);
			
			if (priv_checkTag(line, "PA1", 3, par))
				strcpy(lastInfoSel->name.s, par[2].s);
			else
				lastInfoSel->name.s[0] = 0;
			selezioni.append(lastInfoSel);

		}
		else if (priv_checkTag(line, "PA2", 4, par))
		{
			if (lastInfoSel)
				lastInfoSel->aPagamento.valorizzaFromString_NumValNumVal(par);
		}
		else if (priv_checkTag(line, "PA3", 4, par))
		{
			if (lastInfoSel)
				lastInfoSel->testvend.valorizzaFromString_NumValNumVal(par);
		}
		else if (priv_checkTag(line, "PA4", 4, par))
		{
			if (lastInfoSel)
				lastInfoSel->freevend.valorizzaFromString_NumValNumVal(par);
		}
		else if (priv_checkTag(line, "PA7", 8, par))
		{
			if (lastInfoSel)
			{
                //int prodID = priv_toInt(par[0].s);
				//per come � strutturato il file evadts, si assume che questo prodID sia esattamente lo stesso indicato dal tag PA1 che sequenzialmente
				//nel file � comparso poco prima di questo tag PA7 ( e che qui nel codice � memorizzato in selezioni[iSel].id ).
				//Per fare le cose in maniera pi� corretta bisognerebbe prima parsare tutto il file skippando i PA7 e poi riparsare il file considerando
				//solo i PA7 e, in base al prodID indicato dal PA7, andare a cercare la selezione con l'id corretto                        

				const ePaymentDevice paymentDevice = priv_fromStringToPaymentDevice(par[1].s);
				const int idxListaPrezzi = priv_toInt(par[2].s);
                //const int prezzoApplicato = priv_toInt(par[3].s);
				const int num_tot = priv_toInt(par[4].s);
				const int val_tot = priv_toInt(par[5].s);
				const int num_par = priv_toInt(par[6].s);
				const int val_par = priv_toInt(par[7].s);
				lastInfoSel->matriceContatori.set(idxListaPrezzi, paymentDevice, num_tot, val_tot, num_par, val_par);
			}
		}
	}

	//ho finito di parsare il file, calcolo la [matriceContatori] considerando le singole matriceContatori delle selezioni
	for (u8 i = 1; i <= NUM_LISTE_PREZZI; i++)
	{
		for (u8 t = 0; t < NUM_PAYMENT_DEVICE; t++)
		{
			for (u32 z = 0; z < selezioni.getNElem(); z++)
			{
				//int iListaPrezzo, ePaymentDevice paymentDevice, ContatoreValNumValNum c
				const ContatoreValNumValNum *ct = selezioni.getElem(z)->matriceContatori.get(i, (ePaymentDevice)t);
				matriceContatori.add(i, (ePaymentDevice)t, ct);
			}
		}
	}

	return true;
}


/***************************************************************************************************************************************************
* se la stringa [s] inizia con il [tagToFindAtStartOfTheLine], allora la fn ritorna true e filla out_params con le stringhe contenenti gli [numOfParamsToRead] parametri che
* seguono il tag.
* Ad esempio, data la riga:
*      VA1*15*1804*15*1804*0*0*0*0*0*0*0*0
*
* checkTag (s, "VA1", 4) ritornera true e valorizzera i primi 4 elementi di out_params come segue: {"15", "1804", "15", "1804"}
*
* Se non trova il tag ad inizio riga, ritorna false
*
* E' responsabilit� del chiamante assicurarsi che [out_params] sia un array in grado di contenere almeno [numOfParamsToRead] stringhe
*/
bool EVADTSParser::priv_checkTag (const u8 *s, const char *tagToFindAtStartOfTheLine, u16 numOfParamsToRead, TempStr128 *out) const
{
	if (NULL == s)
		return false;
	const u32 sLen = string::utf8::lengthInBytes(s);
	const u32 tagLen = (u32)strlen(tagToFindAtStartOfTheLine);

	//se non c'� il tag ad inizio stringa, ho finito
	if (sLen < tagLen + 1)
		return false;

	if (strncasecmp(tagToFindAtStartOfTheLine, (const char*)s, tagLen) != 0)
		return false;

	//deve anche esserci * dopo il TAG
	if (s[tagLen] != '*')
		return false;
	if (numOfParamsToRead < 1)
		return true;

	if (sLen < tagLen + 2)
		return false;

	string::utf8::Iter iter1;
	iter1.setup(s, tagLen+1);

	//ok, il tag c'�, recuperiamo i parametri
	for (u16 i = 0; i < numOfParamsToRead; i++)
		out[i].s[0] = 0x00;

	u16 nFound = 0;
	for (u16 i = 0; i < numOfParamsToRead; i++)
	{
		string::utf8::Iter iter2;
		UTF8Char cStar("*");
		if (!string::utf8::extractValue(iter1, &iter2, &cStar, 1))
			break;
		iter2.copyAllStr ((u8*)out[i].s, sizeof(out[i]));
		iter1.advanceOneChar();
		nFound++;
	}

	return (nFound == numOfParamsToRead);
}


//*******************************************************
int EVADTSParser::priv_toInt(const char *s) const
{
	if (NULL == s)
		return 0;
	if (s[0] == 0x00)
		return 0;

	u32 len = (u32)strlen(s);
	for (u32 i = 0; i < len; i++)
	{
		if (s[i]<'0' || s[i]>'9')
		{
			DBGBREAK;
			return 0;
		}
	}
	return atoi((const char*)s);
}

//*******************************************************
EVADTSParser::ePaymentDevice EVADTSParser::priv_fromStringToPaymentDevice(const char *s) const
{
	if (strncasecmp(s, "CA", 2) == 0)
		return ePaymentDevice::cash;
	if (strncasecmp(s, "DA", 2) == 0)
		return ePaymentDevice::cashless1;
	if (strncasecmp(s, "DB", 2) == 0)
		return ePaymentDevice::cashless2;
	if (strncasecmp(s, "TA", 2) == 0)
		return ePaymentDevice::token;
	return ePaymentDevice::unknown;
}


/*******************************************************
 * recupera i dati utili da visualizzare, e li impacchetta in un buffer
 */
u8* EVADTSParser::createBufferWithPackedData (rhea::Allocator *allocator, u32 *out_bufferLen, u8 numDecimali) const
{
	const u16 SIZE = 10 * 1024;
	u8 * ret = (u8*)RHEAALLOC(allocator, SIZE);

	rhea::NetStaticBufferViewW nbw;
	nbw.setup(ret, SIZE, rhea::eEndianess::eBigEndian);

	const u8 nSelezioni = (u8)selezioni.getNElem();
	const u8 nPresel = (u8)elencoPP1.getNElem();

	nbw.writeU8(2);				//versione di questo layout
	nbw.writeU8(nSelezioni);	//num selezioni
	nbw.writeU8(numDecimali);	//num decimali
	nbw.writeU8(nPresel);		//numero preselezioni nei contatori PP1

	//qui ci metto l'indirizzo di inizio del blocco dati 1
	const u32 seek1 = nbw.tell();
	nbw.writeU32(0);

	const u32 seek2 = nbw.tell();	//indirizzo del blocco DATI PARZIALI
	nbw.writeU32(0);

	const u32 seek3 = nbw.tell();	//indirizzo del blocco DATI TOTALI
	nbw.writeU32(0);

	const u32 seek4 = nbw.tell();	//indirizzo del blocco DATI PRESELEZIONI
	nbw.writeU32(0);

	//BLOCCO DATI 1
	nbw.writeU32At(nbw.tell(), seek1);
	nbw.writeU32(VA1.val_tot);	//VA101
	nbw.writeU32(VA1.num_tot);	//VA102
	nbw.writeU32(VA1.val_par);	//VA103
	nbw.writeU32(VA1.num_par);	//VA104
	

	nbw.writeU32(VA2.val_tot);	//VA201
	nbw.writeU32(VA2.num_tot);	//VA202
	nbw.writeU32(VA2.val_par);	//VA203
	nbw.writeU32(VA2.num_par);	//VA204

	nbw.writeU32(VA3.val_tot);	//VA301
	nbw.writeU32(VA3.num_tot);	//VA303
	nbw.writeU32(VA3.val_par);	//VA303
	nbw.writeU32(VA3.num_par);	//VA304
	

	for (u8 i = 0; i < nSelezioni; i++)
	{
		//num test vend
		ContatoreValNumValNum pa2 = selezioni.getElem(i)->aPagamento;
		nbw.writeU32(pa2.num_tot); //pa201
		nbw.writeU32(pa2.val_tot); //pa202
		nbw.writeU32(pa2.num_par); //pa203
		nbw.writeU32(pa2.val_par); //pa204
			

		//num test vend
		ContatoreValNumValNum pa3 = selezioni.getElem(i)->testvend;
		nbw.writeU32(pa3.num_tot);
		nbw.writeU32(pa3.val_tot);
		nbw.writeU32(pa3.num_par);
		nbw.writeU32(pa3.val_par);
		
		//num freevend
		ContatoreValNumValNum pa4 = selezioni.getElem(i)->freevend;
		nbw.writeU32(pa4.num_tot);
		nbw.writeU32(pa4.val_tot);
		nbw.writeU32(pa4.num_par);
		nbw.writeU32(pa4.val_par);
	}

	//BLOCCO DATI PARZIALI
	//parziali per ogni selezione
	nbw.writeU32At(nbw.tell(), seek2);
	for (u8 i = 0; i < nSelezioni; i++)
	{
		//paid (price 1)
		ContatoreValNumValNum c1;
		selezioni.getElem(i)->matriceContatori.getTotaliPerListaPrezzo_Tot_1_4(1, c1);

		//paid (price 2)
		ContatoreValNumValNum c2;
		selezioni.getElem(i)->matriceContatori.getTotaliPerListaPrezzo_Tot_1_4(2, c2);

		//num freevend
		ContatoreValNumValNum c3 = selezioni.getElem(i)->freevend;

		//num test vend
		ContatoreValNumValNum c4 = selezioni.getElem(i)->testvend;

		nbw.writeU32(c1.num_par);	//num paid (price1)
		nbw.writeU32(c2.num_par);	//num paid (price2)
		nbw.writeU32(c3.num_par);	//num freevend
		nbw.writeU32(c4.num_par);	//num testvend
		nbw.writeU32(c1.val_par);	//tot cash (price1)
		nbw.writeU32(c2.val_par);	//tot cash (price2)
	}

	//BLOCCO DATI TOTALI
	//totali per ogni selezione
	nbw.writeU32At(nbw.tell(), seek3);
	for (u8 i = 0; i < nSelezioni; i++)
	{
		//paid (price 1)
		ContatoreValNumValNum c1;
		selezioni.getElem(i)->matriceContatori.getTotaliPerListaPrezzo_Tot_1_4(1, c1);

		//paid (price 2)
		ContatoreValNumValNum c2;
		selezioni.getElem(i)->matriceContatori.getTotaliPerListaPrezzo_Tot_1_4(2, c2);

		//num freevend
		ContatoreValNumValNum c3 = selezioni.getElem(i)->freevend;

		//num test vend
		ContatoreValNumValNum c4 = selezioni.getElem(i)->testvend;

		nbw.writeU32(c1.num_tot);	//num paid (price1)
		nbw.writeU32(c2.num_tot);	//num paid (price2)
		nbw.writeU32(c3.num_tot);	//num freevend
		nbw.writeU32(c4.num_tot);	//num testvend
		nbw.writeU32(c1.val_tot);	//tot cash (price1)
		nbw.writeU32(c2.val_tot);	//tot cash (price2)
	}

	//BLOCCO DATI PREsELEZIONI
	nbw.writeU32At(nbw.tell(), seek4);
	for (u8 i = 0; i < nPresel; i++)
	{
		nbw.writeU32(elencoPP1(i).pp101_preselNumber);
		nbw.writeU32(elencoPP1(i).pp102_price);
		nbw.writeU32(elencoPP1(i).pp104_incrementalPrice);
		nbw.writeU32(elencoPP1(i).pp105_numTimesUsedSinceInit);
		nbw.writeU32(elencoPP1(i).pp106_valueSinceInit);
		nbw.writeU32(elencoPP1(i).pp107_numTimesUsedSinceReset);
		nbw.writeU32(elencoPP1(i).pp108_valueSinceReset);

		const u8 n = static_cast<u8>(strlen(elencoPP1(i).pp103_name.s));
		nbw.writeU8(n);
		if (n > 0)
			nbw.writeBlob (elencoPP1(i).pp103_name.s, n);
	}

	*out_bufferLen = nbw.length();
	return ret;

}
