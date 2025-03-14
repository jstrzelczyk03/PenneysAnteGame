// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <time.h>

#define PORT 8888        // Port serwera
#define BUFFER_SIZE 1024 // Rozmiar bufora
#define MAX_CLIENTS 100  // Maksymalna liczba klientów
#define PATTERN_LENGTH 5 // Długość wzorca
#define GAME_ROUNDS 20   // Ilość rund

#define MAX_LEN 6
#define MAX_DATA_LEN 1 << MAX_LEN

// Typy wiadomości ALP
#define REGISTER 0
#define ACK 1
#define WANT_PLAY 2
#define START_GAME 3
#define PATTERN_SENT 4
#define TOSS_MSG 5
#define END 6
#define EXIT 7

// Struktura klienta
typedef struct
{
    struct sockaddr_in addr;
    char pattern[PATTERN_LENGTH + 1];
    int tosses;
} Client;

// Struktura wiadomości ALP
typedef struct
{
    uint8_t type;
    uint8_t len;
    uint8_t seq_num;
    uint8_t data[MAX_DATA_LEN];
} ALPMessage;

int counter = 0;
int client_count = 0;
int sum_of_tosses = 0;
int messages_sent = 0;
int messages_received = 0;
int winners[MAX_CLIENTS];
Client clients[MAX_CLIENTS];
int index_msg = 0;

// Funkcja kodująca wiadomość ALP
void encodeALPMessage(ALPMessage *msg, uint8_t *buffer)
{
    buffer[0] = (msg->type << 5) | (msg->len << 2);
    buffer[1] = msg->seq_num;

    if (msg->len > 0)
    {
        uint8_t data = 0;
        for (int i = 0; i < msg->len; i++)
        {
            data |= (msg->data[i] << (msg->len - i - 1));
        }
        buffer[2] = data;
    }
}

// Funkcja dekodująca wiadomość ALP
void decodeALPMessage(uint8_t *buffer, ALPMessage *msg)
{
    msg->type = (buffer[0] >> 5) & 0x07;
    msg->len = (buffer[0] >> 2) & 0x07;
    msg->seq_num = buffer[1];

    if (msg->len > 0)
    {
        uint8_t data = buffer[2];
        for (int i = 0; i < msg->len; i++)
        {
            msg->data[i] = (data >> (msg->len - i - 1)) & 0x01;
        }
    }
}

// Funkcja rejestrjąca nowego klienta
bool is_new_client(struct sockaddr_in *addr)
{
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            clients[i].addr.sin_port == addr->sin_port)
        {
            return false;
        }
    }
    return true;
}

// Funkcja dodająca nowego klienta
void add_client(struct sockaddr_in *addr)
{
    if (client_count < MAX_CLIENTS)
    {
        clients[client_count].addr = *addr;
        clients[client_count].tosses = 0;
        client_count++;
    }
}

int find_client_by_addr(struct sockaddr_in *addr) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].addr.sin_addr.s_addr == addr->sin_addr.s_addr &&
            clients[i].addr.sin_port == addr->sin_port) {
            return i;
        }
    }
    return -1;
}


// Funkcja wyświetlająca tablicę klientów
void print_client_table()
{
    printf("\nClient Table:\n");
    printf("--------------------------------------------\n");
    printf("| #  | IP Address      | Port              |\n");
    printf("--------------------------------------------\n");
    for (int i = 0; i < client_count; i++)
    {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clients[i].addr.sin_addr), ip, INET_ADDRSTRLEN);
        printf("| %-2d | %-15s | %-17d |\n", i + 1, ip, ntohs(clients[i].addr.sin_port));
    }
    printf("--------------------------------------------\n\n");
}

// Funkcja wyświetlająca tablicę klientów ze wzorcami
void print_client_table_with_pattern()
{
    printf("--------------------------------------------\n");
    printf("| #  | IP Address      | Pattern           |\n");
    printf("--------------------------------------------\n");
    for (int i = 0; i < client_count; i++)
    {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clients[i].addr.sin_addr), ip, INET_ADDRSTRLEN);
        printf("| %-2d | %-15s | %-17s |\n",
               i + 1, ip, clients[i].pattern);
    }
    printf("--------------------------------------------\n\n");
}

// Funkcja obsługi gry
void make_one_game(int sockfd)
{
    uint8_t buffer[3];

    // Przygotowanie wiadomości o rozpoczęciu gry
    ALPMessage startGameMsg = {START_GAME, 1, 5, {1}};
    encodeALPMessage(&startGameMsg, buffer);

    // Wysłanie wiadomości do wszystkich klientów
    for (int i = 0; i < client_count; i++)
    {
        if (sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr)) < 0)
        {
            perror("sendto failed");
        }
        else
        {
            messages_sent++;
        }
    }

    if (counter++ == 0)
    {
        // Zbiera wzorce od klientów
        for (int i = 0; i < client_count; i++)
        {
            uint8_t buffer[BUFFER_SIZE];
            struct sockaddr_in cliaddr;
            socklen_t len = sizeof(cliaddr);

            // Odbieranie danych
            int n = recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (struct sockaddr *)&cliaddr, &len);
            if (n > 0){
                messages_received++;
                buffer[n] = '\0';

                if (n == 3)
                {
                    index_msg = find_client_by_addr(&cliaddr);
                    if (index_msg == -1) {
                    printf("Unknown client received.\n");
                    continue; // Ignoruj nieznanych klientów
                    }

                    uint8_t data = buffer[2];
                    
                    for (int j = 0; j < 5; j++)
                    {
                        uint8_t bit = (data >> (4 - j)) & 0x01; 
                        clients[index_msg].pattern[j] = (bit == 1) ? 'H' : 'T'; // 1 -> H, 0 -> T
                    }
                    clients[index_msg].pattern[5] = '\0';

                    printf("Pattern received from client %d: %s\n", index_msg + 1, clients[index_msg].pattern);
                }
                else
                {
                    printf("\n");
                    printf("Invalid pattern from client %d. Expected 6 bytes.\n", index_msg + 1);
                    printf("Error - Game finished\n");
                    exit(EXIT_FAILURE);
                }
            }
            else
            {
                perror("recvfrom failed");
            }
        }
    }

    print_client_table_with_pattern();

    bool winner_found = false;
    char last_n_tosses[PATTERN_LENGTH + 1] = "";
    int toss_count = 0;
    int min_tosses = -1;
    int winner_index = -1;

    while (!winner_found)
    {
        char toss = (rand() % 2 == 1) ? 'H' : 'T';
        toss_count++;

        // Aktualizacja sekwencji
        memmove(last_n_tosses, last_n_tosses + 1, PATTERN_LENGTH - 1);
        last_n_tosses[PATTERN_LENGTH - 1] = toss;
        last_n_tosses[PATTERN_LENGTH] = '\0';

        if (strlen(last_n_tosses) >= PATTERN_LENGTH)
        {
            printf("Toss %d: %c | ", toss_count, toss);
            printf(" Last %d tosses: %s\n", PATTERN_LENGTH, last_n_tosses);
        }
        else
        {
            printf("Toss %d: %c\n", toss_count, toss);
        }

        // Wysyłanie wyniku rzutu
        uint8_t buffer[3];
        uint8_t toss_value = (toss == 'H') ? 1 : 0;

        ALPMessage tossMsg = {TOSS_MSG, 1, 7, {toss_value}};
        encodeALPMessage(&tossMsg, buffer);

        // printf("TOSS_MSG - Zakodowana: ");
        // for (int i = 0; i < 3; i++)
        // {
        //     printf("%02X ", buffer[i]);
        // }
        // printf("\n");

        for (int i = 0; i < client_count; i++)
        {
            if (sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr)) < 0)
            {
                perror("sendto failed");
            }
            else
            {
                messages_sent++;
            }
        }

        struct sockaddr_in servaddr, cliaddr;
        socklen_t len = sizeof(cliaddr);
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, &len);
        ALPMessage msg;
        if (n > 0)
        {
            messages_received++;
            buffer[n] = '\0';

            decodeALPMessage((uint8_t *)buffer, &msg);
        }

        // Sprawdzenie zgodności wzorca
        if (msg.type = END){  
            for (int i = 0; i < client_count; i++)
            {
                if (strcmp(last_n_tosses, clients[i].pattern) == 0)
                {
                    clients[i].tosses = toss_count;
                    winner_index = i;
                    winners[i]++;
                    min_tosses = toss_count;
                    winner_found = true;
                    break;
                }
            }
        }
    }

    if (winner_index != -1)
    {
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clients[winner_index].addr.sin_addr), ip, INET_ADDRSTRLEN);
        sum_of_tosses += min_tosses;
        printf("\nWinner:\n");
        printf("--------------------------------------------\n");
        printf("|                Client %d                  |\n", winner_index + 1);
        printf("--------------------------------------------\n");
        printf("| IP                 | %-19s |\n", ip);
        printf("| Pattern            | %-19s |\n", clients[winner_index].pattern);
        printf("| Tosses             | %-19d |\n", min_tosses);
        printf("--------------------------------------------\n\n");
    }

    for (int i = 0; i < client_count; i++)
    {
        uint8_t buffer[3];

        if (winner_index == i)
        {
            ALPMessage endMsg = {END, 1, 8, {1}};
            encodeALPMessage(&endMsg, buffer);
        }
        else
        {
            ALPMessage endMsg = {END, 1, 8, {0}};
            encodeALPMessage(&endMsg, buffer);
        }

        if (sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr)) < 0)
        {
            perror("sendto failed");
        }
        else
        {
            messages_sent++;
        }
    }
}

// Funkcja wyświetlająca statystyki gry
void print_game_statistics()
{
    printf("\nGame Statistics:\n");
    printf("--------------------------------------------\n");
    printf("| Parameter                | Value         |\n");
    printf("--------------------------------------------\n");
    printf("| Number of players        | %-13d |\n", client_count);
    printf("| Length of the patterns   | %-13d |\n", PATTERN_LENGTH);
    printf("| Game rounds played       | %-13d |\n", GAME_ROUNDS);
    printf("| Average number of tosses | %-13.2f |\n", (float)sum_of_tosses / GAME_ROUNDS);
    printf("| Messages sent            | %-13d |\n", messages_sent);
    printf("| Messages received        | %-13d |\n", messages_received);
    printf("--------------------------------------------\n\n");

    printf("Winning Probabilities:\n");
    printf("--------------------------------------------\n");
    printf("| Pattern  | Winning Probability [%%]       |\n");
    printf("--------------------------------------------\n");

    for (int i = 0; i < client_count; i++)
    {
        float prob = (float)winners[i] / GAME_ROUNDS * 100;
        printf("| %-8s | %-29.2f |\n", clients[i].pattern, prob);
    }

    printf("--------------------------------------------\n\n");
}

int main()
{
    srand(time(0));
    int sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in servaddr, cliaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("UDP server started on port %d\n", PORT);

    socklen_t len = sizeof(cliaddr);
    time_t start_time = time(NULL);

    printf("Waiting for REGISTER message for 20 seconds...\n");

    // Pętla oczekująca na wiadomość REGISTER przez 10 sekund
    while (time(NULL) - start_time < 20)
    {
        int n = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr *)&cliaddr, &len);

        if (n > 0)
        {
            messages_received++;
            buffer[n] = '\0';

            // Zdekodowanie odebranej wiadomości
            ALPMessage msg;
            decodeALPMessage((uint8_t *)buffer, &msg);
            // printf("Received message: Type %d, Seq Num %d\n", msg.type, msg.seq_num);

            if (msg.type == REGISTER)
            {
                printf("REGISTER message received.\n");
                if (is_new_client(&cliaddr))
                {
                    add_client(&cliaddr);
                    print_client_table();
                }
            }
        }
    }

    if (client_count == 0)
    {
        printf("No clients. Game not started.\n");
        close(sockfd);
        return 0;
    }

    printf("%d clients registered. Starting the game.\n", client_count);

    for (int game_round = 1; game_round <= GAME_ROUNDS; game_round++)
    {
        printf("\nStarting game round %d\n", game_round);
        char last_n_tosses[PATTERN_LENGTH + 1] = "";
        make_one_game(sockfd);
    
        time_t start_time = time(NULL);
        while (time(NULL) - start_time < 2) {
        }
    }

    for (int i = 0; i < client_count; i++)
    {
        uint8_t buffer[3];

        ALPMessage endMsg = {EXIT, 1, 8, {0}};
        encodeALPMessage(&endMsg, buffer);
        if (sendto(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&clients[i].addr, sizeof(clients[i].addr)) < 0)
        {
            perror("sendto failed");
        }
        else
        {
            messages_sent++;
        }
    }
    print_game_statistics();

    close(sockfd);
    return 0;
}