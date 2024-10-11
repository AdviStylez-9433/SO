#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAX_CANTIDAD_PALABRAS 100
#define TAMANO_MAXIMO_PALABRA 7  // 6 caracteres + '\0'
#define MAX_PARTICIPANTES 3
#define LIMITE_TIEMPO 240  // 4 minutos en segundos

char lista_palabras[MAX_CANTIDAD_PALABRAS][TAMANO_MAXIMO_PALABRA];
int numero_palabras = 0;

typedef struct {
    char nombre[50];
    int tiempo;
} Participante;

Participante tabla_ranking[3] = {{"", 0}, {"", 0}, {"", 0}};

void cargar_lista_palabras() {
    FILE *archivo = fopen("lista_palabras.txt", "r");
    if (archivo == NULL) {
        perror("Error al abrir el archivo");
        exit(1);
    }

    while (fscanf(archivo, "%6s", lista_palabras[numero_palabras]) == 1 && numero_palabras < MAX_CANTIDAD_PALABRAS) {
        numero_palabras++;
    }

    fclose(archivo);
    printf("Archivo lista_palabras.txt cargado\n");
}

char* elegir_palabra_azar() {
    return lista_palabras[rand() % numero_palabras];
}

void actualizar_ranking(const char* nombre, int tiempo) {
	int i = 0;
    for (i = 0; i < 3; i++) {
        if (tiempo < tabla_ranking[i].tiempo || tabla_ranking[i].tiempo == 0) {
			int j = 2;
            for (j = 2; j > i; j--) {
                tabla_ranking[j] = tabla_ranking[j-1];
            }
            strncpy(tabla_ranking[i].nombre, nombre, sizeof(tabla_ranking[i].nombre) - 1);
            tabla_ranking[i].tiempo = tiempo;
            break;
        }
    }
}

void mostrar_ranking(int client_socket) {
    char buffer[256];
    sprintf(buffer, "\nRanking de tiempos:\n");
    send(client_socket, buffer, strlen(buffer), 0);
	int i = 0;
    for (i = 0; i < 3; i++) {
        if (tabla_ranking[i].tiempo > 0) {
            sprintf(buffer, "%d. %s - %d segundos\n", i+1, tabla_ranking[i].nombre, tabla_ranking[i].tiempo);
            send(client_socket, buffer, strlen(buffer), 0);
        }
    }
}

void manejar_conexion(int client_socket) {
    char buffer[256];
    char nombre_jugador[50];
    char palabra[TAMANO_MAXIMO_PALABRA];
    char palabra_adivinada[TAMANO_MAXIMO_PALABRA];
    char letras_usadas[27] = {0};
    int intentos = 0;
    time_t tiempo_inicio, tiempo_actual;

    // Solicitar nombre del jugador
    send(client_socket, "Ingresa tu nombre: ", 19, 0);
    recv(client_socket, nombre_jugador, sizeof(nombre_jugador), 0);
    nombre_jugador[strcspn(nombre_jugador, "\n")] = 0;

    // Seleccionar palabra aleatoria
    strcpy(palabra, elegir_palabra_azar());
    memset(palabra_adivinada, '_', strlen(palabra));
    palabra_adivinada[strlen(palabra)] = '\0';

    tiempo_inicio = time(NULL);

    while (1) {
        sprintf(buffer, "\nPalabra: %s\nIntentos restantes: %d\n", palabra_adivinada, 7 - intentos);
        send(client_socket, buffer, strlen(buffer), 0);
        recv(client_socket, buffer, sizeof(buffer), 0);
        char letra = tolower(buffer[0]);

        if (letra < 'a' || letra > 'z') {
            send(client_socket, "Letra invalida\n", 38, 0);
            continue;
        }

        if (letras_usadas[letra - 'a']) {
            send(client_socket, "Letra ya utilizada\n", 42, 0);
            continue;
        }

        letras_usadas[letra - 'a'] = 1;

        int acierto = 0;
		int i;
        for (i = 0; palabra[i]; i++) {
            if (palabra[i] == letra) {
                palabra_adivinada[i] = letra;
                acierto = 1;
            }
        }

        if (!acierto) {
			send(client_socket, "Letra Incorrecta.\n", strlen("Letra Incorrecta.\n"), 0);
			intentos++;			
        }

        tiempo_actual = time(NULL);
        if (difftime(tiempo_actual, tiempo_inicio) >= LIMITE_TIEMPO) {
            sprintf(buffer, "\n¡Tiempo agotado! La palabra era: %s\n", palabra);
            send(client_socket, buffer, strlen(buffer), 0);
            break;
        }

        if (strcmp(palabra, palabra_adivinada) == 0) {
            int tiempo_total = difftime(tiempo_actual, tiempo_inicio);
            sprintf(buffer, "\n¡Felicidades! Has adivinado la palabra %s en %d segundos.\n", palabra, tiempo_total);
            send(client_socket, buffer, strlen(buffer), 0);
            actualizar_ranking(nombre_jugador, tiempo_total);
            break;
        }

        if (intentos >= 7) {
            sprintf(buffer, "\n¡Has perdido! La palabra era: %s\n", palabra);
            send(client_socket, buffer, strlen(buffer), 0);
            break;
        }
    }

    mostrar_ranking(client_socket);
    close(client_socket);
}

void sigchld_handler(int s) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int iniciar_servidor() {
    srand(time(NULL));
    cargar_lista_palabras();

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error al crear el socket");
        exit(1);
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);
    
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error en bind");
        exit(1);
    }
    
    if (listen(server_socket, 5) == -1) {
        perror("Error en listen");
        exit(1);
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Error en sigaction");
        exit(1);
    }

    printf("Servidor cargado.\n");

    int jugadores_activos = 0;

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("Error en accept");
            continue;
        }

        if (jugadores_activos >= MAX_PARTICIPANTES) {
            char *mensaje = "Servidor está lleno. Intenta más tarde.\n";
            send(client_socket, mensaje, strlen(mensaje), 0);
            close(client_socket);
            continue;
        }

        jugadores_activos++;

        printf("Nueva conexión desde %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        pid_t pid = fork();
        if (pid == 0) {  // Proceso hijo
            close(server_socket);
            manejar_conexion(client_socket);
            exit(0);
        } else if (pid > 0) {  // Proceso padre
            close(client_socket);
        } else {
            perror("Error en fork");
            jugadores_activos--;
        }
    }

    close(server_socket);
    return 0;
}
