# Sistema di Monitoraggio Allarme Bentel con Notifiche Telegram
## Documento di Design, Requisiti e Specifiche Tecniche

**Versione:** 0.9
**Data:** Agosto 2026
**Piattaforma target:** Arduino Nano ESP32

---

## 1. Obiettivo del sistema

Realizzare un sistema che monitori lo stato di una centralina d'allarme Bentel tramite Arduino Nano ESP32, invii notifiche in tempo reale su Telegram all'apertura/chiusura di eventi (allarme, riavvio, problemi di rete, interruzione di corrente, ecc.), e mantenga un registro storico persistente e consultabile degli eventi, con gestione robusta di interruzioni di connettività e di alimentazione, accesso limitato a utenti autorizzati e preferenze configurabili per singolo utente.

**Nota sull'alimentazione**: l'Arduino sarà alimentato dalla centralina d'allarme, che dispone di batteria tampone in caso di interruzione elettrica. I riavvii per mancanza di corrente sono quindi previsti come **eventi rari**; le interruzioni di **sola connettività di rete** (router/ISP, non necessariamente su batteria) restano invece il caso di guasto più plausibile e frequente, e sono quelle per cui il sistema di recupero notifiche è principalmente pensato.

### 1.1 Vincolo architetturale: indipendenza da sistemi esterni

Il dispositivo è **volutamente autonomo**: non dipende da alcun broker MQTT, sistema di home automation (Home Assistant o simili) o server locale. L'ESP32 parla direttamente alle API di Telegram e gestisce in proprio persistenza, storico, retry, utenti e preferenze.

Questa scelta è deliberata e motiva la complessità delle sezioni 4-11 (che in presenza di un sistema a monte sarebbero in gran parte delegabili): **accorciare la catena di guasto tra il rilevamento dell'allarme e il telefono dell'utente**. Ogni componente intermedio sarebbe un ulteriore punto di rottura tra l'evento e la notifica, inaccettabile in un sistema di sicurezza.

Corollario: il sistema non ha alcun osservatore esterno che possa accorgersi di un suo guasto totale. La verifica dello stato è **manuale**, tramite il comando `/status` (sezione 12) — scelta consapevole, preferita a un heartbeat periodico automatico che genererebbe messaggi ricorrenti non desiderati.

---

## 2. Requisiti hardware

### 2.1 Lettura dello stato allarme

La centralina Bentel espone uscite PGM (programmabili) configurabili come indicatori di stato. Sono supportate due modalità di interfacciamento, applicabili a ciascuna uscita utilizzata:

- **Uscita open collector verso negativo**: richiede un **optoisolatore** (es. PC817) tra la centralina e l'ESP32 per garantire isolamento galvanico. Il pin di lettura va configurato in `INPUT_PULLUP`; l'allarme attivo corrisponde a un livello LOW.
- **Uscita a relè (contatto pulito NA/NC)**: collegamento diretto ai pin dell'ESP32, senza necessità di isolamento aggiuntivo, dato che il contatto è meccanicamente isolato dal circuito della centralina. Anche qui si utilizza `INPUT_PULLUP`, con logica invertibile a seconda che si usi il contatto NA o NC.

La scelta tra le due modalità dipende dal modello specifico di centralina e dalla sua configurazione in fase di programmazione installatore (da verificare sul manuale).

**Nota — più tipologie di evento, più input fisici**: con l'introduzione di più tipologie di evento legate a contatti distinti sulla centralina (allarme interno, allarme garage, mancanza rete), è probabile che servano **più uscite PGM dedicate** sulla centralina (una per zona/condizione da monitorare separatamente), e di conseguenza **un pin di lettura ESP32 per ciascuna** (con relativo optoisolatore se l'uscita è open collector). Il numero di pin digitali disponibili sul Nano ESP32 e la disponibilità di uscite PGM configurabili sulla centralina vanno verificati in base al numero finale di tipologie da monitorare. Per "interruzione di corrente" in particolare, molte centraline Bentel espongono una PGM dedicata a "guasto rete / 230V mancante", attiva mentre il sistema resta alimentato dalla batteria tampone.

### 2.2 Debounce

È necessario un meccanismo di anti-rimbalzo software (soglia consigliata: 300 ms) per evitare falsi trigger sulle transizioni di stato del segnale. L'implementazione è vincolata dal modello di concorrenza descritto in sezione 3.3: il debounce **non può essere realizzato con polling nel loop principale**, perché il loop viene bloccato per secondi dalle chiamate di rete.

---

## 3. Architettura software

### 3.1 Componenti principali

| Componente | Responsabilità |
|---|---|
| Lettura pin allarme (ISR + coda) | Rilevamento delle transizioni via interrupt, con debounce e datazione differita (sezione 3.3) |
| Client Telegram (FastBot2) | Invio notifiche, ricezione comandi e callback dei bottoni inline; espone nativamente l'esito strutturato di ogni invio (sezione 3.5, 6.5) |
| Sincronizzazione NTP + gestione timezone | Ottenimento timestamp reali (epoch Unix, UTC) e conversione in ora locale con gestione automatica dell'ora legale |
| Ancora oraria persistente (NVS) | Mantenimento di un riferimento temporale utilizzabile prima della sincronizzazione NTP (sezione 5.4) |
| Gestione connettività e riconnessione | Backoff esponenziale, rilevamento della condizione `NETWORK_ISSUE` (sezione 3.4) |
| Registro eventi (LittleFS, `log.jsonl`) | Persistenza storico eventi (solo rilevamento, non notifiche) |
| Registro notifiche per utente (LittleFS) | Tracciamento degli invii mancati/da recuperare, un file per chat (architettura decisa, vedi sezione 7) |
| Gestione utenti e permessi (whitelist) | Autorizzazione dei `chat_id`, distinzione utente standard/admin, filtro destinatari delle notifiche |
| Gestione configurazione globale (NVS) | Impostazioni di sistema persistenti, modificabili solo da utenti admin |
| Gestione configurazione per utente (LittleFS) | Preferenze individuali (formato data, timezone, tipi di evento notificati) |
| Motore di recupero notifiche | Rilevamento e reinvio notifiche non consegnate, con grace period e retry programmato |
| Rate limiter di invio | Rispetto dei limiti di frequenza delle API Telegram (sezione 6.6) |
| Motore di rotazione | Pulizia periodica del registro eventi e del registro notifiche |
| Sorveglianza del filesystem | Monitoraggio dello spazio e degli errori di scrittura su LittleFS (sezione 9.4) |

### 3.2 Tipologie di evento

Il sistema deve supportare più tipologie di evento, estensibili in futuro. Ogni tipologia è mappata a un valore enum numerico per l'ottimizzazione dello storage (vedi sezione 5.2):

| Valore enum `type` | Tipologia | Natura | Notifica inviata |
|---|---|---|---|
| `0` | `REBOOT` — riavvio dell'Arduino | Istantaneo (`INSTANT`) | `INSTANT` |
| `1` | `POWER_LOSS` — interruzione di corrente (mancanza rete 230V) | Con durata (`START`/`END`) | `START` e `END` |
| `2` | `NETWORK_ISSUE` — problema di connettività di rete | Con durata (`START`/`END`) | **solo `END`** (vedi 3.2.3) |
| `10` | `ALARM_GENERAL` — allarme generale | Con durata (`START`/`END`) | `START` e `END` |
| `11` | `ALARM_INTERNAL` — allarme interno | Con durata (`START`/`END`) | `START` e `END` |
| `12` | `ALARM_GARAGE` — allarme garage | Con durata (`START`/`END`) | `START` e `END` |

*(altre tipologie aggiungibili in coda alla enumerazione, senza rompere la compatibilità con i log esistenti — non riutilizzare/rinumerare valori già assegnati)*

#### 3.2.1 Tabella di configurazione dei tipi

Ogni tipo è descritto nel firmware da una voce di una tabella statica unica, che costituisce **l'unico punto di verità** della mappatura:

| Campo | Significato |
|---|---|
| `type` | Valore enum (mai riassegnato) |
| `label` | Etichetta leggibile per i messaggi Telegram |
| `pin` | Pin ESP32 associato, oppure "nessuno" per gli eventi generati internamente (`REBOOT`, `NETWORK_ISSUE`) |
| `active_low` | Polarità del segnale (dipende da open collector / NA / NC) |
| `enabled` | Se `false`, il tipo è **completamente disattivato**: nessun interrupt registrato, nessuna riga di log, nessuna notifica |
| `notify_policy` | `START_AND_END`, `ONLY_END`, `INSTANT` |

Il flag `enabled` permette di disattivare a livello di firmware una tipologia non cablata o non desiderata, senza rimuoverne il valore enum (che resta riservato per non invalidare i log storici).

#### 3.2.2 Nota su `ALARM_GENERAL` e sovrapposizione con le zone

Se sulla centralina la PGM "allarme generale" è configurata come OR delle zone, un allarme in garage attiverà **sia** il pin di `ALARM_GARAGE` **sia** quello di `ALARM_GENERAL`, generando due eventi distinti e due notifiche per lo stesso fatto fisico.

**Questo comportamento è accettato per scelta**: `ALARM_GENERAL` viene registrato e notificato ogniqualvolta il suo pin si attiva, senza alcuna logica di soppressione o correlazione temporale con le altre zone (che introdurrebbe casi limite di timing difficili da rendere affidabili). L'eventuale rumore si mitiga a due livelli, entrambi già previsti:

- **Per utente**: `/notify ALARM_GENERAL off` (sezione 11.2) disabilita la notifica per chi non la vuole, lasciando comunque l'evento nello storico.
- **Globalmente**: `enabled = false` nella tabella di sezione 3.2.1 disattiva del tutto il tipo.

#### 3.2.3 Nota su `NETWORK_ISSUE` — perché si notifica solo l'`END`

Lo `START` di un problema di connettività si verifica, per definizione, quando la connettività non c'è: la sua notifica fallirebbe **sistematicamente**, finendo sempre in coda di recupero e venendo poi consegnata al ripristino, a distanza di pochi secondi dalla notifica di `END`. Due messaggi in raffica per un unico fatto già concluso.

Il `notify_policy` di `NETWORK_ISSUE` è quindi `ONLY_END`:

- La riga `START` **viene comunque scritta** in `log.jsonl` al momento del rilevamento, così lo storico conserva l'istante esatto di inizio del down ed è consultabile con `/log`.
- Nessuna notifica viene generata (né inviata, né messa in coda di recupero) per quello `START`.
- Alla riconnessione, la notifica di `END` include la **durata del down** calcolata dai due timestamp, es. *"Connettività ripristinata — assente per 1h 28m (dalle 14:02 alle 15:30)"*.

Il caso `POWER_LOSS` resta invece `START_AND_END`: se il router non è sotto UPS, lo `START` fallirà e verrà recuperato dal normale meccanismo di sezione 6, comportamento corretto perché la mancanza di corrente è un evento significativo di per sé anche a posteriori.

### 3.3 Modello di concorrenza: rilevamento vs. rete

**Problema.** Il loop applicativo esegue chiamate HTTPS sincrone verso Telegram (`getUpdates` in long polling, `sendMessage`, handshake TLS). Su ESP32 queste chiamate bloccano il flusso di esecuzione per **secondi**, e fino al timeout configurato in caso di rete degradata. Un debounce implementato con polling nel loop perderebbe qualunque transizione avvenuta durante quelle finestre — cioè, potenzialmente, l'allarme stesso.

**Soluzione adottata: interrupt + coda FreeRTOS + datazione retroattiva.**

1. Su ogni pin abilitato viene registrato un `attachInterrupt()` in modalità `CHANGE`. La ISR è dichiarata `IRAM_ATTR` e fa **una sola cosa**: accodare un record `{indice_pin, livello, millis()}` tramite `xQueueSendFromISR()`. Nessuna allocazione, nessun I/O, nessuna chiamata bloccante nella ISR.
2. La coda ha profondità fissa (32 elementi, ampiamente sufficiente: 32 transizioni durante un singolo blocco di rete rappresentano già una condizione anomala). In caso di coda piena, l'overflow viene **contato** e segnalato in `/status`, mai ignorato silenziosamente.
3. Il loop principale, appena torna disponibile, svuota la coda e applica il debounce sui `millis()` registrati **dalla ISR**, non sull'istante di elaborazione: una transizione è confermata se non è seguita da un'altra transizione sullo stesso pin entro 300 ms.
4. **Datazione retroattiva**: il timestamp dell'evento non è l'istante di elaborazione ma viene ricostruito a ritroso dal `millis()` catturato nella ISR:

   ```
   ts_evento = epoch_corrente - (millis_ora - millis_ISR) / 1000
   ```

   In questo modo un allarme scattato mentre il sistema era bloccato in un timeout TLS di 10 secondi risulta datato correttamente, e non 10 secondi dopo.

**Conseguenze e invarianti:**

- Nessuna transizione viene persa, indipendentemente dalla durata dei blocchi di rete. Un impulso di allarme di durata inferiore al blocco risulta comunque nella coda come coppia di transizioni, e viene ricostruito integralmente (`START` e `END`).
- **Tutti gli accessi a LittleFS avvengono esclusivamente dal loop principale.** La ISR non tocca il filesystem. Non serve quindi alcun mutex sul FS, che non è thread-safe: questo è un invariante da preservare in ogni estensione futura.
- Non viene introdotto alcun task FreeRTOS aggiuntivo: la datazione retroattiva rende superfluo un task di rilevamento dedicato, mantenendo il firmware a singolo flusso applicativo e più semplice da ragionare.
- Il **watchdog hardware** (Task WDT dell'ESP32) è abilitato sul loop applicativo con un timeout superiore al massimo timeout di rete configurato (30 s contro 10 s), così un blocco genuino provoca un riavvio — che a sua volta genera un evento `REBOOT` notificato, rendendo visibile il guasto.

### 3.4 Gestione della connettività e riconnessione

#### 3.4.1 Definizione operativa di "problema di rete"

Ai fini di `NETWORK_ISSUE`, ciò che conta non è lo stato del WiFi ma la **raggiungibilità delle API Telegram**: il caso più comune (router acceso, linea ISP giù) presenta un WiFi perfettamente associato e nessuna connettività utile.

La condizione di problema di rete è quindi definita come:

> WiFi disconnesso **oppure** fallimento di tutte le chiamate verso `api.telegram.org` (comprese le `getUpdates` di polling ordinario)

mantenuta con continuità per più della **soglia configurata** (default: **120 secondi**, `/setnetthreshold`).

- La soglia serve a non generare eventi per micro-interruzioni e per i riavvii del router, che tipicamente rientrano entro i 60-90 secondi. Un valore troppo basso riempirebbe il log di rumore.
- L'`END` di `NETWORK_ISSUE` viene registrato alla **prima chiamata riuscita** verso l'API, con `ts` pari a quell'istante.
- Se la connettività rientra **prima** dello scadere della soglia, nessun evento viene registrato: il down è considerato un blip trascurabile.

#### 3.4.2 Backoff di riconnessione

I tentativi di riconnessione seguono un **backoff esponenziale con tetto**, per non saturare il loop né consumare energia in tentativi inutili durante down prolungati:

| Tentativo | Attesa prima del tentativo successivo |
|---|---|
| 1 | 5 s |
| 2 | 10 s |
| 3 | 20 s |
| 4 | 40 s |
| 5 | 80 s |
| 6 | 160 s |
| 7 e successivi | 300 s (tetto massimo) |

- Il contatore di backoff si **azzera** ad ogni riconnessione riuscita.
- L'attesa è realizzata con timer non bloccante (confronto su `millis()`), mai con `delay()`: il loop deve restare libero di svuotare la coda degli interrupt e di applicare il debounce (sezione 3.3).
- Ogni 10 tentativi consecutivi falliti viene tentato un ciclo completo `WiFi.disconnect()` + `WiFi.begin()`, per recuperare gli stati anomali dello stack WiFi che una semplice `reconnect()` non risolve.
- Il tetto di 300 s garantisce che, a rete ripristinata, il ritardo massimo di rilevamento sia di 5 minuti; la sincronizzazione NTP e la scansione di recupero notifiche (sezione 6.2) seguono immediatamente il rientro.

### 3.5 Libreria client Telegram (FastBot2)

**Libreria scelta: FastBot2** (GyverLibs), al posto di UniversalTelegramBot valutata in una prima stesura del documento. Il motivo della scelta è specifico al punto 6.5: `sendMessage()` **ritorna direttamente un oggetto `fb::Result`**, non un semplice `bool`, con accesso diretto ai campi della risposta Telegram (`isError()`, `getErrorCode()`, `getError()`, e il parser interno per `parameters.retry_after`) — la classificazione degli esiti di invio richiesta da 6.5 è quindi ottenibile con l'API pubblica della libreria, senza wrapper né modifiche locali.

#### 3.5.1 Modalità di polling

FastBot2 offre tre modalità (`bot.setPollMode(...)`), selezionabili con un trade-off diretto tra reattività e blocco del loop:

| Modalità | Comportamento |
|---|---|
| `Sync` (default) | `tick()` attende la risposta al proprio interno; con rete degradata può bloccare fino al timeout configurato |
| `Async` | `tick()` non attende la risposta di polling, ma un invio richiesto **mentre è in corso un polling** forza una riconnessione bloccante di ~1 s |
| `Long` | Long polling asincrono (timeout consigliato ≥ 20 s); gli aggiornamenti arrivano non appena disponibili. Un invio richiesto durante il polling ha lo stesso costo di riconnessione di `Async` |

**Modalità adottata: `Long`**, con timeout 60 s, per la consegna più rapida dei comandi in arrivo. La libreria espone `isPolling()` per sapere se un ciclo di long-poll è in corso; l'invio da fuori dal gestore di aggiornamento (`onUpdate`) — che è esattamente il caso delle notifiche generate dagli eventi rilevati sui pin, asincrone rispetto al ciclo Telegram — **può quindi incorrere nel blocco di ~1 s per la riconnessione**, indipendentemente dalla modalità scelta.

Questo non introduce un requisito nuovo: è esattamente il tipo di blocco di rete già assunto come possibile in sezione 3.3, coperto dall'architettura ISR + coda + datazione retroattiva. Nessuna transizione sui pin viene persa per effetto di questo blocco, e il watchdog (30 s) resta ampiamente al di sopra del caso peggiore (~1 s).

#### 3.5.2 Convivenza con ArduinoJson

FastBot2 usa internamente **GSON** (dello stesso autore) per il parsing delle risposte dell'API Telegram — una dipendenza propria della libreria, non una scelta del progetto. **ArduinoJson resta la libreria usata per tutti i file del progetto** (`users.json`, `userconfig.json`, righe di `log.jsonl` e di `notif_<chat_id>.jsonl`, sezioni 4.4, 5.2, 7.2): è una decisione esplicita, non un'omissione. Le due librerie sono indipendenti e non condividono buffer né tipi, quindi la coesistenza non comporta rischi; il costo è unicamente qualche KB aggiuntivo di flash per avere due parser JSON nel firmware, ritenuto accettabile rispetto al beneficio di non dover riscrivere la logica di lettura/scrittura dei file già specificata nel documento.

**Nota sui `chat_id`**: FastBot2 rappresenta gli identificativi (`fb::ID`) internamente come stringa (buffer di 22 caratteri), costruibile esplicitamente da un intero a 64 bit (`long long`). Passare il `chat_id` come `int64_t` nativo (mai tramite un tipo a 32 bit intermedio) evita quindi qualunque troncamento anche sul lato Telegram della catena; il requisito `int64_t` di sezione 4.2 resta comunque necessario per la parte del sistema scritta con ArduinoJson (whitelist, configurazioni), dove il rischio di troncamento è reale.

---

## 4. Gestione utenti, permessi e sicurezza

### 4.1 Motivazione

Telegram non fornisce un meccanismo nativo di controllo accessi per i bot: chiunque conosca lo username del bot può scrivergli. La protezione è quindi interamente gestita lato applicazione, tramite una whitelist di `chat_id` autorizzati.

### 4.2 Whitelist

Ogni messaggio in arrivo (comando) viene verificato contro la whitelist prima di essere processato:

- Se il `chat_id` mittente **non è in whitelist**, il messaggio viene **ignorato silenziosamente** (nessuna risposta), per non rivelare l'esistenza/funzionamento del bot a chi indovina lo username.
- Se il `chat_id` **è in whitelist**, il comando viene eseguito secondo i permessi associati.

La stessa verifica si applica alle **callback query** generate dai bottoni inline (sezione 8): l'autorizzazione viene rivalutata al momento del click, mai data per acquisita dal fatto che il bottone sia visibile.

La stessa whitelist regola anche l'**invio delle notifiche**: quando un evento genera una notifica, questa viene inviata esclusivamente ai `chat_id` presenti in whitelist, mai a destinatari non autorizzati.

**Nota implementativa sui `chat_id`**: i `chat_id` di Telegram sono interi **con segno a 64 bit**. Le chat private hanno valori positivi che oggi rientrano nei 32 bit, ma gruppi e supergruppi usano valori **negativi** che li superano ampiamente (formato `-100XXXXXXXXXX`). Vanno quindi rappresentati con `int64_t` in ogni punto del sistema — struct in RAM, parsing JSON (ArduinoJson va istruito esplicitamente sul tipo a 64 bit), confronti e formattazione. Un `long` su ESP32 è a 32 bit e produrrebbe un troncamento **silenzioso**, con il risultato che un gruppo autorizzato non verrebbe mai riconosciuto.

Quando un `chat_id` viene usato per comporre un nome file (sezione 7), il segno meno va sostituito da un prefisso testuale (es. `notif_g1001234567890.jsonl`) per evitare nomi che iniziano con caratteri problematici.

### 4.3 Livelli di permesso

Per la fase attuale è previsto un unico flag booleano `admin`, senza permessi granulari:

- **Utente standard**: può consultare il registro (`/log`), verificare lo stato del sistema (`/status`), vedere e modificare le **proprie** preferenze personali (formato data, timezone, tipi di evento notificati).
- **Utente admin**: oltre a quanto sopra, può modificare le **configurazioni globali** di sistema (retention, grace period, retry interval, max retry, soglia di rete), **chiudere manualmente eventi aperti** (via bottone inline o `/closeevent`) e **gestire la whitelist stessa** (vedi 4.5).

Questa distinzione binaria è considerata sufficiente per un uso personale/familiare; lo schema di storage scelto è comunque predisposto per l'aggiunta futura di permessi più granulari senza richiedere una ristrutturazione.

### 4.4 Storage di utenti e configurazioni

| File/Storage | Contenuto | Formato |
|---|---|---|
| `users.json` (LittleFS) | Whitelist dei `chat_id` autorizzati, con flag `admin` e data di aggiunta per ciascuno | JSON, riscritto per intero (write-then-rename) ad ogni modifica |
| `userconfig.json` (LittleFS) | Preferenze per singolo utente: formato data, timezone, tipi di evento notificati | JSON indicizzato per `chat_id`, riscritto per intero ad ogni modifica |
| NVS (Preferences) | Configurazioni **globali** di sistema, versione di schema, ancora oraria, timestamp ultima rotazione | Coppie chiave-valore scalari |

Esempio indicativo di `users.json`:
```json
{
  "authorized": [
    {"chat_id": 111111111, "admin": true, "added_ts": 1755000000},
    {"chat_id": 222222222, "admin": false, "added_ts": 1755600000}
  ]
}
```

### 4.5 Popolamento iniziale e gestione operativa della whitelist

- **Onboarding iniziale**: un `chat_id` iniziale viene **definito nel file dei segreti** (sezione 4.7) in fase di setup, e diventa automaticamente il primo utente admin al primo avvio (popolando `users.json` se ancora vuoto/assente).
- **Comandi di gestione whitelist** (riservati agli admin):
  - Aggiunta di un nuovo utente autorizzato
  - Rimozione di un utente
  - Promozione/rimozione del flag admin per un utente esistente
  - Reset completo della whitelist (da usare con cautela — da valutare se richiedere una conferma esplicita data la natura distruttiva)

*(I nomi esatti dei comandi sono da definire in fase di implementazione, vedi sezione 12 e 15.)*

### 4.6 Filtro degli eventi precedenti all'aggiunta di un utente

**Proposta (non ancora confermata)**: per evitare che un nuovo utente, appena aggiunto alla whitelist, riceva un invio massivo di tutte le notifiche storiche pregresse, si usa il campo `added_ts` già presente in `users.json` come filtro: qualunque evento con timestamp di origine antecedente ad `added_ts` viene **escluso** dall'invio delle notifiche per quell'utente, sia nel flusso normale sia in fase di recupero. Il `/log` storico resta comunque interamente consultabile da chiunque sia autorizzato, indipendentemente da questa data.

### 4.7 Gestione dei segreti

Il token del bot Telegram, le credenziali WiFi e il `chat_id` di onboarding risiedono in un file **`secrets.h`** separato, incluso dallo sketch principale.

- Con Arduino IDE il file compare come **tab aggiuntivo** dello sketch se collocato nella stessa cartella, quindi resta comodamente modificabile senza toccare il sorgente principale.
- `secrets.h` è **escluso dal versionamento** (`.gitignore`). Nel repository viene versionato un `secrets.h.example` con la stessa struttura e valori segnaposto, così la compilazione su una macchina pulita fallisce con un errore chiaro invece che con un comportamento anomalo a runtime.
- I valori sono memorizzati **in chiaro** nel firmware. Questa è una scelta consapevole: la flash encryption dell'ESP32-S3 non viene utilizzata.

  **Modello di minaccia da tenere presente**: il dispositivo è installato *dentro* la centralina d'allarme. Chi ottiene accesso fisico al suo interno può dumpare la flash e recuperare il token del bot, potendo poi inviare messaggi arbitrari agli utenti (non però leggere lo storico né comandare la centralina). Il rischio è considerato accettabile dato che chi ha già aperto la centralina ha problemi più immediati da causare; se il token viene compromesso, la mitigazione è rigenerarlo da BotFather e riprogrammare il dispositivo.

---

## 5. Modello dati del registro eventi

### 5.1 Formato di storage

Il registro eventi è salvato in formato **JSON Lines** (`log.jsonl`) su **LittleFS**, con approccio **append-only**: nessuna riga esistente viene mai modificata. Questo file contiene **esclusivamente il rilevamento degli eventi** (non lo stato delle notifiche, che vive nel registro separato di sezione 7). Il registro resta **unico e condiviso** tra tutti gli utenti autorizzati.

### 5.2 Schema del record

```json
{"id": "<uuid v4 esadecimale, 32 caratteri>", "type": <enum, vedi 3.2>, "status": <enum, vedi sotto>, "ts": <epoch Unix, UTC>, "a": 1}
```

- **`id`**: UUID v4 generato tramite generatore hardware casuale dell'ESP32 (`esp_random()`), rappresentato in **esadecimale senza trattini (32 caratteri)** invece del formato testuale standard con trattini (36 caratteri) — risparmio di 4 caratteri per occorrenza, oltre a semplificare il parsing. Non richiede persistenza di un contatore in NVS. Le righe `START`/`END` di uno stesso evento con durata condividono lo stesso `id`. L'`id` non viene mai digitato manualmente dagli utenti nel flusso ordinario (vedi sezione 8: la chiusura degli eventi avviene tramite bottoni inline).
- **`type`**: valore enum numerico secondo la tabella in sezione 3.2, invece della stringa testuale (es. `0` invece di `"ALARM_GENERAL"`).
- **`status`**: valore enum numerico:

  | Valore | Significato |
  |---|---|
  | `0` | `START` |
  | `1` | `END` |
  | `2` | `INSTANT` |

- **`ts`**: epoch Unix (UTC) del momento del rilevamento, scritto immediatamente e datato retroattivamente secondo la sezione 3.3.
- **`a`** (*approximate*): flag di qualità del timestamp, vedi sezione 5.4. **Presente solo se vale `1`**; nel caso normale (orario sincronizzato via NTP) il campo è **omesso**, quindi non ha alcun costo di storage nella stragrande maggioranza delle righe.

Esempio concreto:
```json
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","type":0,"status":0,"ts":1755500000}
```

**Nota sulla manutenibilità**: la mappatura enum → significato (tabella di sezione 3.2.1) deve essere mantenuta in un unico punto nel codice (header condiviso) e mai riassegnata per valori già in uso, per non invalidare il significato delle righe già scritte nei log esistenti.

### 5.3 Formattazione dei timestamp

I timestamp sono **sempre memorizzati in epoch Unix**. La conversione in formato leggibile e nel fuso orario corretto avviene esclusivamente al momento della visualizzazione (comando `/log`) o dell'invio della notifica, secondo le preferenze **del singolo utente destinatario** (formato data e timezone).

### 5.4 Affidabilità e monotonicità dei timestamp

L'ESP32 non dispone di RTC tamponato: l'orario reale arriva unicamente da NTP, che richiede connettività. Poiché lo scenario di guasto più probabile è proprio l'assenza di rete (sezione 1), è necessario garantire un timestamp sensato **anche prima della prima sincronizzazione NTP** — altrimenti un riavvio durante un down di rete produrrebbe eventi datati 1970, con effetti a cascata su grace period, rotazione e ordinamento.

#### 5.4.1 Ancora oraria persistente (NVS)

- Mentre l'orario è valido (NTP sincronizzato), il sistema salva l'epoch corrente in NVS (chiave `last_epoch`) **ogni 10 minuti**, oltre che immediatamente dopo ogni sincronizzazione NTP riuscita.
- Al boot, prima che NTP sia disponibile, l'orario di lavoro è ricostruito come:

  ```
  ts_stimato = last_epoch + millis() / 1000
  ```

- Ogni riga scritta con un orario così ricostruito porta il flag **`"a": 1`**. Alla prima sincronizzazione NTP riuscita il flag smette di essere applicato alle righe successive; **le righe già scritte non vengono corrette a posteriori**, coerentemente con la natura append-only del log.
- **Usura NVS**: 144 scritture al giorno di un singolo valore a 64 bit. Il wear-leveling della partizione NVS aggrega centinaia di scritture per pagina prima di richiedere una cancellazione, portando l'usura effettiva a poche centinaia di cicli di erase all'anno — del tutto trascurabile rispetto alla vita utile della flash.

#### 5.4.2 Conseguenze del flag `a` sul comportamento

| Ambito | Trattamento di una riga con `a: 1` |
|---|---|
| Notifica | Il timestamp è mostrato preceduto da `~` (es. `~14:02`) e la notifica riporta sempre il prefisso di recupero, **indipendentemente dal grace period**: non essendo affidabile lo scarto temporale, non ha senso decidere in base ad esso (vedi 6.4) |
| `/log` | Stessa marcatura `~` nel rendering |
| Rotazione | Trattata come una riga normale: l'ancora garantisce comunque un valore plausibile, non un 1970 che ne provocherebbe la cancellazione immediata |

#### 5.4.3 Monotonicità garantita

Una correzione NTP all'indietro (o una stima dell'ancora superiore all'orario reale) potrebbe produrre timestamp non crescenti in un file append-only, rompendo l'ordinamento di `/log`, i calcoli di età in rotazione e la ricostruzione delle durate.

Il sistema mantiene quindi in RAM `last_written_ts`, **inizializzato al boot leggendo l'ultima riga di `log.jsonl`** (lettura all'indietro dalla fine del file, senza scansione completa), e applica un clamp prima di ogni scrittura:

```
ts_scritto = max(ts_calcolato, last_written_ts)
```

Se il clamp è intervenuto, la riga viene marcata con `a: 1`, perché il valore scritto non corrisponde più all'istante reale del rilevamento.

### 5.5 Versione di schema

La versione del formato dei dati su disco è memorizzata in NVS come intero (chiave `schema_ver`, valore iniziale `1`), e verificata ad ogni boot contro la costante compilata nel firmware:

- **Coincidenza**: avvio normale.
- **Versione su disco più vecchia**: viene eseguita la migrazione prevista per quel salto di versione; il valore in NVS viene aggiornato **solo dopo** che la migrazione è andata a buon fine.
- **Versione su disco più recente del firmware** (downgrade accidentale): il sistema **non tocca i file esistenti**, entra in modalità degradata e notifica gli admin. Un firmware vecchio che riscrive dati in un formato che non comprende è il modo più rapido per perdere lo storico.

La versione va incrementata ogni volta che cambia la struttura di `log.jsonl`, dei file di notifica, o il significato di un campo esistente. **Non** va incrementata per la semplice aggiunta di un nuovo valore enum in coda (operazione retrocompatibile per costruzione).

---

## 6. Logica di notifica, recupero e grace period

### 6.1 Flusso normale

1. Un evento viene rilevato (transizione confermata su un pin secondo la sezione 3.3, oppure generato internamente: riavvio, problema di rete) e scritto immediatamente in `log.jsonl` (sezione 5).
2. Se il `notify_policy` del tipo prevede una notifica per quel `status` (sezione 3.2.1), si tenta l'invio Telegram a tutti i `chat_id` in whitelist per cui quel tipo di evento è abilitato nelle rispettive preferenze personali (e il cui `added_ts` precede l'evento, vedi 4.6).
3. L'esito dell'invio per ciascun destinatario viene classificato secondo la sezione 6.5 e tracciato secondo l'architettura di sezione 7 (Proposta E).

**Semantica della "consegna"**: il sistema traccia esclusivamente se il messaggio è stato **accettato dalle API di Telegram**, non se l'utente lo abbia ricevuto o letto. È la garanzia corretta da inseguire: una volta che Telegram ha risposto `ok: true`, la consegna al dispositivo dell'utente è responsabilità di Telegram, che la effettua anche se il destinatario è offline in quel momento. Non esiste (né serve) alcun meccanismo di conferma di lettura.

### 6.2 Scansione di recupero

Il registro notifiche **non viene mai scansionato continuamente**. La scansione di recupero viene eseguita esclusivamente in tre occasioni:

- **Al boot** dell'Arduino (sempre, una tantum).
- **Al ripristino della connettività** dopo un evento `NETWORK_ISSUE` (sezione 3.4).
- **Allo scadere del timer di retry programmato** (vedi 6.3).

### 6.3 Retry programmato

Ogni fallimento **transitorio** di invio (sia di una notifica "nuova" sia di una notifica in fase di recupero — vedi la classificazione in 6.5) è gestito tramite un **timer non bloccante** (basato su confronto di `millis()`/tempo corrente, senza polling del file), con durata **configurabile in minuti (default: 60)**:

- Se un invio **fallisce** e il timer non è già attivo, viene **avviato** con la durata configurata.
- Se un invio **fallisce** mentre il timer è già attivo (in attesa), il timer viene **resettato** al valore pieno configurato.
- Se un invio **ha successo** (di qualunque notifica) mentre il timer è attivo, il timer **non viene semplicemente cancellato**: scatta immediatamente una scansione di recupero anticipata (6.2). L'esito di questa scansione determina lo stato finale del timer, secondo la stessa regola valida allo scadere naturale (punto successivo).
- Allo **scadere** del timer (naturale o anticipato), viene eseguita una scansione di recupero: se tutti gli invii pendenti hanno successo (o vengono marcati `ABANDONED`), il timer viene **cancellato**; se anche solo uno fallisce in modo transitorio, il timer viene **riavviato** con la durata configurata.

#### 6.3.1 Protezione contro la rientranza (`scan_in_progress`)

La regola "un successo scatena una scansione anticipata" si riferisce esclusivamente ai successi ottenuti nel **flusso normale** (6.1), mai a quelli ottenuti **all'interno** di una scansione di recupero — che altrimenti innescherebbero ricorsivamente una nuova scansione al primo invio riuscito.

Il sistema mantiene quindi un flag booleano **`scan_in_progress`**, impostato all'inizio della scansione di recupero e azzerato alla sua conclusione (anche in caso di uscita anticipata per errore). Mentre il flag è attivo:

- Nessun successo può innescare una scansione anticipata.
- Nessuna nuova scansione può essere avviata: una richiesta di scansione ricevuta in questa finestra viene semplicemente scartata (la scansione in corso la coprirà comunque, dato che rilegge lo stato completo dei pendenti).

### 6.4 Grace period

Per ogni notifica pendente individuata dal recupero, al momento dell'invio si calcola lo scarto temporale tra l'istante corrente e il `ts` dell'evento originale (in `log.jsonl`):

- Se lo scarto è **entro il grace period configurato** (default: **5 minuti**), la notifica viene inviata come **notifica normale**, senza indicazioni di ritardo.
- Se lo scarto **supera il grace period**, la notifica viene inviata con un prefisso esplicito (es. "⏪ Notifica recuperata") e il timestamp originale formattato secondo il formato e la timezone del destinatario.
- Se il timestamp originale porta il flag `a: 1` (sezione 5.4), la notifica viene **sempre** inviata con il prefisso di recupero e con il timestamp marcato `~`, indipendentemente dallo scarto calcolato.

### 6.5 Classificazione degli esiti di invio

Non tutti i fallimenti meritano un retry: alcune risposte dell'API indicano una condizione **permanente**, che nessun numero di ritentativi risolverà. Senza questa distinzione una singola chat non più raggiungibile (utente che ha bloccato il bot) manterrebbe il timer di retry armato a vita, con un tentativo HTTPS inutile ogni ora e una segnalazione ricorrente nel riepilogo periodico.

| Categoria | Condizione rilevata | Trattamento |
|---|---|---|
| **Successo** | HTTP 200 con `"ok": true` nel corpo | Notifica marcata `RESOLVED` (sezione 7) |
| **Transitorio — rete** | Fallimento DNS/TCP/TLS, timeout, WiFi disconnesso | `PENDING`, retry programmato |
| **Transitorio — server** | HTTP 5xx | `PENDING`, retry programmato |
| **Throttling** | HTTP 429 | Gestito dal rate limiter (sezione 6.6), non conta come fallimento fino all'esaurimento dei ritentativi immediati |
| **Permanente — destinatario** | HTTP 403 (`bot was blocked by the user`, `user is deactivated`), HTTP 400 (`chat not found`) | Notifica marcata `ABANDONED`, **nessun ulteriore tentativo**; segnalazione agli admin con il `chat_id` interessato |
| **Errore di sistema** | HTTP 401 (token non valido), HTTP 404 sull'endpoint | Nessun invio è possibile verso nessuno: i pendenti **restano `PENDING`**, l'errore viene registrato ed esposto in `/status`. Non si marca nulla come `ABANDONED`, perché il problema non è del destinatario |

**Limite ai tentativi transitori**: una notifica che accumula più di `max_retries` tentativi falliti (default: **24**, pari a 24 ore con l'intervallo di retry di default) viene marcata `ABANDONED` e segnalata nel riepilogo, per evitare che un pendente irrisolvibile resti protetto dalla rotazione indefinitamente.

**Nota implementativa (FastBot2)**: a differenza delle librerie che restituiscono un semplice `bool`, `bot.sendMessage(msg)` di FastBot2 (sezione 3.5) **ritorna direttamente un `fb::Result`**, che espone tutto il necessario per la classificazione senza wrapper né modifiche alla libreria:

```cpp
fb::Result r = bot.sendMessage(msg);

if (!r.isError()) {
    // Successo: "ok": true nel corpo
} else if (r.isEmpty()) {
    // Nessun corpo JSON ricevuto: fallimento di connessione (DNS/TCP/TLS/timeout)
    // -> Transitorio - rete
} else {
    // Corpo JSON con "ok": false: r.getErrorCode() e r.getError() rispecchiano
    // esattamente error_code/description restituiti da Telegram
    int code = r.getErrorCode().toInt32();
    // 403/400 -> Permanente - destinatario
    // 401/404 -> Errore di sistema
    // 429     -> Throttling; retry_after tramite il parser interno:
    uint32_t retryAfter = r._parser["parameters"]["retry_after"];
    // 5xx     -> Transitorio - server
}
```

Il campo `error_code` restituito nel corpo JSON da Telegram **coincide numericamente** con lo status HTTP della richiesta (è la stessa convenzione usata dalla Bot API), quindi la tabella sopra si applica invariata leggendo `getErrorCode()` al posto dello status HTTP. La distinzione "nessun corpo JSON" (fallimento di connessione, `isEmpty()`) rispetto a "corpo JSON con errore" (`isError()` con `error_code` valorizzato) è ciò che separa i fallimenti transitori di rete da quelli riportati esplicitamente dall'API.

### 6.6 Rate limiting

Le API Telegram impongono limiti di frequenza (indicativamente ~1 messaggio al secondo per singola chat e ~30 messaggi al secondo complessivi), oltre i quali rispondono `HTTP 429` con un campo `parameters.retry_after` che indica i secondi di attesa richiesti.

Lo scenario a rischio è esattamente quello previsto dal design: al rientro da un down prolungato il sistema invia in raffica il riepilogo degli eventi aperti, quello dei pendenti e tutte le notifiche recuperate, moltiplicati per il numero di utenti. Senza controllo di frequenza si otterrebbe un `429`, che verrebbe contato come fallimento, riarmando il timer di retry e producendo altri `429` al ciclo successivo.

**Meccanismo adottato:**

- Tutti gli invii passano da un unico punto che impone un intervallo minimo di **1100 ms tra due messaggi consecutivi**, qualunque sia il destinatario. Con il numero di utenti previsto (pochi) questo singolo vincolo copre con margine sia il limite per-chat sia quello globale, senza richiedere due contatori separati.
- L'attesa è realizzata con timer non bloccante: il loop continua a servire la coda degli interrupt (sezione 3.3) durante la pausa.
- Alla ricezione di un **429**, il valore di `retry_after` viene rispettato integralmente e l'invio viene **ritentato fino a 3 volte** senza essere conteggiato come fallimento. Solo dopo il terzo `429` consecutivo la notifica viene marcata `PENDING` e passa al normale meccanismo di retry programmato.

---

## 7. Architettura di storage delle notifiche

### 7.1 Decisione

**Architettura scelta: Proposta E — log inverso, per-chat.** Un file dedicato per ciascun utente autorizzato, es. `notif_<chat_id>.jsonl`, contenente **esclusivamente** le righe relative a invii **non andati a buon fine al primo tentativo** (nessuna riga per gli invii riusciti immediatamente). Le proposte alternative valutate e scartate sono documentate in sezione 7.3, per riferimento futuro.

Lo scopo del registro è tracciare **se il messaggio è stato inviato con successo alle API di Telegram per quell'utente**, non se l'utente lo abbia ricevuto o letto (vedi 6.1).

### 7.2 Schema del record

```json
{"id": "<uuid evento, stesso formato esadecimale di 5.2>", "status": <enum, vedi sotto>, "ts": <epoch Unix>, "state": <enum, vedi sotto>, "n": <tentativi>}
```

- **`id`**: stesso identificativo dell'evento in `log.jsonl` (sezione 5.2), per la correlazione.
- **`status`**: quale notifica si sta tracciando, enum numerico:

  | Valore | Significato |
  |---|---|
  | `0` | `NOTIFIED_INSTANT` |
  | `1` | `NOTIFIED_START` |
  | `2` | `NOTIFIED_END` |

- **`ts`**: per una riga `PENDING`, l'epoch dell'evento originale (utile per il calcolo del grace period); per una riga `RESOLVED`, l'epoch del momento dell'invio effettivo andato a buon fine; per una riga `ABANDONED`, l'epoch della rinuncia.
- **`state`**: enum numerico:

  | Valore | Significato |
  |---|---|
  | `0` | `PENDING` — invio fallito in modo transitorio, in attesa di recupero |
  | `1` | `RESOLVED` — invio successivamente riuscito |
  | `2` | `ABANDONED` — invio definitivamente rinunciato (errore permanente o `max_retries` superato, vedi 6.5) |

- **`n`**: numero di tentativi di invio falliti accumulati fino a quel momento. Presente sulle righe `PENDING` e `ABANDONED`, omesso sulle `RESOLVED`. È il campo su cui si valuta il superamento di `max_retries`.

Sequenza tipica per un singolo mancato invio poi recuperato:
```json
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","status":0,"ts":1755500000,"state":0,"n":1}
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","status":0,"ts":1755500910,"state":1}
```

**Comportamento nel caso comune** (invio riuscito al primo tentativo, atteso essere la stragrande maggioranza dei casi data l'alimentazione garantita dalla batteria della centralina): **nessuna riga viene scritta** in questo file. Il file di un utente il cui bot funziona regolarmente resta quasi sempre vuoto o minimo.

**Ricostruzione dello stato pendente**: una notifica è ancora da recuperare se esiste una riga `PENDING` per quel `(id, status)` **non seguita** da una riga `RESOLVED` o `ABANDONED` per la stessa coppia. La ricostruzione avviene leggendo il file una sola volta in streaming e mantenendo in RAM una mappa `(id, status) → stato più recente`.

**Aggiornamento del contatore tentativi**: ad ogni ulteriore fallimento **non viene aggiunta una nuova riga** per ogni tentativo; viene invece appesa una singola riga `PENDING` aggiornata con il nuovo valore di `n`, che sostituisce logicamente la precedente (l'ultima riga presente per una data coppia `(id, status)` è sempre quella valida). Questo mantiene il file compatto anche durante down prolungati, e le righe superate vengono eliminate alla prima rotazione.

**Eventi irrisolti a lungo termine**: un record `PENDING` che si avvicina a `max_retries` viene segnalato nel riepilogo periodico insieme agli eventi aperti (sezione 8), per non perderne traccia silenziosamente. Il passaggio ad `ABANDONED` è a sua volta notificato agli admin.

**Rimozione di un utente**: coerente con l'architettura per-chat, basta cancellare il file `notif_<chat_id>.jsonl` corrispondente, senza impatto sugli altri utenti.

### 7.3 Proposte alternative scartate

Le seguenti alternative sono state valutate ma **non adottate**. Restano documentate per riferimento, nel caso emergano requisiti che ne rendano preferibile una revisione della scelta.

#### Proposta A — Log positivo unico, una riga per (evento, chat)

Un unico file `notifications.jsonl`, con una riga per ogni combinazione evento/destinatario notificato con successo:
```json
{"id": "<uuid evento>", "chat_id": 111, "status": "NOTIFIED_START", "ts": <epoch invio>}
```

| Pro | Contro |
|---|---|
| Semplice, coerente con lo schema del log eventi | Scritture e righe crescono come *eventi × numero di chat* |
| Audit trail completo di ogni notifica ricevuta da ogni utente | File cresce più rapidamente, più usura flash, scansioni di recupero ripetute per ogni chat |

#### Proposta B — Log positivo unico, riga aggregata per round di invio

Una sola riga per ogni "giro" di tentativo, con l'elenco delle chat servite con successo in quel giro:
```json
{"id": "<uuid evento>", "status": "NOTIFIED_START", "ts": <epoch>, "chats": [111, 222]}
```

| Pro | Contro |
|---|---|
| Riduce le scritture (1 per round, non 1 per chat) | Scrive comunque anche nel caso banale (tutti ricevono al primo colpo) |
| Mantiene un log positivo completo | Ricostruzione più complessa (unione di insiemi tra più righe) |

#### Proposta C — Log positivo per-chat (un file per utente)

Un file separato per ciascun utente, es. `notif_111.jsonl`, `notif_222.jsonl`, contenente **tutti** gli invii riusciti (non solo i recuperati).

| Pro | Contro |
|---|---|
| Isolamento naturale: rimuovere un utente = cancellare un file | Tanti file quante le chat |
| Rotazione indipendente per utente | Scrive comunque ad ogni successo |

#### Proposta D — Log inverso (solo i mancati invii), condiviso

Come la Proposta E adottata, ma in un **unico file condiviso** tra tutti gli utenti invece che per-chat.

| Pro | Contro |
|---|---|
| Zero scritture nel caso comune, come E | Nessun isolamento per utente: rimuovere un utente richiede filtrare/riscrivere il file condiviso invece di cancellare un file dedicato |
| Un solo file da gestire | Le scansioni di recupero devono comunque filtrare per chat all'interno di un file più eterogeneo |

#### Proposta F — Watermark per utente (nessun file di notifiche)

Un solo intero per utente (l'epoch dell'ultimo evento notificato con successo), salvato in `userconfig.json`; il recupero rilegge da `log.jsonl` tutti gli eventi con `ts` successivo al watermark. Eliminerebbe completamente i file di notifica e la loro rotazione, unificando il meccanismo con il filtro `added_ts` di sezione 4.6.

| Pro | Contro |
|---|---|
| Rimuove del tutto un file, la sua rotazione e la sincronizzazione con il log eventi | Un fallimento su un evento seguito da un successo su quello dopo non è rappresentabile: o si tiene fermo il watermark (rischio di notifica duplicata) o si perde traccia del fallimento |
| Nessuna scrittura aggiuntiva: il watermark vive in un file già riscritto per altri motivi | Nessun audit di quali notifiche siano state effettivamente recuperate |

Scartata a favore della Proposta E, che rappresenta esattamente lo stato di ogni singola notifica senza ambiguità.

---

## 8. Gestione degli eventi aperti

Se un evento con durata (es. `ALARM_GENERAL`, `ALARM_INTERNAL`, `ALARM_GARAGE`, `POWER_LOSS`, `NETWORK_ISSUE`) risulta ancora privo di riga `END` al momento di un riavvio inatteso dell'Arduino:

- **Non viene chiuso automaticamente.** Resta "aperto" nel registro fino a gestione manuale.
- Al boot, se vengono rilevati eventi aperti precedenti al riavvio, il sistema invia un **messaggio di riepilogo** a tutti gli utenti autorizzati con l'elenco degli eventi ancora aperti (tipo e timestamp di inizio formattato secondo le preferenze di ciascun destinatario). Lo stesso riepilogo include anche eventuali notifiche `PENDING` prossime alla rinuncia (sezione 7.2).
- Lo stesso riepilogo viene incluso anche **ad ogni ciclo di rotazione** (vedi sezione 9), come promemoria periodico.
- Gli eventi aperti sono **sempre esclusi dalla cancellazione automatica** in fase di rotazione, indipendentemente dalla loro età.

### 8.1 Chiusura tramite bottoni inline

La chiusura manuale avviene **primariamente tramite bottoni inline** allegati al messaggio di riepilogo, per non richiedere all'utente di digitare un identificativo di 32 caratteri esadecimali da smartphone — operazione impraticabile, per giunta richiesta proprio nei momenti meno comodi.

- Il messaggio di riepilogo inviato **agli admin** include una tastiera inline (`fb::InlineKeyboard`, sezione 3.5) con un bottone per ciascun evento aperto, costruita dinamicamente in un ciclo (`addButton(label, data).newRow()` per ogni evento — il numero di eventi aperti non è fisso), etichettato in modo leggibile (es. `Chiudi: Allarme garage (14:02)`).
- Il `callback_data` del bottone ha il formato `c:<id>` — 34 byte, entro il limite di 64 byte imposto da Telegram.
- Il riepilogo inviato agli **utenti standard** è identico ma **privo di bottoni**: l'autorizzazione non è delegata alla sola invisibilità del comando.
- Alla ricezione della callback query (`u.isQuery()` nel gestore `onUpdate` di FastBot2) il sistema:
  1. Rivaluta l'autorizzazione del mittente (`u.query().from().id()` deve essere un admin in whitelist), **senza fidarsi del fatto che il bottone fosse visibile**: le callback possono essere inoltrate.
  2. Verifica che l'evento indicato (`u.query().data()`, parsando l'`id` dopo il prefisso `c:`) sia ancora aperto (protezione contro il doppio click e contro un secondo admin che ha già chiuso l'evento).
  3. Scrive la riga `END` con `ts` pari al momento del click e attiva il normale flusso di notifica per quell'`END`.
  4. Risponde con `bot.answerCallbackQuery(u.query().id(), ...)` e aggiorna la tastiera del messaggio rimuovendo il bottone consumato (`bot.editMenu(...)`, ricostruendo la tastiera senza il bottone chiuso). **Nota**: se non si risponde esplicitamente alla query, FastBot2 invia comunque una risposta vuota automatica dopo un timeout — la chiamata esplicita resta comunque preferibile per dare un riscontro testuale immediato ("Evento chiuso") invece di lasciare lo spinner fino al timeout automatico.

Resta disponibile come **fallback** il comando testuale `/closeevent <id> [timestamp]` (riservato agli admin), utile quando il messaggio di riepilogo non è più raggiungibile o quando si vuole specificare un timestamp di chiusura diverso dall'istante corrente. L'`id` completo è ottenibile da `/log`.

---

## 9. Rotazione del registro

### 9.1 Politica di retention

- Il periodo di validità del log eventi è **configurabile in settimane** (default da definire), impostabile solo da utenti admin.
- Gli eventi conclusi (con riga `END` o `INSTANT` presenti) più vecchi del periodo configurato vengono eliminati.
- Gli eventi ancora aperti (senza `END`) sono **sempre protetti** dalla cancellazione, indipendentemente dall'età.
- I file `notif_<chat_id>.jsonl` seguono la stessa politica di retention: righe `RESOLVED` e `ABANDONED` più vecchie del periodo configurato vengono rimosse in fase di rotazione; righe `PENDING` restano sempre protette (coerentemente con la protezione degli eventi aperti). Il passaggio ad `ABANDONED` previsto dalla sezione 6.5 garantisce che nessun pendente resti protetto indefinitamente.

### 9.2 Cadenza

Data la capacità di storage disponibile (16 MB) e il basso volume di eventi atteso, la rotazione avviene con **cadenza settimanale**, tramite un controllo leggero in RAM (confronto col timestamp dell'ultima rotazione, salvato in NVS), senza scansione del file per determinare se la rotazione è dovuta.

Una rotazione **anticipata** può essere innescata dalla sorveglianza dello spazio disponibile (sezione 9.4).

### 9.3 Meccanismo di esecuzione

Poiché LittleFS non supporta la cancellazione selettiva di righe, la rotazione è implementata come **riscrittura filtrata** dell'intero file (per `log.jsonl` e per ciascun `notif_<chat_id>.jsonl`).

#### 9.3.1 Algoritmo a due passate con RAM limitata

Un raggruppamento in memoria di *tutte* le righe per `id` richiederebbe RAM proporzionale alla dimensione del file — inaccettabile su un microcontrollore, e un limite che si manifesterebbe solo dopo mesi di esercizio. L'algoritmo è quindi a due passate con occupazione di memoria **fissa e nota a priori**:

1. **Passata 1 (raccolta)**: lettura in streaming del file, senza mai tenerlo in memoria. Si costruisce l'insieme degli `id` **eliminabili** — eventi che possiedono una riga `END`/`INSTANT` e il cui `ts` di riferimento è anteriore al cutoff di retention. Gli `id` sono memorizzati in forma **binaria a 16 byte** (non come 32 caratteri esadecimali), con un tetto rigido di **256 id** ≈ **4 KB di RAM**. Raggiunto il tetto, la raccolta si ferma.
2. **Passata 2 (riscrittura)**: seconda lettura in streaming, scrivendo su file temporaneo tutte le righe il cui `id` non appartiene all'insieme raccolto.
3. **Ripetizione**: se nella passata 1 il tetto dei 256 id era stato raggiunto, l'intero ciclo viene **ripetuto** sul file appena riscritto, finché una passata 1 termina senza saturare il tetto. La terminazione è garantita perché ogni ciclo rimuove almeno un evento.

Questo mantiene l'occupazione di memoria costante indipendentemente dalla dimensione del file, al prezzo di più passate solo nei casi (rari) di arretrato consistente.

#### 9.3.2 Ordine di commit

L'ordine delle operazioni finali è vincolante per la resistenza alle interruzioni di alimentazione:

1. Scrittura completa del file temporaneo.
2. `flush()` / `close()` del file temporaneo.
3. **Rinomina atomica** sopra il file originale (pattern write-then-rename; LittleFS garantisce l'atomicità del rename anche in caso di power-loss).
4. **Solo dopo** il rename riuscito, aggiornamento in NVS del timestamp dell'ultima rotazione.

Invertire i punti 3 e 4 farebbe sì che un blackout nella finestra intermedia registri come "eseguita" una rotazione che non ha modificato nulla, saltando un intero ciclo settimanale. Con l'ordine indicato, lo stesso blackout provoca al più la ripetizione di una rotazione già fatta — operazione idempotente e innocua.

Il file temporaneo residuo di una rotazione interrotta viene rilevato ed eliminato al boot successivo.

### 9.4 Gestione dello spazio e degli errori di filesystem

Il filesystem è un punto di guasto silenzioso: una scrittura fallita per spazio esaurito o per corruzione farebbe perdere eventi senza che nessuno se ne accorga.

**Sorveglianza dello spazio.** L'occupazione (`LittleFS.usedBytes()` / `totalBytes()`) viene verificata al boot, dopo ogni rotazione e periodicamente:

| Soglia | Comportamento |
|---|---|
| < 80% | Funzionamento normale |
| ≥ 80% | **Rotazione anticipata** immediata, fuori dalla cadenza settimanale, e segnalazione agli admin |
| ≥ 95% | **Modalità degradata**: notifica agli admin; il log eventi continua ad avere priorità di scrittura, mentre le scritture non essenziali (righe di notifica `PENDING`) vengono sospese. Perdere la traccia di un tentativo di notifica è preferibile a perdere il rilevamento dell'evento |

**Errori di scrittura.** Ogni operazione di scrittura verifica sia l'esito di `open()` sia il numero di byte effettivamente scritti da `write()`/`println()` (una scrittura parziale su LittleFS non solleva eccezioni: restituisce semplicemente un conteggio inferiore).

- In caso di fallimento, l'operazione viene ritentata una volta; se fallisce di nuovo, un **contatore di errori di filesystem** viene incrementato.
- Il contatore è esposto in `/status` e il primo errore genera una notifica agli admin.
- Un fallimento di scrittura **non blocca** l'invio della notifica Telegram corrispondente: la notifica in tempo reale ha priorità sulla persistenza dello storico.
- Se `LittleFS.begin()` fallisce al boot, il sistema tenta un solo `format()` **esclusivamente se il filesystem risulta non montabile** (mai come reazione a un errore su un FS montato correttamente), e notifica l'accaduto agli admin: la perdita dello storico deve sempre essere un evento visibile, mai silenzioso.

---

## 10. Gestione timezone e ora legale

### 10.1 Fusi orari preimpostati

L'utente sceglie il proprio fuso orario da un **set predefinito** di opzioni comuni (es. Europe/Rome, Europe/London, UTC, ecc.), evitando di dover inserire manualmente stringhe tecniche. Ogni opzione è internamente mappata a una **stringa TZ in formato POSIX**, ad esempio per l'Italia:

```
CET-1CEST,M3.5.0,M10.5.0/3
```

### 10.2 Gestione automatica dell'ora legale (DST)

Il formato POSIX della stringa TZ codifica sia l'offset standard sia la regola di passaggio ora legale/solare. Impostando questa stringa nell'ambiente del microcontrollore (`setenv("TZ", ...)` + `tzset()`, o l'equivalente `configTzTime()`), tutte le conversioni da epoch a data/ora locale gestiscono **automaticamente** il cambio ora legale, senza calcoli manuali né servizi esterni.

### 10.3 Timezone per utente

Il fuso orario è una **preferenza personale** salvata in `userconfig.json` (sezione 4.4): ogni utente imposta il proprio fuso indipendentemente dagli altri. Poiché la variabile d'ambiente `TZ` è globale al processo, la formattazione per un destinatario specifico richiede di impostare `TZ` immediatamente prima della conversione e di ripristinarla dopo (o di concentrare tutte le formattazioni per destinatario in un unico punto del codice, serializzate).

---

## 11. Configurazione

### 11.1 Configurazioni globali (NVS, solo admin)

| Parametro | Default | Comando |
|---|---|---|
| Periodo di validità log eventi e notifiche | Da definire | `/setretention <settimane>` |
| Grace period recupero notifiche | 5 minuti | `/setgraceperiod <minuti>` |
| Intervallo retry programmato | 60 minuti | `/setretryinterval <minuti>` |
| Numero massimo di tentativi prima della rinuncia | 24 | `/setmaxretries <n>` |
| Soglia di durata per generare `NETWORK_ISSUE` | 120 secondi | `/setnetthreshold <secondi>` |

Chiavi NVS di servizio, non modificabili da comando: `schema_ver` (5.5), `last_epoch` (5.4.1), timestamp ultima rotazione (9.3.2).

### 11.2 Configurazioni per utente (`userconfig.json`, ogni utente sulle proprie)

| Parametro | Default | Comando |
|---|---|---|
| Formato data/ora | ISO 8601 | `/setdateformat <formato>` |
| Timezone | Da definire (es. UTC) | `/settimezone <preset>` |
| Tipi di evento notificati | Tutti abilitati | `/notify <tipo_evento> on\|off` |

**Nota importante**: la whitelist personale dei tipi di evento notificati filtra solo l'**invio delle notifiche a quello specifico utente**, non la **scrittura nel log eventi**. Tutti gli eventi vengono sempre registrati nello storico condiviso, indipendentemente dalle preferenze di notifica di ciascun utente. (Per disattivare invece un tipo a livello di sistema, si usa il flag `enabled` della tabella di sezione 3.2.1.)

---

## 12. Comandi Telegram previsti

| Comando | Permesso richiesto | Funzione |
|---|---|---|
| `/log [n]` | Utente autorizzato | Mostra gli ultimi n **eventi** (non righe) dal registro, aggregati e formattati secondo le preferenze del richiedente — vedi 12.1 |
| `/status` | Utente autorizzato | Stato corrente del sistema — vedi 12.2 |
| `/config` | Utente autorizzato | Mostra la propria configurazione personale (e, se admin, anche quella globale) |
| `/setdateformat <formato>` | Utente autorizzato | Imposta il proprio formato di visualizzazione data/ora |
| `/settimezone <preset>` | Utente autorizzato | Imposta il proprio fuso orario da un set predefinito |
| `/notify <tipo_evento> on\|off` | Utente autorizzato | Abilita/disabilita per sé la notifica per una tipologia di evento |
| `/setretention <settimane>` | Admin | Imposta il periodo di validità globale del log in settimane |
| `/setgraceperiod <minuti>` | Admin | Imposta il grace period globale per il recupero notifiche |
| `/setretryinterval <minuti>` | Admin | Imposta l'intervallo globale del retry programmato |
| `/setmaxretries <n>` | Admin | Imposta il numero di tentativi oltre il quale una notifica è abbandonata |
| `/setnetthreshold <secondi>` | Admin | Imposta la durata minima di un down di connettività perché generi un evento |
| `/closeevent <id> [timestamp]` | Admin | Chiude manualmente un evento rimasto aperto (fallback testuale dei bottoni inline, vedi 8.1) |
| `/adduser <chat_id>` | Admin | Aggiunge un nuovo `chat_id` alla whitelist |
| `/removeuser <chat_id>` | Admin | Rimuove un `chat_id` dalla whitelist |
| `/promoteuser <chat_id>` | Admin | Modifica il flag `admin` di un utente esistente andando a promuovere ad `Admin` |
| `/resetusers <chat_id>` | Admin | Svuota la whitelist riportardola ai valori di default (operazione distruttiva, da proteggere con conferma) |

### 12.1 Rendering di `/log`

`/log` presenta **eventi aggregati**, non le singole righe del file: le coppie `START`/`END` con lo stesso `id` sono unite in una riga sola con la durata calcolata.

```
Allarme garage      21/08 14:02 → 14:07  (5m)
Mancanza rete 230V  20/08 03:11 → 03:44  (33m)
Riavvio             19/08 22:07
Allarme interno     19/08 08:30 → APERTO
```

**Implementazione**: il file viene letto una sola volta in streaming, mantenendo in RAM un **ring buffer degli ultimi N eventi aggregati** (non delle righe). L'occupazione di memoria dipende quindi da `n` richiesto — con un tetto massimo imposto — e non dalla dimensione del file. La riga `END` di un evento aggiorna la voce corrispondente già presente nel buffer; gli eventi ancora aperti sono resi esplicitamente come `APERTO`. I timestamp con flag `a: 1` (sezione 5.4) sono preceduti da `~`.

### 12.2 Contenuto di `/status`

`/status` è l'unico strumento di verifica dello stato del sistema, data l'assenza di un osservatore esterno (sezione 1.1). Deve riportare:

- Uptime dall'ultimo boot e causa dell'ultimo riavvio (`esp_reset_reason()`)
- Stato corrente di ciascun pin monitorato, con la relativa etichetta di tipo
- Elenco degli eventi attualmente aperti
- Stato WiFi: connesso/disconnesso, SSID, RSSI, tentativo di backoff corrente
- Ultima sincronizzazione NTP riuscita e validità dell'orario corrente (esatto / stimato da ancora)
- Numero di notifiche `PENDING` e `ABANDONED` per utente; stato del timer di retry
- Spazio LittleFS usato/totale, contatore errori di filesystem, eventuale modalità degradata
- Contatore di overflow della coda interrupt (sezione 3.3)
- Ultimo errore di sistema registrato (es. token non valido, sezione 6.5)
- Data dell'ultima rotazione eseguita

---

## 13. Considerazioni di robustezza

- **Isolamento galvanico**: obbligatorio in caso di interfacciamento con uscita open collector, per proteggere sia l'Arduino sia la centralina.
- **Perdita di transizioni durante l'I/O di rete**: risolta strutturalmente da ISR + coda + datazione retroattiva (sezione 3.3); nessuna transizione può essere persa a causa di un blocco del loop.
- **Watchdog**: il Task WDT è abilitato sul loop applicativo (timeout 30 s, superiore ai timeout di rete di 10 s). Un blocco genuino provoca un riavvio, che genera a sua volta un evento `REBOOT` notificato — rendendo visibile un guasto che altrimenti sarebbe silenzioso.
- **Sincronizzazione oraria**: l'ESP32 non dispone di RTC con batteria tampone; l'orario viene ottenuto via NTP alla connessione e periodicamente, in UTC. In assenza di NTP il sistema usa l'ancora oraria persistita in NVS e marca i timestamp come approssimati (sezione 5.4). La conversione in ora locale (con gestione automatica DST) avviene solo in fase di presentazione, secondo il fuso di ciascun utente.
- **Monotonicità dei timestamp**: garantita da clamp sull'ultimo valore scritto (sezione 5.4.3), presupposto necessario per rotazione, ordinamento e calcolo delle durate.
- **Alimentazione**: garantita dalla batteria tampone della centralina; i riavvii per interruzione elettrica sono previsti come rari, a differenza delle interruzioni di sola connettività di rete.
- **Singolo punto di guasto (connettività)**: se la connessione di rete cade, le notifiche in tempo reale non sono possibili; il meccanismo di recupero (grace period + retry programmato) mitiga la perdita di notifiche, ma non elimina il ritardo nella loro consegna oltre le soglie configurate.
- **Guasto totale non rilevabile automaticamente**: per scelta esplicita (sezione 1.1) non esiste heartbeat né osservatore esterno. Un guasto hardware totale (alimentazione, dispositivo bruciato) non genera alcuna segnalazione: la verifica è a carico dell'utente tramite `/status`. È il limite consapevolmente accettato dell'architettura autonoma.
- **Integrità dei dati**: il pattern write-then-rename per la rotazione e per i file di configurazione/whitelist, unito all'approccio append-only per la scrittura ordinaria del log, minimizza il rischio di corruzione in caso di interruzione di alimentazione. L'ordine di commit della rotazione (sezione 9.3.2) è vincolante.
- **Spazio ed errori di filesystem**: sorvegliati attivamente con soglie e modalità degradata (sezione 9.4); nessun fallimento di scrittura resta silenzioso.
- **Fallimenti permanenti di invio**: distinti dai transitori tramite la classificazione delle risposte API (sezione 6.5), per evitare retry perpetui verso destinatari irraggiungibili.
- **Rate limiting**: gli invii sono serializzati con intervallo minimo e il `429` è gestito rispettando `retry_after` (sezione 6.6), così una raffica di recupero non si trasforma in un ciclo di errori.
- **Sicurezza dei trasporti**: la connessione TLS verso `api.telegram.org` usa `setInsecure()`, senza validazione del certificato del server. Scelta consapevole: evita il guasto silenzioso e difficilmente diagnosticabile che si verifica quando un root CA fissato nel firmware scade o viene ruotato da Telegram, situazione in cui il bot smetterebbe di funzionare **proprio senza poter notificare il problema**. L'esposizione residua è a un attacco man-in-the-middle sulla rete locale, considerato fuori dal modello di minaccia di un impianto domestico.
- **Sicurezza degli accessi**: nessuna risposta viene inviata a `chat_id` non presenti in whitelist. Le notifiche sono inviate esclusivamente ai `chat_id` autorizzati, e solo per eventi successivi alla loro aggiunta (vedi 4.6). L'autorizzazione è rivalutata anche sulle callback query dei bottoni inline (sezione 8.1).
- **Segreti**: token e credenziali in `secrets.h` non versionato, in chiaro nella flash; modello di minaccia e mitigazione documentati in sezione 4.7.
- **Usura della flash**: il volume di scrittura atteso per il log eventi (eventi rari) e per il registro notifiche (scritture solo sui fallimenti, con la Proposta E adottata) è ampiamente compatibile con la durata della memoria flash interna. L'ancora oraria in NVS aggiunge 144 scritture al giorno di un singolo valore scalare, assorbite dal wear-leveling (sezione 5.4.1).
- **Compattezza dello storage**: l'uso di enum numerici per `type`/`status`/`state` e di UUID in formato esadecimale compatto (32 caratteri) riduce la dimensione media di ogni riga. Si tenga però presente che, con 16 MB disponibili e il volume di eventi atteso, **lo storage non è un vincolo di progetto**: le decisioni future vanno prese privilegiando leggibilità, diagnosticabilità e usabilità rispetto al risparmio di byte.

---

## 14. Decisioni di design registrate

| Argomento | Decisione |
|---|---|
| Dipendenze esterne | Nessuna: dispositivo autonomo, comunicazione diretta con Telegram, per accorciare la catena di guasto |
| Modello di concorrenza | ISR `IRAM_ATTR` + coda FreeRTOS + debounce e datazione retroattiva nel loop; accesso a LittleFS solo dal loop (nessun mutex necessario) |
| Struttura del log eventi | JSON Lines, append-only, contiene solo il rilevamento (`START`/`END`/`INSTANT`), non le notifiche |
| Eventi istantanei (es. REBOOT) | Valore enum dedicato per `INSTANT` |
| Eventi di rete | Tipo unico `NETWORK_ISSUE` con `START`/`END` nel log, ma **notifica del solo `END`** con durata del down nel testo |
| Soglia problema di rete | Assenza di connettività verso le API Telegram (non solo WiFi) per più di 120 s (configurabile) |
| Riconnessione WiFi | Backoff esponenziale 5→300 s non bloccante, reset alla riconnessione |
| Tipologie di allarme | Zone distinte come tipi separati: `ALARM_GENERAL`, `ALARM_INTERNAL`, `ALARM_GARAGE`, ciascuno con proprio pin/PGM dedicato |
| Sovrapposizione `ALARM_GENERAL`/zone | Duplicazione accettata; mitigata da abilitazione per tipo, sia per utente (`/notify`) sia globale (flag `enabled`) |
| Interruzione di corrente | Tipo `POWER_LOSS`, evento con durata (`START`/`END`), rilevato tramite PGM "guasto rete" della centralina |
| Ottimizzazione storage | `type`, `status` (e `state` per le notifiche) come enum numerici invece di stringhe testuali; lo storage non è comunque un vincolo di progetto |
| Formato id evento | UUID v4 in esadecimale senza trattini, 32 caratteri (invariato) |
| Chiusura eventi aperti | **Bottoni inline** (FastBot2 `InlineKeyboard`, costruita dinamicamente) nel messaggio di riepilogo agli admin (`callback_data` `c:<id>`), con `/closeevent` come fallback testuale |
| Timestamp senza NTP | Ancora oraria in NVS salvata ogni 10 min; ricostruzione `last_epoch + millis()`; flag `a: 1` sulle righe approssimate |
| Monotonicità timestamp | Clamp su `last_written_ts`, inizializzato al boot dall'ultima riga del log |
| Versione di schema | Intero in NVS (`schema_ver`), verificato al boot; downgrade → modalità degradata senza toccare i dati |
| Storage delle notifiche | **Proposta E adottata**: log inverso, un file per chat (`notif_<chat_id>.jsonl`); proposte A-D e F scartate ma documentate in 7.3 |
| Semantica del tracciamento notifiche | Traccia l'accettazione da parte delle API Telegram, non la ricezione o lettura da parte dell'utente |
| Client Telegram | **FastBot2** (non UniversalTelegramBot): `sendMessage()` ritorna un `fb::Result` con accesso diretto a `error_code`/`description`/`retry_after`, nessun wrapper necessario per la classificazione di sezione 6.5 |
| Modalità di polling Telegram | `Long` (long polling asincrono, timeout 60 s); invio fuori dal gestore `onUpdate` può incorrere in ~1 s di blocco per riconnessione, coperto dall'architettura ISR + coda di 3.3 |
| Libreria JSON | ArduinoJson per tutti i file del progetto (invariato); GSON usata solo internamente da FastBot2 per il parsing delle risposte Telegram — due librerie indipendenti, coesistenza per scelta |
| Esiti di invio | Classificati in successo / transitorio / throttling / permanente-destinatario / errore di sistema; retry solo sui transitori; ottenuti da `fb::Result` senza wrapper |
| Stato terminale delle notifiche | `ABANDONED` su errore permanente o superamento di `max_retries` (default 24), con contatore `n` nel record |
| Rate limiting | Intervallo minimo di 1100 ms tra invii; `429` rispettato con `retry_after` e fino a 3 ritentativi immediati |
| Notifiche recuperate entro il grace period (5 min default) | Inviate come notifiche normali, senza prefisso di "recupero" |
| Notifiche recuperate oltre il grace period | Inviate con prefisso esplicito e timestamp originale; sempre con prefisso se il timestamp è approssimato |
| Recupero non riuscito | Timer di retry programmato (default 60 min): reset su fallimento transitorio; successo di un invio nel flusso normale scatena una scansione anticipata, protetta da `scan_in_progress` contro la rientranza |
| Eventi aperti dopo riavvio | Non chiusi automaticamente, segnalati via Telegram, chiudibili con bottone inline (solo admin) |
| Eventi/notifiche pendenti oltre il periodo di retention | Sempre esclusi dalla cancellazione automatica |
| Cadenza rotazione | Settimanale, unità di retention in settimane, applicata sia al log eventi sia ai file notifiche per-chat |
| Rotazione: RAM | Due passate con insieme di id limitato a 256 (16 byte ciascuno, ~4 KB), ripetute finché necessario |
| Rotazione: commit | Rename atomico prima, aggiornamento NVS dopo; file temporanei residui puliti al boot |
| Filesystem pieno / errori | Soglie 80% (rotazione anticipata) e 95% (modalità degradata); verifica dei byte scritti; contatore errori in `/status` |
| Generazione id evento | UUID casuale via hardware RNG, nessun contatore NVS necessario |
| Controllo accessi | Whitelist di `chat_id` in `users.json`, nessuna risposta a mittenti non autorizzati, rivalutata sulle callback query |
| Tipo dei `chat_id` | `int64_t` ovunque (gruppi/supergruppi eccedono i 32 bit) |
| Permessi | Singolo flag booleano `admin` (no permessi granulari per ora) |
| Onboarding iniziale | `chat_id` iniziale in `secrets.h`, promosso automaticamente ad admin al primo avvio |
| Gestione whitelist | Comandi dedicati per aggiungere/rimuovere/promuovere/resettare (nomi da definire) |
| Segreti | File `secrets.h` separato, in chiaro, non versionato (con `secrets.h.example` versionato) |
| TLS | `setInsecure()`, per evitare il guasto silenzioso da rotazione del certificato |
| Monitoraggio dello stato | Nessun heartbeat automatico; verifica manuale tramite `/status` |
| Configurazioni globali | NVS (retention, grace period, retry interval, max retries, soglia di rete) |
| Configurazioni per utente | File JSON su LittleFS (`userconfig.json`): formato data, timezone, tipi evento notificati |
| Rendering `/log` | Eventi aggregati con durata (`14:02 → 14:07, 5m`), ring buffer degli ultimi N eventi |
| Timezone | Set di preset predefiniti mappati a stringhe TZ POSIX, gestione DST automatica, preferenza per singolo utente |
| Alimentazione | Fornita dalla centralina (con batteria tampone): riavvii per blackout rari, interruzioni di rete più probabili |

---

## 15. Prossimi passi

- Definire i nomi esatti e la sintassi dei comandi di gestione whitelist (aggiunta, rimozione, promozione, reset) e il meccanismo di conferma per il reset.
- Confermare (o rivedere) il meccanismo `added_ts` per il filtro degli eventi precedenti all'aggiunta di un utente (sezione 4.6), incluso il comportamento per un evento il cui `START` precede `added_ts` ma il cui `END` lo segue.
- Definire il valore di default per il periodo di retention del log eventi e notifiche.
- Definire la lista dei preset di timezone da offrire e le relative stringhe POSIX.
- Verificare che i pin scelti sul Nano ESP32 non siano strapping pin dell'ESP32-S3 (un livello LOW imposto da un optoisolatore al boot potrebbe impedire l'avvio) e valutare un pull-up esterno più robusto di quello interno per tratte di cablaggio lunghe verso la centralina.
- Valutare l'**aggregazione delle notifiche recuperate** in un unico messaggio riassuntivo quando superano una certa soglia numerica (es. *"⏪ 7 eventi durante l'assenza di rete tra le 14:02 e le 15:30"*), come ulteriore mitigazione del rate limiting e miglioramento della leggibilità.
- Valutare (facoltativo, discusso separatamente) l'introduzione di comandi per inserire/disinserire l'allarme da remoto — richiede verifica di supporto hardware sulla centralina e un'attenta analisi di sicurezza aggiuntiva prima di essere formalizzato nel documento.
- Implementazione dello sketch Arduino completo.
- Test di interfacciamento con il modello specifico di centralina Bentel in uso.
