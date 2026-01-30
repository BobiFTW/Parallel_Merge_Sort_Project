#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "ws2_32")

#include <iostream>
#include <mutex>
#include <WinSock2.h>
#include <in6addr.h>
#include <WS2tcpip.h>
#include <ws2def.h>
#include <string.h>
#include <vector>
#include <queue>
#include <future>
#include <chrono>

#include "thread_pool.hpp"

int const BUFF_SIZE = 1024;

int const PORT = 8080;
char const ip_addr[] = "127.0.0.1";

int const UPPER_BOUND = 1000000;

char buff[BUFF_SIZE];

char input_arr[BUFF_SIZE];
char output_arr[BUFF_SIZE];

std::mutex mutex;

int global_thread_counter;

void merge(std::vector<int>& arr, int l, int mid, int h) {
	int n1 = mid - l + 1;
	int n2 = h - mid;

	std::vector<int> a1;
	for (int i = 0; i < n1; i++) {
		a1.push_back(arr[l + i]);
	}

	std::vector<int> a2;
	for (int i = 0; i < n2; i++) {
		a2.push_back(arr[mid + 1 + i]);
	}

	int k = l;
	int i = 0, j = 0;

	while (i < n1 && j < n2) {
		if (a1[i] <= a2[j]) {
			arr[k++] = a1[i++];
		}
		else {
			arr[k++] = a2[j++];
		}
	}

	while (i < n1) {
		arr[k++] = a1[i++];
	}

	while (j < n2) {
		arr[k++] = a2[j++];
	}

}

void mergeSort(std::vector<int>& arr, ThreadPool& workers, int nthreads, int l, int h) {
	if (l < h) {
		int curr_thread;

		mutex.lock();
		curr_thread = global_thread_counter;
		global_thread_counter++;
		mutex.unlock();

		int mid = (l + h) / 2;
		if (curr_thread < nthreads) {

			std::future <void> left = workers.enqueue([&] {
				mergeSort(arr, workers, nthreads, l, mid);
				});
			mergeSort(arr, workers, nthreads, mid + 1, h);

			left.get();
		}
		else {
			mergeSort(arr, workers, nthreads, l, mid);
			mergeSort(arr, workers, nthreads, mid + 1, h);
		}

		merge(arr, l, mid, h);
	}
}

int main() {

	WORD DllVersion = MAKEWORD(2, 2);
	WSADATA wsaData;

	int wsaInitError = WSAStartup(DllVersion, &wsaData);
	if (wsaInitError != 0) {
		printf("Error: Winsock dll not found!\n");
		exit(1);
	}

	printf("------Server------\n");

	SOCKET listenerfd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenerfd == INVALID_SOCKET) {
		printf("Error: Failed to create server socket!\n");
		exit(2);
	}

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));


	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);
	addr.sin_addr.S_un.S_addr = INADDR_ANY;

	if (bind(listenerfd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
		printf("Error: Failed to bind server socket!\n");
		exit(3);
	}

	int backlog = 2;

	if (listen(listenerfd, backlog) == SOCKET_ERROR) {
		printf("Error: Failed to listen for clients!\n");
		exit(4);
	}

	fd_set master;
	FD_ZERO(&master);

	FD_SET(listenerfd, &master);

	int nbytes;
	while (true) {
		fd_set sock_set = master;

		int s_count = select(0, &sock_set, nullptr, nullptr, nullptr);
		if (s_count < 0) {
			printf("Failed to perform select()!\n");
			exit(5);
		}

		for (int i = 0; i < s_count; i++) {
			SOCKET curr_sock = sock_set.fd_array[i];

			if (curr_sock == listenerfd) {
				SOCKET clientfd = accept(listenerfd, nullptr, nullptr);
				if (clientfd == INVALID_SOCKET) {
					printf("Failed to accept connection to client!\n");
					exit(6);
				}
				FD_SET(clientfd, &master);
			}
			else {
				ZeroMemory(input_arr, BUFF_SIZE);
				ZeroMemory(output_arr, BUFF_SIZE);

				ZeroMemory(buff, BUFF_SIZE);
				// revieve input data from client
				nbytes = recv(curr_sock, input_arr, BUFF_SIZE, 0);
				if (nbytes < 0) {
					printf("Failed to recieve expected result!\n");
					exit(7);
				}

				int arr_size, nthreads;
				if (sscanf(input_arr, "%d %d", &arr_size, &nthreads) != 2) {
					printf("Error: recieved invalid data!\n");
					exit(8);
				}

				std::vector<int> arr_serialized;
				std::vector<int> arr_parallel;

				for (int i = 0; i < arr_size; i++) {
					int x = std::rand() % UPPER_BOUND;
					arr_serialized.push_back(x);
					arr_parallel.push_back(x);
					
					sprintf(buff, "%d ", x);
					int nbytes = send(curr_sock, buff, BUFF_SIZE, 0);
					if (nbytes < 0) {
						printf("Failed to send data to server. Please try again.\n");
						exit(-2);
					}
				}

				global_thread_counter = 0;
				ThreadPool worker(1);
				auto startTime = std::chrono::high_resolution_clock::now();
				mergeSort(arr_serialized, worker, 1, 0, arr_size - 1);
				auto endTime = std::chrono::high_resolution_clock::now();
				std::chrono::duration <double, std::milli> ms = endTime - startTime;

				sprintf(output_arr, "Single thread merge sort performed in %f ms.\n", ms.count());
				
				// send result from single threaded merge sort back to the client
				int nbytes = send(curr_sock, output_arr, BUFF_SIZE, 0);
				if (nbytes < 0) {
					printf("Failed to send data to server. Please try again.\n");
					exit(-2);
				}

				for (int& x : arr_serialized) {
					sprintf(buff, "%d ", x);
					int nbytes = send(curr_sock, buff, BUFF_SIZE, 0);
					if (nbytes < 0) {
						printf("Failed to send data to server. Please try again.\n");
						exit(-2);
					}
				}

				global_thread_counter = 0;
				ThreadPool workers(nthreads);
				startTime = std::chrono::high_resolution_clock::now();
				mergeSort(arr_parallel, workers, nthreads, 0, arr_size - 1);
				endTime = std::chrono::high_resolution_clock::now();
				ms = endTime - startTime;

				sprintf(output_arr, "Multithreaded merge sort performed in %f ms.\n", ms.count());

				// send result from multithreaded merge sort back to the client
				nbytes = send(curr_sock, output_arr, BUFF_SIZE, 0);
				if (nbytes < 0) {
					printf("Failed to send data to server. Please try again.\n");
					exit(-3);
				}

				for (int& x : arr_parallel) {
					sprintf(buff, "%d ", x);
					int nbytes = send(curr_sock, buff, BUFF_SIZE, 0);
					if (nbytes < 0) {
						printf("Failed to send data to server. Please try again.\n");
						exit(-2);
					}
				}

				FD_CLR(curr_sock, &master);
				closesocket(curr_sock);
			}
		}
	}
	if (closesocket(listenerfd) < 0) {
		printf("Error: failed to close server socket!\n");
		exit(-1);
	}

	WSACleanup();
	return 0;
}