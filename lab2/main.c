#define _POSIX_C_SOURCE 200809L  //без этой строчки ничего не работало

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>

#define PORT      65000 // устанавливаем порт

static volatile sig_atomic_t g_got_sighup = 0; // Глобальная переменная-флаг для обработки сигнала SIGHUP
static int g_server_socket = -1; // Глобальный дескриптор "сервера"
static int g_client_socket = -1; // Глобальный дескриптор принятого соединения

// Обработчик сигнала SIGHUP
void handle_sighup(int signo) {
    g_got_sighup = 1; // Устанавливаем флаг для обработки
}

// Утилитарная функция “умереть с ошибкой”
void die(const char* msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void)
{
    struct sigaction sa; // Структура для настройки обработчика сигнала
    memset(&sa, 0, sizeof(sa)); // Обнуляем структуру
    sa.sa_handler = handle_sighup; // Указываем функцию-обработчик
    sigemptyset(&sa.sa_mask); // Очищае маску сигналов, которые должны блокироваться
    sa.sa_flags = 0;

    // Устанавливаем обработчик сигнала SIGHUP
    if (sigaction(SIGHUP, &sa, NULL) == -1) {// Если установка обработчика завершилась ошибкой
        die("sigaction"); // Завершаем выполнение программы
    }

    sigset_t blocked_signals, orig_mask; // Множество сигналов, которые мы хотим заблокировать, а также маска
    sigemptyset(&blocked_signals); // Очищаем множество
    sigaddset(&blocked_signals, SIGHUP); // Добавляем в множество сигнал SIGHUP

    if (sigprocmask(SIG_BLOCK, &blocked_signals, &orig_mask) == -1) {// Если блокировка завершилась ошибкой
        die("sigprocmask");
    }

    // Создаем серверный сокет
    g_server_socket = socket(AF_INET, SOCK_STREAM, 0); // Если создание сокета завершилось ошибкой
    if (g_server_socket == -1) {
        die("socket");
    }

    struct sockaddr_in addr; // Структура для хранения адреса сервера
    memset(&addr, 0, sizeof(addr)); // Очищаем структуру
    addr.sin_family = AF_INET; // Используем IPv4
    addr.sin_port = htons(PORT); // Преобразуем порт в сетевой порядок байтов
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // Разрешаем подключение с любого IP-адреса

    // Привязываем сокет к адресу сервера
    if (bind(g_server_socket, (struct sockaddr*)&addr, sizeof(addr)) == -1) { // Если привязка сокета завершилась ошибкой
        die("bind");
    }

    // Начинаем прослушивать входящие соединения
    if (listen(g_server_socket, 1) == -1) {
        die("listen");
    }

    printf("Server started on port %d (PID %d)\n", PORT, (int)getpid());
    printf("One client is kept active, the remaining connections are closed immediately.\n");

    while(1) {
        fd_set read_fds; // Набор файловых дескрипторов для pselect
        // Инициализируем набор файловых дескрипторов
        FD_ZERO(&read_fds); // Очищаем набор
        FD_SET(g_server_socket, &read_fds); // Добавляем серверный сокет
        int maxfd = g_server_socket;

        // Определяем максимальный дескриптор
        if (g_client_socket != -1) {
            FD_SET(g_client_socket, &read_fds);
            if (g_client_socket > maxfd) {
                maxfd = g_client_socket;
            }
        }

        // Ожидаем событий на сокетах с помощью pselect
        int ready = pselect(maxfd + 1, &read_fds, NULL, NULL,
            NULL, &orig_mask);

        if (ready == -1) { // Если произошла ошибка
            if (errno == EINTR) {
                /* pselect прервали сигналом (в т.ч. SIGHUP) */
                if (g_got_sighup) {
                    printf("Received signal SIGHUP (in main loop)\n");
                    g_got_sighup = 0;
                }
                continue;
            }
            else { // Другая ошибка
                die("pselect");
            }
        }

        // Проверяем новые запросы на соединение
        if (FD_ISSET(g_server_socket, &read_fds)) { // Если серверный сокет готов к чтению
            int new_fd = accept(g_server_socket, NULL, NULL); // Принимаем новое соединение
            if (new_fd == -1) { // Если произошла ошибка
                perror("accept");
            }
            else {
                printf("New connection accepted\n");
                if (g_client_socket == -1) { // Если клиентский сокет ещё не установлен
                    g_client_socket = new_fd; // Сохраняем клиентский сокет
                    printf("This connection has become an active client\n");
                }
                else { // Если уже есть клиентский сокет
                    printf("There is already an active client, close the extra connection\n");
                    close(new_fd); // Закрываем лишний сокет
                }
            }
        }

        // Проверяем наличие данных от клиента
        if (g_client_socket != -1 && FD_ISSET(g_client_socket, &read_fds)) {
            char buf[4096];
            ssize_t n = recv(g_client_socket, buf, sizeof(buf), 0);
            if (n > 0) {
                printf("Received %zd bytes\n", n);
            }
            else if (n == 0) {
                printf("The client closed the connection\n");
                close(g_client_socket);
                g_client_socket = -1;
            }
            else {
                if (errno == EINTR) {
                    continue;
                }
                perror("recv");
                close(g_client_socket);
                g_client_socket = -1;
            }
        }
    }
    return 0;
}
