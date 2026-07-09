#include "net.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

namespace net
{
	std::string get_local_addr(const std::string &dns_server)
	{
		const int sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (sock < 0) {
			return "";
		}

		sockaddr_in remote{};
		remote.sin_family = AF_INET;
		remote.sin_port = htons(53);
		if (inet_pton(AF_INET, dns_server.c_str(), &remote.sin_addr) != 1) {
			close(sock);
			return "";
		}

		// UDP 的 connect 只记录对端地址，不发送任何数据包
		if (connect(sock, reinterpret_cast<sockaddr *>(&remote), sizeof(remote)) < 0) {
			close(sock);
			return "";
		}

		// 获取本地套接字地址
		sockaddr_in local{};
		socklen_t len = sizeof(local);
		if (getsockname(sock, reinterpret_cast<sockaddr *>(&local), &len) < 0) {
			close(sock);
			return "";
		}

		char ip[INET_ADDRSTRLEN];
		if (!inet_ntop(AF_INET, &local.sin_addr, ip, sizeof(ip))) {
			close(sock);
			return "";
		}

		close(sock);
		return ip;
	}
} // namespace net
