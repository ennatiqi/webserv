/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aachfenn <aachfenn@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2023/12/29 09:45:04 by eboulhou          #+#    #+#             */
/*   Updated: 2024/03/22 14:36:36 by aachfenn         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../INC/server.hpp"

std::map<int, httpRequest> fdMapRead;
std::map<int, httpResponse> fdMapWrite;
std::map<int, Server> servers_sockets;
std::vector<int> deleteReadFd;
std::vector<int> deleteWriteFd;
fd_set theFdSetRead[NBOFCLIENTS];
fd_set theFdSetWrite[NBOFCLIENTS];

extern std::map<int, string> status_message;
char **envv;


int  readTheRequest(std::map<int, httpRequest>::iterator& it)
{
	int commSocket;
	char buffer[BUFFER_SIZE + 1];
	int size_readed;
	string request;

	commSocket = it->first;
	bzero(buffer, BUFFER_SIZE + 1);
	size_readed = recv(commSocket, buffer, BUFFER_SIZE, 0);
	if(size_readed <= 0)
	{	
		cout << "socket connection ended " << request << endl;
		close(commSocket);
		// fdMapRead.erase(commSocket);
		deleteReadFd.push_back(commSocket);
		return 0;
	}
	else 
	{
		it->second.request.append(buffer, size_readed);
			request = it->second.request;
		if(it->second.method.empty())
		{
			size_t pos = request.find(" ");
			if(pos != string::npos)
				it->second.method = request.substr(0, pos);
		}
		size_t posofend;
		if(it->second.method == "POST" && (posofend = request.find("\r\n\r\n")) != string::npos)
		{
			if(it->second.content_length == -1)
			{
				size_t pos = request.find("Content-Length: ");
				if(pos != string::npos)
				{
					pos += 16;
					string st = request.substr(pos, request.find("\n", pos) - pos);
					it->second.content_length = std::atoi(st.c_str());
				}
			}
			if(it->second.content_length != -1 &&  (size_t)it->second.content_length ==  request.length() - (posofend + 4))
			{
				it->second.generate_response();
				fdMapWrite.insert(std::make_pair(commSocket, httpResponse(it->second)));
				fdMapWrite[commSocket].setData();
				deleteReadFd.push_back(commSocket);
			}
				return 0;
		}
		if(request.size() > 4  && request.substr(request.size() - 4) == "\r\n\r\n")
		{
			it->second.generate_response();
			fdMapWrite.insert(std::make_pair(commSocket, httpResponse(it->second)));
			fdMapWrite[commSocket].setData();
			deleteReadFd.push_back(commSocket);
			return 0;
		}
	}
	return 0;
}

int  readTheRequest(std::map<int, httpRequest>::iterator& it);
int getMaxFd()
{
	int tmp = -1;
    if(fdMapRead.size() >= 1)
	{
		std::map<int, httpRequest>::iterator it = fdMapRead.end();
		it--;
		if( it->first > tmp)
			tmp = it->first;
	}
	if(fdMapWrite.size() >= 1)
	{
		std::map<int, httpResponse>::iterator it = fdMapWrite.end();
		it--;
		if( it->first > tmp)
			tmp = it->first;
	}
	if(servers_sockets.size() >= 1)
	{
		if (servers_sockets.rbegin()->first > tmp)
		{
			tmp = servers_sockets.rbegin()->first;
		}
	}

    return tmp;
}

void refresh_fd_set(fd_set *fdRead, fd_set *fdWrite)
{
	bzero(fdRead, sizeof(fdRead) * NBOFCLIENTS);
	bzero(fdWrite, sizeof(fdWrite) * NBOFCLIENTS);
	for (std::vector<int>::iterator it = deleteReadFd.begin(); it != deleteReadFd.end(); it++)
		if(fdMapRead.find(*it) != fdMapRead.end())
			fdMapRead.erase(*it);
	for (std::vector<int>::iterator it = deleteWriteFd.begin(); it != deleteWriteFd.end(); it++)
		if(fdMapWrite.find(*it) != fdMapWrite.end())
			fdMapWrite.erase(*it);
	
	deleteReadFd.clear();
	deleteWriteFd.clear();
	for (std::map<int, Server>::iterator it  = servers_sockets.begin(); it != servers_sockets.end(); it++)
	{
		FD_SET(it->first, fdRead);
	}
    for (std::map<int, httpRequest>::iterator it = fdMapRead.begin(); it != fdMapRead.end(); it++)
    {
        FD_SET(it->first, fdRead);
    }
    for (std::map<int, httpResponse>::iterator it = fdMapWrite.begin(); it != fdMapWrite.end(); it++)
    {
        FD_SET(it->first, fdWrite);
    }
}


int connectSockets(parceConfFile cf)
{
	int port;
	string host;
	int nbOfSockets = 0;

	for (int i = 0; i < cf.server_nb ; i ++)
	{
		for (size_t pot = 0; pot < cf.server[i].listen.size(); pot++)
		{
			port = std::atoi(cf.server[i].listen[pot].c_str());
			host = cf.server[i].server_name;
			int sockfd = socket(AF_INET, SOCK_STREAM, 0);
			if(sockfd == -1)
			{
				cerr << "Failed to Create Socket"<< endl;
				return 1;
			}
			struct sockaddr_in address;
			bzero(&address, sizeof(struct sockaddr_in));
			address.sin_family = AF_INET;
			address.sin_port = htons(port);
			inet_pton(AF_INET, host.c_str(), &(address.sin_addr));
			memset(address.sin_zero, '\0', sizeof address.sin_zero);
			
			//reuse the port number after closing
			int optval = 1;
			if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
				cerr << "Error setting socket options" << endl;
				continue;
			}

			fcntl(sockfd, F_SETFL, O_NONBLOCK, FD_CLOEXEC);

			if(bind(sockfd, (struct sockaddr*)&address, sizeof(address)) == -1)
			{
				cerr << "\033[31mhost : ["<< host << "] and port : ["<< port <<"] failed to bind.\033[0m"<< endl;
				continue;
			}
			{
				cout << "\033[32mhost : ["<< host << "] and port : ["<< port <<"] has been binded.\033[0m"<< endl;
			}
			

			if(listen(sockfd, 10000) == -1)
			{
				cerr << "Failed to listen"<< endl;
				continue;
			}

			FD_ZERO(theFdSetRead);
			servers_sockets[sockfd] = cf.server[i];

			nbOfSockets ++;
		}
	}
	return nbOfSockets;
}

void acceptNewConnections(int sockfd)
{
	int datasocket = accept(sockfd, NULL, NULL);
		if(datasocket == -1)
		{
			cerr << "accept error" << endl;
			
			return ;
		}
		struct linger linger_opt = {1, 0};
		setsockopt(datasocket, SOL_SOCKET, SO_LINGER, &linger_opt, sizeof(linger_opt));

		int optval =1 ;
		if(setsockopt(datasocket, SOL_SOCKET, SO_NOSIGPIPE, &optval, sizeof(optval)) == -1)
		{
			cerr << "set Socket Options error"<<endl;
			return ;
		}
		fcntl(sockfd, F_SETFL, O_NONBLOCK, FD_CLOEXEC);
		
		
		fdMapRead.insert(std::make_pair(datasocket, httpRequest(datasocket, sockfd)));
}


void writeOnSocket(std::map<int, httpResponse>::iterator& it)
{
	int commSocket, returnNumber;

	commSocket = it->first;			


	returnNumber = it->second.sendChunk();


	if (returnNumber == 1)//if the socket is closed by peer or some error occured in send().
	{
		close(commSocket);
		deleteWriteFd.push_back(commSocket);
			}
	else if(returnNumber == 2)//all good in sendChunk().
	{
		if(it->second.connection == true)//if the connection is on keep-alive.
		{
			
		fdMapRead.insert(std::make_pair(commSocket, httpRequest(commSocket, it->second.server_socket)));
				deleteWriteFd.push_back(commSocket);
		
		}
		else //if the connection is on close.
		{
			close(commSocket);
			deleteWriteFd.push_back(commSocket);
		}
	}
}

void createHtmlFile() {
	std::ofstream file;
	for (std::map<int, string>::iterator it = status_message.begin() ; it != status_message.end(); it++)
	{
		if(it->first == 200)
			continue;
		std::ostringstream strstream;
		strstream << it->first;
		strstream << "Error.html";
		file.open(strstream.str().c_str());
	if(!file.is_open())
	{
		throw(std::runtime_error("HTMLS failed to create"));
	}
	file << "<!DOCTYPE html>\n"
		 << "<html>\n"
		 << "<head>\n"
		 << "<title>"<< it->first <<" "<< it->second <<"</title>\n"
		 << "<style>\n"
		 << "body {\n"
		 << "    font-family: Arial, sans-serif;\n"
		 << "    background-color: #f4f4f4;\n"
		 << "    text-align: center;\n"
		 << "}\n"
		 << "h1 {\n"
		 << "    color: #333;\n"
		 << "}\n"
		 << "p {\n"
		 << "    color: #777;\n"
		 << "}\n"
		 << "</style>\n"
		 << "</head>\n"
		 << "<body>\n"
		 << "<h1>"<< it->first <<" "<< it->second <<"</h1>\n"
		 << "<p>The requested page could not be found.</p>\n"
		 << "</body>\n"
		 << "</html>\n";

	file.close();
}
}


void clear_maps()
{
	for (std::__1::map<int, httpRequest>::iterator it = fdMapRead.begin(); it != fdMapRead.end(); it++)
	{
		close(it->first);
		deleteReadFd.push_back(it->first);
	}
	for (std::__1::map<int, httpResponse>::iterator it = fdMapWrite.begin(); it != fdMapWrite.end(); it++)
	{
		close(it->first);
		deleteWriteFd.push_back(it->first);
	}
}



int main(int __unused ac, char __unused **av, char **env)
{
	try {
		envv = env;
	
		parceConfFile cf;
		parce_conf_file(cf);
		init_status_code();
		createHtmlFile();
	
		struct timeval timout;
		timout.tv_sec = 3;
		timout.tv_usec = 0;
		connectSockets(cf);
		if(servers_sockets.size() == 0)
		{
			cerr << "no server is started" << endl;
			return 1;
		}
		while (1)
		{
			refresh_fd_set(theFdSetRead, theFdSetWrite);
		
			int ret = select(getMaxFd()+1, theFdSetRead, theFdSetWrite, NULL, &timout);
			if(ret == -1)
			{
				cout << "select failed!!"<< endl;
				clear_maps();
				continue;
			}
			else if(ret == 0)
			{
				for (std::map<int, httpRequest>::iterator it = fdMapRead.begin(); it != fdMapRead.end(); it++)
				{
					close(it->first);
					deleteReadFd.push_back(it->first);
				}
				continue;
			}
			
			for(std::map<int, Server>::iterator it = servers_sockets.begin(); it != servers_sockets.end(); it++)
			{
				if(FD_ISSET(it->first, theFdSetRead))
				{
					acceptNewConnections(it->first);
				}
			}
			for (std::map<int, httpRequest>::iterator it = fdMapRead.begin(); it != fdMapRead.end(); it++)
			{
				if(FD_ISSET(it->first, theFdSetRead))
				{
					readTheRequest(it);
				}
			}
			
			for (std::map<int, httpResponse>::iterator it = fdMapWrite.begin(); it != fdMapWrite.end(); it++)
			{
				if(FD_ISSET(it->first, theFdSetWrite))
				{
					writeOnSocket(it);
				}
			}
		}
	
		return 0;
	}
	catch (std::exception &e) {
		cout << e.what() << endl;
		return 1;
	}
	catch (...) {
		cout << "Error" << endl;
		return 1;
	}
}