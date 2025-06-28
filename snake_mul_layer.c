#include <SDL2/SDL_events.h>
#include <SDL2/SDL_keycode.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
#include <stdio.h>
#include <SDL2/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <threads.h>
#include <pthread.h>
#include <unistd.h>

#define WIDTH 900
#define HEIGHT 600

#define CELL_SIZE 25
#define ROW_LINE_HEIGHT 2
#define COL_LINE_WIDTH 2
#define ROWS HEIGHT/CELL_SIZE
#define COLUMNS WIDTH/CELL_SIZE


#define COLOR_GRAY 0x1f1f1f1f
#define COLOR_WHITE 0xffffff
#define COLOR_BLACK 0x0
#define COLOR_RED 0xff0000
#define COLOR_BLUE 0x3498DB
#define COLOR_GREEN 0x2ECC71
#define COLOR_YELLOW 0xF1C40F


struct snakeElement {
    int x;
    int y;
    struct snakeElement *pnext;
    struct snakeElement *prev;
};
struct appleElement {
    int x;
    int y;
};

struct directionElement {
    int x;
    int y;
};

struct snakePacket {
    int x;
    int y;
    int color;
};

struct enemyThreadArgs {
    int sock_fd;
    int port;
    char mode[24];
    char server_ip[24];
    struct sockaddr_in client_addr;
};


struct snakePacket enemy_snake[1024] = {0};
struct appleElement enemy_apple = {0};

int enemy_snake_len = 0;
int my_snake_len = 1;
int my_color = 0x000000ff;
int reset_my_apple = 0;

char reset_buf[8] = {0};

struct snakeElement *g_psnake;
struct appleElement myApple = {0};

int server_sock(int port, struct sockaddr_in *client_addr)
{
    int sock_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    struct sockaddr_in server_addr;
    char buf[1024] = {0};

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    int client_count = 0;
    int enemy_snake_len = 0;

    while(client_count < 1) {
        socklen_t socklen = sizeof(struct sockaddr_in);
        recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr *)client_addr, &socklen);
        if(strncmp(buf, "JOIN", sizeof(buf)) == 0) {
            client_count++;
        }
        memset(buf, 0, sizeof(buf));
        recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr *)client_addr, &socklen);
        char *ptr = NULL;
        if((ptr = strstr(buf, "SNAKE_LENGTH")) != NULL) {
            ptr = strchr(ptr, '=');
            ptr++;
            enemy_snake_len = atoi(ptr);
        }
        memset(buf, 0, sizeof(buf));
        recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr *)client_addr, &socklen);
    }
    struct snakePacket *enemy_data = (struct snakePacket *)buf;
    printf("Position of player 1:  %d,%d\n", enemy_data->x, enemy_data->y);
    return sock_fd;
}

int client_sock(int port, char *ip_addr)
{
    int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in client_addr;
    struct snakePacket snake = {5, 7};
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(port);
    client_addr.sin_addr.s_addr = inet_addr(ip_addr);

    sendto(sock_fd, "JOIN", 4, 0, (struct sockaddr*)&client_addr, sizeof(struct sockaddr_in));
    sendto(sock_fd, "SNAKE_LENGTH=3", 14, 0, (struct sockaddr *) &client_addr, sizeof(struct sockaddr_in));
    sendto(sock_fd, &snake, sizeof(struct snakePacket), 0, (struct sockaddr *) &client_addr, sizeof(struct sockaddr_in));
    return sock_fd;
}

int draw_grid(SDL_Surface *psurface)
{
    SDL_Rect row_line = {0, 0, WIDTH, ROW_LINE_HEIGHT};

    for(row_line.y = 0; row_line.y < HEIGHT; row_line.y += CELL_SIZE) {
        SDL_FillRect(psurface, &row_line, COLOR_GRAY);
    }

    SDL_Rect col_line = {0, 0, COL_LINE_WIDTH, HEIGHT};
    for(col_line.x = 0; col_line.x < WIDTH; col_line.x += CELL_SIZE) {
        SDL_FillRect(psurface, &col_line, COLOR_GRAY);
    }
}

int move_snake(struct snakeElement *psnake, struct directionElement *pdirection)
{
    if(psnake->pnext == NULL && psnake->prev == NULL) {
        psnake->x += pdirection->x;
        psnake->y += pdirection->y;
        return 0;
    }
    if(psnake->pnext == NULL)
    {
        psnake->x = psnake->prev->x;
        psnake->y = psnake->prev->y;
        return 0;
    }
    move_snake(psnake->pnext, pdirection);
    if(psnake->prev != NULL)
    {
        psnake->x = psnake->prev->x;
        psnake->y = psnake->prev->y;
    }
    else if(psnake->prev == NULL) {
        psnake->x += pdirection->x;
        psnake->y += pdirection->y;
    }

}

void fill_cell(SDL_Surface *psurface, int x, int y, uint32_t color)
{
    SDL_Rect rect = {x*CELL_SIZE, y*CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_FillRect(psurface, &rect, color);
}

int draw_snake(SDL_Surface *psurface, struct snakeElement *psnake)
{
    if(psnake != NULL)
    {
        fill_cell(psurface, psnake->x, psnake->y, my_color);
        draw_snake(psurface, psnake->pnext);
    }
}

int check_snake_collision(struct snakeElement *psnake)
{
    struct snakeElement *temp = psnake->pnext;
    if(psnake->x >= COLUMNS || psnake->y >= ROWS || psnake->x < 0 || psnake->y < 0)
        return 1;
    while(temp != NULL) {
        if(psnake->x == temp->x && psnake->y == temp->y)
            return 1;
        temp = temp->pnext;
    }
    return 0;
}
void reset_apple(struct snakeElement *psnake, struct appleElement *papple)
{
    papple->x = COLUMNS * ((double)rand()/RAND_MAX);
    papple->y = ROWS * ((double)rand()/RAND_MAX);
    if(psnake != NULL) {
        struct snakeElement *temp = psnake;
        while(temp != NULL) {
            if(temp->x == papple->x && temp->y == papple->y) {
                reset_apple(psnake, papple);
                break;
            }
            temp=temp->pnext;
        }
    } 
    else {
        for(int i = 0; i < enemy_snake_len; i++) {
            if(enemy_snake[i].x == papple->x && enemy_snake[i].y == papple->y) {
                reset_apple(NULL, papple);
                break;
            }
        }
    }
}

int draw_apple(SDL_Surface *psurface, struct appleElement *papple)
{
    SDL_Rect rect = {papple->x*CELL_SIZE, papple->y*CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_FillRect(psurface, &rect, COLOR_RED);
}

struct snakeElement * lengthen_snake(struct snakeElement *psnake, struct directionElement *pdirection)
{
    struct snakeElement *new_head = malloc(sizeof(struct snakeElement));
    new_head->x = psnake->x + pdirection->x;
    new_head->y = psnake->y + pdirection->y;
    new_head->pnext = psnake;
    new_head->prev = NULL;
    psnake->prev = new_head;
    my_snake_len++;
    return new_head;
}

void send_my_snake(struct snakeElement *psnake, int sock_fd, struct sockaddr_in *client_addr) 
{
    struct snakePacket snake_arr[my_snake_len];
    struct snakeElement *temp = psnake;
    int i = 0;
    while(temp != NULL) {
        snake_arr[i].x = temp->x;
        snake_arr[i].y = temp->y;
        snake_arr[i].color = my_color;
        i++;
        temp = temp->pnext;
    }
    char buf[24] = {0};
    snprintf(buf, sizeof(buf), "SNAKE_LENGTH=%d", my_snake_len);
    sendto(sock_fd, buf, sizeof(buf), 0, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
    sendto(sock_fd, snake_arr, sizeof(snake_arr), 0, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
    sendto(sock_fd, &myApple, sizeof(myApple), 0, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
    sendto(sock_fd, reset_buf, sizeof(reset_buf), 0, (struct sockaddr *)client_addr, sizeof(struct sockaddr_in));
    memset(reset_buf, 0, sizeof(reset_buf));
}

int recv_enemy_snake(struct snakePacket *enemy_snake, int sock_fd)
{
    int enemy_snake_len = 0;
    char buf[32] = {0};
    struct sockaddr_in client_addr;
    socklen_t socklen = sizeof(struct sockaddr_in);
    memset(buf, 0, sizeof(buf));

    recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr *)&client_addr, &socklen);
    char *ptr = NULL;
    if((ptr = strstr(buf, "SNAKE_LENGTH")) != NULL) {
        ptr = strchr(ptr, '=');
        ptr++;
        enemy_snake_len = atoi(ptr);
    }
    recvfrom(sock_fd, enemy_snake, 1024, 0, (struct sockaddr*)&client_addr, &socklen);
    recvfrom(sock_fd, &enemy_apple, sizeof(enemy_apple), 0, (struct sockaddr*)&client_addr, &socklen);
    memset(buf, 0, sizeof(buf));
    recvfrom(sock_fd, buf, sizeof(buf), 0, (struct sockaddr*)&client_addr, &socklen);
    if(strncmp(buf,"reset", 5) == 0) {
        reset_my_apple = 1;
    }
    return enemy_snake_len;
}

int draw_enemy_apple(SDL_Surface *psurface)
{
    SDL_Rect rect = {enemy_apple.x*CELL_SIZE, enemy_apple.y*CELL_SIZE, CELL_SIZE, CELL_SIZE};
    SDL_FillRect(psurface, &rect, COLOR_RED);
}

void draw_enemy_snake(SDL_Surface *psurface, struct snakePacket *enemy_snake)
{
    for(int i = 0; i < enemy_snake_len; i++)
    {
        fill_cell(psurface, enemy_snake[i].x, enemy_snake[i].y, enemy_snake[i].color);
    }
}

void *enemy_thread_handler(void *arg)
{
    struct enemyThreadArgs *data = (struct enemyThreadArgs*)arg;
    while(1){
        if(strncmp(data->mode, "client", sizeof(data->mode)) == 0) {
            data->client_addr.sin_port = htons(data->port);
            data->client_addr.sin_family = AF_INET;
            data->client_addr.sin_addr.s_addr = inet_addr(data->server_ip);
            send_my_snake(g_psnake, data->sock_fd, &data->client_addr);
            enemy_snake_len = recv_enemy_snake(enemy_snake, data->sock_fd);
        }
        else if(strncmp(data->mode, "server", sizeof(data->mode)) == 0) {
            enemy_snake_len = recv_enemy_snake(enemy_snake, data->sock_fd);
            send_my_snake(g_psnake, data->sock_fd, &data->client_addr);
        }
        //usleep(20000);
    }
}

void get_user_color()
{
    char color_buf[8] = {0};
    printf("Enter the color for player from below : \n");
    printf("blue, green, yellow, white\n");
    printf("Your option : ");
    scanf("%s", color_buf);
    if(strncmp(color_buf, "blue", sizeof(color_buf)) == 0) {
        my_color = COLOR_BLUE;
    }
    else if(strncmp(color_buf, "green", sizeof(color_buf)) == 0) {
        my_color = COLOR_GREEN;
    }
    else if(strncmp(color_buf, "yellow", sizeof(color_buf)) == 0) {
        my_color = COLOR_YELLOW;
    }
    else if(strncmp(color_buf, "white", sizeof(color_buf)) == 0) {
        my_color = COLOR_WHITE;
    }
}

int main(int argc, char *argv[])
{
    srand(time(NULL));
    // struct snakeElement *psnake = malloc(sizeof(struct snakeElement));
    g_psnake = malloc(sizeof(struct snakeElement));
    struct snakeElement *psnake = g_psnake;
    psnake->x = 5;
    psnake->y = 5;
    psnake->pnext = NULL;
    psnake->prev = NULL;

    char mode[8] = {0};
    char server_ip[24] = {0};
    int port = 0;
    int sock_fd = 0;

    struct sockaddr_in client_addr;
    printf("Enter the mode client or server\n");
    scanf("%s", mode);
    if (strncmp(mode, "server", sizeof(mode)) == 0) {
        printf("Enter the port number : ");
        scanf("%d", &port);
        get_user_color();
        sock_fd = server_sock(port, &client_addr);
    }
    else if (strncmp(mode, "client", sizeof(mode)) == 0) {
        printf("Enter the port number : ");
        scanf("%d", &port);
        printf("Enter the ip address of server : ");
        scanf("%s", server_ip);
        get_user_color();
        sock_fd = client_sock(port, server_ip);
    }

    struct appleElement *papple = &myApple;
    reset_apple(psnake,papple);
    struct directionElement direction = {0,0};
    struct directionElement *pdirection = &direction;

    pthread_t thread;
    struct enemyThreadArgs *arg = malloc(sizeof(struct enemyThreadArgs));
    strncpy(arg->mode, mode, 24);
    strncpy(arg->server_ip, server_ip, 24);
    arg->port = port;
    arg->sock_fd = sock_fd;
    arg->client_addr = client_addr;

    pthread_create(&thread, NULL, enemy_thread_handler, (void*)arg);
    int game = 1;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window *window = SDL_CreateWindow("Snake game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    SDL_Surface *psurface =  SDL_GetWindowSurface(window);
    SDL_Event event;
    SDL_Rect overrid_rect = {0, 0, WIDTH, HEIGHT};
    while(game) {
        while(SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                game = 0;
            }
            if(event.type == SDL_KEYDOWN) {
                pdirection->x = 0;
                pdirection->y = 0;
                if(event.key.keysym.sym == SDLK_UP)
                    pdirection->y = -1;
                if(event.key.keysym.sym == SDLK_DOWN)
                    pdirection->y = 1;
                if(event.key.keysym.sym == SDLK_RIGHT)
                    pdirection->x = 1;
                if(event.key.keysym.sym == SDLK_LEFT)
                    pdirection->x = -1;
            }
        }
        SDL_FillRect(psurface, &overrid_rect, COLOR_BLACK);

        if(reset_my_apple == 1) {
            reset_apple(psnake, papple);
            reset_my_apple = 0;
        }

        move_snake(psnake, pdirection);
        /* Snake Eats Apple */
        if(psnake->x == papple->x && psnake->y == papple->y) {
            reset_apple(psnake, papple);
            psnake = lengthen_snake(psnake, pdirection);
            g_psnake = psnake;
        }
        if(psnake->x == enemy_apple.x && psnake->y == enemy_apple.y) {
            printf("x=%d, y = %d\n", psnake->x, psnake->y);
            printf("Eats enemy apple\n");
            strncpy(reset_buf, "reset", sizeof(reset_buf));
            psnake = lengthen_snake(psnake, pdirection);
            g_psnake = psnake;
        }
        if(check_snake_collision(psnake) == 1) {
            game = 0;
            printf("Snake collision happen\n");
            break;
        }
        draw_apple(psurface, papple);
        draw_snake(psurface, psnake);
        draw_enemy_apple(psurface);
        draw_enemy_snake(psurface, enemy_snake);
        draw_grid(psurface);
        SDL_UpdateWindowSurface(window);
        SDL_Delay(200);
    }
}
