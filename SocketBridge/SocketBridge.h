/******************************************************************************************************************************
	SocketBridge � un'entit� che si frappone tra il mondo esterno e CPUBridge.
	SocketBridge riceve comandi dal mondo esterno tramite il suo canale di input (una socket TCP/IP) ed eventualmente comunica il 
	da farsi a CPUBridge tramite una serie di messaggi tra thread.
	
	SocketBridge � un thread che opera tramite l'utilizzo di 1 canale di input e di un canale di output.
	L'input proviene da una socket TCP/IP aperta da SocketBridge sulla porta 2280.
	L'output � una message queue thread safe che permette la comunicazione bidirezionale tra il thread di SocketBridge ed il thread di CPUBridge.

	Il "main" di SocketBridge � il metodo run() della classe socketbridge::Server, il cui src � nel file SocketBridgeServer.cpp

	I comandi accettati da SocketBridge sono di 3 tipi:
		1- tipo evento
		2- tipo ajax
		3- tipo file transfer

	Tutti i comandi di tipo "evento" devono derivare dalla classe socketbridge::CmdHandler_eventReq (vedi CmdHandler_eventReq.h).
	Tutti i comandi di tipo "ajax" devono derivare dalla classe socketbridge::CmdHandler_ajaxReq (vedi CmdHandler_ajaxReq.h).
	La gestione del trasferimento di file � interna, non � necessaria l'implementazione di nessuno nuovo comando.
	Per i dettagli sull'aggiunta di nuovi comandi "evento" o "ajax", vedi i relativi .h (CmdHandler_eventReq.h e CmdHandler_ajaxReq.h).

	I comandi di tipo "evento" sono pi� semplici. Generalmente il comando � una richiesta da girare direttamente alla CPU la quale, a sua volta,
	produce una risposta all'evento da inviare indietro al chiamante.
	Il meccanismo di invio e del tipo "fire and forget" ovvero il client invia il comando evento e poi se ne dimentica.
	Eventuali risposte (a loro volta in formato evento) possono o non possono arrivare da SocketBridge in un tempo non determinabile.
	Ad esempio, quando il client invia un comando di "richiesta stato CPU", la risposta non � detto che arrivi immediatamente.
	Il client semplicemente dimentica di aver fatto la richiesta e procede con il suo lavoro.
	Ad un certo punto, eventualmente, il client a sua volta ricever� un "evento stato CPU" che riporta lo stato della CPU.
	Questo evento probabilmente � la risposta alla richiesta del client, ma non � detto, perch� gli "eventi" possono essere generati anche
	senza alcuna richiesta da parte del client.
	Ogni volta che la CPU cambia stato per esempio, il client viene notificato con un evento senza necessariamente che questo sia stato esplicitamente
	richiesto dal client stesso.
	Esempi di comando "evento" sono:
		- richiesta stato della CPU (il client vuole conosce lo stato della CPU)
		- richiesta inizio selezione (il cliente chiede che la selezione n sia eseguita)
		- richiesta dell'elenco dei prezzi delle selezioni
		- richiesta dello stato di disponibilit� delle selezioni


	I comandi di tipo "ajax" sono generalmente pi� complessi e servono per mimare le classiche chiamate ajax fatte dai browser usando JS.
	Prevedono un numero variabile di parametri di input forniti dal client (il numero ed il formato dei parametri varia da comando a comando) e
	prevedono una risposta entro un tempo massimo stabilito dal client (di default 2 sec).
	Possono anche essere comandi semplici alla stregua dei comandi evento; la vera differenza tra i due � il meccanismo di risposta.
	Un comando "ajax" inviato dal browser incorpora un meccanismo automatico di "attesa della risposta e gestione del timeout". In pratica il browser
	invia il comando ajax e poi aspetta una risposta che potrebbe anche non arrivare. Se entro "timeout" secondi la risposta non arriva, il browser viene
	notificato della mancata risposta. Se la risposta arriva, il browser viene notificato con il risultato inviato da SocketBrdige in risposta.
	La risposta generalmente viene fornita in formato JSON.
	Non � inusuale avere un comando "ajax" che fa esattamente la stessa richiesta di un comando "evento".
	La differenza � che nel caso ajax, mi aspetto una risposta o un timeout in tempi brevi e quindi il browser � bloccato fino alla risoluzione della richiesta,
	mentre nel caso "evento", non mi aspetto necessariamente una risposta ora e subito per cui una volta richiesto l'evento, il browser prosegue con
	le sue cose senza rimanere in attesa attiva della risposta.
		

*/
#ifndef _SocketBridge_h_
#define _SocketBridge_h_
#include "SocketBridgeServer.h"
#include "../rheaCommonLib/SimpleLogger/ISimpleLogger.h"
#include "../rheaCommonLib/Protocol/IProtocolChannell.h"
#include "../rheaCommonLib/Protocol/IProtocol.h"


namespace socketbridge
{
	bool        startServer (rhea::ISimpleLogger *logger, const HThreadMsgW &hCPUServiceChannelW, bool bDieWhenNoClientConnected, bool bWaitUntilThreadIsStarted, rhea::HThread *out_hThread);
				/*	crea il thread che monitora e gestisce la socket.
					Se hCPUServiceChannelW � valido, allora il thread in questione si "subscribe()" al thread della CPU in modo da riceverne le notifiche
				*/
					
	Server*		priv_getInstanceFromHThread (const rhea::HThread hThread);



				template<class TTask>
	void		addTask (const rhea::HThread hThread, const char *taskName) 
				{ 
					priv_getInstanceFromHThread(hThread)->taskAdd<TTask>(taskName);
				}

} // namespace socketbridge


#endif // _SocketBridge_h_

