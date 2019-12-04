#include <stdio.h> // for perror
#include <stdlib.h> // for atoi
#include <string.h> // for memset
#include <unistd.h> // for close
#include <arpa/inet.h> // for htons
#include <netinet/in.h> // for sockaddr_in
#include <sys/socket.h> // for socket

#include <set> // for managing socket descriptor
#include <mutex> // for lock
#include <thread>

bool b_opt_check = false;
void usage() {
    printf("syntax: echo_server <port> [-b]\n");
    printf("sample: echo_server 1234 -b\n");
}

int main(int argc, char  * argv[]) 
{
	int Sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (Sockfd == -1) {
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
	setsockopt(Sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(Sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(Sockfd, 2); // backlog
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

	std::mutex M;
	std::set<int> socket_Descriptor;
	while (true) {
		std::thread echo([](int sockfd, std::set<int> socket_descriptor, std::mutex &m) {

			m.lock();
			struct sockaddr_in addr;
			socklen_t clientlen = sizeof(sockaddr);
			int childfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
				
			if (childfd < 0) {
				perror("ERROR on accept");
				return -1;
			}
			printf("connected\n");
			socket_descriptor.insert(childfd);
			m.unlock();

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
					for(std::set<int>::iterator it = socket_descriptor.begin(); it != socket_descriptor.end(); it++){
						checksum += send(*it, buf, strlen(buf), 0);
					}
				}
				else checksum += send(childfd, buf, strlen(buf), 0);
				if (checksum == 0) {
					perror("send failed");
					break;
				}
			}

			m.lock();
			socket_descriptor.erase(childfd);
			m.unlock();

		}, Sockfd, socket_Descriptor, &M);
		echo.detach();
	}

	close(Sockfd);
}
