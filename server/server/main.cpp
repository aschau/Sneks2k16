#include <stdlib.h>
#include <iostream>
#include <string>
#include <sstream>
#include <time.h>
#include <random>
#include <chrono>
#include "websocket.h"

using namespace std;

//le webserver
webSocket server;

/* called when a client connects */
void openHandler(int clientID){
    ostringstream os;
    os << "Scumbag " << clientID << " has joined.";

	//if more than 2 clients, reject new ones
	if (clientID > 1)
	{
		server.wsClose(clientID);
		return;
	}


	vector<int> clientIDs = server.getClientIDs();

	//if second client has joined the ring, send start game to both clients
	if (clientID == 1)
	{
		for (int i = 0; i < clientIDs.size(); i++){
			server.wsSend(clientIDs[i], "Start");
		}
	}

    for (int i = 0; i < clientIDs.size(); i++){
        if (clientIDs[i] != clientID)
            server.wsSend(clientIDs[i], os.str());
    }
    server.wsSend(clientID, "Eggs hatched!");
}

/* called when a client disconnects */
void closeHandler(int clientID){
    ostringstream os;
    os << "Scumbag " << clientID << " has left.";

    vector<int> clientIDs = server.getClientIDs();

	//if a client disconncets, send end to remaining client to stop the game
	for (int i = 0; i < clientIDs.size(); i++){
		server.wsSend(clientIDs[i], "End");
	}

    for (int i = 0; i < clientIDs.size(); i++){
            server.wsSend(clientIDs[i], os.str());
    }
}

/* called when a client sends a message to the server */
void messageHandler(int clientID, string message){
    ostringstream os;
    os << "Scumbag " << clientID << " says: " << message;

	//if message == reset, clear the buffer because any remaining messages are now irrelevant, won't affect the gameplay
	if (message == "Reset")
	{
		server.messages.clear();
	}

	//if message is of type ID then save the ids in the server
	if (message.find("ID") != string::npos)
	{
		if (clientID == 0)
		{
			server.id1 = message;
		}

		else if (clientID == 1)
		{
			server.id2 = message;
		}

		//ids still need to be sent immediately
		vector<int> clientIDs = server.getClientIDs();
		for (int i = 0; i < clientIDs.size(); i++){
			if (clientIDs[i] == clientID)
			{
				server.wsSend(clientIDs[i], os.str());
			}
		}
		
		return;
	}

	//instead of directly sending all the messages right here to the client, instead add to a buffer (deque)
	//also add server timestamp to the message
	chrono::milliseconds ms = chrono::duration_cast<chrono::milliseconds>(chrono::system_clock::now().time_since_epoch());
	server.messages.push_back(pair<string, int>(message + ": " + to_string(ms.count()), clientID));
}

/* called once per select() loop */
void periodicHandler(){
	//how we get milliseconds instead of seconds, send every 30-50ms
	static chrono::milliseconds next = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()) + chrono::milliseconds(rand() % 50 + 30);
	chrono::milliseconds current = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch());

	//send out all messages only when set amount of time has passed
    if (current >= next)
	{
        vector<int> clientIDs = server.getClientIDs();	
		int win_count = 0;
		for (int n = 0; n < server.messages.size(); n++)
		{
			//if message == wins, check who sent the win message first, and they are the actual winners, then proceed
			//to send new winner to both clients and erase the other win messages
			if (server.messages[n].first.find("Wins") != string::npos)
			{
				++win_count;
				if (win_count > 1)
				{
					server.messages.erase(server.messages.begin() + n);
				}
			}
		}

		//send rest of messages
		for (int n = 0; n < server.messages.size(); n++)
		{
			for (int i = 0; i < clientIDs.size(); i++)
			{
				//all clients need to receive pickup message
				if (server.messages[n].first.find("Pickup") != string::npos || server.messages[n].first.find("Wins") != string::npos || server.messages[n].first.find("Ties") != string::npos)
				{
					server.wsSend(clientIDs[i], "Scumbag " + to_string(server.messages[n].second) + " says: " + server.messages[n].first);
				}

				//only the opposite client needs to receive messages about current client
				else if (server.messages[n].second != i)
				{
					server.wsSend(clientIDs[i], "Scumbag " + to_string(server.messages[n].second) + " says: " + server.messages[n].first);
				}
			}
		}
		//clear buffer once all messages have been sent
		server.messages.clear();
		//reset next time frame to send
		next = chrono::duration_cast< chrono::milliseconds >(chrono::system_clock::now().time_since_epoch()) + chrono::milliseconds(rand() % 50 + 30);
    }
}

int main(int argc, char *argv[]){
    int port;

    cout << "Please set server port: ";
    cin >> port;

    /* set event handler */
    server.setOpenHandler(openHandler);
    server.setCloseHandler(closeHandler);
    server.setMessageHandler(messageHandler);
    server.setPeriodicHandler(periodicHandler);

    /* start the chatroom server, listen to ip '127.0.0.1' and port '8000' */
    server.startServer(port);

    return 1;
}
