// client1.c
#include <ZsutEthernetUdp.h>
#include <ZsutEthernet.h>
#include <ZsutIPAddress.h>
#include <ZsutFeatures.h>
#include <stdio.h>
#include <string.h>

#define PATTERN_LENGTH 5
#define MAX_LEN 5
#define MAX_DATA_LEN 1 << MAX_LEN

#define REGISTER 0
#define ACK 1
#define WANT_PLAY 2
#define START_GAME 3
#define PATTERN_SENT 4
#define TOSS_MSG 5
#define END 6
#define EXIT 7

byte mac[] = {0x01, 0xff, 0xaa, 0x12, 0x34, 0x57};

ZsutEthernetUDP Udp;
ZsutIPAddress serverIP(192, 168, 56, 108); // IP serwera
unsigned int serverPort = 8888;            // Port serwera
unsigned int localPort = 8889;             // Lokalny port klienta

int counter = 0;
int toss_count = 0;
uint8_t pattern_bin[1];
char pattern[PATTERN_LENGTH + 1];        
char last_n_tosses[PATTERN_LENGTH + 1] = "";

// Struktura wiadomości ALP
typedef struct
{
    uint8_t type;              
    uint8_t len;               
    uint8_t seq_num;           
    uint8_t data[MAX_DATA_LEN];
} ALPMessage;

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

// Funkcja odczytująca wzór z pliku infile.txt
void readPatternFromPins()
{
    pattern_bin[0] = 0;
    for (int i = 0; i < PATTERN_LENGTH; i++)
    {
        pattern[i] = (((ZsutDigitalRead() >> (i + 1)) & 1) == 1) ? 'H' : 'T';

        // Konwersja na wartość binarną
        if (pattern[i] == 'H')
        {
            pattern_bin[0] |= (1 << (PATTERN_LENGTH - i - 1));
        }
        else
        {
            pattern_bin[0] &= ~(1 << (PATTERN_LENGTH - i - 1));
        }
    }
    pattern[PATTERN_LENGTH] = '\0';
}

void setup()
{
    ZsutEthernet.begin(mac);
    ZsutPinMode(ZSUT_PIN_D0, INPUT);
    ZsutPinMode(ZSUT_PIN_D1, INPUT);
    ZsutPinMode(ZSUT_PIN_D2, INPUT);
    ZsutPinMode(ZSUT_PIN_D3, INPUT);
    ZsutPinMode(ZSUT_PIN_D4, INPUT);
    ZsutPinMode(ZSUT_PIN_D5, INPUT);

    Udp.begin(localPort);
    Serial.println("UDP client started.");

    // Przygotowanie wiadomości ALP REGISTER
    uint8_t buffer[3];
    ALPMessage registerMsg = {REGISTER, 0, 1, {0}};
    encodeALPMessage(&registerMsg, buffer);

    // Serial.print("REGISTER - Zakodowana: ");
    // for (int i = 0; i < 3; i++)
    // {
    //     Serial.print(buffer[i], HEX);
    //     Serial.print(" ");
    // }
    // Serial.println();

    Udp.beginPacket(serverIP, serverPort);
    Udp.write(buffer, sizeof(buffer));
    Udp.endPacket();
    Serial.println("REGISTER message sent to server.");
}

void loop()
{
    uint8_t packetBuffer[3];
    ALPMessage receivedMsg;
    int packetSize = Udp.parsePacket();

    if (packetSize > 0)
    {
        memset(packetBuffer, 0, sizeof(packetBuffer));
        Udp.read(packetBuffer, sizeof(packetBuffer));

        // Serial.print("Received: ");
        // decodeMessage(packetBuffer);
        // for (int i = 0; i < packetSize; i++)
        // {
        //     Serial.print(packetBuffer[i]);
        //     Serial.print(" ");
        // }
        // Serial.println();

        decodeALPMessage(packetBuffer, &receivedMsg);

        // Obsługa różnych typów wiadomości ALP
        switch (receivedMsg.type)
        {
        case START_GAME:
            Serial.println("Game starting!");
            toss_count = 0;
            memset(last_n_tosses, 0, sizeof(last_n_tosses));
            
            if (counter++ == 0)
            {
                readPatternFromPins();
                Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
                uint8_t buffer[3]; 

                // Przygotowanie wiadomości PATTERN_SENT
                ALPMessage patternSentMsg = {PATTERN_SENT, 1, 6, {pattern_bin[0]}};
                encodeALPMessage(&patternSentMsg, buffer);

                // Serial.print("PATTERN_SENT - Zakodowana: ");
                // for (int i = 0; i < sizeof(buffer); i++)
                // {
                //     Serial.print(buffer[i], HEX);
                //     Serial.print(" ");
                // }
                // Serial.println();

                Udp.write(buffer, sizeof(buffer));
                Udp.endPacket();

                // Serial.print("Pattern sent (bin): ");
                // for (int i = 0; i < PATTERN_LENGTH; i++)
                // {
                //     Serial.print((pattern_bin[0] >> (PATTERN_LENGTH - i - 1)) & 0x01);
                // }
                // Serial.println();
            }

            break;
        case TOSS_MSG:
            // Odczytujemy rzut (H/T) i zapisujemy
            if (receivedMsg.len == 1)
            {
                char toss = (receivedMsg.data[0] == 1) ? 'H' : 'T';
                toss_count++;

                // Aktualizacja sekwencji
                memmove(last_n_tosses, last_n_tosses + 1, PATTERN_LENGTH - 1);
                last_n_tosses[PATTERN_LENGTH - 1] = toss;
                last_n_tosses[PATTERN_LENGTH] = '\0';

                if (strlen(last_n_tosses) >= PATTERN_LENGTH)
                {
                    Serial.print("Toss: ");
                    Serial.print(toss_count);
                    Serial.print(": ");
                    Serial.print(toss);
                    Serial.print(" | ");
                    Serial.print("Last ");
                    Serial.print(PATTERN_LENGTH);
                    Serial.print(" tosses: ");
                    Serial.println(last_n_tosses);
                }
                else
                {
                    Serial.print("Toss: ");
                    Serial.print(toss_count);
                    Serial.print(": ");
                    Serial.println(toss);
                }

                // Sprawdzanie, czy wzorzec pasuje
                if (strcmp(last_n_tosses, pattern) == 0)
                {
                    uint8_t buffer[3];                   
                    ALPMessage endMsg = {END, 1, 8, {1}};
                    encodeALPMessage(&endMsg, buffer);
                    Serial.println("Pattern matched! Sending result to server.");
                    Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
                    Udp.write(buffer, sizeof(buffer));
                    Udp.endPacket();
                }
            }
            break;

        case END:
            if (receivedMsg.len == 1)
            {
                if (receivedMsg.data[0] == 1){
                    Serial.println("--------------------------------------------\n");
                    Serial.println("|                  You won!                 |\n");
                    Serial.println("--------------------------------------------\n");
                    Serial.println("");
                }
                else {
                    Serial.println("--------------------------------------------\n");
                    Serial.println("|                  You lost!                |\n");
                    Serial.println("--------------------------------------------\n");
                    Serial.println("");
                }
            }
            break;

        case EXIT:
            if (receivedMsg.len == 1)
            {
                Serial.println("--------------------------------------------\n");
                Serial.println("|               Game finished!              |\n");
                Serial.println("--------------------------------------------\n");
            }
            break;

        default:
            Serial.println("Unknown message type received.");
            break;
        }
    }
}