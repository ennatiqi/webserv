/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   httpRequest.hpp                                    :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: aachfenn <aachfenn@student.42.fr>          +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/03/22 14:10:05 by aachfenn          #+#    #+#             */
/*   Updated: 2024/03/22 14:14:00 by aachfenn         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#pragma once

#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <fstream>
#include <sstream>
#include <map>
#include <dirent.h>
#include <sys/stat.h>
#include "../parcing/parceConfFile.hpp"

using std::cout;
using std::cerr;
using std::endl;
using std::string;

#define BUFFER_SIZE 4096

class httpRequest
{
public:
    int socket;
    int server_socket;
    string request;
	string method;
	string uri;
	string http_version;
	string hostname;
	string port;
	bool connection;
	double body_size;
	double content_length;
	int status;
    string location;
    string simple_uri;
	std::map<string, string> form_data;
	string filename;
    string content_type;

    string query_string;

    const int& getSocket() const
    {
        return socket;
    }

    httpRequest(const httpRequest& obj)
    {
        *this = obj;
    }

    httpRequest& operator=(const httpRequest& obj);


    httpRequest(int socket ): socket(socket), server_socket(-1), request(""), connection(false), content_length(-1) , status(200)
    {
    }
    httpRequest(int socket , int serverSocket): socket(socket), server_socket(serverSocket), request(""), connection(false), content_length(-1), status(200)
    {
    }
    
    ~httpRequest(){}
    
	void	generate_response();
	void	parce_request();
	void	extract_form_data();
	void	checks_();
	void	extract_uri_data();
	void	init_status_code();
    void    upload_files(string up_name);
	


};
