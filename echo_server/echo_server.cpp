#include <stdio.h> // for perror
#include <stdlib.h> // for atoi
#include <string.h> // for memset
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket
#include <pthread.h> // for thread

#include <thread> // for thread
#include <mutex> // for lock

using namespace std;

bool b_opt_check = false;
void usage() {
    printf("syntax: echo_server <port> [-b]\n");
    printf("sample: echo_server 1234 -b\n");
}

thread echo_client[1001]; // thread arr
bool using_client[1001]; // false -> now used, true -> used
int client_childfd[1001];

void echo(int sockfd, mutex& m, int now_index){
	
	m.lock();
	struct sockaddr_in addr;
	socklen_t clientlen = sizeof(sockaddr);
	int childfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
	m.unlock();

	if (childfd < 0) {
		perror("ERROR on accept");
	}
	else{
		printf("connected\n");
		using_client[now_index] = true;
		client_childfd[now_index] = childfd;

		while (true) {
			const static int BUFSIZE = 1024;
			char buf[BUFSIZE];

			ssize_t received = recv(childfd, buf, BUFSIZE - 1, 0);
			if (received == 0 || received == -1) {
				perror("recv failed");
				break;
			}
			buf[received] = '\0';
			printf("%s\n", buf);

			int checksum = 0;
			if(b_opt_check){
				for(int i = 0; i < 1001; i++){
					if(using_client[i]) checksum += send(client_childfd[i], buf, strlen(buf), 0);
				}
			}
			else checksum += send(childfd, buf, strlen(buf), 0);
			if (checksum == 0) {
				perror("send failed");
				break;
			}
		}
		m.lock();
		using_client[now_index] = false;
		m.unlock();
	}
}

int main(int argc, char  * argv[]) 
{
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}
	if (argc != 2 && argc != 3){
        usage();
        return -1;
    }
	if(argc == 3){
		if(strncmp(argv[2], "-b", 2) == 0) b_opt_check = true;
		else{
			usage();
			return -1;
		}
	}
    int port = atoi(argv[1]);

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 2); // backlog
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

	mutex M;
	while (true) {
		int now_index = -1;
		for(int i = 0; i < 1001; i++){
			if(!using_client[i]) now_index = i;
		}
		if(now_index == -1) continue;
		echo_client[now_index] = thread(sockfd, ref(M), now_index);
	}

	close(sockfd);
}

// void *echo(int sockfd, std::set<int>& socket_descriptor){