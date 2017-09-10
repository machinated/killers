
# Oznaczenia
W sprawozdaniu (niekoniecznie w kodzie) używane są następujące oznaczenia:

N - liczba procesów firm oferujących zabójców
Z - liczba zabójców w każdej z firm
P - liczba wszystkich procesów
K - liczba procesów klientów
S - średni czas, jaki klienci oczekują między kolejnymi zleceniami
T - średni czas wykonania zlecenia

# Założenia
Kanały komunikacyjne FIFO, komunikacja niezawodna.

## Wymagania techniczne
OpenMPI 1.10 z opcją MPI_THREAD_MULTIPLE (zapewnia, że wywołania OpenMPI są thread-safe).

# Typy procesów

Procesy o identyfikatorze (rank) od 0 do N-1 to procesy firm.
Pozostałe procesy (od N do P-1) to klienci.
P = N + K

# Implementacja

## Klient

Każdy proces klienta może zgłosić maksymalnie jedno zlecenie.
Wartość zmiennej "state" oznacza zarówno stan procesu klienta, jak i zlecenia.

Klient oczekuje losowy okres czasu zanim zacznie ubigać się o zabójcę.
Następnie wysyła wiadomości typu REQUEST do wszystkich firm.
Liczba firm (N) jest znana dla klienta, więc zna numery procesów, do których powinien wysłać zlecenie.
Odbiór wszystkich wiadomości REQUEST nie jest konieczny do kontynuowania przetwarzania przez klienta.

Proces następnie oczekuje na przychodzące wiadomości UPDATE.
Po odebraniu wiadomości, pozycja w kolejce jest zapisywana w tablicy "queues".
Przejście do następnego stanu następuje na skutek odebrania wiadomości ze statusem IN_PROGRESS;
W odpowiedzi klient wysyła wiadomość typu ACK.


Odbiór wiadomości typu REVIEW następuje za każdym razem, kiedy proces oczekuje na wiadomości UPDATE.
Oznacza to, że kiedy proces nie ubiega się o zabójcę, wiadomości oczekują w kanale komunikacyjnym.

### Deskryptor

enum State state
int queues[N]

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
