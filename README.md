# Explicación de Librerías Usadas en el Proyecto Publisher–Subscriber

Este proyecto implementa un modelo **Publicador–Suscriptor** utilizando los protocolos **TCP y UDP** en **Windows**, haciendo uso de la biblioteca **Winsock2** para la comunicación en red.

A continuación se describen las librerías empleadas y su función dentro del programa:

---

## 1. #include <stdio.h>

### Descripción:
La librería **Standard Input Output** provee las funciones básicas para manejar la entrada y salida estándar del programa.

### Funcionalidad:
Permite imprimir mensajes en consola, leer texto ingresado por el usuario y mostrar información del sistema.

### Principales funciones utilizadas:
- `printf()`: Muestra mensajes o datos por pantalla.  
- `fgets()`: Lee una línea de texto desde la entrada estándar (teclado).  



## 2. #include <stdlib.h>

### Descripción:
La librería Standard Library contiene funciones generales del lenguaje C para el manejo de memoria, control del programa y conversiones de datos.

### Funcionalidad:
Permite finalizar el programa en caso de error, gestionar memoria dinámica y convertir cadenas a valores numéricos.

### Principales funciones utilizadas:

- exit(): Termina la ejecución del programa de manera controlada.
- malloc() / free(): Reservan y liberan memoria dinámica.
- atoi(): Convierte una cadena de texto en un número entero.


## 3. #include <string.h>

### Descripción:
Esta librería contiene funciones para manipular cadenas de caracteres (strings) en C.

### Funcionalidad:
Permite copiar, comparar, concatenar o dividir cadenas de texto, lo cual es útil para procesar los mensajes enviados y recibidos entre los clientes y el broker.

### Principales funciones utilizadas:
- strcpy(): Copia el contenido de una cadena a otra.
- strcmp(): Compara dos cadenas de texto.
- strtok(): Divide una cadena en fragmentos (tokens) según un delimitador, como espacios o comas.
- strlen(): Devuelve la longitud de una cadena de texto.

## 4. #include <winsock2.h>

### Descripción:
Es la librería de red de Windows, conocida como Windows Sockets 2 (Winsock2).
Permite crear y manejar conexiones de red utilizando protocolos como TCP y UDP.

### Funcionalidad:
Proporciona todas las funciones necesarias para enviar y recibir datos, crear sockets, conectar clientes con servidores y controlar el flujo de la comunicación.

### Principales funciones utilizadas:
- WSAStartup(): Inicializa la librería Winsock antes de crear sockets.
- socket(): Crea un socket (puerto lógico de comunicación).
- bind(): Asocia un socket a una dirección IP y un puerto.
- listen() / accept(): Esperan y aceptan conexiones entrantes (solo en el servidor TCP).
- connect(): Conecta un cliente con el servidor.
- send() / recv(): Envía y recibe datos en TCP.
- sendto() / recvfrom(): Envía y recibe datos en UDP.
- closesocket(): Cierra un socket cuando ya no se usa.
- WSACleanup(): Libera los recursos utilizados por Winsock al finalizar el programa.
