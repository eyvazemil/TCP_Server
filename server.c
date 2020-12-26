// 29675

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/in.h>
#include <wait.h>
#include <sys/select.h>
#include <ctype.h>

//#define BUFFER_SIZE 1025
#define SERVER_KEY 54621
#define CLIENT_KEY 45328
// set timeouts
#define TIMEOUT 1
#define TIMEOUT_RECHARGING 5
// server responses
char server_move[] = "102 MOVE\a\b";
char server_turn_left[] = "103 TURN LEFT\a\b";
char server_turn_right[] = "104 TURN RIGHT\a\b";
char server_pick_up[] = "105 GET MESSAGE\a\b";
char server_logout[] = "106 LOGOUT\a\b";
char server_ok[] = "200 OK\a\b";
char server_login_failed[] = "300 LOGIN FAILED\a\b";
char server_syntax_error[] = "301 SYNTAX ERROR\a\b";
char server_logic_error[] = "302 LOGIC ERROR\a\b";
// client responses
char client_recharging[] = "RECHARGING";
char client_full_power[] = "FULL POWER";

typedef struct param {
    int _socket;
    int _client;
} Param;

// server functions
void * client_process(void *);
void authenticate(int);
void move(int);
char * receive_message(int, int, int, int *, int *);
char * charging(int, char *);
void client_ok_parse(int, char *, int *, int *);
void parse_client_response(int, int *, int *);
void move_helper(int, int *, int *, char *, char);
void pick_up_command(int);
char * receive_full_power(int, int);
// miscelaneous
void terminate(int);

int main(int argc, char ** argv) {
    // create a socket
    int _socket = socket(AF_INET, SOCK_STREAM, 0);
    // create and initialize an address structure
    struct sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(atoi(argv[1]));
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    // bind socket to ip and port
    bind(_socket, (struct sockaddr*) &address, sizeof(address));
    // set the maximum number for listening clients
    listen(_socket, 10);
    // initialize clients structure
    struct sockaddr_in client_address;
    socklen_t client_address_length;
    // accept clients
    while(1) {
        int _client = accept(_socket, (struct sockaddr *) &client_address, &client_address_length);
        pid_t new_pid = fork();
        // check if it is a child process
        if(new_pid == 0) {
            Param * tmp = (Param *) malloc(sizeof(Param));
            tmp->_socket = _socket;
            tmp->_client = _client;
            client_process((void *) tmp);
            free(tmp);
            close(_client);
            return 0;
        }
        int status = 0;
        waitpid(0, &status, WNOHANG);
        close(_client);
    }
    close(_socket);
    return 0;
}

void * client_process(void * para) {
    // extract _socket and _client from parameter
    Param * tmp = (Param *) para;
    int _socket = tmp->_socket;
    int _client = tmp->_client;
    // close listening socket
    close(_socket);
    // authenticate client
    authenticate(_client);
    // move robot
    move(_client);
}

void terminate(int _client) {
    close(_client);
    exit(0);
}

char * receive_message(int _client, int _timeout, int flag_confirmation, int * str_length, int * sum) {
    char * str_return;
    char message[101];
    char recv_buffer[2];
    memset(message, '\0', 101);
    // set timeout parameters
    struct timeval timeout;
    timeout.tv_sec = _timeout;
    timeout.tv_usec = 0;
    fd_set sockets;
    // receive
    int msg_length;
    if(sum) *sum = 0;
    for(msg_length = 0; msg_length < 101; msg_length++) {
        //if(_timeout != 5) {
        timeout.tv_sec = _timeout;
        timeout.tv_usec = 0;
        //}
        FD_ZERO(&sockets);
        FD_SET(_client, &sockets);
        if(!select(_client + 1, &sockets, NULL, NULL, &timeout)) {
            // timeout
            terminate(_client);
        }
        recv(_client, &recv_buffer, 1, 0);
        recv_buffer[1] = '\0';
        if(msg_length == 99 && recv_buffer[0] != '\b') {
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            terminate(_client);
        }
        if(msg_length > 0 && recv_buffer[0] == '\b' && message[msg_length - 1] == '\a') {
            if(flag_confirmation == 1 && msg_length > 6 && msg_length < 11) {
                send(_client, server_syntax_error, strlen(server_syntax_error), 0);
                terminate(_client);
            }
            break;
        }
        // when it should be client username
        if(flag_confirmation == 2 && msg_length == 11) {
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            terminate(_client);
        }
        // when it should be OK, but it is not OK
        if(!flag_confirmation && msg_length == 11 && !strncmp(message, "OK", 2)) {
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            terminate(_client);
        }
        //
        message[msg_length] = recv_buffer[0];
        if(sum)
            *sum += (int) recv_buffer[0];
    }
    str_return = (char *) malloc(msg_length * sizeof(str_return[0]));
    strncpy(str_return, message, msg_length - 1);
    str_return[msg_length - 1] = '\0';
    if(str_length)
        *str_length = msg_length - 1;
    if(sum) *sum -= 7;
    return str_return;
}

char * receive_full_power(int _client, int _timeout) {
    char * str_return;
    char message[13];
    char recv_buffer[2];
    memset(message, '\0', 13);
    // set timeout parameters
    struct timeval timeout;
    timeout.tv_sec = _timeout;
    timeout.tv_usec = 0;
    fd_set sockets;
    // receive
    for(int i = 0; i < 12; i++) {
        timeout.tv_sec = _timeout;
        timeout.tv_usec = 0;
        FD_ZERO(&sockets);
        FD_SET(_client, &sockets);
        if(!select(_client + 1, &sockets, NULL, NULL, &timeout)) {
            // timeout
            terminate(_client);
        }
        recv(_client, &recv_buffer, 1, 0);
        recv_buffer[1] = '\0';
        if(i == 11 && recv_buffer[0] != '\b') {
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            terminate(_client);
        }
        if(i > 0 && recv_buffer[0] == '\b' && message[i - 1] == '\a') break;
        message[i] = recv_buffer[0];
    }
    str_return = (char *) malloc(strlen(message) * sizeof(str_return[0]));
    strncpy(str_return, message, strlen(message) - 1);
    str_return[strlen(message) - 1] = '\0';
    return str_return;
}

char * charging(int _client, char * str) {
    free(str);
    char * message = receive_full_power(_client, TIMEOUT_RECHARGING);
    if(strlen(message) != 10 || (strlen(message) == 10 && strncmp(message, client_full_power, strlen(client_full_power)) != 0)) {
        // logic error
        send(_client, server_logic_error, strlen(server_logic_error), 0);
        terminate(_client);
    }
    free(message);
    // return the expected by server response
    char * str_return = receive_message(_client, TIMEOUT, 0, NULL, NULL);
    return str_return;
}

void client_ok_parse(int _client, char * str, int * x_pos, int * y_pos) {
    char x[6], y[6];
    memset(x, '\0', 6);
    memset(y, '\0', 6);
    int i, count_x = 0, count_y = 0;
    for(i = 3; str[i] != ' '; i++) {
        if(str[i] != '-' && !isdigit(str[i])) {
            // syntax error
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            free(str);
            terminate(_client);
        }
        x[count_x] = str[i];
        count_x++;
    }
    for(i++; i < strlen(str); i++) {
        if(str[i] != '-' && !isdigit(str[i])) {
            // syntax error
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            free(str);
            terminate(_client);
        }
        y[count_y] = str[i];
        count_y++;
    }
    *x_pos = atoi(x);
    *y_pos = atoi(y);
    free(str);
}

void authenticate(int _client) {
    int username_sum;
    int * hash = (int *) malloc(sizeof(int));
    int * str_length = (int *) malloc(sizeof(int));
    char * username = receive_message(_client, TIMEOUT, 2, str_length, hash);
    char hash_str[8];
    memset(hash_str, '\0', 8);
    //printf("str_length: %d\n", *str_length);
    if(*str_length > 10) {
        //puts("Hey");
        send(_client, server_syntax_error, strlen(server_syntax_error), 0);
        free(username);
        terminate(_client);
    }
    free(str_length);
    *hash = (*hash * 1000) % 65536;
    username_sum = *hash;
    *hash = (username_sum + SERVER_KEY) % 65536;
    sprintf(hash_str, "%d", *hash);
    strcat(hash_str, "\a\b");
    free(hash);
    // send server confirmation to the client
    send(_client, hash_str, strlen(hash_str), 0);
    // dealocate username
    free(username);
    // receive from client
    char * client_confirmation = receive_message(_client, TIMEOUT, 1, NULL, NULL);
    //printf("confirmation: %s\n", client_confirmation);
    if(strlen(client_confirmation) == 10) {
        if(strncmp(client_confirmation, client_recharging, strlen(client_recharging)) != 0) {
            //puts("Hey");
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            free(client_confirmation);
            terminate(_client);
        }
        // charging message(receives expected result after charging)
        client_confirmation = charging(_client, client_confirmation);
    }
    if(strlen(client_confirmation) > 5) {
        // neither charging nor confirmation
        send(_client, server_syntax_error, strlen(server_syntax_error), 0);
        free(client_confirmation);
        terminate(_client);
    }
    // if client confirmation is not a number
    for(int i = 0; i < strlen(client_confirmation); i++) {
        if(!isdigit(client_confirmation[i])) {
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            terminate(_client);
        }
    }
    // check received client confirmation
    if(atoi(client_confirmation) == (username_sum + CLIENT_KEY) % 65536) {
        send(_client, server_ok, strlen(server_ok), 0);
        // deallocate memory
        free(client_confirmation);
    } else {
        send(_client, server_login_failed, strlen(server_login_failed), 0);
        // deallocate memory
        free(client_confirmation);
        // terminate the program
        terminate(_client);
    }
}

void move(int _client) {
    // client positions
    int x_pos = 0, y_pos = 0, prev_x, prev_y;
    char direction;
    char * client_response;
    // send first two move 
    move_helper(_client, &x_pos, &y_pos, &direction, 'm');
    prev_x = x_pos;
    prev_y = y_pos;
    move_helper(_client, &x_pos, &y_pos, &direction, 'm');
    // find out the direction
    if(prev_x > x_pos) direction = 'w';
    else if(prev_x < x_pos) direction = 'e';
    else if(prev_y > y_pos) direction = 's';
    else direction = 'n';
    // get to y-plane
    if(y_pos > 0) {
        if(direction == 'n') {
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        } else if(direction == 'e')
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        else if(direction == 'w')
            move_helper(_client, &x_pos, &y_pos, &direction, 'l');
        // get to y = 0
        for(int y = y_pos; y > 0; y--)
            move_helper(_client, &x_pos, &y_pos, &direction, 'm');
    } else if(y_pos < 0) {
        if(direction == 's') {
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        } else if(direction == 'e')
            move_helper(_client, &x_pos, &y_pos, &direction, 'l');
        else if(direction == 'w')
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        // get to y = 0
        for(int y = y_pos; y < 0; y++)
            move_helper(_client, &x_pos, &y_pos, &direction, 'm');
    }
    // get to x-plane
    if(x_pos > 0) {
        if(direction == 'n')
            move_helper(_client, &x_pos, &y_pos, &direction, 'l');
        else if(direction == 'e') {
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        } else if(direction == 's')
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        // get to x = 0
        for(int x = x_pos; x > 0; x--)
            move_helper(_client, &x_pos, &y_pos, &direction, 'm');
    } else if(x_pos < 0) {
        if(direction == 's')
            move_helper(_client, &x_pos, &y_pos, &direction, 'l');
        else if(direction == 'n')
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        else if(direction == 'w') {
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
            move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        }
        // get to x = 0
        for(int x = x_pos; x < 0; x++)
            move_helper(_client, &x_pos, &y_pos, &direction, 'm');
    }
    /* now the robot is in coordinates (0, 0) */
    // set robot to correct direction
    if(direction == 'n')
        move_helper(_client, &x_pos, &y_pos, &direction, 'r');
    else if(direction == 's')
        move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    else if(direction == 'w') {
        move_helper(_client, &x_pos, &y_pos, &direction, 'r');
        move_helper(_client, &x_pos, &y_pos, &direction, 'r');
    }
    /* start travelling around and picking up */
    pick_up_command(_client);
    move_helper(_client, &x_pos, &y_pos, &direction, 'm');
    pick_up_command(_client);
    move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    move_helper(_client, &x_pos, &y_pos, &direction, 'm');
    pick_up_command(_client);
    move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    for(int i = 0; i < 2; i++) {
        move_helper(_client, &x_pos, &y_pos, &direction, 'm');
        pick_up_command(_client);
    }
    move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    for(int i = 0; i < 2; i++) {
        move_helper(_client, &x_pos, &y_pos, &direction, 'm');
        pick_up_command(_client);
    }
    move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    for(int i = 0; i < 3; i++) {
        move_helper(_client, &x_pos, &y_pos, &direction, 'm');
        pick_up_command(_client);
    }
    move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    for(int i = 0; i < 3; i++) {
        move_helper(_client, &x_pos, &y_pos, &direction, 'm');
        pick_up_command(_client);
    }
    move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    for(int i = 0; i < 4; i++) {
        move_helper(_client, &x_pos, &y_pos, &direction, 'm');
        pick_up_command(_client);
    }
    move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    for(int i = 0; i < 4; i++) {
        move_helper(_client, &x_pos, &y_pos, &direction, 'm');
        pick_up_command(_client);
    }
    move_helper(_client, &x_pos, &y_pos, &direction, 'l');
    for(int i = 0; i < 4; i++) {
        move_helper(_client, &x_pos, &y_pos, &direction, 'm');
        pick_up_command(_client);
    }
    terminate(_client);
}

void move_helper(int _client, int * x_pos, int * y_pos, char * direction, char dir) {
    int prev_x = *x_pos, prev_y = *y_pos;
    char * client_response;
    if(dir != 'm') {
        // turn command
        if(dir == 'r') {
            send(_client, server_turn_right, strlen(server_turn_right), 0);
            // change direction
            if(*direction == 'n') *direction = 'e';
            else if(*direction == 'e') *direction = 's';
            else if(*direction == 's') *direction = 'w';
            else *direction = 'n';
        } else {
            send(_client, server_turn_left, strlen(server_turn_left), 0);
            // change direction
            if(*direction == 'n') *direction = 'w';
            else if(*direction == 'e') *direction = 'n';
            else if(*direction == 's') *direction = 'e';
            else *direction = 's';
        }
        parse_client_response(_client, x_pos, y_pos);
    } else {
        // move command
        while(1) {
            send(_client, server_move, strlen(server_move), 0);
            parse_client_response(_client, x_pos, y_pos);
            if(prev_x != *x_pos || prev_y != *y_pos) break;
            prev_x = *x_pos;
            prev_y = *y_pos;
        }
    }
}

void parse_client_response(int _client, int * x_pos, int * y_pos) {
    char * client_response = receive_message(_client, TIMEOUT, 0, NULL, NULL);
    if(strlen(client_response) <= 10) {
        if(strlen(client_response) == 10 && !strncmp(client_response, client_recharging, strlen(client_recharging))) {
            // charging message(receives expected result after charging)
            client_response = charging(_client, client_response);
        }
        if(!strncmp(client_response, "OK", 2)) {
            // CLIENT_OK response
            client_ok_parse(_client, client_response, x_pos, y_pos);
        } else {
            // syntax error
            send(_client, server_syntax_error, strlen(server_syntax_error), 0);
            free(client_response);
            terminate(_client);
        }
    } else {
        // syntax error
        send(_client, server_syntax_error, strlen(server_syntax_error), 0);
        free(client_response);
        terminate(_client);
    }
}

void pick_up_command(int _client) {
    char * client_response;
    send(_client, server_pick_up, strlen(server_pick_up), 0);
    // client will send message or recharging
    client_response = receive_message(_client, TIMEOUT, 0, NULL, NULL);
    if(strlen(client_response) == 10 && !strncmp(client_response, client_recharging, strlen(client_recharging)))
        client_response = charging(_client, client_response);
    if(strlen(client_response)) {
        // message is not empty(logout and terminate connection)
        send(_client, server_logout, strlen(server_logout), 0);
        free(client_response);
        terminate(_client);
    }
    free(client_response);
}