# Sistema di Monitoraggio Allarme Bentel con Notifiche Telegram
## Documento di Design, Requisiti e Specifiche Tecniche

**Versione:** 0.8
**Data:** Agosto 2026
**Piattaforma target:** Arduino Nano ESP32

---

## 1. Obiettivo del sistema

Realizzare un sistema che monitori lo stato di una centralina d'allarme Bentel tramite Arduino Nano ESP32, invii notifiche in tempo reale su Telegram all'apertura/chiusura di eventi (allarme, riavvio, problemi di rete, interruzione di corrente, ecc.), e mantenga un registro storico persistente e consultabile degli eventi, con gestione robusta di interruzioni di connettività e di alimentazione, accesso limitato a utenti autorizzati e preferenze configurabili per singolo utente.

**Nota sull'alimentazione**: l'Arduino sarà alimentato dalla centralina d'allarme, che dispone di batteria tampone in caso di interruzione elettrica. I riavvii per mancanza di corrente sono quindi previsti come **eventi rari**; le interruzioni di **sola connettività di rete** (router/ISP, non necessariamente su batteria) restano invece il caso di guasto più plausibile e frequente, e sono quelle per cui il sistema di recupero notifiche è principalmente pensato.

---

## 2. Requisiti hardware

### 2.1 Lettura dello stato allarme

La centralina Bentel espone uscite PGM (programmabili) configurabili come indicatori di stato. Sono supportate due modalità di interfacciamento, applicabili a ciascuna uscita utilizzata:

- **Uscita open collector verso negativo**: richiede un **optoisolatore** (es. PC817) tra la centralina e l'ESP32 per garantire isolamento galvanico. Il pin di lettura va configurato in `INPUT_PULLUP`; l'allarme attivo corrisponde a un livello LOW.
- **Uscita a relè (contatto pulito NA/NC)**: collegamento diretto ai pin dell'ESP32, senza necessità di isolamento aggiuntivo, dato che il contatto è meccanicamente isolato dal circuito della centralina. Anche qui si utilizza `INPUT_PULLUP`, con logica invertibile a seconda che si usi il contatto NA o NC.

La scelta tra le due modalità dipende dal modello specifico di centralina e dalla sua configurazione in fase di programmazione installatore (da verificare sul manuale).

**Nota — più tipologie di evento, più input fisici**: con l'introduzione di più tipologie di evento legate a contatti distinti sulla centralina (allarme interno, allarme garage, mancanza rete), è probabile che servano **più uscite PGM dedicate** sulla centralina (una per zona/condizione da monitorare separatamente), e di conseguenza **un pin di lettura ESP32 per ciascuna** (con relativo optoisolatore se l'uscita è open collector). Il numero di pin digitali disponibili sul Nano ESP32 e la disponibilità di uscite PGM configurabili sulla centralina vanno verificati in base al numero finale di tipologie da monitorare. Per "interruzione di corrente" in particolare, molte centraline Bentel espongono una PGM dedicata a "guasto rete / 230V mancante", attiva mentre il sistema resta alimentato dalla batteria tampone.

### 2.2 Debounce

È necessario un meccanismo di anti-rimbalzo software (soglia consigliata: 300 ms) per evitare falsi trigger sulle transizioni di stato del segnale.

---

## 3. Architettura software

### 3.1 Componenti principali

| Componente | Responsabilità |
|---|---|
| Lettura pin allarme | Rilevamento stato con debounce |
| Client Telegram (UniversalTelegramBot) | Invio notifiche, ricezione comandi |
| Sincronizzazione NTP + gestione timezone | Ottenimento timestamp reali (epoch Unix, UTC) e conversione in ora locale con gestione automatica dell'ora legale |
| Registro eventi (LittleFS, `log.jsonl`) | Persistenza storico eventi (solo rilevamento, non notifiche) |
| Registro notifiche per utente (LittleFS) | Tracciamento degli invii mancati/da recuperare, un file per chat (architettura decisa, vedi sezione 7) |
| Gestione utenti e permessi (whitelist) | Autorizzazione dei `chat_id`, distinzione utente standard/admin, filtro destinatari delle notifiche |
| Gestione configurazione globale (NVS) | Impostazioni di sistema persistenti, modificabili solo da utenti admin |
| Gestione configurazione per utente (LittleFS) | Preferenze individuali (formato data, timezone, tipi di evento notificati) |
| Motore di recupero notifiche | Rilevamento e reinvio notifiche non consegnate, con grace period e retry programmato |
| Motore di rotazione | Pulizia periodica del registro eventi e del registro notifiche |

### 3.2 Tipologie di evento

Il sistema deve supportare più tipologie di evento, estensibili in futuro. Ogni tipologia è mappata a un valore enum numerico per l'ottimizzazione dello storage (vedi sezione 5.2):

| Valore enum `type` | Tipologia | Natura |
|---|---|---|
| `0` | `ALARM_GENERAL` — allarme generale | Con durata (`START`/`END`) |
| `1` | `ALARM_INTERNAL` — allarme interno | Con durata (`START`/`END`) |
| `2` | `ALARM_GARAGE` — allarme garage | Con durata (`START`/`END`) |
| `3` | `POWER_LOSS` — interruzione di corrente (mancanza rete 230V) | Con durata (`START`/`END`) |
| `4` | `REBOOT` — riavvio dell'Arduino | Istantaneo (`INSTANT`) |
| `5` | `NETWORK_ISSUE` — problema di connettività di rete | Con durata (`START`/`END`) |

*(altre tipologie aggiungibili in coda alla enumerazione, senza rompere la compatibilità con i log esistenti — non riutilizzare/rinumerare valori già assegnati)*

---

## 4. Gestione utenti, permessi e sicurezza

### 4.1 Motivazione

Telegram non fornisce un meccanismo nativo di controllo accessi per i bot: chiunque conosca lo username del bot può scrivergli. La protezione è quindi interamente gestita lato applicazione, tramite una whitelist di `chat_id` autorizzati.

### 4.2 Whitelist

Ogni messaggio in arrivo (comando) viene verificato contro la whitelist prima di essere processato:
- Se il `chat_id` mittente **non è in whitelist**, il messaggio viene **ignorato silenziosamente** (nessuna risposta), per non rivelare l'esistenza/funzionamento del bot a chi indovina lo username.
- Se il `chat_id` **è in whitelist**, il comando viene eseguito secondo i permessi associati.

La stessa whitelist regola anche l'**invio delle notifiche**: quando un evento genera una notifica, questa viene inviata esclusivamente ai `chat_id` presenti in whitelist, mai a destinatari non autorizzati.

### 4.3 Livelli di permesso

Per la fase attuale è previsto un unico flag booleano `admin`, senza permessi granulari:
- **Utente standard**: può consultare il registro (`/log`), vedere e modificare le **proprie** preferenze personali (formato data, timezone, tipi di evento notificati).
- **Utente admin**: oltre a quanto sopra, può modificare le **configurazioni globali** di sistema (retention, grace period, retry interval), **chiudere manualmente eventi aperti** (`/closeevent`) e **gestire la whitelist stessa** (vedi 4.5).

Questa distinzione binaria è considerata sufficiente per un uso personale/familiare; lo schema di storage scelto è comunque predisposto per l'aggiunta futura di permessi più granulari senza richiedere una ristrutturazione.

### 4.4 Storage di utenti e configurazioni

| File/Storage | Contenuto | Formato |
|---|---|---|
| `users.json` (LittleFS) | Whitelist dei `chat_id` autorizzati, con flag `admin` e data di aggiunta per ciascuno | JSON, riscritto per intero (write-then-rename) ad ogni modifica |
| `userconfig.json` (LittleFS) | Preferenze per singolo utente: formato data, timezone, tipi di evento notificati | JSON indicizzato per `chat_id`, riscritto per intero ad ogni modifica |
| NVS (Preferences) | Configurazioni **globali** di sistema: retention, grace period, retry interval | Coppie chiave-valore scalari |

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

- **Onboarding iniziale**: un `chat_id` iniziale viene **flashato/hardcoded nel firmware** in fase di setup, e diventa automaticamente il primo utente admin al primo avvio (popolando `users.json` se ancora vuoto/assente).
- **Comandi di gestione whitelist** (riservati agli admin):
  - Aggiunta di un nuovo utente autorizzato
  - Rimozione di un utente
  - Promozione/rimozione del flag admin per un utente esistente
  - Reset completo della whitelist (da usare con cautela — da valutare se richiedere una conferma esplicita data la natura distruttiva)

*(I nomi esatti dei comandi sono da definire in fase di implementazione, vedi sezione 12 e 15.)*

### 4.6 Filtro degli eventi precedenti all'aggiunta di un utente

**Proposta (non ancora confermata)**: per evitare che un nuovo utente, appena aggiunto alla whitelist, riceva un invio massivo di tutte le notifiche storiche pregresse, si usa il campo `added_ts` già presente in `users.json` come filtro: qualunque evento con timestamp di origine antecedente ad `added_ts` viene **escluso** dall'invio delle notifiche per quell'utente, sia nel flusso normale sia in fase di recupero. Il `/log` storico resta comunque interamente consultabile da chiunque sia autorizzato, indipendentemente da questa data.

---

## 5. Modello dati del registro eventi

### 5.1 Formato di storage

Il registro eventi è salvato in formato **JSON Lines** (`log.jsonl`) su **LittleFS**, con approccio **append-only**: nessuna riga esistente viene mai modificata. Questo file contiene **esclusivamente il rilevamento degli eventi** (non lo stato delle notifiche, che vive nel registro separato di sezione 7). Il registro resta **unico e condiviso** tra tutti gli utenti autorizzati.

### 5.2 Schema del record (ottimizzato)

```json
{"id": "<uuid v4 esadecimale, 32 caratteri>", "type": <enum, vedi 3.2>, "status": <enum, vedi sotto>, "ts": <epoch Unix, UTC>}
```

- **`id`**: UUID v4 generato tramite generatore hardware casuale dell'ESP32 (`esp_random()`), rappresentato in **esadecimale senza trattini (32 caratteri)** invece del formato testuale standard con trattini (36 caratteri) — risparmio di 4 caratteri per occorrenza, oltre a semplificare il parsing. Non richiede persistenza di un contatore in NVS. Le righe `START`/`END` di uno stesso evento con durata condividono lo stesso `id`.
- **`type`**: valore enum numerico secondo la tabella in sezione 3.2, invece della stringa testuale (es. `0` invece di `"ALARM_GENERAL"`).
- **`status`**: valore enum numerico:

  | Valore | Significato |
  |---|---|
  | `0` | `START` |
  | `1` | `END` |
  | `2` | `INSTANT` |

- **`ts`**: epoch Unix (UTC) del momento del rilevamento, scritto immediatamente.

Esempio concreto:
```json
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","type":0,"status":0,"ts":1755500000}
```

**Nota sulla manutenibilità**: la mappatura enum → significato (sezione 3.2 e tabella sopra) deve essere mantenuta in un unico punto nel codice (es. header condiviso) e mai riassegnata per valori già in uso, per non invalidare il significato delle righe già scritte nei log esistenti.

### 5.3 Formattazione dei timestamp

I timestamp sono **sempre memorizzati in epoch Unix**. La conversione in formato leggibile e nel fuso orario corretto avviene esclusivamente al momento della visualizzazione (comando `/log`) o dell'invio della notifica, secondo le preferenze **del singolo utente destinatario** (formato data e timezone).

---

## 6. Logica di notifica, recupero e grace period

### 6.1 Flusso normale

1. Un evento viene rilevato (cambio di stato sul pin, riavvio, problema di rete, interruzione di corrente, ecc.) e scritto immediatamente in `log.jsonl` (sezione 5).
2. Si tenta l'invio della notifica Telegram a tutti i `chat_id` in whitelist per cui quel tipo di evento è abilitato nelle rispettive preferenze personali (e il cui `added_ts` precede l'evento, vedi 4.6).
3. L'esito dell'invio (successo/fallimento) per ciascun destinatario viene tracciato secondo l'architettura di sezione 7 (Proposta E).

### 6.2 Scansione di recupero

Il registro notifiche **non viene mai scansionato continuamente**. La scansione di recupero viene eseguita esclusivamente in due occasioni:

- **Al boot** dell'Arduino (sempre, una tantum).
- **Allo scadere del timer di retry programmato** (vedi 6.3).

### 6.3 Retry programmato

Ogni fallimento di invio (sia di una notifica "nuova" sia di una notifica in fase di recupero) è gestito tramite un **timer non bloccante** (basato su confronto di `millis()`/tempo corrente, senza polling del file), con durata **configurabile in minuti (default: 60)**:

- Se un invio **fallisce** e il timer non è già attivo, viene **avviato** con la durata configurata.
- Se un invio **fallisce** mentre il timer è già attivo (in attesa), il timer viene **resettato** al valore pieno configurato.
- Se un invio **ha successo** (di qualunque notifica) mentre il timer è attivo, il timer **non viene semplicemente cancellato**: scatta immediatamente una scansione di recupero anticipata (6.2). L'esito di questa scansione determina lo stato finale del timer, secondo la stessa regola valida allo scadere naturale (punto successivo).
- Allo **scadere** del timer (naturale o anticipato), viene eseguita una scansione di recupero: se tutti gli invii pendenti hanno successo, il timer viene **cancellato**; se anche solo uno fallisce, il timer viene **riavviato** con la durata configurata.

### 6.4 Grace period

Per ogni notifica pendente individuata dal recupero, al momento dell'invio si calcola lo scarto temporale tra l'istante corrente e il `ts` dell'evento originale (in `log.jsonl`):

- Se lo scarto è **entro il grace period configurato** (default: **5 minuti**), la notifica viene inviata come **notifica normale**, senza indicazioni di ritardo.
- Se lo scarto **supera il grace period**, la notifica viene inviata con un prefisso esplicito (es. "⏪ Notifica recuperata") e il timestamp originale formattato secondo il formato e la timezone del destinatario.

---

## 7. Architettura di storage delle notifiche

### 7.1 Decisione

**Architettura scelta: Proposta E — log inverso, per-chat.** Un file dedicato per ciascun utente autorizzato, es. `notif_<chat_id>.jsonl`, contenente **esclusivamente** le righe relative a invii **non andati a buon fine al primo tentativo** (nessuna riga per gli invii riusciti immediatamente). Le proposte alternative valutate e scartate sono documentate in sezione 7.3, per riferimento futuro.

### 7.2 Schema del record

```json
{"id": "<uuid evento, stesso formato esadecimale di 5.2>", "status": <enum, vedi sotto>, "ts": <epoch Unix>, "state": <enum, vedi sotto>}
```

- **`id`**: stesso identificativo dell'evento in `log.jsonl` (sezione 5.2), per la correlazione.
- **`status`**: quale notifica si sta tracciando, enum numerico:

  | Valore | Significato |
  |---|---|
  | `0` | `NOTIFIED_START` |
  | `1` | `NOTIFIED_END` |
  | `2` | `NOTIFIED_INSTANT` |

- **`ts`**: per una riga `PENDING`, l'epoch dell'evento originale (utile per il calcolo del grace period); per una riga `RESOLVED`, l'epoch del momento dell'invio effettivo andato a buon fine.
- **`state`**: enum numerico:

  | Valore | Significato |
  |---|---|
  | `0` | `PENDING` — invio fallito, in attesa di recupero |
  | `1` | `RESOLVED` — invio successivamente riuscito |

Sequenza tipica per un singolo mancato invio poi recuperato:
```json
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","status":0,"ts":1755500000,"state":0}
{"id":"a1b2c3d4e5f60718293a4b5c6d7e8f90","status":0,"ts":1755500910,"state":1}
```

**Comportamento nel caso comune** (invio riuscito al primo tentativo, atteso essere la stragrande maggioranza dei casi data l'alimentazione garantita dalla batteria della centralina): **nessuna riga viene scritta** in questo file. Il file di un utente il cui bot funziona regolarmente resta quasi sempre vuoto o minimo.

**Ricostruzione dello stato pendente**: una notifica è ancora da recuperare se esiste una riga `PENDING` per quel `(id, status)` senza una corrispondente riga `RESOLVED`.

**Eventi irrisolti a lungo termine**: se un record `PENDING` resta irrisolto oltre il periodo di retention o oltre un numero elevato di cicli di retry, va segnalato nel riepilogo periodico insieme agli eventi aperti (sezione 8), per non perderne traccia silenziosamente in una futura rotazione.

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

---

## 8. Gestione degli eventi aperti

Se un evento con durata (es. `ALARM_GENERAL`, `ALARM_INTERNAL`, `ALARM_GARAGE`, `POWER_LOSS`, `NETWORK_ISSUE`) risulta ancora privo di riga `END` al momento di un riavvio inatteso dell'Arduino:

- **Non viene chiuso automaticamente.** Resta "aperto" nel registro fino a gestione manuale.
- Al boot, se vengono rilevati eventi aperti precedenti al riavvio, il sistema invia un **messaggio di riepilogo** a tutti gli utenti autorizzati con l'elenco degli eventi ancora aperti (id, tipo, timestamp di inizio formattato secondo le preferenze di ciascun destinatario). Lo stesso riepilogo include anche eventuali notifiche `PENDING` irrisolte a lungo termine (sezione 7.2).
- Lo stesso riepilogo viene incluso anche **ad ogni ciclo di rotazione** (vedi sezione 9), come promemoria periodico.
- Gli eventi aperti sono **sempre esclusi dalla cancellazione automatica** in fase di rotazione, indipendentemente dalla loro età.
- La chiusura può essere effettuata manualmente tramite il comando `/closeevent` (riservato agli utenti **admin**): scrive la riga `END` per l'`id` indicato (con `ts` pari al momento del comando, o a un timestamp esplicitamente fornito) e attiva il normale flusso di notifica per quell'`END`.

---

## 9. Rotazione del registro

### 9.1 Politica di retention

- Il periodo di validità del log eventi è **configurabile in settimane** (default da definire), impostabile solo da utenti admin.
- Gli eventi conclusi (con riga `END` o `INSTANT` presenti) più vecchi del periodo configurato vengono eliminati.
- Gli eventi ancora aperti (senza `END`) sono **sempre protetti** dalla cancellazione, indipendentemente dall'età.
- I file `notif_<chat_id>.jsonl` seguono la stessa politica di retention: righe `RESOLVED` più vecchie del periodo configurato vengono rimosse in fase di rotazione; righe `PENDING` restano sempre protette (coerentemente con la protezione degli eventi aperti).

### 9.2 Cadenza

Data la capacità di storage disponibile (16 MB) e il basso volume di eventi atteso, la rotazione avviene con **cadenza settimanale**, tramite un controllo leggero in RAM (confronto col timestamp dell'ultima rotazione, salvato in NVS), senza scansione del file per determinare se la rotazione è dovuta.

### 9.3 Meccanismo di esecuzione

Poiché LittleFS non supporta la cancellazione selettiva di righe, la rotazione è implementata come **riscrittura filtrata** dell'intero file (per `log.jsonl` e per ciascun `notif_<chat_id>.jsonl`):

1. Raggruppamento delle righe per `id`.
2. Determinazione dell'età di ogni evento/notifica in base al `ts` di riferimento (`START`/`INSTANT` per gli eventi, riga `RESOLVED` per le notifiche).
3. Esclusione dalla riscrittura di tutte le righe relative a elementi conclusi più vecchi del periodo di retention configurato.
4. Scrittura su **file temporaneo**, seguita da **rinomina atomica** sopra il file originale (pattern write-then-rename).

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

Il fuso orario è una **preferenza personale** salvata in `userconfig.json` (sezione 4.4): ogni utente imposta il proprio fuso indipendentemente dagli altri.

---

## 11. Configurazione

### 11.1 Configurazioni globali (NVS, solo admin)

| Parametro | Default | Comando |
|---|---|---|
| Periodo di validità log eventi e notifiche | Da definire | `/setretention <settimane>` |
| Grace period recupero notifiche | 5 minuti | `/setgraceperiod <minuti>` |
| Intervallo retry programmato | 60 minuti | `/setretryinterval <minuti>` |

### 11.2 Configurazioni per utente (`userconfig.json`, ogni utente sulle proprie)

| Parametro | Default | Comando |
|---|---|---|
| Formato data/ora | ISO 8601 | `/setdateformat <formato>` |
| Timezone | Da definire (es. UTC) | `/settimezone <preset>` |
| Tipi di evento notificati | Tutti abilitati | `/notify <tipo_evento> on\|off` |

**Nota importante**: la whitelist personale dei tipi di evento notificati filtra solo l'**invio delle notifiche a quello specifico utente**, non la **scrittura nel log eventi**. Tutti gli eventi vengono sempre registrati nello storico condiviso, indipendentemente dalle preferenze di notifica di ciascun utente.

---

## 12. Comandi Telegram previsti

| Comando | Permesso richiesto | Funzione |
|---|---|---|
| `/log [n]` | Utente autorizzato | Mostra gli ultimi n eventi dal registro, formattati secondo le preferenze del richiedente |
| `/config` | Utente autorizzato | Mostra la propria configurazione personale (e, se admin, anche quella globale) |
| `/setdateformat <formato>` | Utente autorizzato | Imposta il proprio formato di visualizzazione data/ora |
| `/settimezone <preset>` | Utente autorizzato | Imposta il proprio fuso orario da un set predefinito |
| `/notify <tipo_evento> on\|off` | Utente autorizzato | Abilita/disabilita per sé la notifica per una tipologia di evento |
| `/setretention <settimane>` | Admin | Imposta il periodo di validità globale del log in settimane |
| `/setgraceperiod <minuti>` | Admin | Imposta il grace period globale per il recupero notifiche |
| `/setretryinterval <minuti>` | Admin | Imposta l'intervallo globale del retry programmato |
| `/closeevent <id> [timestamp]` | Admin | Chiude manualmente un evento rimasto aperto |
| *Aggiunta utente* (nome comando da definire) | Admin | Aggiunge un nuovo `chat_id` alla whitelist |
| *Rimozione utente* (nome comando da definire) | Admin | Rimuove un `chat_id` dalla whitelist |
| *Promozione/rimozione admin* (nome comando da definire) | Admin | Modifica il flag `admin` di un utente esistente |
| *Reset whitelist* (nome comando da definire) | Admin | Svuota la whitelist (operazione distruttiva, da proteggere con conferma) |

---

## 13. Considerazioni di robustezza

- **Isolamento galvanico**: obbligatorio in caso di interfacciamento con uscita open collector, per proteggere sia l'Arduino sia la centralina.
- **Sincronizzazione oraria**: l'ESP32 non dispone di RTC con batteria tampone; l'orario viene ottenuto via NTP alla connessione e periodicamente, in UTC. La conversione in ora locale (con gestione automatica DST) avviene solo in fase di presentazione, secondo il fuso di ciascun utente.
- **Alimentazione**: garantita dalla batteria tampone della centralina; i riavvii per interruzione elettrica sono previsti come rari, a differenza delle interruzioni di sola connettività di rete.
- **Singolo punto di guasto (connettività)**: se la connessione di rete cade, le notifiche in tempo reale non sono possibili; il meccanismo di recupero (grace period + retry programmato) mitiga la perdita di notifiche, ma non elimina il ritardo nella loro consegna oltre le soglie configurate.
- **Integrità dei dati**: il pattern write-then-rename per la rotazione e per i file di configurazione/whitelist, unito all'approccio append-only per la scrittura ordinaria del log, minimizza il rischio di corruzione in caso di interruzione di alimentazione.
- **Sicurezza degli accessi**: nessuna risposta viene inviata a `chat_id` non presenti in whitelist. Le notifiche sono inviate esclusivamente ai `chat_id` autorizzati, e solo per eventi successivi alla loro aggiunta (vedi 4.6).
- **Usura della flash**: il volume di scrittura atteso per il log eventi (eventi rari) e per il registro notifiche (scritture solo sui fallimenti, con la Proposta E adottata) è ampiamente compatibile con la durata della memoria flash interna.
- **Compattezza dello storage**: l'uso di enum numerici per `type`/`status`/`state` e di UUID in formato esadecimale compatto (32 caratteri) riduce sensibilmente la dimensione media di ogni riga rispetto a uno schema con stringhe testuali complete (vedi sezione 5.2 e 7.2).

---

## 14. Decisioni di design registrate

| Argomento | Decisione |
|---|---|
| Struttura del log eventi | JSON Lines, append-only, contiene solo il rilevamento (`START`/`END`/`INSTANT`), non le notifiche |
| Eventi istantanei (es. REBOOT) | Valore enum dedicato per `INSTANT` |
| Eventi di rete | Tipo unico `NETWORK_ISSUE` con `START`/`END` |
| Tipologie di allarme | Zone distinte come tipi separati: `ALARM_GENERAL`, `ALARM_INTERNAL`, `ALARM_GARAGE`, ciascuno con proprio pin/PGM dedicato |
| Interruzione di corrente | Tipo `POWER_LOSS`, evento con durata (`START`/`END`), rilevato tramite PGM "guasto rete" della centralina |
| Ottimizzazione storage | `type`, `status` (e `state` per le notifiche) come enum numerici invece di stringhe testuali |
| Formato id evento | UUID v4 in esadecimale senza trattini, 32 caratteri (invece del formato standard a 36) |
| Storage delle notifiche | **Proposta E adottata**: log inverso (solo `PENDING`/`RESOLVED`), un file per chat (`notif_<chat_id>.jsonl`); proposte A-D scartate ma documentate in sezione 7.3 |
| Notifiche recuperate entro il grace period (5 min default) | Inviate come notifiche normali, senza prefisso di "recupero" |
| Notifiche recuperate oltre il grace period | Inviate con prefisso esplicito e timestamp originale |
| Recupero non riuscito | Timer di retry programmato (default 60 min): reset su fallimento; successo di un qualunque invio mentre il timer è attivo scatena una scansione anticipata invece di limitarsi a cancellare il timer |
| Eventi aperti dopo riavvio | Non chiusi automaticamente, segnalati via Telegram, chiudibili manualmente con `/closeevent` (solo admin) |
| Eventi/notifiche pendenti oltre il periodo di retention | Sempre esclusi dalla cancellazione automatica |
| Cadenza rotazione | Settimanale, unità di retention in settimane, applicata sia al log eventi sia ai file notifiche per-chat |
| Generazione id evento | UUID casuale via hardware RNG, nessun contatore NVS necessario |
| Controllo accessi | Whitelist di `chat_id` in `users.json`, nessuna risposta a mittenti non autorizzati |
| Permessi | Singolo flag booleano `admin` (no permessi granulari per ora) |
| Onboarding iniziale | `chat_id` iniziale hardcoded nel firmware, promosso automaticamente ad admin al primo avvio |
| Gestione whitelist | Comandi dedicati per aggiungere/rimuovere/promuovere/resettare (nomi da definire) |
| Configurazioni globali | NVS (retention, grace period, retry interval) |
| Configurazioni per utente | File JSON su LittleFS (`userconfig.json`): formato data, timezone, tipi evento notificati |
| Timezone | Set di preset predefiniti mappati a stringhe TZ POSIX, gestione DST automatica, preferenza per singolo utente |
| Alimentazione | Fornita dalla centralina (con batteria tampone): riavvii per blackout rari, interruzioni di rete più probabili |

---

## 15. Prossimi passi

- Definire i nomi esatti e la sintassi dei comandi di gestione whitelist (aggiunta, rimozione, promozione, reset) e il meccanismo di conferma per il reset.
- Confermare (o rivedere) il meccanismo `added_ts` per il filtro degli eventi precedenti all'aggiunta di un utente (sezione 4.6).
- Definire il valore di default per il periodo di retention del log eventi e notifiche.
- Definire la lista dei preset di timezone da offrire e le relative stringhe POSIX.
- Valutare (facoltativo, discusso separatamente) l'introduzione di comandi per inserire/disinserire l'allarme da remoto — richiede verifica di supporto hardware sulla centralina e un'attenta analisi di sicurezza aggiuntiva prima di essere formalizzato nel documento.
- Implementazione dello sketch Arduino completo.
- Test di interfacciamento con il modello specifico di centralina Bentel in uso.
