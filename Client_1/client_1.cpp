#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ws2_32")

#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <in6addr.h>
#include <WS2tcpip.h>
#include <ws2def.h>
#include <string.h>

int const BUFF_SIZE = 1024;

int const PORT = 8080;
char const ip_addr[] = "127.0.0.1";

int nbytes;
char input_arr[BUFF_SIZE] = "";
char output_arr[BUFF_SIZE] = "";

void flush_stdin() {
	char c;
	while ((c = getchar()) != '\n' && c != EOF) {}
}

void compressUserInput(char* input, int& arr_size) {
	int n;

	char buff[BUFF_SIZE];
	while (true) {
		printf("Please enter the array size: ");

		if (scanf("%d", &n) == 1 && n > 0) {
			sprintf(input, "%d ", n);
			break;
		}

		printf("Invalid input detected: expected to recieve a positive integer value. Please try again.\n");
		flush_stdin();
	}
	flush_stdin();

	arr_size = n;

	int nthreads;
	while (true) {
		printf("Please enter the number of threads: ");
		if (scanf("%d", &nthreads) == 1 && nthreads > 0) {
			sprintf(buff, "%d", nthreads);
			break;
		}

		printf("Invalid input detected: expected to recieve a positive integer value. Please try again.\n");
		flush_stdin();
	}
	flush_stdin();
	strcat(input, buff);
}

int main() {
	printf("------Client 1------\n");

	WORD DllVersion = MAKEWORD(2, 2);
	WSADATA wsaData;

	int wsaInitError = WSAStartup(DllVersion, &wsaData);
	if (wsaInitError != 0) {
		printf("Error: Winsock dll not found!\n");
		exit(1);
	}

	SOCKET clientfd = socket(AF_INET, SOCK_STREAM, 0);
	if (clientfd == INVALID_SOCKET) {
		printf("Error: Failed to create client socket!\n");
		exit(2);
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));


	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.s_addr = inet_addr(ip_addr);

	if (connect(clientfd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		printf("Error: Failed to connect to server socket!\n");
		exit(3);
	}

	ZeroMemory(input_arr, BUFF_SIZE);
	ZeroMemory(output_arr, BUFF_SIZE);

	int arr_size;
	// Recieve user input and compress it in a single string message
	compressUserInput(input_arr, arr_size);

	std::ofstream output_file("client1.txt", std::ios::out | std::ios::trunc);

	// Send recieved message to server
	if (send(clientfd, input_arr, BUFF_SIZE, 0) < 0) {
		output_file << "Failed to send data to server. Please try again.\n";
		exit(4);
	}

	// recieve array of integers data before sorting
	output_file << "Generated array of integers: ";
	for (int i = 0; i < arr_size; i++) {
		nbytes = recv(clientfd, output_arr, BUFF_SIZE, 0);
		if (nbytes < 0) {
			output_file << "Failed to recieve expected result! Please try again.\n";
			exit(5);
		}

		output_file << output_arr;
	}
	output_file << "\n";

	// recieve result from single threaded merge sort
	ZeroMemory(output_arr, BUFF_SIZE);
	nbytes = recv(clientfd, output_arr, BUFF_SIZE, 0);
	if (nbytes < 0) {
		output_file << "Failed to recieve expected result! Please try again.\n";
		exit(6);
	}

	output_file << output_arr;

	output_file << "Resulting array after single-threaded sorting: ";
	for (int i = 0; i < arr_size; i++) {
		nbytes = recv(clientfd, output_arr, BUFF_SIZE, 0);
		if (nbytes < 0) {
			output_file << "Failed to recieve expected result! Please try again.\n";
			exit(7);
		}

		output_file << output_arr;
	}
	output_file << "\n";

	// recieve result from multithreaded merge sort
	ZeroMemory(output_arr, BUFF_SIZE);
	nbytes = recv(clientfd, output_arr, BUFF_SIZE, 0);
	if (nbytes < 0) {
		output_file << "Failed to recieve expected result! Please try again.\n";
	}

	output_file << output_arr;

	output_file << "Resulting array after multithreaded sorting: ";
	for (int i = 0; i < arr_size; i++) {
		nbytes = recv(clientfd, output_arr, BUFF_SIZE, 0);
		if (nbytes < 0) {
			output_file << "Failed to recieve expected result! Please try again.\n";
			exit(7);
		}

		output_file << output_arr;
	}
	output_file << "\n";

	if (closesocket(clientfd) < 0) {
		output_file << "Error: failed to close client socket!\n";
		exit(-1);
	}

	WSACleanup();
	
	output_file.close();
	return 0;
}	