# Penney’s Ante Game

Repozytorium zawiera projekt symulacji rozproszonej gry Penney’s Ante, zrealizowany w ramach zajęć PSIR. Projekt implementuje system, w którym serwer zarządza rozgrywką, a klienci wprowadzają wzorce rzutów monet, odbierają wyniki i raportują swoje rezultaty.

## Opis projektu

- **Serwer:**  
  Napisany w języku C, serwer obsługuje rejestrację klientów poprzez odbieranie wiadomości HELLO, wysyła komunikat START GAME, generuje wyniki rzutów monet (TOSS MSG) oraz zbiera potwierdzenia końca rundy (END). System monitoruje dostępność klientów i zbiera statystyki rozgrywki.

- **Klient:**  
  Klient umożliwia wprowadzenie wzorca (ciągu H i T), odbiera od serwera wyniki rzutów monet, porównuje je z wprowadzonym wzorcem i wysyła odpowiedź, gdy wzorzec zostanie znaleziony. Implementacja klienta wykorzystuje standardowe API Arduino oraz rozszerzenia PSIR.

- **Protokół ALP:**  
  Projekt korzysta z własnoręcznie zdefiniowanego protokołu ALP opartego na UDP, który określa strukturę wiadomości na poziomie bitowym. Protokół definiuje typy wiadomości, takie jak REGISTER, START GAME, PATTERN SENT, TOSS MSG, END oraz EXIT, co pozwala na efektywną i oszczędną komunikację między serwerem a klientami.

## Autorzy

- Jakub Strzelczyk
- Jakub Sierocki
- Jakub Grzechnik

---

Więcej szczegółowych informacji, opis implementacji oraz wyniki testów znajdziesz w sprawozdaniu dołączonym do projektu.
