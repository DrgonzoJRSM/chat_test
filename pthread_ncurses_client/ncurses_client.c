#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include <ctype.h>
#include <ncursesw/ncurses.h>
#include <wchar.h>
#include <locale.h>

#define PORT                2024
#define MESSAGE_SIZE        1024
#define MAX_NAME_LENGTH     32
#define MAX_MESSAGES_COUNT  100

#define INPUT_WIN_HEIGHT    3
#define STATUS_WIN_HEIGHT   1

struct windows_t {
    WINDOW* chat_win;
    WINDOW* input_win;
    WINDOW* status_win;
    int chat_height;
    int chat_width; 
};

struct message_history_t {
    wchar_t messages[MAX_MESSAGES_COUNT][MESSAGE_SIZE];
    int count;
    int offset;
    pthread_mutex_t mutex;
};

struct pthread_data_t {
    struct message_history_t* history;
    struct windows_t* wins;
    atomic_int* keep_working;
    int fd;
};

enum colors {
    GREEN = 1,
    YELLOW,
    RED,
    CYAN
};

void ncurses_settings(void) {
    cbreak();  
    noecho();
    keypad(stdscr, TRUE);
    curs_set(1);

    start_color();

    init_pair(GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(RED, COLOR_RED, COLOR_BLACK);
    init_pair(CYAN, COLOR_CYAN, COLOR_BLACK);

    refresh();
}

void create_windows(struct windows_t* wins) {
    int max_y = 0;
    int max_x = 0;

    getmaxyx(stdscr, max_y, max_x);

    wins->chat_height = max_y - INPUT_WIN_HEIGHT - STATUS_WIN_HEIGHT;
    wins->chat_width = max_x;

    wins->chat_win = newwin(wins->chat_height, wins->chat_width, 0, 0);
    wins->status_win = newwin(STATUS_WIN_HEIGHT, wins->chat_width, wins->chat_height, 0);
    wins->input_win = newwin(INPUT_WIN_HEIGHT, wins->chat_width, wins->chat_height + STATUS_WIN_HEIGHT, 0);

    box(wins->input_win, 0, 0);
    box(wins->chat_win, 0, 0);

    scrollok(wins->chat_win, TRUE);

    keypad(wins->input_win, TRUE);

    wrefresh(wins->chat_win);
    wrefresh(wins->status_win);
    wrefresh(wins->input_win);

    refresh();
}

void add_message(struct message_history_t* history, int chat_height, char* message) {
    pthread_mutex_lock(&(history->mutex));

    wchar_t w_message[MESSAGE_SIZE] = {0};

    mbstowcs(w_message, message, MESSAGE_SIZE - 1);
    
    w_message[MESSAGE_SIZE - 1] = L'\0';

    if (history->count < MAX_MESSAGES_COUNT) {
        wcsncpy(history->messages[history->count], w_message, MESSAGE_SIZE - 1);

        history->messages[history->count][MESSAGE_SIZE - 1] = L'\0';

        history->count++;

        if (history->count > chat_height) {
            history->offset = history->count - chat_height;
        }

    } else {

        for (int i = 1; i < MAX_MESSAGES_COUNT; i++) {
            wcscpy(history->messages[i - 1], history->messages[i]);
        }

        wcsncpy(history->messages[MAX_MESSAGES_COUNT - 1], w_message, MESSAGE_SIZE - 1);

        history->messages[history->count][MESSAGE_SIZE - 1] = L'\0';

        if (history->offset > 0) {
            history->offset--;
        }

    }

    pthread_mutex_unlock(&(history->mutex));
}

void mvwprintw_with_time_prefix(WINDOW* win, int y_coord, int x_coord, wchar_t* message, enum colors color) {
    time_t mytime = time(NULL);
    struct tm *now = localtime(&mytime);

    wattron(win, COLOR_PAIR(color));

    mvwprintw(win, y_coord, x_coord, "[%02d:%02d:%02d]    %ls", now->tm_hour, now->tm_min, now->tm_sec, message);

    wattroff(win, COLOR_PAIR(color));

    wrefresh(win);
}

enum colors color_defination(wchar_t* message) {

    if (wcsstr(message, L"You:")) {
        return GREEN;
    } else if (wcsstr(message, L"joined") || wcsstr(message, L"left")) {
        return CYAN;
    } else if (wcsstr(message, L"disconnected")) {
        return RED;
    } else {
        return YELLOW;
    }

    return GREEN;
}

void redraw_chat(struct message_history_t* history, WINDOW* chat_win, int chat_height) {
    pthread_mutex_lock(&(history->mutex));

    werase(chat_win);

    int start = history->offset;
    int end = history->count;
    int y = 1;

    enum colors color = GREEN;

    for (int i = start; i < end && y < chat_height; i++, y++) {
        color = color_defination(history->messages[i]);

        mvwprintw_with_time_prefix(chat_win, y, 1, history->messages[i], color);
    }

    box(chat_win, 0, 0);

    wrefresh(chat_win);

    pthread_mutex_unlock(&(history->mutex));
}

void* recv_handle(void* arg) {
    struct pthread_data_t* p_data = (struct pthread_data_t*) arg;

    struct message_history_t* history = p_data->history;
    struct windows_t* wins = p_data->wins; 
    
    atomic_int* keep_working = p_data->keep_working;

    int fd = p_data->fd;

    free(p_data);

    char message[MESSAGE_SIZE] = {0};

    ssize_t count_of_bytes = 0;
    size_t length = 0;

    while (atomic_load(keep_working)) {
        memset(message, 0, MESSAGE_SIZE);

        count_of_bytes = recv(fd, message, MESSAGE_SIZE - 1, 0);

        if (count_of_bytes <= 0) {

            if (count_of_bytes == 0) {
                add_message(history, wins->chat_height, "Server disonnected!");
            } else {
                perror("recv_handle: recv");
            }

            atomic_store(keep_working, 0);

            break;
        }

        length = strlen(message);

        if (length > 0 && message[length - 1] == '\n') {
            message[length - 1] = '\0';
        }

        add_message(history, wins->chat_height, message);
    }

    return NULL;
}

int send_name(struct message_history_t* history, struct windows_t* wins, int fd) {
    char name[MAX_NAME_LENGTH] = {0};
    char message[MAX_NAME_LENGTH] = {0};

    echo();
    mvwprintw(wins->input_win, 1, 1, "Enter your name: "); 
    wrefresh(wins->input_win);
    
    wgetnstr(wins->input_win, name, MAX_NAME_LENGTH - 1);
    noecho();

    werase(wins->input_win);
    box(wins->input_win, 0, 0);
    wrefresh(wins->input_win);

    nodelay(wins->input_win, TRUE);

    size_t length = strlen(name);
    ssize_t count_of_bytes = send(fd, name, length + 1, 0);

    if (count_of_bytes <= 0) {
        perror("main: send");

        return -1;
    }

    if (strcmp(name, "!anonim") == 0 || length == 0) {
        snprintf(message, MESSAGE_SIZE, "You joined as: <ANONIM>");
    } else {
        snprintf(message, MESSAGE_SIZE, "You joined as: %s", name);
    }
    
    add_message(history, wins->chat_height, message);
    redraw_chat(history, wins->chat_win, wins->chat_height);

    return 0;
}

int disconnecting_from_server(int fd, char* buffer) {
    
    if (strcmp(buffer, "!quit") == 0) {

        ssize_t count_of_bytes = send(fd, "!quit", strlen("!quit"), 0);

        if (count_of_bytes <= 0) {
            perror("disconnecting_from_server: send");
        }

        return 0;  
    }

    return -1;
}

int input_handler(struct message_history_t* history, struct windows_t* wins, int ch, int* pos, int fd, char* buffer) {

    switch (ch) {
        case KEY_UP:

            if (history->offset > 0) {
                history->offset--;

                redraw_chat(history, wins->chat_win, wins->chat_height);
            }

            break;
        
        case KEY_DOWN:
            
            if (history->offset < history->count - wins->chat_height) {
                history->offset++;

                redraw_chat(history, wins->chat_win, wins->chat_height);
            }

            break;

        case KEY_BACKSPACE:
        case 127:
        case 8:
            
            if (*pos > 0) {
                (*pos)--;
                buffer[*pos] = '\0';
            }

            break;
        
        case '\n':
            
            if (*pos > 0) {
                char message[MAX_NAME_LENGTH] = {0};

                if (disconnecting_from_server(fd, buffer) == 0) {
                    return -1;
                }

                ssize_t count_of_bytes = send(fd, buffer, *pos + 1, 0);

                if (count_of_bytes <= 0) {
                    perror("main: send");

                    return -1;
                }

                snprintf(message, MESSAGE_SIZE, "You: %s", buffer);
                add_message(history, wins->chat_height, message);
                redraw_chat(history, wins->chat_win, wins->chat_height);

                memset(buffer, 0, MESSAGE_SIZE);

                *pos = 0;
            }

            break;

        default:
            if ((isprint(ch) || ch >= 128) && *pos < MESSAGE_SIZE - 1) {
                buffer[*pos] = ch;
            
                (*pos)++;
            
                buffer[*pos] = '\0';
            }

            break;
    
    }

    return 0;
}

void cleanup(struct message_history_t* history, struct windows_t* wins, atomic_int* keep_working, pthread_t pthread, int fd) {
    add_message(history, wins->chat_height, "Shutting down connection...");
    redraw_chat(history, wins->chat_win, wins->chat_height);

    napms(1000);

    atomic_store(keep_working, 0);
    pthread_join(pthread, NULL);

    pthread_mutex_destroy(&(history->mutex));

    shutdown(fd, SHUT_RDWR);
    close(fd);

    delwin(wins->chat_win);
    delwin(wins->status_win);
    delwin(wins->input_win);

    endwin();
}

int main(int argc, char* argv[]) {
    setlocale(LC_ALL, "");

    if (argc < 2) {
        fprintf(stderr, "Usage: %s <ip>\n", argv[0]);   

        return EXIT_FAILURE;
    } 

    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("main: socket");

        return EXIT_FAILURE;
    } 

    struct sockaddr_in addr = {0};

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, argv[1],  &addr.sin_addr.s_addr) <= 0) {
        fprintf(stderr, "Incorrect address: %s\n", argv[1]);

        close(fd);

        return EXIT_FAILURE;
    }

    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        perror("main: connect");

        close(fd);

        return EXIT_FAILURE;
    }

    initscr();
    ncurses_settings();
    
    struct windows_t wins = {0};
    
    create_windows(&wins);

    struct message_history_t history = {
        .count = 0,
        .offset = 0,
        .mutex = PTHREAD_MUTEX_INITIALIZER
    };

    wattron(wins.status_win, COLOR_PAIR(CYAN));
    mvwprintw(wins.status_win, 0, 1, "Connected to server! Enter your name or '!anonim' to remain anonymous"); 
    wrefresh(wins.status_win);   
    
    if (send_name(&history, &wins, fd) < 0) {
        close(fd);

        endwin();

        return EXIT_FAILURE;
    }

    werase(wins.status_win);
    wrefresh(wins.status_win);
    mvwprintw(wins.status_win, 0, 1, "Type messages or '!quit' to exit or '!list' to get list of clients");
    wrefresh(wins.status_win);
    wattroff(wins.status_win, COLOR_PAIR(CYAN));

    atomic_int keep_working = 1;

    struct pthread_data_t* p_data = malloc(sizeof(struct pthread_data_t)); 

    if (!p_data) {
        perror("main: malloc");

        close(fd);

        endwin();

        return EXIT_FAILURE;
    }

    p_data->history = &history;
    p_data->wins = &wins;
    p_data->keep_working = &keep_working;
    p_data->fd = fd;

    pthread_t pthread = 0;

    if (pthread_create(&pthread, NULL, recv_handle, p_data) != 0) {
        perror("main: pthread_create");

        close(fd);

        free(p_data);

        endwin();

        return EXIT_FAILURE;
    }

    int ch = 0;
    int pos = 0; 
    int last_message_count = 0;

    char buffer[MESSAGE_SIZE] = {0};

    while (atomic_load(&keep_working)) {

        if (last_message_count != history.count) {
            redraw_chat(&history, wins.chat_win, wins.chat_height);

            last_message_count = history.count;
        }

        werase(wins.input_win);
        
        box(wins.input_win, 0, 0);        
        
        mvwprintw(wins.input_win, 1, 1, "> %s", buffer);
        
        wrefresh(wins.input_win);

        ch = wgetch(wins.input_win);

        if (ch == ERR) {
            napms(50);

            continue;
        }

        if (input_handler(&history, &wins, ch, &pos, fd, buffer) < 0) {
            cleanup(&history, &wins, &keep_working, pthread, fd);

            return EXIT_FAILURE;
        }

    }

    cleanup(&history, &wins, &keep_working, pthread, fd);

    return EXIT_SUCCESS;
}


