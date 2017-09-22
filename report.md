
# Oznaczenia
W sprawozdaniu (niekoniecznie w kodzie) używane są następujące oznaczenia:

- *N*: liczba procesów firm oferujących zabójców
- *Z*: liczba zabójców w każdej z firm
- *P*: liczba wszystkich procesów
- *K*: liczba procesów klientów
- *S*: średni czas, jaki klienci oczekują między kolejnymi zleceniami
- *T*: średni czas wykonania zlecenia

# Założenia
Kanały komunikacyjne między procesami są typu FIFO i zapewniają niezawodną komunikację.
Środowisko w pełni asynchroniczne.
Czas działania, czas przeysłania kominikatów - brak założeń.

## Wymagania techniczne
OpenMPI 1.10 z opcją MPI_THREAD_MULTIPLE (zapewnia, że wywołania OpenMPI są thread-safe).

# Typy procesów

Procesy o identyfikatorze (rank) od 0 do N-1 to procesy reprezentujące firmy (oferujące usługi zabójców).
Pozostałe procesy (od N do P-1) to klienci.
Suma wszystkich procesów P jest zdefiniowana następująco:
  P = N + K

# Implementacja

## Klient

*K1*: Każdy proces klienta powinien zgłosić maksymalnie jedno zlecenie (aż do czasu końca jego realizacji). Potem może zgłosić następne.
  *Notes 1*: Wartość zmiennej "state" oznacza zarówno stan procesu klienta, jak i jego aktualnego zlecenia. Wyróżniamy następujące wartości zmiennej "state":
  - *WAITING*: Klient nie ma aktualnie zlecenia dla zabójcy;
  - *NOQUEUE*: Klient ma zlecenie do realizacji - informuje o tym usługodawców. Ponadto nie ma jeszcze żadnego wyznaczonego miejsca w kolejce.
  - *QUEUE*: Klient oczekuje na przyjęcie zlecenia do realizacji i wyznaczenie miejsca w kolejkach przez poszczególne firmy.
  - *INPROGRESS*: Zlecenia klienta jest w trakcie realizacji; Po otrzymaniu wiadomości typu *TAG_REQUEST* ze statusem *Q_DONE*, klient rozsyła swoją recenzję (w postaci oceny wykonanej usługi do wszystkich klientów).


*K2*: Klient powinien oczekiwać w stanie *WAITING* losowy okres czasu zanim zacznie ubigać się o usługę zabójcy. Po upływie tego czasu przechodzi do stanu *NOQUEUE*.

*K3*: Klient w stanie *NOQUEUE* powinien wysłać wiadomość typu *TAG_REQUEST* do wszystkich firm (aby sprawdzić dostępne miejsca w kolejkach), a następnie przejść do kolejnego stanu *QUEUE*.
  *Notes 1*: Liczba firm (N) jest z góry znana klientowi, więc znane są mu także numery procesów, do których powinien wysłać zlecenie.
  *Notes 2*: Potwierdzenie odbioru wiadomości typu *TAG_REQUEST* przez wszystkie firmy nie jest konieczne do kontynuowania przetwarzania zlecenia przez klienta.

*K4*: Po wykonaniu kroku *K3*, proces klienta powinien oczekiwać na przychodzące wiadomości typu *TAG_UPDATE*.

*K5*: W czasie oczekiwania na wiadomości typu *TAG_UPDATE* klient powinien również aktualizować informację na temat wystawionych recenzji.
  *Notes 1*: Nowe recenzje razem z aktualnym miejscem w kolejce mogą wpłynąć na zmianę preferencji klienta i rezygnację z kolejki u innych firm.

*K5*: Po odebraniu wiadomości typu *TAG_UPDATE* z wartością nieujemną, klient powinien zapisać wyznaczone miejsce w kolejce n-tej firmy w tablicy "queues" na n-tej pozycji.
  *Notes 1*: Wyznaczone miejsce w kolejce n-tej firmy jest przesłane w wiadomości typu *TAG_UPDATE* jako wartość nieujemna. Wartości ujemne są zarezerwowane na inne komunikaty specjalnego przeznaczenia (np. *Q_IN_PROGRESS*, *Q_DONE* itd.);

*K6* Przejście klienta ze stanu *QUEUE* do następnego *INPROGRESS* powinno nastąpić po odebrania pierwszej wiadomości typu *TAG_UPDATE* ze statusem *Q_IN_PROGRESS*;
  *K6.1* W odpowiedzi na pierwszy komunikat *Q_IN_PROGRESS*, klient powinien wysłać wiadomość typu *ACK_OK* do tej firmy a do pozostałych *ACK_REJECT*.
  *K6.2* Niezależnie od otrzymanych wiadomości, klient powinien zrezygnować z miejsc w kolejkach pozostałych firm poprzez wysłanie wiadomości typu *TAG_CANCEL* do każdej z nich i oznaczeniu odpowiednich miejsc w tablicy *queues* jako *Q_CANCELLED*.

*K7* Klient powinien oczekiwać na pomyślne wykonanie zlecenia (w stanie *INPROGRESS* na wiadomość typu *TAG_UPDATE* ze statusem *Q_DONE*).
  *Notes 1* Można założyć, że otrzymamy tylko jedną taką wiadomość (od firmy, która podjęła się zlecenia jako pierwsza).
  Oznacza to, że nie ma konieczności sprawdzania czy ta wiadomość pochodzi z dobrego źródła.
  (FIXME FYI odważne założenine)

  *Notes 2* Przetworzenie wiadomości typu *TAG_REVIEW* może nastąpić za każdym razem, kiedy proces klienta oczekuje na wiadomości typu *TAG_UPDATE*. Oznacza to, że kiedy proces nie ubiega się o zabójcę, wiadomości oczekują w kanale komunikacyjnym.
  (FIXME - potencjalnie to może być problem, bo kolejka może się zapchać po dłuższym oczekiwaniu w stanie *WAITING* albo może znacząco opóźnić odczyt wiadomości od firm.)

  *K7.1* Klient powinien rozesłać ocenę wykonanej usługi do wszystkich klientów. (Działa to jak marketing szeptany.)
  *K7.2* Klient powinien posprzątać informację na temat zajmowanych kolejek z tego zlecenia i przejść do stanu zadumy *WAITING*.




### Deskryptor stanu klienta (tzn. stanu zlecenia danego klienta)

--->
/* Values of customer's state */
typedef enum State
{
    WAITING,       /* A customer has no new task/job yet for a killer. */
    NOQUEUE,       /* A customer has a task/job, but it is not in any queue yet.
                    * In this state the customer sends a request to all companies. */
    QUEUE,         /* A customer has been waiting for infomration, that the task/job is in progress by one of these companies. In this state the customer considers new review ratings (if any). */
    INPROGRESS     /* The task/job is in progress. Awaiting for conformation that it is completed. In this state the customer sends notification to other customers about his review rating. */
} State;
<--


int queues[N]
(??)


### Funkcje

function onUpdate(MessageUpdate)
    if (msg.status == IN_PROGRESS):
        if (state != IN_PROGRESS):
            state = IN_PROGRESS
            wysłanie ACK(ACK_OK) do N_i
            wysłanie CANCEL do pozostałych firm
        if (state == IN_PROGRESS)
            Send(ACK(ACK_REJECT), N_i)



## Firma

Do procesu firmy należy główny wątek oraz wątek agenta.
Główny wątek oczekuje w pętli na przychodzące wiadomości.
Wątek agenta oczekuje na wykonanie zlecenia przez jednego z zabójców i sygnalizuje
to poprzez wysłanie wiadomości KILLER_READY, która jest obsługiwana przez główny wątek.


### Deskryptor

int Queue[K]
struct Killer {
    int client
    int status
    timer }
struct Killer Killers[Z]
mutex killersMutex
conditional wakeUpAgent

## Typy wiadomości

REQUEST
Przesyłane od procesu klienta K_i do firmy N_i.
Dane: brak
Wymagania: brak poprzedniej wiadomości REQUEST
           lub po ostatniej REQUEST wysłano wiadomość CANCEL
Po odebraniu: dodanie K_i do kolejki
    XXX jeśli kolejka jest pusta i jest dostępny zabójca wysłanie UPDATE(IN_PROGRESS)

CANCEL
K_i -> N_i
Dane: brak
Wymagania: K_i wysłał do N_i REQUEST, ale nie wysłał CANCEL ani ACK
Po odebraniu: usunięcie K_i z kolejki

UPDATE
N_i -> K_i
Dane:
    - status: dostępny, w kolejce, rozpoczęnie obsługi zlecenia, zlecenie wykonane
    - pozycja w kolejce
Wymagania: K_i jest w kolejce firmy N_i lub zlecenie od K_i jest wykonywane
    przez zabójcę należącego do N_i
    zmiana miejsca w kolejce
    "rozpoczęcie" wykonywania zlecenia - możliwość rozpoczęcia
    zakończenie wykonywania zlecenia
Po odebraniu:
    jeśli status wiadomości == rozpoczęcie obsługi:
        jeśli klient czeka:
            zmiana stanu klienta
            wysłanie ACK(ACK_OK) do N_i
            wysłanie CANCEL do pozostałych firm
        jeśli K_i odebrał już UPDATE ze stanem IN_PROGRESS:
            wysłanie ACK(ACK_REJECT) do N_i

ACK
K_i -> N_i
Dane: status (akceptacja / odmowa)
Wymagania: odebranie wiadomości UPDATA N_i -> K_i ze stanem IN_PROGRESS
Po odebraniu: rozpoczęcie obsługi zlecenia

KILLER READY
N_i -> N_i
Dane: numer zabójcy
Wymagania: wykonanie zadania przez zabójcę
Po odebraniu:
    jeśli kolejka nie jest pusta ...

# Uruchomienie programu

make debug
mpirun -np P killers -c N -k Z [-s S -t T]
