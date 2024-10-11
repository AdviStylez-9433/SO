#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/enchufe.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TAM_BUFFER 1024

void mostrar_ahorcado(int numero_intentos) {
    const char *dibujo[] = {
        "\n########   \n"
        "#      |   \n"
        "#\n"
        "#\n"
        "#\n"
        "#\n"
        "#\n"
        "########   \n",

        "\n########   \n"
        "#      |   \n"
        "#      O   \n"
        "#          \n"
        "#          \n"
        "#\n"
        "#\n"
        "########   \n",

        "\n########   \n"
        "#      |   \n"
        "#      O   \n"
        "#      |   \n"
        "#      |   \n"
        "#\n"
        "#\n"
        "########   \n",

        "\n########   \n"
        "#      |   \n"
        "#      O   \n"
        "#     /|   \n"
        "#    / |   \n"
        "#\n"
        "#\n"
        "########   \n",

        "\n########   \n"
        "#      |   \n"
        "#      O   \n"
        "#     /|\\  \n"
        "#    / | \\ \n"
        "#\n"
        "#\n"
        "########   \n",

        "\n########   \n"
        "#      |   \n"
        "#      O   \n"
        "#     /|\\  \n"
        "#    / | \\ \n"
        "#     /    \n"
        "#   _/     \n"
        "########   \n",

        "\n########   \n"
        "#      |   \n"
        "#      O   \n"
        "#     /|\\  \n"
        "#    / | \\ \n"
        "#     / \\  \n"
        "#   _/   \\_\n"
        "########   \n"
    };

    printf("%s", dibujo[ numero_intentos ]);
}

int iniciar_juego(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <dirección IP del servidor>\n", argv[0]);
        exit(1);
    }

    int sock = enchufe(AF_INET, SOCK_STREAM, 0);

    if (sock == -1) {
        perror("Error al crear el enchufe");
        exit(1);
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    if (inet_pton(AF_INET, argv[1], &server_addr.sin_addr) <= 0) {
        perror("Dirección IP inválida");
        exit(1);
    }

    if (conectar(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error al conectar con el servidor");
        exit(1);
    }

    printf("Conectado al servidor. ¡Comienza el juego!\n");

    char buffer[TAM_BUFFER];
    int numero_intentos = 0;

    while (1) {
        memset(buffer, 0, TAM_BUFFER);
        int bytes_received = recibir_mensaje(sock, buffer, TAM_BUFFER - 1, 0);

        if (bytes_received <= 0) {
            printf("Conexión cerrada en el servidor.\n");
            break;
        }

        printf("%s", buffer);		
		
		if (strstr(buffer, "Letra Incorrecta.")) {
            numero_intentos++;        
        }		

        if (strstr(buffer, "\n")) {
            mostrar_ahorcado(numero_intentos);
            char input[3];
            printf("Tu letra: ");
            fgets(input, sizeof(input), stdin);
            enviar_mensaje(sock, input, strlen(input), 0);

		} else if (strstr(buffer, "Ingresa tu nombre:")) {
            char nombre[50];
            fgets(nombre, sizeof(nombre), stdin);
            enviar_mensaje(sock, nombre, strlen(nombre), 0);

        }

        if (strstr(buffer, "¡Felicidades!") || strstr(buffer, "¡Has perdido!") || strstr(buffer, "¡Tiempo agotado!")) {
            printf("Juego terminado.\n");
            break;
        }
    }

    cerrar_conexion(sock);
    return 0;
}
